#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CGTypes.h"
#include "CGDeckSaveGame.generated.h"

// プレイヤーが構築した複数のデッキをディスクへ保存するためのSaveGame。
// ゲームを閉じても次回起動時にデッキ一覧とアクティブなデッキを覚えておく
// 要件のために使う(docs/architecture.md「デッキの永続化」参照)。
UCLASS()
class UCGDeckSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FCGSavedDeck> SavedDecks;

	// 直近にバトルで使う対象として選ばれていたデッキの名前。SavedDecks内の
	// 名前と突き合わせて、次回起動時にどれをアクティブにするか復元する。
	UPROPERTY()
	FString ActiveDeckName;
};
