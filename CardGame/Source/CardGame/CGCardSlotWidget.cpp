#include "CGCardSlotWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

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

	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotLabel"));
	Button->AddChild(Label);
	WidgetTree->RootWidget = Button;
	Button->OnClicked.AddDynamic(this, &UCGCardSlotWidget::HandleClicked);
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
