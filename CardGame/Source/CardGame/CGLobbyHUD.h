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

// ロビー画面。「デッキ構築」「対戦開始」(押すと対戦相手・自分のデッキが今
// 何になっているかを表示する「デッキ選択」画面が開き、そこから一覧画面へ
// 遷移してデッキを選び直せる)に加え、全カードを検索/並び替えしながら眺められる
// 「カード図鑑」、対戦ルールを説明する「遊び方」、自分のデッキを選んでから
// ホスト/参加する「オンライン対戦」を持つ(docs/architecture.md「レベルと
// 画面遷移」「ロビーのモーダル構成」)。UIはCGGameHUDと同じくC++側で
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

	// 「対戦開始」ボタン。以前はここで直ちにOpenLevelしていたが、「対戦ボタンを
	// 押した後に相手と自分のデッキを選択してから対戦が開始されるようにして
	// ほしい」というフィードバックへの対応で、まず対戦相手・自分それぞれの
	// 「今選ばれているデッキ」を表示するだけの`BattleSetupLayer`を開く
	// (docs/architecture.md「ロビーのモーダル構成」参照)。
	UFUNCTION()
	void HandleOpenBattleSetupClicked();

	UFUNCTION()
	void HandleCloseBattleSetupClicked();

	// `BattleSetupLayer`の「決定」ボタン。実際のOpenLevelはここで行う
	// (以前の`HandleStartBattleClicked`の中身)。
	UFUNCTION()
	void HandleConfirmBattleSetupClicked();

	// `BattleSetupLayer`の対戦相手側「選択」ボタン。`AIDeckSelectLayer`
	// (一覧画面)を開き、選んだら`BattleSetupLayer`へ自動的に戻ってくる
	// (`PendingReturnLayer`参照)。
	UFUNCTION()
	void HandleSelectAIDeckClicked();

	// `BattleSetupLayer`の自分側「選択」ボタン。`DeckSelectLayer`(一覧画面)を
	// 開き、選んだら`BattleSetupLayer`へ自動的に戻ってくる。
	UFUNCTION()
	void HandleSelectMyDeckFromSummaryClicked();

	// `OnlineLayer`の自分側「選択」ボタン。`DeckSelectLayer`を開き、選んだら
	// `OnlineLayer`へ自動的に戻ってくる(自分のデッキ一覧はどちらの画面からも
	// 開かれる共通画面。docs/architecture.md「ロビーのモーダル構成」参照)。
	UFUNCTION()
	void HandleSelectMyDeckFromOnlineClicked();

	// UCGDeckListRowWidget::OnSelectClicked/OnDeleteClickedから、押された行の
	// インデックス(=GameInstance::SavedDecksのインデックス)付きで呼ばれる。
	// 選択後は`DeckSelectLayer`を閉じ、`PendingReturnLayer`(呼び出し元の
	// `BattleSetupLayer`または`OnlineLayer`)へ自動的に戻る。
	UFUNCTION()
	void HandleDeckSelectRowSelected(int32 SlotIndex);

	UFUNCTION()
	void HandleDeckSelectRowDeleted(int32 SlotIndex);

	// UCGDeckListRowWidget::OnSelectClickedから、押された行のインデックスが
	// 渡される。0=ランダム、1以上はGetBasicDeckColors()のインデックス+1
	// (HandleAIDeckSelectRowSelected参照)。選択後は`AIDeckSelectLayer`を閉じ、
	// `PendingReturnLayer`(常に`BattleSetupLayer`)へ自動的に戻る。
	UFUNCTION()
	void HandleAIDeckSelectRowSelected(int32 SlotIndex);

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

	// 自分のデッキ一覧(保存済みデッキ+基本デッキから選ぶ)。カード図鑑・遊び方と
	// 同じ全画面モーダルレイヤー方式(BuildRootOverlayのModalLayer)。デッキ構築
	// 画面と違いレベル遷移しない(docs/architecture.md「デッキの永続化」)。
	// `BattleSetupLayer`と`OnlineLayer`の両方から開かれる共通の一覧画面
	// (docs/architecture.md「ロビーのモーダル構成」参照)。この画面自体には「閉じる」を
	// 置かず、行を選ぶと`PendingReturnLayer`へ自動的に戻る。
	UPROPERTY()
	TObjectPtr<UBorder> DeckSelectLayer;

	UPROPERTY()
	TObjectPtr<UVerticalBox> DeckSelectListContainer;

	// GameInstance::SavedDecksの内容でDeckSelectListContainerの中身を作り直す。
	void RefreshDeckSelectList();

	// 対戦相手(AI)デッキ一覧。自分のデッキ一覧と同じ全画面モーダルレイヤー
	// 方式だが、保存済みデッキの概念は無く「ランダム+5色」の固定6択のみ
	// (docs/architecture.md「デッキの永続化」の`SelectedAIOpponentColor`参照)。
	// `BattleSetupLayer`からのみ開かれ、行を選ぶと自動的に戻る。
	UPROPERTY()
	TObjectPtr<UBorder> AIDeckSelectLayer;

	UPROPERTY()
	TObjectPtr<UVerticalBox> AIDeckSelectListContainer;

	// GameInstance::SelectedAIOpponentColorの内容でAIDeckSelectListContainerの
	// 中身(ランダム+5色、現在選択中のものをハイライト)を作り直す。
	void RefreshAIDeckSelectList();

	// 対戦準備(「デッキ選択」画面、docs/architecture.md「ロビーのモーダル構成」参照)。
	// ロビーの「対戦開始」ボタンから開く全画面モーダルで、対戦相手・自分それぞれ
	// 「今選ばれているデッキ」の情報表示(名前+枚数)のみを持ち、一覧はここには
	// 出さない。「選択」ボタンでそれぞれの一覧画面(AIDeckSelectLayer/
	// DeckSelectLayer)へ遷移し、選び終えるとこの画面へ戻ってくる。
	UPROPERTY()
	TObjectPtr<UBorder> BattleSetupLayer;

	UPROPERTY()
	TObjectPtr<UTextBlock> BattleSetupAIDeckSummaryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BattleSetupMyDeckSummaryText;

	// BattleSetupLayerの表示内容(対戦相手・自分それぞれの現在の選択)を
	// GameInstanceから読み直す。開いたとき、および一覧画面から戻ってきたときに呼ぶ。
	void RefreshBattleSetupSummary();

	// OnlineLayer内の「自分のデッキ」情報表示(BattleSetupAIDeckSummaryTextの
	// 自分側と同じ考え方)。
	UPROPERTY()
	TObjectPtr<UTextBlock> OnlineMyDeckSummaryText;

	void RefreshOnlineMyDeckSummary();

	// 現在のGameInstance::SelectedAIOpponentColor/PlayerDeckCardIdsを
	// 「表示名 (N枚)」の形式で返す(情報表示用。一覧行の表記と統一する)。
	FString GetAIDeckSummaryDisplayText() const;
	FString GetMyDeckSummaryDisplayText() const;

	// DeckSelectLayer/AIDeckSelectLayer(一覧画面)を開く直前にセットしておく、
	// 「選び終えたら戻る先」のレイヤー。この画面自体は「閉じる」を持たず、
	// 行を選ぶことでしか離脱できないため、戻り先を必ず1つ覚えておく必要がある
	// (自分のデッキ一覧はBattleSetupLayer/OnlineLayerの2箇所から開かれ得るため)。
	UPROPERTY()
	TObjectPtr<UBorder> PendingReturnLayer;

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
