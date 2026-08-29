#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGGameHUD.generated.h"

class UTextBlock;
class UHorizontalBox;
class UButton;
class ACGGameMode;

// ゲーム全体のHUD。マーケット/両陣営の場/手札/ステータス/EndTurnボタンを持つ。
// WidgetBlueprintのWidgetTreeをPythonから編集できない制約(docs/automation-notes.md)
// のため、UIは全てC++側で構築している。
UCLASS()
class UCGGameHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RefreshUI();

protected:
	// WidgetTree->RootWidgetは NativeConstruct() ではなく RebuildWidget() の中で
	// 設定しないと、Slate側が先に空のプレースホルダー(SSpacer)を取得してキャッシュ
	// してしまい、後からRootWidgetを設定しても画面に反映されない
	// (Widget Reflectorで実際に確認した既知の落とし穴。docs/automation-notes.md参照)。
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void EnsureWidgetTreeBuilt();
	bool bWidgetTreeBuilt = false;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> MarketBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> EnemyBoardBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> SelfBoardBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> HandBox;

	UPROPERTY()
	TObjectPtr<UButton> EndTurnButton;

	UFUNCTION()
	void HandleHandSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleMarketSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleBoardSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleEndTurnClicked();

	ACGGameMode* GetCGGameMode() const;

private:
	// RefreshUI時点のスロット内容(クリック時にSlotIndexからCardIdへ逆引きするため)。
	TArray<FName> LastHandCardIds;
	TArray<FName> LastMarketCardIds;
};
