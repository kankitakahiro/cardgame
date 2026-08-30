#include "CGCardSlotWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UCGCardSlotWidget::RebuildWidget()
{
	EnsureBuilt();
	return Super::RebuildWidget();
}

void UCGCardSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureBuilt();
}

void UCGCardSlotWidget::EnsureBuilt()
{
	if (Button)
	{
		return;
	}

	// カードごとに文字数が違うと横幅がバラバラになり画面外へあふれてしまうため、
	// 固定サイズのSizeBoxに収め、はみ出す分はテキスト折り返しで対応する。
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSizeBox"));
	SizeBox->SetWidthOverride(CardWidth);
	SizeBox->SetHeightOverride(CardHeight);

	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotLabel"));
	Label->SetAutoWrapText(true);
	Label->SetWrapTextAt(CardWidth - 16.f);
	Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13));

	Button->AddChild(Label);
	SizeBox->AddChild(Button);
	WidgetTree->RootWidget = SizeBox;
	Button->OnClicked.AddDynamic(this, &UCGCardSlotWidget::HandleClicked);
}

void UCGCardSlotWidget::SetCardSize(float InWidth, float InHeight)
{
	// EnsureBuilt()が既に実行済み(=サイズ確定後)だと反映できないため、
	// 呼び出し側はCreateWidget直後、SetLabelより前に呼ぶこと。
	CardWidth = InWidth;
	CardHeight = InHeight;
}

void UCGCardSlotWidget::SetLabel(const FString& InText)
{
	// CreateWidget()直後、まだTakeWidget()/RebuildWidget()が一度も走っていない
	// (=RootWidgetが未構築の)状態でSetLabelが呼ばれることがあるため、ここで確実に
	// 構築しておく(EnsureBuiltは2回目以降は何もしない)。
	EnsureBuilt();
	if (Label)
	{
		Label->SetText(FText::FromString(InText));
	}
}

void UCGCardSlotWidget::HandleClicked()
{
	OnSlotClicked.Broadcast(SlotIndex);
}
