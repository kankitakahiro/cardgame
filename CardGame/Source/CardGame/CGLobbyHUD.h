#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGCardHostWidget.h"
#include "CGCardFilterSort.h"
#include "CGLobbyHUD.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UWrapBox;
class UEditableTextBox;
class UBorder;
struct FCGCardDef;

// ロビー画面。「デッキ構築」「デッキ選択」(保存済みデッキから選ぶ)「バトル開始」に
// 加え、全カードを検索/並び替えしながら
// 眺められる「カード図鑑」、対戦ルールを説明する「遊び方」を持つ
// (docs/architecture.md「レベルと画面遷移」)。UIはCGGameHUDと同じくC++側で
// WidgetTreeを組み立てる(UMG編集の制約はdocs/automation-notes.md参照)。カード図鑑・
// 遊び方はどちらもレベル遷移を伴わない全画面レイヤーとして実装しており
// (UCGCardHostWidget::BuildRootOverlayのModalLayer、両方まとめて1つのUOverlayに
// 載せて渡している)、カード図鑑側のホバー拡大プレビューもこの画面自身の
// PreviewLayerでそのまま使える。
UCLASS()
class UCGLobbyHUD : public UCGCardHostWidget
{
	GENERATED_BODY()

protected:
	// WidgetTree->RootWidgetは NativeConstruct() ではなく RebuildWidget() の中で
	// 設定しないと、Slate側が先に空のプレースホルダー(SSpacer)を取得してキャッシュ
	// してしまい、後からRootWidgetを設定しても画面に反映されない
	// (docs/automation-notes.mdの黒画面バグ参照)。
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void EnsureWidgetTreeBuilt();
	bool bWidgetTreeBuilt = false;

	UFUNCTION()
	void HandleDeckBuilderClicked();

	UFUNCTION()
	void HandleStartBattleClicked();

	UFUNCTION()
	void HandleOpenDeckSelectClicked();

	UFUNCTION()
	void HandleCloseDeckSelectClicked();

	// UCGDeckListRowWidget::OnSelectClicked/OnDeleteClickedから、押された行の
	// インデックス(=GameInstance::SavedDecksのインデックス)付きで呼ばれる。
	UFUNCTION()
	void HandleDeckSelectRowSelected(int32 SlotIndex);

	UFUNCTION()
	void HandleDeckSelectRowDeleted(int32 SlotIndex);

	UFUNCTION()
	void HandleOpenCodexClicked();

	UFUNCTION()
	void HandleCloseCodexClicked();

	UFUNCTION()
	void HandleCodexSearchTextChanged(const FText& NewText);

	UFUNCTION()
	void HandleCodexFilterAllClicked();

	UFUNCTION()
	void HandleCodexFilterUnitClicked();

	UFUNCTION()
	void HandleCodexFilterSpellClicked();

	UFUNCTION()
	void HandleCodexSortCostClicked();

	UFUNCTION()
	void HandleCodexSortNameClicked();

	UFUNCTION()
	void HandleCodexSortTribeClicked();

	UFUNCTION()
	void HandleCodexCompactToggleClicked();

	UFUNCTION()
	void HandleOpenHowToPlayClicked();

	UFUNCTION()
	void HandleCloseHowToPlayClicked();

	// オンライン対戦(docs/online-play-design.md参照)。ホストは対戦レベルを
	// リッスンサーバーとして開き、ゲストはIPアドレス(LAN)またはホストの
	// SteamID(インターネット越し、SteamSocketsプラグイン経由でポート開放不要)
	// を指定して直接接続する。
	UFUNCTION()
	void HandleOpenOnlineClicked();

	UFUNCTION()
	void HandleCloseOnlineClicked();

	UFUNCTION()
	void HandleHostGameClicked();

	UFUNCTION()
	void HandleJoinGameClicked();

private:
	// カード図鑑(BuildRootOverlayのModalLayer)。普段はCollapsedで、「カード図鑑」
	// ボタンで表示する全画面レイヤー。
	UPROPERTY()
	TObjectPtr<UBorder> CodexLayer;

	// 検索/フィルタ/並び替え後の一覧を並べる場所(グルーピング時は見出し+WrapBoxの
	// 組を複数、それ以外は単一のWrapBox。UCGDeckBuilderHUDと同じ構造)。
	UPROPERTY()
	TObjectPtr<UVerticalBox> CodexListContainer;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> CodexSearchBox;

	UPROPERTY()
	TObjectPtr<UButton> CodexFilterAllButton;

	UPROPERTY()
	TObjectPtr<UButton> CodexFilterUnitButton;

	UPROPERTY()
	TObjectPtr<UButton> CodexFilterSpellButton;

	UPROPERTY()
	TObjectPtr<UButton> CodexSortCostButton;

	UPROPERTY()
	TObjectPtr<UButton> CodexSortNameButton;

	UPROPERTY()
	TObjectPtr<UButton> CodexSortTribeButton;

	UPROPERTY()
	TObjectPtr<UButton> CodexCompactToggleButton;

	// 検索/フィルタ/並び替えの状態(CGCardFilterSort、デッキ構築画面と共通ロジック)。
	FCGCardFilterSortState FilterSortState;
	bool bCompactDisplay = false;

	// カード図鑑の中身(検索/フィルタ/並び替え/グルーピング後の一覧)を作り直す。
	void RefreshCodexList();

	// カード1枚ぶんのウィジェットを作ってWrapBoxへ追加する(図鑑は選ぶ操作がないため、
	// クリックハンドラは登録しない。ホバー拡大プレビューのみ有効)。
	void AddCodexCardToWrap(class UWrapBox* Wrap, const FCGCardDef& Def);

	// フィルタ/並び替えボタンの見た目(選択中のものだけ強調)を更新する。
	void UpdateCodexButtonVisuals();

	// 対戦ルールを説明する全画面レイヤー(BuildRootOverlayのModalLayer、CodexLayerと
	// 同じUOverlayに載せている)。普段はCollapsedで、「遊び方」ボタンで表示する。
	// カード一覧と違って中身は固定テキストのため、RefreshUI相当の再構築は不要
	// (EnsureWidgetTreeBuiltで1回だけ組み立てる)。
	UPROPERTY()
	TObjectPtr<UBorder> HowToPlayLayer;

	// 見出し+本文の1セクションを組み立ててRootへ追加する(遊び方画面専用)。
	void AddHowToPlaySection(class UVerticalBox* Root, const FString& Heading, const FString& Body);

	// デッキ選択(保存済みデッキの一覧から、バトルで使うデッキを選ぶ)。
	// カード図鑑・遊び方と同じ全画面モーダルレイヤー方式(BuildRootOverlayの
	// ModalLayer)。デッキ構築画面と違いレベル遷移しない
	// (docs/architecture.md「デッキの永続化」)。
	UPROPERTY()
	TObjectPtr<UBorder> DeckSelectLayer;

	UPROPERTY()
	TObjectPtr<UVerticalBox> DeckSelectListContainer;

	// GameInstance::SavedDecksの内容でDeckSelectListContainerの中身を作り直す。
	void RefreshDeckSelectList();

	// オンライン対戦(ホスト/参加)の全画面モーダルレイヤー(docs/online-play-
	// design.md「ロビー画面の変更点」)。カード図鑑・遊び方・デッキ選択と同じ方式。
	UPROPERTY()
	TObjectPtr<UBorder> OnlineLayer;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> JoinIPBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> OnlineStatusText;

	// 自分のSteamID(64bit、10進数文字列)を表示する行。ホストする側が
	// このIDを友達に伝えれば、友達はJoinIPBoxにこれを入力してインターネット
	// 越しに参加できる(SteamSocketsプラグイン、docs/online-play-design.md参照)。
	// Steamが使えない環境(Steamクライアント未起動等)では空文字のまま。
	// コピペできるよう、普通のTextBlockではなく読み取り専用のEditableTextBox
	// にしている(「SteamIDをコピペできるようにしてほしい」というフィードバックへの
	// 対応。UTextBlockは選択/コピー操作に対応していない)。
	UPROPERTY()
	TObjectPtr<UEditableTextBox> YourSteamIdText;

	// YourSteamIdTextの上に表示する固定の説明文(SteamID自体はEditableTextBox側のみに
	// 入れ、コピー時に説明文まで一緒に選択されてしまわないようにする)。
	UPROPERTY()
	TObjectPtr<UTextBlock> YourSteamIdLabel;

	// ローカルプレイヤーのSteamID64を10進数文字列で返す(取得できなければ空文字)。
	FString GetLocalSteamIdString() const;

	class UCGGameInstance* GetCGGameInstance() const;
};
