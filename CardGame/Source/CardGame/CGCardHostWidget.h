#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGCardHostWidget.generated.h"

class UCGCardSlotWidget;
class UCanvasPanel;
class UWidget;
class UTextBlock;

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
	// ModalLayerを渡すと、MainContentとホバープレビュー層の間に重ねる(選択ポップアップ
	// 等、MainContentより手前・ホバープレビューより奥にしたいレイヤー用。
	// docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。ホバープレビュー層は常に最後に追加され、
	// ModalLayerを含むどのレイヤーよりも手前になる(ポップアップ内のカードを
	// ホバーしたときの拡大表示がポップアップの下に隠れないようにするため)。
	UWidget* BuildRootOverlay(UWidget* MainContent, UWidget* ModalLayer = nullptr);

	// カードを1枚作るたびにこれを呼んでおくと、ホバー時に自動でプレビュー連携される。
	void RegisterCardHoverPreview(UCGCardSlotWidget* Card);

	// ホバー中のカードがClearChildren()等で画面から消えると、NativeOnMouseLeaveが
	// 発火せず拡大プレビューだけが画面に残り続けてしまう。カード一覧を作り直す
	// RefreshUI()系の処理の先頭で必ず呼んでおくと、この「拡大カードの残留」を防げる。
	void HidePreview();

	// 攻撃演出/HP変化演出用: AbsolutePosition(画面絶対座標)を起点に、上へ浮かびながら
	// フェードする数値ポップアップを1つ表示する(docs/architecture.md「カードUIの設計」)。
	// ホバープレビューと同じ最前面レイヤーに乗せるため、カードや他のUIに隠れない。
	// bIsHealでダメージ(赤・"-N")/回復(緑・"+N")の見た目を切り替える
	// (「ダメージなどの表記を変更してほしい」というフィードバックへの対応)。
	void ShowFloatingNumber(const FVector2D& AbsolutePosition, int32 Amount, bool bIsHeal);

	// ShowFloatingNumber()と同じ浮遊+フェードの仕組みで、任意の文字列を表示する
	// (カードプレイ演出でカード名を出す用途等。docs/architecture.md「カードUIの設計」)。
	void ShowFloatingText(const FVector2D& AbsolutePosition, const FString& Text, const FLinearColor& Color);

	// NativeTick()で進行中のダメージポップアップを更新する。派生クラスが
	// Super::NativeTick()を呼べば、このクラスのTick処理も一緒に走る。
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleCardHoverChanged(UCGCardSlotWidget* Card, bool bIsHovering);

	// 普段は空の、ホバープレビュー専用の最前面レイヤー。
	UPROPERTY()
	TObjectPtr<UCanvasPanel> PreviewLayer;

	// 遅延生成して使い回す、拡大表示用のプレビューカード(常に1枚だけ)。
	UPROPERTY()
	TObjectPtr<UCGCardSlotWidget> PreviewCardWidget;

	// 表示中のダメージ数値ポップアップ。3本の配列はインデックスで対応する
	// (専用USTRUCTを増やすほどの複雑さではないため、簡易な並列配列にしている)。
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ActivePopupWidgets;
	TArray<float> ActivePopupElapsed;
	TArray<FVector2D> ActivePopupStartLocalPos;
};
