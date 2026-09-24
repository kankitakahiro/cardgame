#include "CGCardHostWidget.h"
#include "CGCardSlotWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	// ダメージポップアップが上へ浮かぶ距離と、全体の表示時間。
	constexpr float DamagePopupRiseDistance = 70.f;
	constexpr float DamagePopupDuration = 0.9f;
}

UWidget* UCGCardHostWidget::BuildRootOverlay(UWidget* MainContent, UWidget* ModalLayer)
{
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CardHostRootOverlay"));

	if (UOverlaySlot* MainSlot = RootOverlay->AddChildToOverlay(MainContent))
	{
		MainSlot->SetHorizontalAlignment(HAlign_Fill);
		MainSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 選択ポップアップ等、MainContentより手前だがホバープレビューよりは奥にしたい
	// レイヤー(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。MainContentの直後、
	// ホバープレビュー層より前に追加することで、この順序を保証する。
	if (ModalLayer)
	{
		if (UOverlaySlot* ModalSlot = RootOverlay->AddChildToOverlay(ModalLayer))
		{
			ModalSlot->SetHorizontalAlignment(HAlign_Fill);
			ModalSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// ホバープレビュー専用の最前面レイヤー。普段は空(何も描画しない)。必ず最後に
	// AddChildToOverlay()するため、ModalLayerを含むどのレイヤーよりも手前(=最前面)に
	// 描画される(ポップアップ内のカードをホバーしたときの拡大表示が、ポップアップの
	// 下に隠れてしまわないようにするため)。
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

void UCGCardHostWidget::HidePreview()
{
	if (PreviewCardWidget)
	{
		PreviewCardWidget->SetVisibility(ESlateVisibility::Collapsed);
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

	// 裏向きカード(相手の手札等)は中身が無く、拡大しても情報の無い空白が
	// 大きく表示されるだけで邪魔なだけなので、拡大プレビュー自体を出さない
	// (「何も見えない大きな空白が出るだけで邪魔」というフィードバックへの対応)。
	if (Card->IsFaceDown())
	{
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

	// ホバーされたカードは盤面/マーケット等では縮小表示されていることがある
	// (UCGCardSlotWidget::WrapForCompactDisplay)。中心座標は基準サイズ決め打ちではなく、
	// 左上・右下の2点をローカル座標へ変換して実際の表示サイズから求める。プレビュー
	// 自体の表示サイズは常に基準サイズ(等倍)にする。
	const FVector2D LocalTopLeft = LayerGeometry.AbsoluteToLocal(CardGeometry.GetAbsolutePosition());
	const FVector2D LocalBottomRight = LayerGeometry.AbsoluteToLocal(CardGeometry.GetAbsolutePosition() + CardGeometry.GetAbsoluteSize());
	const FVector2D ActualLocalSize = LocalBottomRight - LocalTopLeft;
	const FVector2D LocalCenter = LocalTopLeft + ActualLocalSize * 0.5f;

	const FVector2D PreviewSize(UCGCardSlotWidget::CardWidth, UCGCardSlotWidget::CardHeight);

	// 画面の端に近いカード(敵の手札の左端等)をホバーすると、拡大後のプレビューが
	// 画面外へはみ出して情報が見切れてしまう。拡大後の半分サイズぶんだけ画面内側へ
	// 収まるよう中心座標をクランプする(手前に出す位置がずれるだけで、常に全体が
	// 画面内に収まることを優先する)。
	const FVector2D LayerSize = LayerGeometry.GetLocalSize();
	const FVector2D EnlargedHalfSize = PreviewSize * UCGCardSlotWidget::CardHoverScale * 0.5f;
	FVector2D ClampedCenter = LocalCenter;
	ClampedCenter.X = FMath::Clamp(ClampedCenter.X, EnlargedHalfSize.X, FMath::Max(EnlargedHalfSize.X, LayerSize.X - EnlargedHalfSize.X));
	ClampedCenter.Y = FMath::Clamp(ClampedCenter.Y, EnlargedHalfSize.Y, FMath::Max(EnlargedHalfSize.Y, LayerSize.Y - EnlargedHalfSize.Y));

	if (UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(PreviewCardWidget->Slot))
	{
		PreviewSlot->SetAutoSize(false);
		PreviewSlot->SetSize(PreviewSize);
		// 位置の基準点をカードの中心にしておくことで、拡大時にカードの中心から
		// 均等にはみ出すようにする(SetRenderScaleは既定でウィジェット自身の中心を
		// 基準に拡大するため、位置の基準もそこに合わせている)。
		PreviewSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PreviewSlot->SetPosition(ClampedCenter);
	}

	PreviewCardWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	PreviewCardWidget->SetRenderScale(FVector2D(UCGCardSlotWidget::CardHoverScale, UCGCardSlotWidget::CardHoverScale));
}

void UCGCardHostWidget::ShowFloatingNumber(const FVector2D& AbsolutePosition, int32 Amount, bool bIsHeal)
{
	// ダメージ(赤・"-N")と回復(緑・"+N")を見た目で区別する(「ダメージなどの
	// 表記を変更してほしい」というフィードバックへの対応)。符号付きの数字だけだと
	// 咄嗟に読み取りにくいため、符号も明示的に前置する。
	const FString Text = FString::Printf(TEXT("%s%d"), bIsHeal ? TEXT("+") : TEXT("-"), FMath::Abs(Amount));
	const FLinearColor Color = bIsHeal ? FLinearColor(0.35f, 1.f, 0.4f) : FLinearColor(1.f, 0.2f, 0.15f);
	ShowFloatingText(AbsolutePosition, Text, Color);
}

void UCGCardHostWidget::ShowFloatingText(const FVector2D& AbsolutePosition, const FString& Text, const FLinearColor& Color)
{
	if (!PreviewLayer)
	{
		return;
	}

	// 縁取り(FontOutlineSettings、マテリアル不要)を付けて、背景がどんな色でも
	// 文字がくっきり見えるようにする。
	UTextBlock* PopupText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PopupText->SetText(FText::FromString(Text));
	PopupText->SetColorAndOpacity(FSlateColor(Color));
	FSlateFontInfo Font = PopupText->GetFont();
	Font.Size = 40;
	Font.TypefaceFontName = TEXT("Bold");
	Font.OutlineSettings.OutlineSize = 2;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	PopupText->SetFont(Font);

	UCanvasPanelSlot* PopupSlot = PreviewLayer->AddChildToCanvas(PopupText);
	if (!PopupSlot)
	{
		return;
	}

	const FGeometry LayerGeometry = PreviewLayer->GetCachedGeometry();
	const FVector2D LocalPos = LayerGeometry.AbsoluteToLocal(AbsolutePosition);

	PopupSlot->SetAutoSize(true);
	PopupSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PopupSlot->SetPosition(LocalPos);

	PopupText->SetVisibility(ESlateVisibility::HitTestInvisible);

	ActivePopupWidgets.Add(PopupText);
	ActivePopupElapsed.Add(0.f);
	ActivePopupStartLocalPos.Add(LocalPos);
}

void UCGCardHostWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	for (int32 Index = ActivePopupWidgets.Num() - 1; Index >= 0; --Index)
	{
		UTextBlock* PopupText = ActivePopupWidgets[Index];
		if (!PopupText)
		{
			ActivePopupWidgets.RemoveAt(Index);
			ActivePopupElapsed.RemoveAt(Index);
			ActivePopupStartLocalPos.RemoveAt(Index);
			continue;
		}

		ActivePopupElapsed[Index] += InDeltaTime;
		const float Alpha = FMath::Clamp(ActivePopupElapsed[Index] / DamagePopupDuration, 0.f, 1.f);

		if (Alpha >= 1.f)
		{
			PopupText->RemoveFromParent();
			ActivePopupWidgets.RemoveAt(Index);
			ActivePopupElapsed.RemoveAt(Index);
			ActivePopupStartLocalPos.RemoveAt(Index);
			continue;
		}

		// 上へ浮かびながら、後半でフェードアウトする。
		if (UCanvasPanelSlot* PopupSlot = Cast<UCanvasPanelSlot>(PopupText->Slot))
		{
			FVector2D NewPos = ActivePopupStartLocalPos[Index];
			NewPos.Y -= DamagePopupRiseDistance * Alpha;
			PopupSlot->SetPosition(NewPos);
		}
		const float Opacity = 1.f - FMath::Clamp((Alpha - 0.5f) / 0.5f, 0.f, 1.f);
		PopupText->SetRenderOpacity(Opacity);
	}
}
