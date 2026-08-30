#include "CGPlayerState.h"
#include "CGCardDatabase.h"
#include "CGGameState.h"

// カード効果ディスパッチ(docs/architecture.md「カード効果ディスパッチ」)。
// 以前はResolveSpellEffect/ResolveUnitOnPlayEffectがEffectId文字列で分岐する
// if/elseの塊で、カードが増えるほど際限なく伸びる作りだった。
// EffectId -> ハンドラ関数 のテーブルに置き換え、新しいカード効果を足すときは
// 「Handle_XXX関数を1つ書いて、対応するGet*Handlers()のテーブルに1行足すだけ」で
// 済むようにしている。ハンドラは全て静的な関数ポインタ(状態を持たない)なので、
// Side0/Side1どちらのACGPlayerStateインスタンスに対しても同じテーブルを使い回せる。
namespace
{
	using FSpellEffectHandler = void(*)(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus);
	using FUnitOnPlayEffectHandler = void(*)(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent);
	using FUnitOnDeathEffectHandler = void(*)(ACGPlayerState& Self, const FCGCardDef& Def, int32& OutDrawCount);
	using FEndTurnAuraEffectHandler = void(*)(ACGPlayerState& Self);

	// ダメージ系スペルで繰り返し使う「対象ユニット指定があればそこへ、なければ先頭ユニット、
	// ユニットが1体もいなければ顔面へ」という単一対象フォールバックルール
	// (docs/game-rules-minimum.md)をまとめたヘルパー。
	void DealDamageToTargetOrFallback(ACGPlayerState& Opponent, int32 TargetUnitIndex, int32 Damage)
	{
		if (Opponent.BoardUnits.IsValidIndex(TargetUnitIndex))
		{
			Opponent.ApplyDamageToUnit(TargetUnitIndex, Damage);
		}
		else if (Opponent.BoardUnits.Num() > 0)
		{
			Opponent.ApplyDamageToUnit(0, Damage);
		}
		else
		{
			Opponent.ApplyDamage(Damage);
		}
	}

	// --- Spell効果ハンドラ ---

	void Handle_OnPlayDamageTarget(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus)
	{
		if (!Opponent)
		{
			return;
		}
		DealDamageToTargetOrFallback(*Opponent, TargetUnitIndex, Def.EffectValue + FirstSpellDamageBonus);
	}

	void Handle_OnPlayHealSelf(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus)
	{
		Self.Heal(Def.EffectValue);
	}

	void Handle_Discard1Draw2(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C019 手札の選別
	{
		Self.DiscardRandomFromHand();
		Self.DrawCard();
		Self.DrawCard();
	}

	void Handle_Summon2x1_1Unit(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C020 見習い召集
	{
		Self.AddBoardUnitDirect(FName(TEXT("TK_APPRENTICE")), 1, 1, false, false);
		Self.AddBoardUnitDirect(FName(TEXT("TK_APPRENTICE")), 1, 1, false, false);
	}

	void Handle_ReturnGraveyardSpellSelfDamage1(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C021 墓地再点火
	{
		if (Self.TryReturnRandomSpellFromDiscardToHand())
		{
			Self.ApplyDamage(1);
		}
	}

	void Handle_BuyFromMarketCostUnder3ToHand(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C022 市場調達
	{
		if (!CGState || Self.HandCardIds.Num() >= 10)
		{
			return;
		}
		for (int32 i = 0; i < CGState->MarketCardIds.Num(); ++i)
		{
			FCGCardDef MarketDef;
			if (UCGCardDatabase::FindCard(CGState->MarketCardIds[i], MarketDef) && MarketDef.Cost <= 3)
			{
				Self.HandCardIds.Add(CGState->MarketCardIds[i]);
				CGState->MarketCardIds.RemoveAt(i);
				CGState->RefillMarket();
				break;
			}
		}
	}

	void Handle_RandomEnemyDamage1x4(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C023 連弾の雨
	{
		if (!Opponent)
		{
			return;
		}
		for (int32 Hit = 0; Hit < 4; ++Hit)
		{
			if (Opponent->BoardUnits.Num() > 0)
			{
				const int32 RandomUnitIndex = FMath::RandRange(0, Opponent->BoardUnits.Num() - 1);
				Opponent->ApplyDamageToUnit(RandomUnitIndex, 1);
			}
			else
			{
				Opponent->ApplyDamage(1);
			}
		}
	}

	void Handle_ConditionalDamage3or2(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C024 逆転の号令
	{
		if (!Opponent)
		{
			return;
		}
		const int32 Damage = ((Self.CurrentHP <= Opponent->CurrentHP) ? 3 : 2) + FirstSpellDamageBonus;
		DealDamageToTargetOrFallback(*Opponent, TargetUnitIndex, Damage);
	}

	const TMap<FName, FSpellEffectHandler>& GetSpellEffectHandlers()
	{
		static const TMap<FName, FSpellEffectHandler> Handlers = {
			{ FName(CGEffectId::OnPlayDamageTarget), &Handle_OnPlayDamageTarget },
			{ FName(CGEffectId::OnPlayHealSelf), &Handle_OnPlayHealSelf },
			{ FName(CGEffectId::Discard1Draw2), &Handle_Discard1Draw2 },
			{ FName(CGEffectId::Summon2x1_1Unit), &Handle_Summon2x1_1Unit },
			{ FName(CGEffectId::ReturnGraveyardSpellSelfDamage1), &Handle_ReturnGraveyardSpellSelfDamage1 },
			{ FName(CGEffectId::BuyFromMarketCostUnder3ToHand), &Handle_BuyFromMarketCostUnder3ToHand },
			{ FName(CGEffectId::RandomEnemyDamage1x4), &Handle_RandomEnemyDamage1x4 },
			{ FName(CGEffectId::ConditionalDamage3or2), &Handle_ConditionalDamage3or2 },
		};
		return Handlers;
	}

	// --- Unit登場時効果ハンドラ ---

	void Handle_OnPlayDiscard1(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent) // C008 錆びた巨兵
	{
		Self.DiscardRandomFromHand();
	}

	void Handle_OnPlayReturnGraveyardCheapCard(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent) // C011 再誕の司祭
	{
		Self.TryReturnCheapestFromDiscardToHand(1);
	}

	void Handle_GraveyardToDeckBottomDraw1(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent) // C006 墓場あさり
	{
		Self.TryMoveRandomDiscardCardToDeckBottom();
		Self.DrawCard();
	}

	void Handle_ScoutTop1(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent) // C001 先駆けの斥候
	{
		Self.ScryTop();
	}

	const TMap<FName, FUnitOnPlayEffectHandler>& GetUnitOnPlayEffectHandlers()
	{
		static const TMap<FName, FUnitOnPlayEffectHandler> Handlers = {
			{ FName(CGEffectId::OnPlayDiscard1), &Handle_OnPlayDiscard1 },
			{ FName(CGEffectId::OnPlayReturnGraveyardCheapCard), &Handle_OnPlayReturnGraveyardCheapCard },
			{ FName(CGEffectId::GraveyardToDeckBottomDraw1), &Handle_GraveyardToDeckBottomDraw1 },
			{ FName(CGEffectId::ScoutTop1), &Handle_ScoutTop1 },
		};
		return Handlers;
	}

	// --- Unit死亡時効果ハンドラ ---

	void Handle_OnDeathDraw(ACGPlayerState& Self, const FCGCardDef& Def, int32& OutDrawCount)
	{
		OutDrawCount += Def.EffectValue;
	}

	void Handle_OnDeathReturnRandomGraveyardUnit(ACGPlayerState& Self, const FCGCardDef& Def, int32& OutDrawCount) // C014 霊廟の守り手
	{
		Self.TryMoveRandomDiscardUnitToDeckTop();
	}

	const TMap<FName, FUnitOnDeathEffectHandler>& GetUnitOnDeathEffectHandlers()
	{
		static const TMap<FName, FUnitOnDeathEffectHandler> Handlers = {
			{ FName(CGEffectId::OnDeathDraw), &Handle_OnDeathDraw },
			{ FName(CGEffectId::OnDeathReturnRandomGraveyardUnit), &Handle_OnDeathReturnRandomGraveyardUnit },
		};
		return Handlers;
	}

	// --- ターン終了時の常在効果ハンドラ(場にそのEffectIdを持つUnitがいれば発動) ---

	void Handle_OnBuyEndTurnDiscardDraw(ACGPlayerState& Self) // C007 市場の仲買人
	{
		if (Self.bBoughtThisTurn && Self.HandCardIds.Num() > 0)
		{
			Self.DiscardRandomFromHand();
			Self.DrawCard();
		}
	}

	const TMap<FName, FEndTurnAuraEffectHandler>& GetEndTurnAuraEffectHandlers()
	{
		static const TMap<FName, FEndTurnAuraEffectHandler> Handlers = {
			{ FName(CGEffectId::OnBuyEndTurnDiscardDraw), &Handle_OnBuyEndTurnDiscardDraw },
		};
		return Handlers;
	}
}

void ACGPlayerState::InitializeStartingDeck(const TArray<FName>& StarterCardIds)
{
	DeckCardIds = StarterCardIds;
	HandCardIds.Reset();
	DiscardCardIds.Reset();
	BoardUnits.Reset();
	ShuffleDeck();
}

void ACGPlayerState::ShuffleDeck()
{
	const int32 LastIndex = DeckCardIds.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		const int32 SwapIndex = FMath::RandRange(i, LastIndex);
		DeckCardIds.Swap(i, SwapIndex);
	}
}

bool ACGPlayerState::DrawCard()
{
	if (DeckCardIds.Num() == 0)
	{
		// 山札切れ負け(docs/game-rules-minimum.md)
		bIsDefeated = true;
		return false;
	}

	const FName Top = DeckCardIds[0];
	DeckCardIds.RemoveAt(0);

	// 手札上限10を超える分は、ルール未規定のため引いた瞬間に捨て札へ送る運用としている。
	if (HandCardIds.Num() < 10)
	{
		HandCardIds.Add(Top);
	}
	else
	{
		DiscardCardIds.Add(Top);
	}
	return true;
}

bool ACGPlayerState::PlayCardFromHand(FName CardId, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState)
{
	if (!HandCardIds.Contains(CardId))
	{
		return false;
	}

	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return false;
	}

	if (CurrentMana < Def.Cost)
	{
		return false;
	}

	HandCardIds.RemoveSingle(CardId);
	CurrentMana -= Def.Cost;

	// 街道の突撃兵(C009)判定用: このカードが「今ターン2枚目以降」かどうかを、
	// カウンタをインクリメントする前に確定させる。
	const bool bIsSecondOrLaterPlayThisTurn = (CardsPlayedThisTurn >= 1);
	CardsPlayedThisTurn++;

	if (Def.CardType == ECGCardType::Unit)
	{
		int32 EnterAtk = Def.Atk;
		if (Def.EffectId == FName(CGEffectId::SecondPlayBuff) && bIsSecondOrLaterPlayThisTurn) // C009 街道の突撃兵
		{
			EnterAtk += 1;
		}

		FCGBoardUnit NewUnit;
		NewUnit.CardId = CardId;
		NewUnit.Atk = EnterAtk;
		NewUnit.Hp = Def.Hp;
		NewUnit.bCanAttack = Def.HasTag(TEXT("Haste"));
		NewUnit.bHasGuard = Def.HasTag(TEXT("Guard"));
		BoardUnits.Add(NewUnit);
		ResolveUnitOnPlayEffect(Def, Opponent);

		if (Def.EffectId == FName(CGEffectId::AllyBuffAtkThisTurn)) // C013 戦場の旗手
		{
			// 簡易実装: 本来は「ターン中のみ」の一時バフだが、一時バフ管理の仕組みを
			// 新設するコストを避けるため、登場時点にいる味方(このユニット自身を除く)へ
			// 永続的に+1/+0を付与する形に簡略化している(docs/game-rules-minimum.md「未実装・今後の検討事項」参照)。
			for (int32 i = 0; i < BoardUnits.Num() - 1; ++i)
			{
				BoardUnits[i].Atk += 1;
			}
		}
	}
	else
	{
		SpellsPlayedThisTurn++;
		ResolveSpellEffect(Def, Opponent, TargetUnitIndex, CGState);
		DiscardCardIds.Add(CardId);

		// 追撃の射手(C010): 味方Spell使用時、1ターンに1回だけ敵リーダーへ1ダメージ。
		if (Opponent && !bAllySpellPingUsedThisTurn && HasBoardUnitWithEffect(FName(CGEffectId::OnAllySpellPing1)))
		{
			Opponent->ApplyDamage(1);
			bAllySpellPingUsedThisTurn = true;
		}
	}

	return true;
}

void ACGPlayerState::ResolveSpellEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState)
{
	// 連鎖術の教授(C016)判定用: このSpellが「今ターン最初のSpell」かどうか
	// (呼び出し元でSpellsPlayedThisTurnをインクリメント済みのため、1なら初回)。
	const int32 FirstSpellDamageBonus =
		(SpellsPlayedThisTurn == 1 && HasBoardUnitWithEffect(FName(CGEffectId::FirstSpellBonusDamage))) ? 1 : 0;

	// 実装済みの効果のみテーブルに登録されている。未登録のEffectId(TODO_接頭辞等)は
	// 何もしない(docs/architecture.md「カード効果ディスパッチ」参照)。
	if (const FSpellEffectHandler* Handler = GetSpellEffectHandlers().Find(Def.EffectId))
	{
		(*Handler)(*this, Opponent, Def, TargetUnitIndex, CGState, FirstSpellDamageBonus);
	}
}

void ACGPlayerState::ResolveUnitOnPlayEffect(const FCGCardDef& Def, ACGPlayerState* Opponent)
{
	if (const FUnitOnPlayEffectHandler* Handler = GetUnitOnPlayEffectHandlers().Find(Def.EffectId))
	{
		(*Handler)(*this, Def, Opponent);
	}
}

bool ACGPlayerState::DiscardRandomFromHand()
{
	if (HandCardIds.Num() == 0)
	{
		return false;
	}
	const int32 Index = FMath::RandRange(0, HandCardIds.Num() - 1);
	DiscardCardIds.Add(HandCardIds[Index]);
	HandCardIds.RemoveAt(Index);
	return true;
}

bool ACGPlayerState::TryReturnCheapestFromDiscardToHand(int32 MaxCost)
{
	if (HandCardIds.Num() >= 10)
	{
		return false;
	}
	for (int32 i = 0; i < DiscardCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(DiscardCardIds[i], Def) && Def.Cost <= MaxCost)
		{
			HandCardIds.Add(DiscardCardIds[i]);
			DiscardCardIds.RemoveAt(i);
			return true;
		}
	}
	return false;
}

bool ACGPlayerState::TryMoveRandomDiscardCardToDeckBottom()
{
	if (DiscardCardIds.Num() == 0)
	{
		return false;
	}
	const int32 Index = FMath::RandRange(0, DiscardCardIds.Num() - 1);
	DeckCardIds.Add(DiscardCardIds[Index]); // DrawCardは先頭(index 0)から引くため、末尾に追加=デッキの一番下。
	DiscardCardIds.RemoveAt(Index);
	return true;
}

bool ACGPlayerState::TryReturnRandomSpellFromDiscardToHand()
{
	if (HandCardIds.Num() >= 10)
	{
		return false;
	}
	for (int32 i = 0; i < DiscardCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(DiscardCardIds[i], Def) && Def.CardType == ECGCardType::Spell)
		{
			HandCardIds.Add(DiscardCardIds[i]);
			DiscardCardIds.RemoveAt(i);
			return true;
		}
	}
	return false;
}

bool ACGPlayerState::TryMoveRandomDiscardUnitToDeckTop()
{
	TArray<int32> UnitIndices;
	for (int32 i = 0; i < DiscardCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(DiscardCardIds[i], Def) && Def.CardType == ECGCardType::Unit)
		{
			UnitIndices.Add(i);
		}
	}
	if (UnitIndices.Num() == 0)
	{
		return false;
	}
	const int32 Chosen = UnitIndices[FMath::RandRange(0, UnitIndices.Num() - 1)];
	DeckCardIds.Insert(DiscardCardIds[Chosen], 0);
	DiscardCardIds.RemoveAt(Chosen);
	return true;
}

void ACGPlayerState::ScryTop()
{
	if (DeckCardIds.Num() == 0)
	{
		return;
	}
	FCGCardDef TopDef;
	if (!UCGCardDatabase::FindCard(DeckCardIds[0], TopDef))
	{
		return;
	}
	// プレイヤー操作の代わりの簡易ヒューリスティック: 今すぐ(+1マナ後まで)払えなさそうな
	// 高コストカードなら山札の一番下へ送り、そうでなければ一番上に残す。
	if (TopDef.Cost > CurrentMana + 1)
	{
		const FName Top = DeckCardIds[0];
		DeckCardIds.RemoveAt(0);
		DeckCardIds.Add(Top);
	}
}

void ACGPlayerState::AddBoardUnitDirect(FName CardId, int32 Atk, int32 Hp, bool bCanAttackImmediately, bool bHasGuard)
{
	FCGBoardUnit NewUnit;
	NewUnit.CardId = CardId;
	NewUnit.Atk = Atk;
	NewUnit.Hp = Hp;
	NewUnit.bCanAttack = bCanAttackImmediately;
	NewUnit.bHasGuard = bHasGuard;
	BoardUnits.Add(NewUnit);
}

bool ACGPlayerState::BuyCard(FName CardId)
{
	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return false;
	}

	// 市場監督官(C012)が場にいる間、購入コストを1軽減(最小1)。
	const int32 EffectiveCost = HasBoardUnitWithEffect(FName(CGEffectId::BuyCostReductionThisTurn))
		? FMath::Max(1, Def.Cost - 1)
		: Def.Cost;

	if (CurrentMana < EffectiveCost || HandCardIds.Num() >= 10)
	{
		return false;
	}

	CurrentMana -= EffectiveCost;
	HandCardIds.Add(CardId);
	bBoughtThisTurn = true;
	return true;
}

void ACGPlayerState::ApplyDamage(int32 Amount)
{
	CurrentHP = FMath::Max(0, CurrentHP - Amount);
	if (CurrentHP <= 0)
	{
		bIsDefeated = true;
	}
}

void ACGPlayerState::Heal(int32 Amount)
{
	CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);
}

void ACGPlayerState::ApplyDamageToUnit(int32 UnitIndex, int32 Amount)
{
	if (!BoardUnits.IsValidIndex(UnitIndex))
	{
		return;
	}
	BoardUnits[UnitIndex].Hp -= Amount;
}

int32 ACGPlayerState::RemoveDeadUnitsAndGetDeathDrawCount()
{
	int32 DrawCount = 0;
	for (int32 i = BoardUnits.Num() - 1; i >= 0; --i)
	{
		if (BoardUnits[i].Hp > 0)
		{
			continue;
		}

		FCGCardDef Def;
		const bool bFound = UCGCardDatabase::FindCard(BoardUnits[i].CardId, Def);

		DiscardCardIds.Add(BoardUnits[i].CardId);
		BoardUnits.RemoveAt(i);

		if (bFound)
		{
			if (const FUnitOnDeathEffectHandler* Handler = GetUnitOnDeathEffectHandlers().Find(Def.EffectId))
			{
				(*Handler)(*this, Def, DrawCount);
			}
		}
	}
	return DrawCount;
}

bool ACGPlayerState::HasGuardUnit() const
{
	return BoardUnits.ContainsByPredicate([](const FCGBoardUnit& Unit) { return Unit.bHasGuard; });
}

bool ACGPlayerState::HasBoardUnitWithEffect(FName EffectId) const
{
	for (const FCGBoardUnit& Unit : BoardUnits)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(Unit.CardId, Def) && Def.EffectId == EffectId)
		{
			return true;
		}
	}
	return false;
}

void ACGPlayerState::RefreshManaForNewTurn()
{
	// docs/initial-cards-v0.1.md「毎ターン自動増加マナ」。上限10は暫定ルール。
	MaxMana = FMath::Min(MaxMana + 1, 10);
	CurrentMana = MaxMana;
	CardsPlayedThisTurn = 0;
	SpellsPlayedThisTurn = 0;
	bBoughtThisTurn = false;
	bAllySpellPingUsedThisTurn = false;
}

void ACGPlayerState::ResolveEndTurnEffects()
{
	for (const TPair<FName, FEndTurnAuraEffectHandler>& Pair : GetEndTurnAuraEffectHandlers())
	{
		if (HasBoardUnitWithEffect(Pair.Key))
		{
			Pair.Value(*this);
		}
	}
}
