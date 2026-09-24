#include "CGDeckListRowWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

namespace
{
	UButton* MakeRowButton(UWidgetTree* WidgetTree, const TCHAR* Name, const FString& Label, UHorizontalBox* Root)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sSizeBox"), Name));
		SizeBox->SetWidthOverride(100.f);
		SizeBox->SetHeightOverride(36.f);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%sLabel"), Name));
		Text->SetText(FText::FromString(Label));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
		Text->SetJustification(ETextJustify::Center);
		Button->AddChild(Text);

		if (USizeBoxSlot* ButtonSlot = Cast<USizeBoxSlot>(SizeBox->AddChild(Button)))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UHorizontalBoxSlot* RowSlot = Root->AddChildToHorizontalBox(SizeBox))
		{
			RowSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}
}

TSharedRef<SWidget> UCGDeckListRowWidget::RebuildWidget()
{
	EnsureBuilt();
	return Super::RebuildWidget();
}

void UCGDeckListRowWidget::EnsureBuilt()
{
	if (RowBorder)
	{
		return;
	}

	RowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RowBorder"));
	RowBorder->SetPadding(FMargin(16.f, 10.f));
	RowBorder->SetHorizontalAlignment(HAlign_Fill);
	RowBorder->SetVerticalAlignment(VAlign_Fill);
	WidgetTree->RootWidget = RowBorder;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	RowBorder->AddChild(Row);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 17));
	if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameText))
	{
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	SelectButton = MakeRowButton(WidgetTree, TEXT("SelectButton"), TEXT("選択"), Row);
	SelectButton->OnClicked.AddDynamic(this, &UCGDeckListRowWidget::HandleSelectClicked);

	DeleteButton = MakeRowButton(WidgetTree, TEXT("DeleteButton"), TEXT("削除"), Row);
	DeleteButton->OnClicked.AddDynamic(this, &UCGDeckListRowWidget::HandleDeleteClicked);
}

void UCGDeckListRowWidget::SetRowData(int32 InSlotIndex, const FString& DeckName, int32 CardCount, bool bIsActive, bool bAllowDelete)
{
	EnsureBuilt();

	SlotIndex = InSlotIndex;
	NameText->SetText(FText::FromString(FString::Printf(TEXT("%s%s (%d枚)"),
		bIsActive ? TEXT("[選択中] ") : TEXT(""), *DeckName, CardCount)));
	NameText->SetColorAndOpacity(bIsActive
		? FSlateColor(FLinearColor(0.4f, 0.9f, 0.5f))
		: FSlateColor(FLinearColor::White));

	RowBorder->SetBrushColor(bIsActive
		? FLinearColor(0.12f, 0.22f, 0.14f, 1.f)
		: FLinearColor(0.14f, 0.14f, 0.14f, 1.f));

	// 既にアクティブなデッキを選び直す操作は無意味なので、選択ボタンを無効化する。
	SelectButton->SetIsEnabled(!bIsActive);
	DeleteButton->SetVisibility(bAllowDelete ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UCGDeckListRowWidget::HandleSelectClicked()
{
	OnSelectClicked.Broadcast(SlotIndex);
}

void UCGDeckListRowWidget::HandleDeleteClicked()
{
	OnDeleteClicked.Broadcast(SlotIndex);
}
