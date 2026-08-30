#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGCardHostWidget.h"
#include "CGDeckBuilderHUD.generated.h"

class UTextBlock;
class UHorizontalBox;
class UButton;
class UCGGameInstance;

// デッキ構築画面。24種のカードから12枚をトグルで選び、保存してロビーへ戻る
// (docs/architecture.md「レベルと画面遷移」)。UIはCGGameHUDと同じくC++側で
// WidgetTreeを組み立てる(UMG編集の制約はdocs/automation-notes.md参照)。カードの
// ホバー拡大プレビューはUCGCardHostWidget側の共通実装を利用する
// (docs/architecture.md「ホバー拡大とZ順序」)。
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

	// 上段: 現在のデッキ(シャドウバース風に、選んだ結果をここで確認しながら組む)。
	// クリックで外す。
	UPROPERTY()
	TObjectPtr<UHorizontalBox> DeckRowBox;

	// 下段: 24種のカード一覧。クリックで上段のデッキへ追加する。
	UPROPERTY()
	TObjectPtr<UHorizontalBox> PoolRowBox;

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

private:
	// この画面内だけで保持する「編集中の選択デッキ」(選んだ順)。保存を押すまで
	// GameInstance側には反映しない(キャンセルで変更を破棄してロビーへ戻れるようにするため)。
	TArray<FName> SelectedCardIds;

	// RefreshUI時点の上段/下段の並び(クリック時にSlotIndexからCardIdへ逆引きするため)。
	TArray<FName> LastDeckCardIds;
	TArray<FName> LastPoolCardIds;

	UCGGameInstance* GetCGGameInstance() const;
};
