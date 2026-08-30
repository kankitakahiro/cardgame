#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CGGameInstance.generated.h"

// ロビー/デッキ構築/バトルを別レベルとして行き来する構成にしたため、
// GameMode/GameState(レベル遷移で破棄される)ではなく、レベルをまたいで
// 生存するGameInstanceに「プレイヤーが選んだデッキ」を持たせている
// (docs/architecture.md「デッキの永続化」参照)。
UCLASS()
class UCGGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	// 現在選択中のデッキ(常に12枚を想定)。デッキ構築画面が更新し、
	// バトル開始時にACGGameMode::InitializeMatch()がここから読み取る。
	UPROPERTY(BlueprintReadWrite, Category = "CardGame")
	TArray<FName> PlayerDeckCardIds;

	// 現在のPlayerDeckCardIdsをディスクへ保存する(デッキ構築画面の保存ボタンから呼ぶ)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void SaveDeckToDisk();
};
