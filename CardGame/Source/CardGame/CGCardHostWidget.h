#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGCardHostWidget.generated.h"

class UCGCardSlotWidget;
class UCanvasPanel;
class UWidget;

// CGGameHUD・CGDeckBuilderHUDなど、UCGCardSlotWidgetを並べて表示する画面の共通基盤。
// カードをカーソルでホバーしたときに拡大表示するプレビューを、画面の最前面レイヤーに
// 出すための仕組みをここにまとめている。UHorizontalBox等の通常のパネルは常に子の
// 追加順で描画され、個々の子だけをZ順序で前面へ出す簡単な方法がないため、拡大表示は
// カード自身を大きくするのではなく、「本体はそのまま小さく」「見た目だけ最前面の
// 専用レイヤーに複製して拡大」することで、隣のカードに隠れずに手前へ出す
// (docs/architecture.md「ホバー拡大とZ順序」)。
UCLASS(Abstract)
class UCGCardHostWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// 派生クラスがEnsureWidgetTreeBuilt()相当の最後で、それまで組み立てたHUD本体
	// (MainContent)を渡して呼ぶ。戻り値をWidgetTree->RootWidgetに設定すること。
	// 内部でMainContentとプレビュー専用レイヤーを重ねたOverlayを作って返す。
	UWidget* BuildRootOverlay(UWidget* MainContent);

	// カードを1枚作るたびにこれを呼んでおくと、ホバー時に自動でプレビュー連携される。
	void RegisterCardHoverPreview(UCGCardSlotWidget* Card);

private:
	UFUNCTION()
	void HandleCardHoverChanged(UCGCardSlotWidget* Card, bool bIsHovering);

	// 普段は空の、ホバープレビュー専用の最前面レイヤー。
	UPROPERTY()
	TObjectPtr<UCanvasPanel> PreviewLayer;

	// 遅延生成して使い回す、拡大表示用のプレビューカード(常に1枚だけ)。
	UPROPERTY()
	TObjectPtr<UCGCardSlotWidget> PreviewCardWidget;
};
