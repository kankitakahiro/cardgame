#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Blueprint/UserWidget.h"
#include "CGTypes.h"
#include "CGCardSlotWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UOverlay;
class UCGCardSlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCGOnSlotClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCGOnCardHoverChanged, UCGCardSlotWidget*, Card, bool, bIsHovering);

// カード1枚を、名前/コスト/種別/イラスト欄(プレースホルダー)/部族/効果テキスト/
// フレーバーテキスト/右下ATK-HPを持つ「カードらしい」見た目で表示するウィジェット。
// 手札・マーケット・場のユニット・相手の裏向き手札・デッキ構築画面、カードを表示する
// 箇所すべてでこの1つのウィジェットを共通利用している(docs/architecture.md「カードUIの設計」)。
// UMGのWidgetTreeはPython Editor Scripting APIから編集できない(docs/automation-notes.md)
// ため、UI一式はC++側(RebuildWidget)で組み立てている。
UCLASS()
class UCGCardSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// カード1枚分の基本サイズとホバー時の拡大率。ホバー時の拡大表示は自分自身ではなく
	// ホスト側(UCGCardHostWidget)が最前面レイヤーに複製したプレビューとして行うため、
	// 公開しているこのサイズはそのプレビューの配置計算にも使われる
	// (docs/architecture.md「ホバー拡大とZ順序」)。
	// 一般的なトレーディングカード(2.5:3.5インチ、比率約0.714)に近い縦横比にしている。
	static constexpr float CardWidth = 240.f;
	static constexpr float CardHeight = 336.f;
	static constexpr float CardHoverScale = 1.35f;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 SlotIndex = -1;

	UPROPERTY(BlueprintAssignable, Category = "CardGame")
	FCGOnSlotClicked OnSlotClicked;

	// ホバー状態が変わるたびに通知する。HorizontalBox等の通常のパネルは常に子の追加順で
	// 描画され、個々の子だけをZ順序で前面に出す簡単な方法がないため、拡大表示は
	// このカード自身ではなく、通知を受けたホスト側(UCGCardHostWidget)が最前面レイヤーに
	// 複製したプレビューとして行う(docs/architecture.md「ホバー拡大とZ順序」)。
	UPROPERTY(BlueprintAssignable, Category = "CardGame")
	FCGOnCardHoverChanged OnHoverChanged;

	// カードの中身(名前/コスト/種別枠/部族/効果文/フレーバー/右下ATK-HP)を反映する。
	// 場のユニットはバフ等でATK/HPがカード基本値と異なることがあるため、
	// OverrideAtk/OverrideHpで現在値を上書きできる(省略時はDefの基本値を使う)。
	void SetCardData(const FCGCardDef& Def, TOptional<int32> OverrideAtk = TOptional<int32>(),
		TOptional<int32> OverrideHp = TOptional<int32>());

	// 相手の裏向き手札など、中身を見せたくないカード用。名前欄に "?" とだけ表示し、
	// 種別枠は無地のまま、他のゾーンは全て空にする。
	void SetFaceDown();

	// ホバー時の最前面プレビュー用に、このカードと全く同じ内容をTargetへ複製する。
	// 直近に呼ばれたSetCardData/SetFaceDownの内容を覚えておいて、そのまま再現する。
	void CopyCardDataTo(UCGCardSlotWidget* Target) const;

protected:
	// WidgetTree->RootWidgetは NativeConstruct() ではなく RebuildWidget() の中で
	// 設定しないと、Slate側が先に空のプレースホルダー(SSpacer)を取得してキャッシュ
	// してしまい、後からRootWidgetを設定しても画面に反映されない
	// (Widget Reflectorで実際に確認した既知の落とし穴。docs/automation-notes.md参照)。
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	// ホバー状態が変わったことをOnHoverChangedで通知するだけで、自分自身の見た目は
	// 変えない(拡大表示はホスト側が最前面レイヤーで行う。上のOnHoverChangedのコメント参照)。
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	void EnsureBuilt();
	void ApplyCardTypeColor();
	void RefreshDisplay();

	UPROPERTY()
	TObjectPtr<UButton> Button;

	// 各ゾーンのテキスト。
	UPROPERTY() TObjectPtr<UTextBlock> CostText;
	UPROPERTY() TObjectPtr<UTextBlock> NameText;
	UPROPERTY() TObjectPtr<UTextBlock> TribeText;
	UPROPERTY() TObjectPtr<UTextBlock> EffectText;
	UPROPERTY() TObjectPtr<UTextBlock> FlavorText;
	UPROPERTY() TObjectPtr<UTextBlock> StatText;

	// 多層フレーム。外側から: OuterBorder(境界線)→TypeBorder(種別色)→RarityBorder(飾り枠、
	// 現状はデータなしの中間色のみ)。
	UPROPERTY() TObjectPtr<UBorder> OuterBorder;
	UPROPERTY() TObjectPtr<UBorder> TypeBorder;
	UPROPERTY() TObjectPtr<UBorder> RarityBorder;

	// 右下のATK/HP表示をカード面に重ねるためのOverlayと、その背景バッジ。
	UPROPERTY() TObjectPtr<UOverlay> Overlay;
	UPROPERTY() TObjectPtr<UBorder> StatBadgeBorder;

	UFUNCTION()
	void HandleClicked();

private:
	TOptional<ECGCardType> CardType;

	// SetCardData/SetFaceDownで設定された、RefreshDisplay()が反映する内容。
	// EnsureBuilt()より前に呼ばれる可能性があるため、一旦ここへ溜めてから反映する。
	FString PendingCostText;
	FString PendingNameText;
	FString PendingTribeText;
	FString PendingEffectText;
	FString PendingFlavorText;
	FString PendingStatText;

	// CopyCardDataTo()でホバープレビュー用の複製に再現するための、直近の
	// SetCardData/SetFaceDown呼び出し内容。
	bool bHasCardData = false;
	bool bLastWasFaceDown = false;
	FCGCardDef LastCardDef;
	TOptional<int32> LastOverrideAtk;
	TOptional<int32> LastOverrideHp;
};
