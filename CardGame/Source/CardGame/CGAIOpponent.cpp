#include "CGAIOpponent.h"
#include "CGGameMode.h"
#include "CGGameState.h"
#include "CGPlayerState.h"
#include "CGCardDatabase.h"

namespace
{
	// 対象を必要とする効果が、対象となるユニットが1体も無いために不発になるか
	// どうかを判定する。RunTurn()のプレイ選択で、こうした「出しても何も起きない」
	// カードを除外するのに使う(封印/対象指定バフ等を持つカードが対象不在で
	// 空撃ちされ、外からは「何もして来ない」ように見えてしまう不具合への対策)。
	// 厳密な候補判定(コスト上限フィルタ等)はACGGameMode側の各Resolve関数が
	// 別途行うため、ここでは「そもそも対象になり得るUnitが存在するか」だけを見る。
	bool WouldCardEffectBeUseless(const FCGCardDef& Def, const ACGPlayerState* Self, const ACGPlayerState* Opponent)
	{
		if (Def.EffectId == FName(CGEffectId::SealSpell) || Def.EffectId == FName(CGEffectId::SealOnPlay)
			|| Def.EffectId == FName(CGEffectId::SealSpellGrantPurchaseMana) || Def.EffectId == FName(CGEffectId::SealSpellTwo)
			|| Def.EffectId == FName(CGEffectId::OnPlayDebuffTarget))
		{
			return !Opponent || Opponent->BoardUnits.Num() == 0;
		}
		if (Def.EffectId == FName(CGEffectId::BuffAllyTarget) || Def.EffectId == FName(CGEffectId::OnPlayBuffAllyTarget)
			|| Def.EffectId == FName(CGEffectId::BuffAllyTargetAndDraw) || Def.EffectId == FName(CGEffectId::OnPlayBuffAllyTargetAndUnitDiscount))
		{
			return !Self || Self->BoardUnits.Num() == 0;
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

int32 UCGAIOpponent::ChooseDamageTarget(const ACGPlayerState& Opponent, int32 Damage)
{
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
	return BestIndex; // 見つからなければ顔面(-1)。
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
		// O15独占商人: 場に直接出すため、候補はUnitのみに絞る
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
	// ①Unitを優先(単にコストが高いものから出すだけだと、対象を必要とする
	//   Spell(封印/対象指定バフ等)ばかり出て場が育たず、外からは「何もして
	//   来ない」ように見えてしまう不具合があったため)
	// ②対象を必要とする効果で、対象が1体も存在しない(=何も起きない)カードは
	//   除外する(封印/対象指定デバフは敵の場が、対象指定の味方強化は自分の場が
	//   空だと不発になる)
	for (int32 SafetyCounter = 0; SafetyCounter < 30 && CGState->WinnerPlayerIndex == -1; ++SafetyCounter)
	{
		FName BestCardId = NAME_None;
		int32 BestCost = -1;
		bool bBestIsUnit = false;
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
			const bool bIsUnit = (Def.CardType == ECGCardType::Unit);
			const bool bBetter = (BestCardId == NAME_None)
				|| (bIsUnit != bBestIsUnit ? bIsUnit : Def.Cost > BestCost);
			if (bBetter)
			{
				BestCost = Def.Cost;
				BestCardId = HandCardId;
				bBestIsUnit = bIsUnit;
			}
		}
		if (BestCardId == NAME_None || !GameMode->RequestPlayCard(SideIndex, BestCardId))
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
