#include "CGCardSlotWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

void UCGCardSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Button)
	{
		Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotLabel"));
		Button->AddChild(Label);
		WidgetTree->RootWidget = Button;
		Button->OnClicked.AddDynamic(this, &UCGCardSlotWidget::HandleClicked);
	}
}

void UCGCardSlotWidget::SetLabel(const FString& InText)
{
	if (Label)
	{
		Label->SetText(FText::FromString(InText));
	}
}

void UCGCardSlotWidget::HandleClicked()
{
	OnSlotClicked.Broadcast(SlotIndex);
}
