#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGCardSlotWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCGOnSlotClicked, int32, SlotIndex);

// カード1枚(手札/マーケット)またはユニット1体(場)をボタン表示する最小ウィジェット。
// UMGのWidgetTreeはPython Editor Scripting APIから編集できない(docs/automation-notes.md)
// ため、UI一式はC++側(NativeConstruct)で組み立てている。
UCLASS()
class UCGCardSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 SlotIndex = -1;

	UPROPERTY(BlueprintAssignable, Category = "CardGame")
	FCGOnSlotClicked OnSlotClicked;

	virtual void NativeConstruct() override;

	void SetLabel(const FString& InText);

protected:
	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	UFUNCTION()
	void HandleClicked();
};
