#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGCardSlotWidget.h"
#include "CGDeckListRowWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;

// デッキ選択画面(UCGLobbyHUD)の一覧1行分。デッキ名+カード枚数+アクティブ中
// マーカーを表示し、「選択」「削除」ボタンを持つ。UButton::OnClickedは
// パラメータを取れないため、UCGCardSlotWidgetと同じ「自分のインデックスを
// 持ち、クリックされたら該当インデックス付きでブロードキャストする」方式で
// 一覧のどの行が押されたかをHUD側へ伝える(docs/architecture.md「カードUIの設計」
// と同じ考え方。FCGOnSlotClickedはCGCardSlotWidget.hで宣言済みのものを再利用)。
UCLASS()
class UCGDeckListRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 SlotIndex = -1;

	UPROPERTY(BlueprintAssignable, Category = "CardGame")
	FCGOnSlotClicked OnSelectClicked;

	UPROPERTY(BlueprintAssignable, Category = "CardGame")
	FCGOnSlotClicked OnDeleteClicked;

	// この行の表示内容を設定する。bIsActiveなら「選択中」の見た目にし、
	// 選択ボタンを無効化する(既にアクティブなものを選び直す必要はないため)。
	// bAllowDelete=falseなら削除ボタン自体を隠す(色ごとの基本デッキ一覧など、
	// 削除の概念が無い行に使う)。
	void SetRowData(int32 InSlotIndex, const FString& DeckName, int32 CardCount, bool bIsActive, bool bAllowDelete = true);

protected:
	// WidgetTree->RootWidgetは NativeConstruct() ではなく RebuildWidget() の中で
	// 設定しないと、Slate側が先に空のプレースホルダー(SSpacer)を取得してキャッシュ
	// してしまい、後からRootWidgetを設定しても画面に反映されない
	// (docs/automation-notes.mdの黒画面バグ参照)。
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void EnsureBuilt();

	UPROPERTY()
	TObjectPtr<UBorder> RowBorder;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UButton> SelectButton;

	UPROPERTY()
	TObjectPtr<UButton> DeleteButton;

	UFUNCTION()
	void HandleSelectClicked();

	UFUNCTION()
	void HandleDeleteClicked();
};
