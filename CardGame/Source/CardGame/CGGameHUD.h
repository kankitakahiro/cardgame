#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CGCardHostWidget.h"
#include "CGTypes.h"
#include "CGGameHUD.generated.h"

class UTextBlock;
class UHorizontalBox;
class UVerticalBox;
class UWrapBox;
class UButton;
class UScrollBox;
class UBorder;
class ACGGameMode;
class ACGGameState;
class ACGPlayerState;
struct FCGPendingChoice;
struct FCGLastAttackResult;

// ゲーム全体のHUD。マーケット/両陣営の場/手札/ステータス/EndTurnボタンを持つ。
// WidgetBlueprintのWidgetTreeをPythonから編集できない制約(docs/automation-notes.md)
// のため、UIは全てC++側で構築している。カードのホバー拡大プレビューは
// UCGCardHostWidget側の共通実装を利用する(docs/architecture.md「ホバー拡大とZ順序」)。
UCLASS()
class UCGGameHUD : public UCGCardHostWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RefreshUI();

protected:
	// WidgetTree->RootWidgetは NativeConstruct() ではなく RebuildWidget() の中で
	// 設定しないと、Slate側が先に空のプレースホルダー(SSpacer)を取得してキャッシュ
	// してしまい、後からRootWidgetを設定しても画面に反映されない
	// (Widget Reflectorで実際に確認した既知の落とし穴。docs/automation-notes.md参照)。
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	// AttackSequenceNumberの変化検知と、その演出再生(PlayAttackAnimation)をここで行う。
	// RefreshUI()の中で直接呼ばないのは、盤面の行はRefreshUI()の中でClearChildren()
	// して作り直したばかりで、そのフレームではまだ一度もSlateのPaintを経ておらず
	// GetCachedGeometry()が(0,0)を返してしまうため(実際に座標が取れるのはPaintを
	// 経た後の次のフレーム以降)。Tick側で座標が有効になるまで数フレーム再試行する。
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void EnsureWidgetTreeBuilt();
	bool bWidgetTreeBuilt = false;

	// RefreshUI()の各行更新はほぼ同じ形(ClearChildren→カードごとにSlot作成→Add)を
	// 繰り返すため、共通処理をここにまとめている。DisplayScaleは
	// UCGCardSlotWidget::WrapForCompactDisplay()にそのまま渡す縮小率
	// (docs/architecture.md「カードUIの設計」)。
	void PopulateFaceDownHandRow(UHorizontalBox* Box, int32 CardCount, float DisplayScale);
	void PopulateBoardRow(UHorizontalBox* Box, ACGPlayerState* Side, bool bIsSelfSide, float DisplayScale);
	void PopulateCardRow(UHorizontalBox* Box, const TArray<FName>& CardIds, float DisplayScale, FName ClickHandlerName);

	// マーケットは横長の行ではなく縦長のサイドレールに2列で折り返し表示するため、
	// 他の行(横一列)とは別にWrapBox用の関数を用意している
	// (docs/architecture.md「カードUIの設計」)。次期ルール(docs/next-ruleset-
	// design.md)でCardId単体では枠を一意に特定できなくなった(両者の山札に同じ
	// カードが同時に並び得るため)ため、FCGMarketSlot(出どころ付き)を受け取る。
	// 空(NAME_None)の枠はスキップするが、SlotIndexは元のインデックスをそのまま
	// 使う(クリック時にCGState->MarketSlotsへ正しく対応させるため)。
	// SelfSideは「今のコイン+マナで買えるか」を判定して買えない枠を薄く表示する
	// ために渡す(「マーケットの買えないカードを分かりやすくしてほしい」という
	// フィードバックへの対応)。nullptrなら判定をスキップする。
	void PopulateMarketColumn(UWrapBox* Box, const TArray<FCGMarketSlot>& Slots, float DisplayScale, ACGPlayerState* SelfSide);

	// 攻撃演出(突進/被弾フラッシュ/ダメージ数値)を再生する。対象カードのウィジェットが
	// まだ有効な座標を持っていない(NativeTickのコメント参照)場合は何もせずfalseを
	// 返す(NativeTick側が次のフレームで再試行する)。再生できた/対象がもう場に
	// 存在せず諦めた場合はtrueを返す(docs/architecture.md「カードUIの設計」)。
	bool PlayAttackAnimation(const FCGLastAttackResult& Result);

	// カードプレイ演出。「それぞれのカードがプレイされた演出もない」という
	// フィードバックへの対応。Unitは着地した盤面のカードを金色に点滅させつつ
	// (PlayHitFlashを被弾用の赤ではなく金色で流用)、その上にカード名の浮遊
	// テキストも出す(点滅だけだとStateVersionの追い更新で盤面が作り直され、
	// 演出が数フレームで打ち切られて見えないことがあったため)。Spellは場に
	// 残らないため、代わりにプレイヤーの顔(ライフオーブ)の位置に同じ浮遊
	// テキストを表示する。
	// CardPlaySequenceNumber(ACGGameState)とBoardUnits(ACGPlayerState)は別Actorの
	// レプリケーションで同じフレームに揃う保証が無いため(NativeTickのStateVersion
	// コメント参照)、Unit対象がまだ盤面に見当たらない間はPlayAttackAnimation()と
	// 同様にfalseを返して次のフレームに再試行する。
	bool PlayCardPlayAnimation(const FCGLastCardPlayResult& Result);

	// 墓地から1枚選ぶ選択待ち(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)の間だけ、
	// 自分の墓地の中身を条件(コスト上限/Spell限定)で絞り込んで
	// 一時的に表示する。墓地は普段は枚数しか表示していない。
	void PopulateGraveyardViewer(const ACGPlayerState& Self, const FCGPendingChoice& Choice);

	// KeepOrBury選択待ち中だけ、山札の一番上のカード(1枚)を表示する
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	void PopulateScryViewer(FName RevealedCardId);

	// BuyDestination選択待ち中だけ、購入したカード(1枚)を表示する
	// (「購入時に行き先を選べるようにしてほしい」というフィードバックのため)。
	void PopulateBuyDestinationViewer(FName CardId);

	// HandleXxxClickedの先頭で毎回繰り返していたGameMode/GameState取得+nullチェックの共通化。
	bool TryGetGameModeAndState(ACGGameMode*& OutGameMode, ACGGameState*& OutCGState) const;

	// ターン数/勝敗のみを表示する最上部の細いバナー。HPはライフオーブ
	// (EnemyOrbText/SelfOrbText)、それ以外はEnemySecondaryInfoText/
	// SelfSecondaryInfoTextへ分離した(docs/architecture.md「カードUIの設計」の
	// 方針に合わせ、情報は各陣営に対で近い位置へ)。
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	// 選択式カード効果/攻撃対象選択(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)の
	// 選択待ち中だけ表示するプロンプト文言。選択待ちが無いときはCollapsed。
	UPROPERTY()
	TObjectPtr<UTextBlock> ChoicePromptText;

	// 墓地/山札スクライのように普段は画面に無いカードを選ぶときだけ、画面の手前に
	// 出す共通ポップアップ(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。背景を暗く
	// 覆って中央にパネルを出す。中身(GraveyardModalSection/ScryBox)は選択の種類に
	// 応じてどちらか一方だけを表示する。
	UPROPERTY()
	TObjectPtr<UBorder> ChoiceModalLayer;

	// GraveyardCard選択待ち中だけ表示する、一時墓地ビューアーの中身(見出し+カード行)。
	UPROPERTY()
	TObjectPtr<UVerticalBox> GraveyardModalSection;

	// 行動ログ(次期ルール):「CPUと対戦したときに何をされたのか分からない」という
	// フィードバックへの対応。ChoiceModalLayerとは独立に、いつでもボタンで開閉できる
	// (PendingChoiceの状態に連動しない)。ActionLogToggleButtonで開閉、中身は
	// ACGGameState::ActionLogをRefreshUI()のたびに反映する。
	UPROPERTY()
	TObjectPtr<UBorder> ActionLogModalLayer;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ActionLogListBox;

	UPROPERTY()
	TObjectPtr<UButton> ActionLogToggleButton;

	// 候補が多いときに横スクロールではなく2列以上に折り返して表示するため、
	// 横一列固定のUHorizontalBoxではなくUWrapBoxを使う(マーケットの2列表示と同じ
	// 考え方。docs/architecture.md「カードUIの設計」)。
	UPROPERTY()
	TObjectPtr<UWrapBox> GraveyardViewerBox;

	// KeepOrBury選択待ち中(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)だけ表示する、
	// 山札の一番上のカード+「上に残す」「下に送る」ボタン。
	// ScryBoxを表示/非表示し、ScryCardContainerへ見せているカードを詰める。
	UPROPERTY()
	TObjectPtr<UHorizontalBox> ScryBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> ScryCardContainer;

	// BuyDestination選択待ち中(購入したカードを手札に入れるか山札の一番下に送るか)
	// だけ表示する、購入したカード+「手札へ」「山札の下へ」ボタン。ScryBoxと同じ構造。
	UPROPERTY()
	TObjectPtr<UHorizontalBox> BuyDestinationBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> BuyDestinationCardContainer;

	// HPはMTG Arena風の円形バッジ(ライフオーブ)の中に大きな数字で表示する
	// (「MTGアリーナを参考にして画面を作ってください」というフィードバックへの
	// 対応、docs/architecture.md「カードUIの設計」)。敵は上部・小さめ、自分は
	// 最下部・大きめにして主従をつける。円自体はテクスチャ資産を使わず、
	// FSlateBrush(DrawAs=RoundedBox、RoundingType=HalfHeightRadius)で作る
	// (MakeCircleBadge参照)。
	UPROPERTY()
	TObjectPtr<UTextBlock> EnemyOrbText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SelfOrbText;

	// マナ・山札枚数・墓地枚数・コイン・発動中パッシブ・購入割引など、HP以外の
	// 付随情報。ライフオーブの隣に小さめの文字で表示する。
	UPROPERTY()
	TObjectPtr<UTextBlock> EnemySecondaryInfoText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SelfSecondaryInfoText;

	// 相手の手札は中身を見せず、枚数分の裏向きカードとして表示する
	// (Shadowverse/MTG Arenaのような見せ方)。
	UPROPERTY()
	TObjectPtr<UHorizontalBox> EnemyHandBox;

	// 縦長のサイドレールに2列で折り返し表示する(横一列/1列縦積みは見づらいという
	// フィードバックのため)。
	UPROPERTY()
	TObjectPtr<UWrapBox> MarketBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> EnemyBoardBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> SelfBoardBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> HandBox;

	UPROPERTY()
	TObjectPtr<UButton> EndTurnButton;

	// 勝敗確定後のみ表示する、ロビーへ戻るボタン(docs/architecture.md「レベルと画面遷移」)。
	UPROPERTY()
	TObjectPtr<UButton> BackToLobbyButton;

	// EnemyOrFaceTarget選択待ち中だけ表示する「顔面を狙う」ボタン(docs/card-effect-
	// player-choice-plan.md「③敵ユニット/顔面を選ぶ」)。敵に守護がいるときは
	// 選べないため非表示にする。
	UPROPERTY()
	TObjectPtr<UButton> TargetFaceButton;

	// 選択待ちがキャンセル可能(FCGPendingChoice::bCancellable)なときだけ有効化する、
	// 画面全体を覆う透明なクリックキャッチャー。「取りやめるボタンでなく、画面の
	// 関係ないところをタップしたら選択をやめるようにしてほしい」というフィード
	// バックへの対応。ResultClickCatcherと同じ考え方だが、Root(盤面・手札等の
	// 実際の操作対象)より手前ではなく奥に置くことで、実際に対象を選べる場所への
	// クリックはそちらが先に消費し、それ以外の「関係ない場所」への クリックだけが
	// ここまで素通りしてキャンセルになる(EnsureWidgetTreeBuilt参照)。
	UPROPERTY()
	TObjectPtr<UButton> ChoiceCancelClickCatcher;

	// KeepOrBury選択待ち中に表示する2択ボタン。
	UPROPERTY()
	TObjectPtr<UButton> KeepOnTopButton;

	UPROPERTY()
	TObjectPtr<UButton> SendToBottomButton;

	// BuyDestination選択待ち中に表示する2択ボタン。手札が上限(10枚)のときは
	// ToHandButtonを隠す(選べても無意味なため。RefreshUI参照)。
	UPROPERTY()
	TObjectPtr<UButton> ToHandButton;

	UPROPERTY()
	TObjectPtr<UButton> ToDeckBottomButton;

	UFUNCTION()
	void HandleHandSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleGraveyardSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleMarketSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleBoardSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleEnemyBoardSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleTargetFaceClicked();

	UFUNCTION()
	void HandleCancelChoiceClicked();

	UFUNCTION()
	void HandleKeepOnTopClicked();

	UFUNCTION()
	void HandleSendToBottomClicked();

	UFUNCTION()
	void HandleToHandClicked();

	UFUNCTION()
	void HandleToDeckBottomClicked();

	UFUNCTION()
	void HandleEndTurnClicked();

	UFUNCTION()
	void HandleBackToLobbyClicked();

	UFUNCTION()
	void HandleActionLogToggleClicked();

	UFUNCTION()
	void HandleActionLogCloseClicked();

	// ActionLogModalLayerの中身をACGGameState::ActionLogから作り直す
	// (新しい行動が一番上に来るよう逆順に並べる)。
	void PopulateActionLog();

	ACGGameMode* GetCGGameMode() const;

	// アクション要求(カードプレイ/購入/攻撃/ターン終了/選択の解決)の送信先。
	// オンライン/オフライン問わずこの経由でServer RPCを呼ぶ(docs/online-play-
	// design.md「HUD側の変更点」)。取得できない場合はnullptr。
	class ACGPlayerController* GetCGPlayerController() const;

	// このHUDを所有するクライアントの実際のSideIndex(旧・固定値だった
	// HumanSideIndexの置き換え)。オフライン(vs AI)では常に0(人間はSide0)。
	// オンラインではホスト=0、ゲスト=1になる(docs/online-play-plan.md
	// 「フェーズ4」参照)。PlayerState未取得時は0にフォールバックする。
	int32 GetMySideIndex() const;

private:
	// RefreshUI時点のスロット内容(クリック時にSlotIndexからCardIdへ逆引きするため)。
	TArray<FName> LastHandCardIds;
	TArray<FCGMarketSlot> LastMarketSlots;
	TArray<FName> LastGraveyardCandidateCardIds;

	// 直近でPlayAttackAnimation()を再生した攻撃のAttackSequenceNumber。GameStateの
	// 値と一致しなくなったら新しい攻撃が起きたということなので、演出を1回再生する。
	int32 LastAnimatedAttackSequence = 0;

	// 直近でPlayCardPlayAnimation()を再生したCardPlaySequenceNumber。仕組みは
	// LastAnimatedAttackSequenceと同じ。
	int32 LastAnimatedCardPlaySequence = 0;

	// PlayCardPlayAnimation()がまだ対象を見つけられず再試行した回数。仕組みは
	// AttackAnimationRetryCountと同じ。
	int32 CardPlayAnimationRetryCount = 0;

	// 直近のRefreshUI()時点で見たCurrentHP(自分/相手)。次のRefreshUI()で差分が
	// あれば、原因(戦闘/スプル/常在効果等)を問わずライフオーブの位置にダメージ
	// (赤・"-N")または回復(緑・"+N")のポップアップを表示する
	// (「ダメージなどの表記を変更してほしい」というフィードバックへの対応。
	// 戦闘によるユニットへのダメージは引き続きPlayAttackAnimation()側で個別に
	// 表示するため、ここでは顔面/リーダーのHPだけを見る)。-1は「まだ見ていない」
	// 判定用の番兵で、接続直後の初回RefreshUI()では差分があってもポップアップを
	// 出さない(初期値0からの「変化」を毎回演出扱いしないため)。
	int32 LastSeenSelfHP = -1;
	int32 LastSeenEnemyHP = -1;

	// PlayAttackAnimation()がまだ有効な座標を取得できず再試行した回数。何らかの理由で
	// 座標が永久に取れないケースに備え、一定回数で諦める(NativeTickのコメント参照)。
	int32 AttackAnimationRetryCount = 0;

	// 直近で見た`ACGGameState::StateVersion`。オンライン対戦では、この値が
	// 変わったらサーバー側の状態が変化した(≒再描画が必要)とみなしてRefreshUI()
	// する(docs/online-play-design.md参照。オフラインでは各操作の直後に明示的に
	// RefreshUI()しているため、ここでの検知は主にオンライン用)。
	int32 LastSeenStateVersion = -1;

	// StateVersionの変化を検知するたびにリセットされる、「追いのRefreshUI()」
	// までの残りフレーム数。GameStateとPlayerStateのレプリケーション到着
	// タイミングが揃わない問題への対策(NativeTickのコメント参照)。
	// 0になった時点で1回だけRefreshUI()し、以後は次の変化まで何もしない
	// (行動のたびに画面が何度も作り直される問題を避けるため、毎フレーム
	// 呼び続ける方式は取らない)。
	int32 ResyncTicksRemaining = 0;

	// 勝敗確定演出(「勝利、敗北したときにもっと派手に演出を入れてほしい」という
	// フィードバックへの対応)。ResultOverlayLayer(全画面を暗く覆う背景)+
	// ResultBannerText(「勝利!」/「敗北...」の大きな文字)をNativeTick()で
	// 手動アニメーションさせる(ShowFloatingNumber()と同じ、経過時間を貯めて
	// 毎フレーム補間する方式。docs/architecture.md「カードUIの設計」)。
	UPROPERTY()
	TObjectPtr<UBorder> ResultOverlayLayer;

	UPROPERTY()
	TObjectPtr<UTextBlock> ResultBannerText;

	// ResultOverlayLayerを包む透明なクリック受け皿。「演出が出た後操作できなくなる」
	// というフィードバックへの対応。BackToLobbyButton(小さなボタン)を探させるのではなく、
	// 全画面を覆うこのボタンをクリックするだけでロビーに戻れるようにする。見た目は
	// 完全に透明(ApplyStyledButtonLookにアルファ0を渡す)にし、中身のResultOverlayLayer/
	// ResultBannerTextの表示はそのまま見せる。
	UPROPERTY()
	TObjectPtr<UButton> ResultClickCatcher;

	// 直近に演出を再生した勝敗(-1=未確定/未再生)。WinnerPlayerIndexがこれと
	// 変わった瞬間に1回だけ再生する(AttackSequenceNumberの検知と同じ考え方)。
	int32 LastAnimatedWinner = -2;

	bool bResultAnimationActive = false;
	float ResultAnimationElapsed = 0.f;

	void PlayResultAnimation(bool bIsVictory);
	void TickResultAnimation(float DeltaTime);
};
