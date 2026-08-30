#include "CGCardHostWidget.h"
#include "CGCardSlotWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"

UWidget* UCGCardHostWidget::BuildRootOverlay(UWidget* MainContent)
{
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CardHostRootOverlay"));

	if (UOverlaySlot* MainSlot = RootOverlay->AddChildToOverlay(MainContent))
	{
		MainSlot->SetHorizontalAlignment(HAlign_Fill);
		MainSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// ホバープレビュー専用の最前面レイヤー。普段は空(何も描画しない)。あとから
	// AddChildToOverlay()するため、常にMainContentより手前(=最前面)に描画される。
	PreviewLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CardHoverPreviewLayer"));
	PreviewLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* PreviewSlot = RootOverlay->AddChildToOverlay(PreviewLayer))
	{
		PreviewSlot->SetHorizontalAlignment(HAlign_Fill);
		PreviewSlot->SetVerticalAlignment(VAlign_Fill);
	}

	return RootOverlay;
}

void UCGCardHostWidget::RegisterCardHoverPreview(UCGCardSlotWidget* Card)
{
	if (Card)
	{
		Card->OnHoverChanged.AddDynamic(this, &UCGCardHostWidget::HandleCardHoverChanged);
	}
}

void UCGCardHostWidget::HandleCardHoverChanged(UCGCardSlotWidget* Card, bool bIsHovering)
{
	if (!PreviewLayer || !Card)
	{
		return;
	}

	if (!bIsHovering)
	{
		if (PreviewCardWidget)
		{
			PreviewCardWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (!PreviewCardWidget)
	{
		PreviewCardWidget = CreateWidget<UCGCardSlotWidget>(this, UCGCardSlotWidget::StaticClass());
		PreviewLayer->AddChildToCanvas(PreviewCardWidget);
	}

	// ホバーされたカードと全く同じ内容を複製し、そのカードと同じ画面上の位置に
	// 拡大した状態で表示する。
	Card->CopyCardDataTo(PreviewCardWidget);

	const FGeometry CardGeometry = Card->GetCachedGeometry();
	const FGeometry LayerGeometry = PreviewLayer->GetCachedGeometry();
	const FVector2D LocalTopLeft = LayerGeometry.AbsoluteToLocal(CardGeometry.GetAbsolutePosition());
	const FVector2D CardSize(UCGCardSlotWidget::CardWidth, UCGCardSlotWidget::CardHeight);

	if (UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(PreviewCardWidget->Slot))
	{
		PreviewSlot->SetAutoSize(false);
		PreviewSlot->SetSize(CardSize);
		// 位置の基準点をカードの中心にしておくことで、拡大時にカードの中心から
		// 均等にはみ出すようにする(SetRenderScaleは既定でウィジェット自身の中心を
		// 基準に拡大するため、位置の基準もそこに合わせている)。
		PreviewSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PreviewSlot->SetPosition(LocalTopLeft + CardSize * 0.5f);
	}

	PreviewCardWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	PreviewCardWidget->SetRenderScale(FVector2D(UCGCardSlotWidget::CardHoverScale, UCGCardSlotWidget::CardHoverScale));
}
