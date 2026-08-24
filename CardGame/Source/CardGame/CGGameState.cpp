#include "CGGameState.h"

void ACGGameState::RefillMarket()
{
	while (MarketCardIds.Num() < 5 && MarketDeckCardIds.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, MarketDeckCardIds.Num() - 1);
		MarketCardIds.Add(MarketDeckCardIds[Index]);
		MarketDeckCardIds.RemoveAt(Index);
	}
}
