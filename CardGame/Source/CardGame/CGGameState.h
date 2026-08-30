#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CGTypes.h"
#include "CGGameState.generated.h"

class ACGPlayerState;

// 対戦全体の公開状態(docs/architecture.md「対戦ロジックのクラス責務」)。
UCLASS()
class ACGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CurrentTurnPlayerIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 TurnCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	ECGPhase CurrentPhase = ECGPhase::Draw;

	// -1 = 未決着。docs/game-rules-minimum.mdの勝敗条件を満たすと0または1になる。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 WinnerPlayerIndex = -1;

	// 常時公開5枚(docs/initial-cards-v0.1.md「マーケット: 常時5枚公開」)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<FName> MarketCardIds;

	// マーケット補充用の残りプール。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<FName> MarketDeckCardIds;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<TObjectPtr<ACGPlayerState>> Sides;

	// MarketDeckCardIdsから不足分をランダム補充して5枚に保つ。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RefillMarket();
};
