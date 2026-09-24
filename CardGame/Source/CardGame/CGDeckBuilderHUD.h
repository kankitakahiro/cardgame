#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Blueprint/UserWidget.h"
#include "CGCardHostWidget.h"
#include "CGTypes.h"
#include "CGCardFilterSort.h"
#include "CGDeckBuilderHUD.generated.h"

class UTextBlock;
class UHorizontalBox;
class UVerticalBox;
class UWrapBox;
class UButton;
class UEditableTextBox;
class UCGGameInstance;
struct FCGCardDef;

// デッキ構築画面。カードプールから25枚(同名カードは3枚まで重複可)をトグルで
// 選び、保存してロビーへ戻る(次期ルール、docs/next-ruleset-design.md)
// (docs/architecture.md「レベルと画面遷移」)。UIはCGGameHUDと同じくC++側で
// WidgetTreeを組み立てる(UMG編集の制約はdocs/automation-notes.md参照)。カードの
// ホバー拡大プレビューはUCGCardHostWidget側の共通実装を利用する
// (docs/architecture.md「ホバー拡大とZ順序」)。
//
// カードの種類が今後増えていく前提で、一覧(下段のカードプール)には検索/種別
// フィルタ/並び替え/種族・種別ごとのグルーピング/コンパクト表示切り替えを設けている
// (「カードが増えたときの工夫」についてのフィードバックを受けて追加)。
UCLASS()
class UCGDeckBuilderHUD : public UCGCardHostWidget
{
	GENERATED_BODY()

protected:
	// WidgetTree->RootWidgetは NativeConstruct() ではなく RebuildWidget() の中で
	// 設定しないと、Slate側が先に空のプレースホルダー(SSpacer)を取得してキャッシュ
	// してしまい、後からRootWidgetを設定しても画面に反映されない
	// (docs/automation-notes.mdの黒画面バグ参照)。
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void EnsureWidgetTreeBuilt();
	bool bWidgetTreeBuilt = false;

	void RefreshUI();

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	// このデッキを保存するときの名前。編集開始時はGameInstanceの現在アクティブな
	// デッキ名を初期値にする(同じ名前のまま保存すれば上書き、変えれば新規保存)。
	UPROPERTY()
	TObjectPtr<UEditableTextBox> DeckNameBox;

	// 上段: 現在のデッキ(シャドウバース風に、選んだ結果をここで確認しながら組む)。
	// クリックで外す。
	UPROPERTY()
	TObjectPtr<UHorizontalBox> DeckRowBox;

	// 下段: カード一覧。クリックで上段のデッキへ追加する。検索/フィルタ/並び替えの
	// 結果に応じて、PoolListContainerの中身をRefreshPoolList()が作り直す
	// (グルーピング表示のときは見出し+UWrapBoxの組を複数、それ以外は単一のUWrapBox)。
	UPROPERTY()
	TObjectPtr<UVerticalBox> PoolListContainer;

	// カード名で絞り込む検索ボックス。
	UPROPERTY()
	TObjectPtr<UEditableTextBox> SearchBox;

	// 種別フィルタ(全て/Unit/Spell)。現在選択中のものだけ不透明度を上げて示す。
	UPROPERTY()
	TObjectPtr<UButton> FilterAllButton;

	UPROPERTY()
	TObjectPtr<UButton> FilterUnitButton;

	UPROPERTY()
	TObjectPtr<UButton> FilterSpellButton;

	// 色フィルタ(全色/赤/橙/緑/青/紫/無色)。「デッキ構築で色でフィルターを
	// かけれるようにしてほしい」というフィードバックへの対応。種別フィルタと
	// 独立して併用できる(AND条件、CGCardFilterSort::BuildFilteredSortedList参照)。
	UPROPERTY()
	TObjectPtr<UButton> FilterColorAllButton;

	UPROPERTY()
	TObjectPtr<UButton> FilterColorRedButton;

	UPROPERTY()
	TObjectPtr<UButton> FilterColorOrangeButton;

	UPROPERTY()
	TObjectPtr<UButton> FilterColorGreenButton;

	UPROPERTY()
	TObjectPtr<UButton> FilterColorBlueButton;

	UPROPERTY()
	TObjectPtr<UButton> FilterColorPurpleButton;

	UPROPERTY()
	TObjectPtr<UButton> FilterColorNoneButton;

	// 並び替え基準(コスト順/名前順/種族順)。種族順選択時はTribeで、それ以外は
	// 選択していないときはグルーピングなし(単一のWrapBox)。
	UPROPERTY()
	TObjectPtr<UButton> SortCostButton;

	UPROPERTY()
	TObjectPtr<UButton> SortNameButton;

	UPROPERTY()
	TObjectPtr<UButton> SortTribeButton;

	// 一覧をコンパクト(縮小)表示するかどうかの切り替え。多数のカードを一度に
	// 見比べたいときに使う。
	UPROPERTY()
	TObjectPtr<UButton> CompactToggleButton;

	UPROPERTY()
	TObjectPtr<UButton> SaveButton;

	UFUNCTION()
	void HandleDeckSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandlePoolSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleSearchTextChanged(const FText& NewText);

	UFUNCTION()
	void HandleFilterAllClicked();

	UFUNCTION()
	void HandleFilterUnitClicked();

	UFUNCTION()
	void HandleFilterSpellClicked();

	UFUNCTION()
	void HandleFilterColorAllClicked();

	UFUNCTION()
	void HandleFilterColorRedClicked();

	UFUNCTION()
	void HandleFilterColorOrangeClicked();

	UFUNCTION()
	void HandleFilterColorGreenClicked();

	UFUNCTION()
	void HandleFilterColorBlueClicked();

	UFUNCTION()
	void HandleFilterColorPurpleClicked();

	UFUNCTION()
	void HandleFilterColorNoneClicked();

	UFUNCTION()
	void HandleSortCostClicked();

	UFUNCTION()
	void HandleSortNameClicked();

	UFUNCTION()
	void HandleSortTribeClicked();

	UFUNCTION()
	void HandleCompactToggleClicked();

private:
	// この画面内だけで保持する「編集中の選択デッキ」(選んだ順)。保存を押すまで
	// GameInstance側には反映しない(キャンセルで変更を破棄してロビーへ戻れるようにするため)。
	TArray<FName> SelectedCardIds;

	// RefreshUI時点の上段/下段の並び(クリック時にSlotIndexからCardIdへ逆引きするため)。
	// LastPoolCardIdsは検索/フィルタ/並び替え後、実際に画面へ表示している順。
	TArray<FName> LastDeckCardIds;
	TArray<FName> LastPoolCardIds;

	// 検索/フィルタ/並び替えの状態(CGCardFilterSort、カード図鑑と共通)。
	FCGCardFilterSortState FilterSortState;
	bool bCompactPoolDisplay = false;

	// 現在の検索/フィルタ/並び替え条件を適用した一覧を作る。
	TArray<FCGCardDef> BuildFilteredSortedPool() const;

	// PoolListContainerの中身(検索/フィルタ/並び替え/グルーピング後の一覧)を作り直す。
	void RefreshPoolList();

	// カード1枚ぶんのウィジェットを作ってWrapBoxへ追加する(PoolのカードはFilteredPool
	// 内でのインデックスをSlotIndexとして持たせ、クリック時にLastPoolCardIdsで逆引きする)。
	void AddPoolCardToWrap(UWrapBox* Wrap, const FCGCardDef& Def, int32 SlotIndex);

	// フィルタ/並び替えボタンの見た目(選択中のものだけ強調)を更新する。
	void UpdateFilterSortButtonVisuals();

	UCGGameInstance* GetCGGameInstance() const;
};
