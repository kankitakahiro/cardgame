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

	void SetLabel(const FString& InText);

	// 行ごとに必要な情報量が違う(場のユニットは名前+ステータスだけ、マーケット/手札は
	// 説明文まで必要)ため、ウィジェット構築前(SetLabelより前)に呼んでサイズを変えられる
	// ようにしている。呼ばなければデフォルトサイズを使う。
	void SetCardSize(float InWidth, float InHeight);

protected:
	// WidgetTree->RootWidgetは NativeConstruct() ではなく RebuildWidget() の中で
	// 設定しないと、Slate側が先に空のプレースホルダー(SSpacer)を取得してキャッシュ
	// してしまい、後からRootWidgetを設定しても画面に反映されない
	// (Widget Reflectorで実際に確認した既知の落とし穴。docs/automation-notes.md参照)。
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void EnsureBuilt();

	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	UFUNCTION()
	void HandleClicked();

private:
	float CardWidth = 170.f;
	float CardHeight = 190.f;
};
