#include "CGGameState.h"
#include "CGPlayerState.h"
#include "Net/UnrealNetwork.h"

void ACGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 全て非公開情報を含まない公開情報のため、条件無し(全員へ複製)で良い
	// (docs/online-play-design.md「`ACGGameState`のレプリケーション」)。
	DOREPLIFETIME(ACGGameState, CurrentTurnPlayerIndex);
	DOREPLIFETIME(ACGGameState, TurnCount);
	DOREPLIFETIME(ACGGameState, CurrentPhase);
	DOREPLIFETIME(ACGGameState, WinnerPlayerIndex);
	DOREPLIFETIME(ACGGameState, bOpponentDisconnected);
	DOREPLIFETIME(ACGGameState, MarketSlots);
	DOREPLIFETIME(ACGGameState, Sides);
	DOREPLIFETIME(ACGGameState, PendingChoice);
	DOREPLIFETIME(ACGGameState, LastAttackResult);
	DOREPLIFETIME(ACGGameState, AttackSequenceNumber);
	DOREPLIFETIME(ACGGameState, LastCardPlayResult);
	DOREPLIFETIME(ACGGameState, CardPlaySequenceNumber);
	DOREPLIFETIME(ACGGameState, StateVersion);
	DOREPLIFETIME(ACGGameState, ActionLog);
}

namespace
{
	// 1側あたりの初期マーケット枠数(計6枠。docs/next-ruleset-design.md「マーケット」)。
	constexpr int32 InitialMarketSlotsPerSide = 3;

	// 行動ログの最大保持件数(古いものから切り捨てる)。
	constexpr int32 MaxActionLogEntries = 30;
}

void ACGGameState::AppendActionLog(int32 SideIndex, const FString& Text)
{
	FCGActionLogEntry Entry;
	Entry.SideIndex = SideIndex;
	Entry.Text = Text;
	ActionLog.Add(Entry);
	if (ActionLog.Num() > MaxActionLogEntries)
	{
		ActionLog.RemoveAt(0, ActionLog.Num() - MaxActionLogEntries);
	}
}

void ACGGameState::InitializeMarket()
{
	MarketSlots.Reset();
	for (int32 SideIndex = 0; SideIndex < Sides.Num(); ++SideIndex)
	{
		ACGPlayerState* Side = Sides[SideIndex];
		if (!Side)
		{
			continue;
		}
		for (int32 i = 0; i < InitialMarketSlotsPerSide; ++i)
		{
			FCGMarketSlot Slot;
			Slot.OriginSideIndex = SideIndex;
			Slot.CardId = Side->DrawCardForMarket();
			MarketSlots.Add(Slot);
		}
	}
}

void ACGGameState::RefillMarketSlot(int32 SlotIndex)
{
	if (!MarketSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FCGMarketSlot& Slot = MarketSlots[SlotIndex];
	ACGPlayerState* Origin = Sides.IsValidIndex(Slot.OriginSideIndex) ? Sides[Slot.OriginSideIndex] : nullptr;
	Slot.CardId = Origin ? Origin->DrawCardForMarket() : NAME_None;
}

void ACGGameState::RerollMarketSlot(int32 SlotIndex)
{
	if (!MarketSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FCGMarketSlot& Slot = MarketSlots[SlotIndex];
	if (!Slot.CardId.IsNone())
	{
		ACGPlayerState* Origin = Sides.IsValidIndex(Slot.OriginSideIndex) ? Sides[Slot.OriginSideIndex] : nullptr;
		if (Origin)
		{
			// 追い出されたカードは無くならず、出どころの山札の一番下へ戻す
			// (デッキ配列は先頭=山札トップの前提。ACGPlayerState::MoveDeckTopToBottom
			// と同じ並び)。
			Origin->DeckCardIds.Add(Slot.CardId);
		}
	}

	RefillMarketSlot(SlotIndex);
}

TArray<FName> ACGGameState::GetMarketCardIds() const
{
	TArray<FName> Ids;
	Ids.Reserve(MarketSlots.Num());
	for (const FCGMarketSlot& Slot : MarketSlots)
	{
		Ids.Add(Slot.CardId);
	}
	return Ids;
}
