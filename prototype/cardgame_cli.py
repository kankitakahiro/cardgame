import random
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


@dataclass
class CardDef:
    card_id: str
    name: str
    card_type: str  # Unit or Spell
    cost: int
    atk: int = 0
    hp: int = 0
    tags: List[str] = field(default_factory=list)


@dataclass
class UnitInstance:
    card_id: str
    owner: int
    atk: int
    hp: int
    max_hp: int
    can_attack: bool = False
    temp_atk: int = 0
    guard: bool = False
    charge: bool = False

    def current_atk(self) -> int:
        return max(0, self.atk + self.temp_atk)


@dataclass
class PlayerState:
    name: str
    hp: int = 15
    deck: List[str] = field(default_factory=list)
    hand: List[str] = field(default_factory=list)
    board: List[UnitInstance] = field(default_factory=list)
    grave: List[str] = field(default_factory=list)
    mana_max: int = 0
    mana: int = 0
    market_discount: int = 0
    bought_this_turn: bool = False
    spell_cast_this_turn: bool = False
    spell_triggered_ping_done: bool = False


class Game:
    def __init__(self, seed: Optional[int] = None):
        if seed is not None:
            random.seed(seed)
        self.cards: Dict[str, CardDef] = self._build_cards()
        self.market_pool: List[str] = list(self.cards.keys())
        self.market: List[str] = [self._random_market_card() for _ in range(5)]

        starter = [
            "C001", "C002", "C005", "C006", "C008", "C009",
            "C010", "C017", "C018", "C019", "C020", "C021"
        ]

        self.players = [
            PlayerState(name="You", deck=starter.copy()),
            PlayerState(name="CPU", deck=starter.copy()),
        ]
        for p in self.players:
            random.shuffle(p.deck)
            for _ in range(4):
                self._draw_card(p)

        self.turn_player = 0
        self.turn_count = 1
        self.game_over = False

    def _build_cards(self) -> Dict[str, CardDef]:
        return {
            "C001": CardDef("C001", "Pathfinder Scout", "Unit", 1, 1, 1, ["scry"]),
            "C002": CardDef("C002", "Flame Imp", "Unit", 1, 2, 1, []),
            "C003": CardDef("C003", "Shield Trainee", "Unit", 1, 1, 2, ["guard"]),
            "C004": CardDef("C004", "Tiny Researcher", "Unit", 1, 1, 1, ["death_draw"]),
            "C005": CardDef("C005", "Vanguard Raider", "Unit", 2, 2, 2, ["charge"]),
            "C006": CardDef("C006", "Grave Forager", "Unit", 2, 2, 2, ["grave_cycle_draw"]),
            "C007": CardDef("C007", "Market Broker", "Unit", 2, 1, 3, ["buy_discard_draw"]),
            "C008": CardDef("C008", "Rust Golem", "Unit", 2, 3, 2, ["play_discard"]),
            "C009": CardDef("C009", "Road Lancer", "Unit", 3, 3, 3, ["second_play_buff"]),
            "C010": CardDef("C010", "Pursuit Archer", "Unit", 3, 2, 3, ["spell_ping"]),
            "C011": CardDef("C011", "Rebirth Priest", "Unit", 3, 2, 4, ["revive_low_cost_unit"]),
            "C012": CardDef("C012", "Market Overseer", "Unit", 3, 3, 4, ["buy_discount"]),
            "C013": CardDef("C013", "Battle Standard", "Unit", 4, 4, 4, ["team_atk_buff_turn"]),
            "C014": CardDef("C014", "Mausoleum Guard", "Unit", 4, 3, 5, ["guard", "death_recycle_unit"]),
            "C015": CardDef("C015", "Meteor Beast", "Unit", 4, 5, 4, []),
            "C016": CardDef("C016", "Chain Professor", "Unit", 4, 3, 4, ["first_spell_bonus"]),
            "C017": CardDef("C017", "Spark Shot", "Spell", 1, tags=["deal2_unit"]),
            "C018": CardDef("C018", "First Aid", "Spell", 1, tags=["heal3"]),
            "C019": CardDef("C019", "Hand Refinement", "Spell", 1, tags=["discard1_draw2"]),
            "C020": CardDef("C020", "Recruit Call", "Spell", 2, tags=["summon_two_1_1"]),
            "C021": CardDef("C021", "Grave Rekindle", "Spell", 2, tags=["return_spell_self1"]),
            "C022": CardDef("C022", "Market Procurement", "Spell", 2, tags=["gain_market_le3_to_hand"]),
            "C023": CardDef("C023", "Rain of Shots", "Spell", 3, tags=["random_ping4"]),
            "C024": CardDef("C024", "Rally of Reversal", "Spell", 3, tags=["conditional_face_damage"]),
        }

    def _random_market_card(self) -> str:
        return random.choice(self.market_pool)

    def _opponent(self, idx: int) -> int:
        return 1 - idx

    def _draw_card(self, player: PlayerState, n: int = 1) -> bool:
        for _ in range(n):
            if not player.deck:
                return False
            player.hand.append(player.deck.pop())
        return True

    def _check_game_over(self) -> bool:
        for p in self.players:
            if p.hp <= 0:
                self.game_over = True
                return True
        return False

    def _start_turn(self):
        p = self.players[self.turn_player]
        p.mana_max = min(10, p.mana_max + 1)
        p.mana = p.mana_max
        p.market_discount = 0
        p.bought_this_turn = False
        p.spell_cast_this_turn = False
        p.spell_triggered_ping_done = False

        for u in p.board:
            u.can_attack = True
            u.temp_atk = 0

        if not self._draw_card(p):
            # Deck-out defeat
            p.hp = 0
            self._check_game_over()
            return

    def _end_turn(self):
        p = self.players[self.turn_player]
        # Market Broker: optional discard then draw when bought this turn
        if p.bought_this_turn and self._has_tag_on_board(p, "buy_discard_draw") and p.hand:
            if self.turn_player == 0:
                ans = input("Use Market Broker effect? (y/n): ").strip().lower()
                if ans == "y":
                    self._print_hand(p)
                    idx = self._ask_index("Discard index", len(p.hand))
                    if idx is not None:
                        p.grave.append(p.hand.pop(idx))
                        self._draw_card(p)
            else:
                # CPU uses only when hand has low-cost clutter
                low_cost = [i for i, cid in enumerate(p.hand) if self.cards[cid].cost <= 1]
                if low_cost:
                    i = low_cost[0]
                    p.grave.append(p.hand.pop(i))
                    self._draw_card(p)

        self.turn_player = self._opponent(self.turn_player)
        self.turn_count += 1

    def _has_tag_on_board(self, player: PlayerState, tag: str) -> bool:
        for u in player.board:
            if tag in self.cards[u.card_id].tags:
                return True
        return False

    def _play_card(self, idx: int, actor_idx: int):
        p = self.players[actor_idx]
        o = self.players[self._opponent(actor_idx)]

        if idx < 0 or idx >= len(p.hand):
            return

        cid = p.hand[idx]
        c = self.cards[cid]
        if p.mana < c.cost:
            if actor_idx == 0:
                print("Not enough mana.")
            return

        p.mana -= c.cost
        p.hand.pop(idx)

        if c.card_type == "Unit":
            unit = UnitInstance(
                card_id=cid,
                owner=actor_idx,
                atk=c.atk,
                hp=c.hp,
                max_hp=c.hp,
                can_attack=("charge" in c.tags),
                guard=("guard" in c.tags),
                charge=("charge" in c.tags),
            )
            p.board.append(unit)
            self._resolve_unit_on_play(cid, p, o, actor_idx)
        else:
            p.grave.append(cid)
            p.spell_cast_this_turn = True
            self._resolve_spell(cid, p, o, actor_idx)
            self._resolve_spell_triggers(p, o)

        self._cleanup_dead_units()
        self._check_game_over()

    def _resolve_spell_triggers(self, p: PlayerState, o: PlayerState):
        # C010: one ping per turn when you cast spell.
        if not p.spell_triggered_ping_done and self._has_tag_on_board(p, "spell_ping"):
            o.hp -= 1
            p.spell_triggered_ping_done = True
            print(f"{p.name}'s Pursuit Archer pinged enemy leader for 1.")

    def _spell_bonus_damage(self, p: PlayerState, base: int) -> int:
        # C016: first spell each turn +1 damage.
        if self._has_tag_on_board(p, "first_spell_bonus") and not hasattr(p, "first_spell_bonus_spent"):
            setattr(p, "first_spell_bonus_spent", True)
            return base + 1
        return base

    def _reset_turn_soft_flags(self):
        for p in self.players:
            if hasattr(p, "first_spell_bonus_spent"):
                delattr(p, "first_spell_bonus_spent")

    def _resolve_unit_on_play(self, cid: str, p: PlayerState, o: PlayerState, actor_idx: int):
        tags = self.cards[cid].tags
        if "grave_cycle_draw" in tags:
            if p.grave:
                moved = p.grave.pop(0)
                p.deck.insert(0, moved)
            self._draw_card(p)

        if "play_discard" in tags and p.hand:
            if actor_idx == 0:
                self._print_hand(p)
                ans = input("Rust Golem: discard one card? (y/n): ").strip().lower()
                if ans == "y":
                    i = self._ask_index("Discard index", len(p.hand))
                    if i is not None:
                        p.grave.append(p.hand.pop(i))
            else:
                i = min(range(len(p.hand)), key=lambda x: self.cards[p.hand[x]].cost)
                p.grave.append(p.hand.pop(i))

        if "revive_low_cost_unit" in tags:
            valid = [g for g in p.grave if self.cards[g].card_type == "Unit" and self.cards[g].cost <= 1]
            if valid:
                pick = random.choice(valid)
                p.grave.remove(pick)
                p.hand.append(pick)

        if "buy_discount" in tags:
            p.market_discount = max(p.market_discount, 1)

        if "team_atk_buff_turn" in tags:
            for u in p.board:
                if u.card_id != cid:
                    u.temp_atk += 1

    def _resolve_spell(self, cid: str, p: PlayerState, o: PlayerState, actor_idx: int):
        tags = self.cards[cid].tags

        if "deal2_unit" in tags:
            if not o.board:
                return
            target = self._choose_enemy_unit_target(actor_idx, o)
            if target is not None:
                dmg = self._spell_bonus_damage(p, 2)
                o.board[target].hp -= dmg

        if "heal3" in tags:
            p.hp += 3

        if "discard1_draw2" in tags:
            if p.hand:
                if actor_idx == 0:
                    self._print_hand(p)
                    i = self._ask_index("Discard index", len(p.hand))
                    if i is not None:
                        p.grave.append(p.hand.pop(i))
                else:
                    i = min(range(len(p.hand)), key=lambda x: self.cards[p.hand[x]].cost)
                    p.grave.append(p.hand.pop(i))
            self._draw_card(p, 2)

        if "summon_two_1_1" in tags:
            for _ in range(2):
                p.board.append(UnitInstance("TOKEN_1_1", actor_idx, 1, 1, 1, can_attack=False))

        if "return_spell_self1" in tags:
            spells = [g for g in p.grave if self.cards.get(g) and self.cards[g].card_type == "Spell" and g != cid]
            if spells:
                pick = random.choice(spells)
                p.grave.remove(pick)
                p.hand.append(pick)
            p.hp -= 1

        if "gain_market_le3_to_hand" in tags:
            candidates = [i for i, mcid in enumerate(self.market) if self.cards[mcid].cost <= 3]
            if candidates:
                if actor_idx == 0:
                    print("Pick market slot to gain to hand (cost <= 3):")
                    self._print_market()
                    pick = self._ask_index("Market index", len(self.market), filter_indices=candidates)
                    if pick is None:
                        pick = candidates[0]
                else:
                    pick = max(candidates, key=lambda i: self.cards[self.market[i]].cost)
                gained = self.market[pick]
                p.hand.append(gained)
                self.market[pick] = self._random_market_card()

        if "random_ping4" in tags:
            for _ in range(4):
                targets = ["leader"] + [f"unit:{i}" for i in range(len(o.board))]
                if not targets:
                    break
                t = random.choice(targets)
                dmg = self._spell_bonus_damage(p, 1)
                if t == "leader":
                    o.hp -= dmg
                else:
                    idx = int(t.split(":")[1])
                    if idx < len(o.board):
                        o.board[idx].hp -= dmg

        if "conditional_face_damage" in tags:
            dmg = 3 if p.hp <= o.hp else 2
            dmg = self._spell_bonus_damage(p, dmg)
            o.hp -= dmg

    def _choose_enemy_unit_target(self, actor_idx: int, enemy: PlayerState) -> Optional[int]:
        if not enemy.board:
            return None
        if actor_idx == 0:
            print("Choose enemy unit target:")
            for i, u in enumerate(enemy.board):
                print(f"  [{i}] {self.cards.get(u.card_id, CardDef(u.card_id, u.card_id, 'Unit', 0)).name} {u.current_atk()}/{u.hp}")
            return self._ask_index("Target", len(enemy.board))
        # CPU target: lowest hp
        return min(range(len(enemy.board)), key=lambda i: enemy.board[i].hp)

    def _cleanup_dead_units(self):
        for p in self.players:
            i = 0
            while i < len(p.board):
                u = p.board[i]
                if u.hp <= 0:
                    p.board.pop(i)
                    if u.card_id != "TOKEN_1_1":
                        p.grave.append(u.card_id)
                    self._on_unit_death(p, u)
                else:
                    i += 1

    def _on_unit_death(self, owner: PlayerState, unit: UnitInstance):
        if unit.card_id == "C004":
            self._draw_card(owner)
        if unit.card_id == "C014":
            candidates = [g for g in owner.grave if self.cards.get(g) and self.cards[g].card_type == "Unit"]
            if candidates:
                pick = random.choice(candidates)
                owner.grave.remove(pick)
                owner.deck.insert(0, pick)

    def _auto_attack(self, actor_idx: int):
        p = self.players[actor_idx]
        o = self.players[self._opponent(actor_idx)]

        for u in p.board:
            if not u.can_attack or u.hp <= 0:
                continue

            guard_targets = [x for x in o.board if x.guard]
            if guard_targets:
                t = min(guard_targets, key=lambda x: x.hp)
                t.hp -= u.current_atk()
                u.hp -= t.current_atk()
            elif o.board:
                t = min(o.board, key=lambda x: x.hp)
                t.hp -= u.current_atk()
                u.hp -= t.current_atk()
            else:
                o.hp -= u.current_atk()

            u.can_attack = False
            self._cleanup_dead_units()
            if self._check_game_over():
                return

    def _buy_market(self, slot: int, actor_idx: int):
        p = self.players[actor_idx]
        if slot < 0 or slot >= len(self.market):
            return

        cid = self.market[slot]
        c = self.cards[cid]
        final_cost = max(1, c.cost - p.market_discount)
        if p.mana < final_cost:
            if actor_idx == 0:
                print("Not enough mana to buy.")
            return

        p.mana -= final_cost
        p.bought_this_turn = True
        p.grave.append(cid)
        self.market[slot] = self._random_market_card()

    def _print_hand(self, p: PlayerState):
        print("Hand:")
        for i, cid in enumerate(p.hand):
            c = self.cards[cid]
            if c.card_type == "Unit":
                print(f"  [{i}] {cid} {c.name} ({c.cost}) {c.atk}/{c.hp}")
            else:
                print(f"  [{i}] {cid} {c.name} ({c.cost})")

    def _print_board(self, p: PlayerState):
        if not p.board:
            print("  (empty)")
            return
        for i, u in enumerate(p.board):
            if u.card_id == "TOKEN_1_1":
                nm = "Token"
            else:
                nm = self.cards[u.card_id].name
            guard = "G" if u.guard else "-"
            ready = "R" if u.can_attack else "S"
            print(f"  [{i}] {nm} {u.current_atk()}/{u.hp} [{guard}|{ready}]")

    def _print_market(self):
        print("Market:")
        for i, cid in enumerate(self.market):
            c = self.cards[cid]
            print(f"  [{i}] {cid} {c.name} ({c.cost})")

    def _status(self):
        p = self.players[self.turn_player]
        o = self.players[self._opponent(self.turn_player)]
        print("\n" + "=" * 64)
        print(f"Turn {self.turn_count}: {p.name}")
        print(f"You HP {self.players[0].hp} | CPU HP {self.players[1].hp}")
        print(f"Mana: {p.mana}/{p.mana_max}")
        print("Your board:" if self.turn_player == 0 else "CPU board:")
        self._print_board(p)
        print("Enemy board:" if self.turn_player == 0 else "Your board:")
        self._print_board(o)
        if self.turn_player == 0:
            self._print_hand(p)
            self._print_market()
        print("=" * 64)

    def _ask_index(self, label: str, n: int, filter_indices: Optional[List[int]] = None) -> Optional[int]:
        raw = input(f"{label} [0-{n-1}] (blank=cancel): ").strip()
        if raw == "":
            return None
        if not raw.isdigit():
            return None
        i = int(raw)
        if i < 0 or i >= n:
            return None
        if filter_indices is not None and i not in filter_indices:
            return None
        return i

    def _human_turn(self):
        while not self.game_over:
            self._status()
            cmd = input("Command (play N / buy N / attack / end / help): ").strip().lower()
            if cmd == "help":
                print("play N: play hand card index N")
                print("buy N: buy market slot N")
                print("attack: auto attack with all ready units")
                print("end: end your turn")
                continue
            if cmd.startswith("play "):
                parts = cmd.split()
                if len(parts) == 2 and parts[1].isdigit():
                    self._play_card(int(parts[1]), 0)
                    if self._check_game_over():
                        break
                continue
            if cmd.startswith("buy "):
                parts = cmd.split()
                if len(parts) == 2 and parts[1].isdigit():
                    self._buy_market(int(parts[1]), 0)
                continue
            if cmd == "attack":
                self._auto_attack(0)
                if self._check_game_over():
                    break
                continue
            if cmd == "end":
                break
            print("Unknown command. Type 'help'.")

    def _cpu_turn(self):
        p = self.players[1]
        acted = True
        while acted and not self.game_over:
            acted = False

            affordable = [(i, self.cards[cid].cost) for i, cid in enumerate(p.hand) if self.cards[cid].cost <= p.mana]
            if affordable:
                # Prefer highest cost, and prefer units slightly.
                affordable.sort(key=lambda x: (x[1], self.cards[p.hand[x[0]]].card_type == "Unit"), reverse=True)
                self._play_card(affordable[0][0], 1)
                acted = True
                if self._check_game_over():
                    return
                continue

            buy_options: List[Tuple[int, int]] = []
            for i, cid in enumerate(self.market):
                cost = max(1, self.cards[cid].cost - p.market_discount)
                if cost <= p.mana:
                    buy_options.append((i, self.cards[cid].cost))
            if buy_options:
                buy_options.sort(key=lambda x: x[1], reverse=True)
                self._buy_market(buy_options[0][0], 1)
                acted = True

        self._auto_attack(1)

    def run(self):
        print("Card Game Prototype v0.1")
        print("Rules: 1v1, shared mana for buy/play, market has 5 open slots.")

        while not self.game_over:
            self._reset_turn_soft_flags()
            self._start_turn()
            if self.game_over:
                break

            if self.turn_player == 0:
                self._human_turn()
            else:
                self._cpu_turn()

            if self._check_game_over():
                break
            self._end_turn()

        if self.players[0].hp <= 0 and self.players[1].hp <= 0:
            print("Draw game.")
        elif self.players[0].hp <= 0:
            print("CPU wins.")
        else:
            print("You win.")


if __name__ == "__main__":
    game = Game()
    game.run()
