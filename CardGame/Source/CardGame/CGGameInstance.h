#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CGTypes.h"
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

	// 現在バトルで使うデッキ(常に25枚を想定)。デッキ選択画面/デッキ構築画面の
	// 保存操作が更新し、バトル開始時にACGGameMode::InitializeMatch()がここから
	// 読み取る。
	UPROPERTY(BlueprintReadWrite, Category = "CardGame")
	TArray<FName> PlayerDeckCardIds;

	// 保存済みデッキの一覧(名前+カードID)。デッキ選択画面(UCGLobbyHUD)が
	// 一覧表示に使う。常に1件以上存在する(全て削除されるとスターターデッキが
	// 自動的に再生成される)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<FCGSavedDeck> SavedDecks;

	// 現在アクティブな(=PlayerDeckCardIdsの元になっている)デッキの名前。
	// SavedDecks内のいずれかの名前と一致する。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString ActiveDeckName;

	// DeckNameで保存する(空文字なら仮の名前を付ける)。既存の同名デッキが
	// あれば上書き、無ければ新規追加する。保存したデッキはそのままアクティブ
	// (PlayerDeckCardIds)にし、ディスクへ永続化する。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void SaveDeckAs(const FString& DeckName, const TArray<FName>& CardIds);

	// SavedDecks[Index]をアクティブ(PlayerDeckCardIds)にし、ディスクへ永続化する。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void SelectSavedDeck(int32 Index);

	// SavedDecks[Index]を削除する。アクティブだったデッキを削除した場合は
	// 残りの先頭を新たにアクティブにする(1つも残らない場合はスターター
	// デッキを再生成する)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void DeleteSavedDeck(int32 Index);

private:
	// SavedDecks/ActiveDeckNameの現在の内容をディスクへ書き出す。
	void PersistSavedDecksToDisk();
};
