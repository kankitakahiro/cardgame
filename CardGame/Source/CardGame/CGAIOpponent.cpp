#include "CGAIOpponent.h"
#include "CGGameMode.h"
#include "CGGameState.h"
#include "CGPlayerState.h"
#include "CGCardDatabase.h"

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
	ACGPlayerState* Opponent = CGState->Sides.IsValidIndex(SideIndex == 0 ? 1 : 0) ? CGState->Sides[SideIndex == 0 ? 1 : 0] : nullptr;

	// 購入: 使えるマナがある限り、買えるカードの中で一番コストが高いものから買っていく。
	// (RequestBuyCardが失敗した=想定外の理由で買えない場合は無限ループ回避のため打ち切る)
	for (int32 SafetyCounter = 0; SafetyCounter < 30 && CGState->WinnerPlayerIndex == -1; ++SafetyCounter)
	{
		FName BestCardId = NAME_None;
		int32 BestCost = -1;
		for (const FName& MarketCardId : CGState->MarketCardIds)
		{
			FCGCardDef Def;
			if (UCGCardDatabase::FindCard(MarketCardId, Def) && Def.Cost <= Self->CurrentMana && Def.Cost > BestCost)
			{
				BestCost = Def.Cost;
				BestCardId = MarketCardId;
			}
		}
		if (BestCardId == NAME_None || !GameMode->RequestBuyCard(SideIndex, BestCardId))
		{
			break;
		}
	}

	// プレイ: 手札の中で一番コストが高い、出せるカードから順に出していく。
	for (int32 SafetyCounter = 0; SafetyCounter < 30 && CGState->WinnerPlayerIndex == -1; ++SafetyCounter)
	{
		FName BestCardId = NAME_None;
		int32 BestCost = -1;
		for (const FName& HandCardId : Self->HandCardIds)
		{
			FCGCardDef Def;
			if (UCGCardDatabase::FindCard(HandCardId, Def) && Def.Cost <= Self->CurrentMana && Def.Cost > BestCost)
			{
				BestCost = Def.Cost;
				BestCardId = HandCardId;
			}
		}
		if (BestCardId == NAME_None || !GameMode->RequestPlayCard(SideIndex, BestCardId))
		{
			break;
		}
	}

	// 攻撃: 攻撃可能なユニットで、相手に守護がいれば必ずそちらを、いなければ顔面を攻撃する
	// (人間側のUCGGameHUD::HandleBoardSlotClickedと同じ自動ターゲットルール)。
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

		int32 TargetUnitIndex = -1;
		if (Opponent && Opponent->HasGuardUnit())
		{
			for (int32 i = 0; i < Opponent->BoardUnits.Num(); ++i)
			{
				if (Opponent->BoardUnits[i].bHasGuard)
				{
					TargetUnitIndex = i;
					break;
				}
			}
		}

		if (!GameMode->RequestAttack(SideIndex, AttackerUnitIndex, TargetUnitIndex))
		{
			break;
		}
	}

	if (CGState->WinnerPlayerIndex == -1)
	{
		GameMode->RequestEndTurn(SideIndex);
	}
}
