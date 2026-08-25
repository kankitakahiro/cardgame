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
// のため、UIは全てC++側(NativeConstruct)で構築している。
UCLASS()
class UCGGameHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RefreshUI();

protected:
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
