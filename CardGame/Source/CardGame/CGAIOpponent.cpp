#include "CGAIOpponent.h"
#include "CGGameMode.h"
#include "CGGameState.h"
#include "CGPlayerState.h"
#include "CGCardDatabase.h"

namespace
{
	// 対象を必要とする効果が、対象となるユニットが1体も無いために不発になるか
	// どうかを判定する。RunTurn()のプレイ選択で、こうした「出しても何も起きない」
	// カードを除外するのに使う(断罪/対象指定バフ等を持つカードが対象不在で
	// 空撃ちされ、外からは「何もして来ない」ように見えてしまう不具合への対策)。
	// 厳密な候補判定(コスト上限フィルタ等)はACGGameMode側の各Resolve関数が
	// 別途行うため、ここでは「そもそも対象になり得るUnitが存在するか」だけを見る。
	bool WouldCardEffectBeUseless(const FCGCardDef& Def, const ACGPlayerState* Self, const ACGPlayerState* Opponent)
	{
		if (Def.EffectId == FName(CGEffectId::SealSpell) || Def.EffectId == FName(CGEffectId::SealOnPlay)
			|| Def.EffectId == FName(CGEffectId::SealSpellGrantPurchaseMana) || Def.EffectId == FName(CGEffectId::SealSpellTwo)
			|| Def.EffectId == FName(CGEffectId::OnPlayDebuffTarget) || Def.EffectId == FName(CGEffectId::MassDebuffEnemies)
			|| Def.EffectId == FName(CGEffectId::OnPlayDamageUnitTarget))
		{
			return !Opponent || Opponent->BoardUnits.Num() == 0;
		}
		if (Def.EffectId == FName(CGEffectId::BuffAllyTarget) || Def.EffectId == FName(CGEffectId::OnPlayBuffAllyTarget)
			|| Def.EffectId == FName(CGEffectId::BuffAllyTargetAndDraw) || Def.EffectId == FName(CGEffectId::OnPlayBuffAllyTargetAndUnitDiscount)
			|| Def.EffectId == FName(CGEffectId::BuffAllAlliesFlat) || Def.EffectId == FName(CGEffectId::BuffAllAlliesAtkOnly)
			|| Def.EffectId == FName(CGEffectId::GrantBoardBuffOnPurchaseThisTurn))
		{
			return !Self || Self->BoardUnits.Num() == 0;
		}
		// 生け贄(Sacrifice、R15黒鉄を継ぐ大剣士): 道連れにする自分のUnitが1体も
		// いなければそもそもプレイできない(ACGPlayerState::PlayCardFromHand側の
		// legality checkと同じ条件。ここで弾いておかないと、RunTurn()のプレイ
		// ループがRequestPlayCard失敗でそのターンの残りの候補評価ごと止まって
		// しまう)。
		if (Def.HasTag(TEXT("Sacrifice")))
		{
			return !Self || Self->BoardUnits.Num() == 0;
		}
		return false;
	}

	// カードをこのタイミングでプレイする「今出す価値」をスコア化する。単純な
	// コスト降順(旧実装)だと、同ターン中に他のUnitを出してから使うべきカード
	// (R09連携の立会人等)や、味方が並んでから撃つべき全体バフが、たまたま
	// コストの都合が合わない限り腐ってしまう。「自分に得なカードの効果が
	// できるだけ発動し、勝利に近づくような行動をしてほしい」というフィード
	// バックへの対応として、カード同士の噛み合わせ(シナジー)と盤面状況を
	// 加味したスコアに基づいて選ぶようにする。
	int32 ScorePlayCandidate(const FCGCardDef& Def, const ACGPlayerState* Self, const ACGPlayerState* Opponent)
	{
		// ベースはコスト(高コスト=マナを無駄にしない)。Unitは場に残って攻撃・
		// キーワード発動源になるため、Spellより基本的に優先する(対象指定Spell
		// ばかり出て場が育たない問題への既存の対策を踏襲)。
		int32 Score = Def.Cost * 10;
		if (Def.CardType == ECGCardType::Unit)
		{
			Score += 1000;
		}

		const int32 SelfAllyCount = Self ? Self->BoardUnits.Num() : 0;
		const int32 EnemyCount = Opponent ? Opponent->BoardUnits.Num() : 0;

		// R09連携の立会人/C009廃墟街道の突撃兵(SecondPlayBuff): このターン
		// 他のカードを1枚もプレイしていなければ+2/+0が発動しないため後回しにし、
		// 既に1枚以上プレイ済みなら発動を確実にするため優先度を上げる
		// (発動条件はACGPlayerState::PlayCardFromHand「CardsPlayedThisTurn>=1」と一致させる)。
		if (Def.EffectId == FName(CGEffectId::SecondPlayBuff))
		{
			Score += (Self && Self->CardsPlayedThisTurn > 0) ? 500 : -1500;
		}

		// 場全体バフ(G08祈りの輪/G14世界樹の加護等)は対象が多いほど得なので、
		// 味方Unit数に応じてボーナスを乗せる(1体もいない/1体だけでは損なので
		// 減点し、場が育つまで後回しにする。WouldCardEffectBeUselessは「0体なら
		// 不発」しか見ていないため、ここで段階的な価値を評価する)。
		if (Def.EffectId == FName(CGEffectId::BuffAllAlliesFlat) || Def.EffectId == FName(CGEffectId::BuffAllAlliesAtkOnly)
			|| Def.EffectId == FName(CGEffectId::GrantBoardBuffOnPurchaseThisTurn))
		{
			Score += SelfAllyCount * 300 - 400;
		}

		// 対象指定の味方強化(P02/P04/P07/P10/P05等): 味方が既にいるほど「伸ばす」
		// 選択肢が増える(ChooseAllyBuffTargetが一番育ったユニットを選ぶため、
		// 場に複数いる方が強化が無駄になりにくい)。
		if (Def.EffectId == FName(CGEffectId::BuffAllyTarget) || Def.EffectId == FName(CGEffectId::OnPlayBuffAllyTarget)
			|| Def.EffectId == FName(CGEffectId::BuffAllyTargetAndDraw) || Def.EffectId == FName(CGEffectId::OnPlayBuffAllyTargetAndUnitDiscount))
		{
			Score += FMath::Min(SelfAllyCount, 3) * 150;
		}

		// 断罪/対象指定デバフ(B02等)・全体デバフ(B12禁書一斉開帳): 敵の場が
		// 育っているほど脅威除去の価値が高いため、敵Unit数に応じてボーナスを
		// 乗せる(逆に敵が1体も育っていないうちに使うのは弱い相手を封じるだけで
		// 損なので、既存の「対象0体なら不発」判定より一歩踏み込んで評価する)。
		if (Def.HasTag(TEXT("Seal")) || Def.EffectId == FName(CGEffectId::OnPlayDebuffTarget)
			|| Def.EffectId == FName(CGEffectId::MassDebuffEnemies))
		{
			Score += FMath::Min(EnemyCount, 3) * 150;
		}

		// 分身(Clone)持ちUnit: 味方Unitが多いほど発動条件(味方N体以上等)を
		// 満たしやすいため、場が育っているときに優先度を上げる。
		if (!Def.CloneConditionId.IsNone())
		{
			Score += FMath::Min(SelfAllyCount, 3) * 100;
		}

		// 先物(Discount)持ちUnit: 経済を早めに育てておくほど後続のプレイ/購入が
		// 楽になるため、わずかに優先する。
		if (Def.HasTag(TEXT("Discount")))
		{
			Score += 50;
		}

		return Score;
	}

	// 今の(CurrentMana+PurchaseMana、割引込み)で買えるマーケットカードが1枚でも
	// あるか。「空撃ちするくらいならマーケットから購入してほしい」というフィード
	// バックへの対応で、シナジーの無い弱いスペルを温存して購入に回すかどうかの
	// 判定に使う(RunTurn参照)。
	bool HasAffordableMarketCard(const ACGGameState& CGState, const ACGPlayerState& Self)
	{
		const int32 Budget = Self.CurrentMana + Self.PurchaseMana;
		for (const FCGMarketSlot& Slot : CGState.MarketSlots)
		{
			if (Slot.CardId.IsNone())
			{
				continue;
			}
			FCGCardDef Def;
			if (!UCGCardDatabase::FindCard(Slot.CardId, Def))
			{
				continue;
			}
			const int32 EffectiveCost = FMath::Max(0, Def.Cost - Self.ComputeCurrentPurchaseDiscount());
			if (EffectiveCost <= Budget)
			{
				return true;
			}
		}
		return false;
	}
}

FName UCGAIOpponent::ChooseHandCardToDiscard(const ACGPlayerState& Self)
{
	FName Best = NAME_None;
	int32 BestCost = -1;
	for (const FName& CardId : Self.HandCardIds)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(CardId, Def) && Def.Cost > BestCost)
		{
			BestCost = Def.Cost;
			Best = CardId;
		}
	}
	return Best;
}

int32 UCGAIOpponent::ChooseAttackTarget(const ACGPlayerState& Attacker, const ACGPlayerState& Defender, int32 AttackerUnitIndex)
{
	if (!Attacker.BoardUnits.IsValidIndex(AttackerUnitIndex))
	{
		return -1;
	}

	// 赤は相手を削りきることを最大の目標にする(盤面のUnitを無視して常に顔面を
	// 狙う)。顔面攻撃は反撃を受けないため、純粋なレースとして常に合理的でもある。
	if (Attacker.ActiveColors.Contains(ECGColor::Red))
	{
		return -1;
	}

	// 緑パッシブ等、盤面状況で変動する継続的なボーナスを含む実効値で判断する
	// (ACGPlayerState::GetEffectiveAtk参照)。
	const int32 MyAtk = Attacker.GetEffectiveAtk(AttackerUnitIndex);
	const int32 MyHp = Attacker.BoardUnits[AttackerUnitIndex].Hp;

	int32 BestIndex = -1;
	int32 BestScore = -1;
	for (int32 i = 0; i < Defender.BoardUnits.Num(); ++i)
	{
		const FCGBoardUnit& Target = Defender.BoardUnits[i];
		const bool bCanKill = MyAtk >= Target.Hp;
		if (!bCanKill)
		{
			// 倒せない相手を殴っても打点の無駄になりやすいため、簡易的に除外する。
			continue;
		}
		const int32 TargetAtk = Defender.GetEffectiveAtk(i);
		const bool bSurvive = TargetAtk < MyHp;
		const int32 Score = TargetAtk + Target.Hp + (bSurvive ? 10 : 0);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestIndex = i;
		}
	}
	return BestIndex; // 見つからなければ顔面(-1)。
}

int32 UCGAIOpponent::ChooseDamageTarget(const ACGPlayerState& Self, const ACGPlayerState& Opponent, int32 Damage, bool bRequireUnitTarget)
{
	// 赤は相手を削りきることを最大の目標にする(常に顔面を狙う。ChooseAttackTarget
	// 参照)。ただしbRequireUnitTarget(R05黒鉄の抜き打ち等)のときは顔面を選べない
	// ため、この近道は使わず必ず敵Unitを選ぶ。
	if (!bRequireUnitTarget && Self.ActiveColors.Contains(ECGColor::Red))
	{
		return -1;
	}

	int32 BestIndex = -1;
	int32 BestHp = TNumericLimits<int32>::Max();
	for (int32 i = 0; i < Opponent.BoardUnits.Num(); ++i)
	{
		const FCGBoardUnit& Target = Opponent.BoardUnits[i];
		if (Target.Hp <= Damage && Target.Hp < BestHp)
		{
			BestHp = Target.Hp;
			BestIndex = i;
		}
	}
	if (BestIndex == -1 && bRequireUnitTarget)
	{
		// 打点で倒せる相手がいなくても、顔面は選べないため一番HPが低い
		// (倒すのに一番近い)ユニットを選ぶ。
		for (int32 i = 0; i < Opponent.BoardUnits.Num(); ++i)
		{
			if (Opponent.BoardUnits[i].Hp < BestHp)
			{
				BestHp = Opponent.BoardUnits[i].Hp;
				BestIndex = i;
			}
		}
	}
	return BestIndex; // bRequireUnitTargetがfalseで見つからなければ顔面(-1)。
}

bool UCGAIOpponent::ChooseKeepOnTop(const ACGPlayerState& Self, FName RevealedCardId)
{
	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(RevealedCardId, Def))
	{
		return true; // 情報が取れなければ無難に残す。
	}
	return Def.Cost <= Self.CurrentMana + 1;
}

bool UCGAIOpponent::ChooseBuyDestination(const ACGPlayerState& Self, FName CardId)
{
	// 手札が上限(10)なら選択の余地なく山札下へ。
	if (Self.HandCardIds.Num() >= 10)
	{
		return false;
	}

	// 手札に十分余裕があるうちは、すぐ使える手札を優先する。
	if (Self.HandCardIds.Num() < 8)
	{
		return true;
	}

	// 手札が埋まってきた場合は、すぐには払えなさそうな高コストカードだけ
	// 山札の一番下へ送り、安いカードは引き続き手札で構える。
	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return true; // 情報が取れなければ無難に手札へ。
	}
	return Def.Cost <= Self.CurrentMana + 1;
}

int32 UCGAIOpponent::ChooseEnemyUnitTarget(const ACGPlayerState& Opponent, const FCGPendingChoice& Choice)
{
	int32 BestIndex = -1;
	int32 BestRank = MIN_int32;
	for (int32 i = 0; i < Opponent.BoardUnits.Num(); ++i)
	{
		const FCGBoardUnit& Target = Opponent.BoardUnits[i];

		// B11/B13(現在の攻撃力=パワーで判定)は、カード定義を引かずBoardUnitの
		// 実際のAtk(バフ/デバフ後、マイナスもあり得る)をそのまま見る
		// (docs/next-ruleset-cards-v1.md「青」)。候補の中で最もAtkが高い
		// (＝上限ギリギリまで強い)ものを選ぶ、コスト版と同じ貪欲方針。
		if (Choice.bFilterByCurrentAtk)
		{
			if (Choice.MaxCost >= 0 && Target.Atk > Choice.MaxCost)
			{
				continue;
			}
			if (Target.Atk > BestRank)
			{
				BestRank = Target.Atk;
				BestIndex = i;
			}
			continue;
		}

		// 変貌済みでも、判定は変貌前のカードのコストで行う(SealUnit()と同じ扱い)。
		const FName TargetCardId = Target.OriginalCardId.IsNone() ? Target.CardId : Target.OriginalCardId;
		FCGCardDef Def;
		if (!UCGCardDatabase::FindCard(TargetCardId, Def))
		{
			continue;
		}
		if (Choice.MaxCost >= 0 && Def.Cost > Choice.MaxCost)
		{
			continue;
		}
		if (Def.Cost > BestRank)
		{
			BestRank = Def.Cost;
			BestIndex = i;
		}
	}
	return BestIndex;
}

int32 UCGAIOpponent::ChooseAllyBuffTarget(const ACGPlayerState& Self)
{
	int32 BestIndex = -1;
	int32 BestAtk = -1;
	for (int32 i = 0; i < Self.BoardUnits.Num(); ++i)
	{
		const int32 Atk = Self.GetEffectiveAtk(i);
		if (Atk > BestAtk)
		{
			BestAtk = Atk;
			BestIndex = i;
		}
	}
	return BestIndex;
}

int32 UCGAIOpponent::ChooseAllySacrificeTarget(const ACGPlayerState& Self)
{
	int32 BestIndex = -1;
	int32 BestHp = TNumericLimits<int32>::Max();
	int32 BestAtk = TNumericLimits<int32>::Max();
	for (int32 i = 0; i < Self.BoardUnits.Num(); ++i)
	{
		const FCGBoardUnit& Unit = Self.BoardUnits[i];
		if (Unit.Hp < BestHp || (Unit.Hp == BestHp && Unit.Atk < BestAtk))
		{
			BestHp = Unit.Hp;
			BestAtk = Unit.Atk;
			BestIndex = i;
		}
	}
	return BestIndex;
}

int32 UCGAIOpponent::ChooseAllyTransformTarget(const ACGPlayerState& Self)
{
	int32 BestIndex = -1;
	int32 BestCost = -1;
	for (int32 i = 0; i < Self.BoardUnits.Num(); ++i)
	{
		if (!Self.CanUnitTransform(i))
		{
			continue;
		}
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(Self.BoardUnits[i].CardId, Def) && Def.Cost > BestCost)
		{
			BestCost = Def.Cost;
			BestIndex = i;
		}
	}
	return BestIndex;
}

int32 UCGAIOpponent::ChooseMarketSlotToReroll(const ACGGameState& CGState)
{
	int32 BestIndex = 0;
	int32 BestCost = INT32_MAX;
	for (int32 i = 0; i < CGState.MarketSlots.Num(); ++i)
	{
		FCGCardDef Def;
		// 空枠(NAME_None)はFindCardが失敗しコストが定まらないため、
		// 最優先で選び直す対象として扱う(-1で他のどのコストよりも低くする)。
		const int32 Cost = UCGCardDatabase::FindCard(CGState.MarketSlots[i].CardId, Def) ? Def.Cost : -1;
		if (Cost < BestCost)
		{
			BestCost = Cost;
			BestIndex = i;
		}
	}
	return BestIndex;
}

FName UCGAIOpponent::ChooseMarketCard(const TArray<FName>& MarketCardIds, const FCGPendingChoice& Choice)
{
	FName Best = NAME_None;
	int32 BestCost = -1;
	for (const FName& CardId : MarketCardIds)
	{
		FCGCardDef Def;
		if (!UCGCardDatabase::FindCard(CardId, Def))
		{
			continue;
		}
		if (Choice.MaxCost >= 0 && Def.Cost > Choice.MaxCost)
		{
			continue;
		}
		// O15黒鉄の買い占め屋: 場に直接出すため、候補はUnitのみに絞る
		// (docs/next-ruleset-cards-v1.md「橙」)。
		if (Choice.EffectId == FName(CGEffectId::OnPlayDeployFromMarketFree) && Def.CardType != ECGCardType::Unit)
		{
			continue;
		}
		if (Def.Cost > BestCost)
		{
			BestCost = Def.Cost;
			Best = CardId;
		}
	}
	return Best;
}

FName UCGAIOpponent::ChooseGraveyardCard(const ACGPlayerState& Self, const FCGPendingChoice& Choice)
{
	FName Best = NAME_None;
	int32 BestCost = -1;
	for (const FName& CardId : Self.DiscardCardIds)
	{
		FCGCardDef Def;
		if (!UCGCardDatabase::FindCard(CardId, Def))
		{
			continue;
		}
		if (Choice.MaxCost >= 0 && Def.Cost > Choice.MaxCost)
		{
			continue;
		}
		if (Choice.bRequireSpell && Def.CardType != ECGCardType::Spell)
		{
			continue;
		}
		if (Def.Cost > BestCost)
		{
			BestCost = Def.Cost;
			Best = CardId;
		}
	}
	return Best;
}

void UCGAIOpponent::RunTurn(ACGGameMode* GameMode, int32 SideIndex)
{
	if (!GameMode)
	{
		return;
	}

	ACGGameState* CGState = GameMode->GetCGGameState();
	ACGPlayerState* Self = (CGState && CGState->Sides.IsValidIndex(SideIndex)) ? CGState->Sides[SideIndex] : nullptr;
	if (!CGState || !Self)
	{
		return;
	}
	const int32 OpponentSideIndex = (SideIndex == 0) ? 1 : 0;
	ACGPlayerState* Opponent = CGState->Sides.IsValidIndex(OpponentSideIndex) ? CGState->Sides[OpponentSideIndex] : nullptr;

	// プレイ: 手札の中から出すカードを選ぶ。マナはプレイと購入で共通のリソース
	// のため、先に盤面を作る(=攻撃機会を確保する)ことを優先し、購入は後回しに
	// して残ったマナで行う(元は購入を先に行っていたが、購入だけでマナを使い
	// 切ってしまい、AIが一切カードをプレイ/攻撃しなくなる不具合があったため
	// 順序を入れ替えた)。
	// 選び方は次の2点を優先する:
	// ①対象を必要とする効果で、対象が1体も存在しない(=何も起きない)カードは
	//   除外する(断罪/対象指定デバフは敵の場が、対象指定の味方強化は自分の場が
	//   空だと不発になる)
	// ②残った候補は`ScorePlayCandidate`のスコアが一番高いものを選ぶ(コスト・
	//   Unit優先に加えて、カード同士のシナジーや盤面状況を加味する。「自分に
	//   得なカードの効果ができるだけ発動し、勝利に近づくような行動をして
	//   ほしい」というフィードバックへの対応)。スコアはSelf->CardsPlayedThisTurn/
	//   BoardUnits.Num()を毎回参照するため、1枚プレイするたびに次の候補評価へ
	//   自動的に反映される。
	for (int32 SafetyCounter = 0; SafetyCounter < 30 && CGState->WinnerPlayerIndex == -1; ++SafetyCounter)
	{
		FName BestCardId = NAME_None;
		int32 BestScore = MIN_int32;
		for (const FName& HandCardId : Self->HandCardIds)
		{
			FCGCardDef Def;
			if (!UCGCardDatabase::FindCard(HandCardId, Def) || Def.Cost > Self->CurrentMana)
			{
				continue;
			}
			if (WouldCardEffectBeUseless(Def, Self, Opponent))
			{
				continue;
			}
			const int32 Score = ScorePlayCandidate(Def, Self, Opponent);
			if (BestCardId == NAME_None || Score > BestScore)
			{
				BestScore = Score;
				BestCardId = HandCardId;
			}
		}
		if (BestCardId == NAME_None)
		{
			break;
		}

		// 「シナジーの無い単発スペル(スコアが素点+わずかなボーナスしか無い)を
		// 空撃ちするくらいなら、そのマナ/コインをマーケット購入に回してほしい」
		// というフィードバックへの対応。Unitは常に+1000のボーナスが乗るため
		// この足切りには掛からない(盤面を残すプレイは引き続き優先する)。
		FCGCardDef BestDef;
		if (UCGCardDatabase::FindCard(BestCardId, BestDef) && BestDef.CardType == ECGCardType::Spell
			&& BestScore <= BestDef.Cost * 10 + 100 && HasAffordableMarketCard(*CGState, *Self))
		{
			break;
		}

		if (!GameMode->RequestPlayCard(SideIndex, BestCardId))
		{
			break;
		}
	}

	// 購入: プレイで使わずに残ったマナで、買えるカードの中で一番コストが高い
	// ものから買っていく。同コストなら相手の山札由来の枠を優先する(自分の
	// 山札は減らさずに済むため。docs/next-ruleset-design.md「マーケット」の
	// 出どころ補充ルール参照)。
	// (RequestBuyCardが失敗した=想定外の理由で買えない場合は無限ループ回避のため打ち切る)
	for (int32 SafetyCounter = 0; !GameMode->bDiagDisableBuyPhase && SafetyCounter < 30 && CGState->WinnerPlayerIndex == -1; ++SafetyCounter)
	{
		int32 BestSlotIndex = INDEX_NONE;
		int32 BestCost = -1;
		bool bBestIsOpponentOrigin = false;
		for (int32 SlotIndex = 0; SlotIndex < CGState->MarketSlots.Num(); ++SlotIndex)
		{
			const FCGMarketSlot& Slot = CGState->MarketSlots[SlotIndex];
			if (Slot.CardId.IsNone())
			{
				continue;
			}
			FCGCardDef Def;
			if (!UCGCardDatabase::FindCard(Slot.CardId, Def))
			{
				continue;
			}
			// 割引(ComputeCurrentPurchaseDiscount)とコイン(PurchaseMana、
			// 次期ルール、docs/game-rules-minimum.md「色ガイド」)の両方を
			// 加味した実質コストで判定する。これらを無視すると、AIが
			// 貯まったコインを使わずに済ませてしまい、パッシブの効果を
			// 実戦で正しく検証できない。
			const int32 EffectiveCost = FMath::Max(0, Def.Cost - Self->ComputeCurrentPurchaseDiscount());
			if (EffectiveCost > Self->CurrentMana + Self->PurchaseMana)
			{
				continue;
			}
			const bool bIsOpponentOrigin = (Slot.OriginSideIndex != SideIndex);
			const bool bBetter = (Def.Cost > BestCost)
				|| (Def.Cost == BestCost && bIsOpponentOrigin && !bBestIsOpponentOrigin);
			if (bBetter)
			{
				BestCost = Def.Cost;
				BestSlotIndex = SlotIndex;
				bBestIsOpponentOrigin = bIsOpponentOrigin;
			}
		}
		if (BestSlotIndex == INDEX_NONE || !GameMode->RequestBuyCard(SideIndex, BestSlotIndex))
		{
			break;
		}
	}

	// 攻撃: 攻撃可能なユニットで順に攻撃を仕掛ける。対象(守護がいれば強制的にそちら、
	// いなければChooseAttackTargetでの判断)はACGGameMode::RequestAttack側で解決する
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	for (int32 SafetyCounter = 0; SafetyCounter < 30 && CGState->WinnerPlayerIndex == -1; ++SafetyCounter)
	{
		int32 AttackerUnitIndex = -1;
		for (int32 i = 0; i < Self->BoardUnits.Num(); ++i)
		{
			if (Self->BoardUnits[i].bCanAttack)
			{
				AttackerUnitIndex = i;
				break;
			}
		}
		if (AttackerUnitIndex == -1)
		{
			break;
		}

		if (!GameMode->RequestAttack(SideIndex, AttackerUnitIndex))
		{
			break;
		}
	}

	if (CGState->WinnerPlayerIndex == -1)
	{
		GameMode->RequestEndTurn(SideIndex);
	}
}
