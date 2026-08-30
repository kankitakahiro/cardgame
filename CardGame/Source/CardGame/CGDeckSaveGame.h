#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CGDeckSaveGame.generated.h"

// プレイヤーが構築したデッキをディスクへ保存するためのSaveGame。
// ゲームを閉じても次回起動時にデッキを覚えておく要件のために使う
// (docs/architecture.md「デッキの永続化」参照)。
UCLASS()
class UCGDeckSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FName> SavedDeckCardIds;
};
