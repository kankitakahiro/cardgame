#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/TimerHandle.h"
#include "CGTypes.h"
#include "CGGameMode.generated.h"

class ACGGameState;
class ACGPlayerState;
class UCGAIOpponent;
class APlayerController;
class AController;

// 対戦進行の中核(docs/architecture.md「対戦ロジックのクラス責務」)。
UCLASS()
class ACGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACGGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// 先攻後攻ランダム決定、両プレイヤーの初期デッキ構築・シャッフル・初期手札5枚配布、
	// マーケット6枠公開までを行う(docs/game-rules-minimum.md「初期設定」)。AI(Side1)の
	// デッキは色ごとの基本デッキ(`UCGCardDatabase::GetBasicColorDeckCardIds`)から
	// 毎回ランダムに1色選ぶ。オフライン(vs AI、スタンドアロン)専用。オンライン対戦は
	// PostLogin()/InitializeOnlineMatch()を使う(docs/online-play-design.md参照)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void InitializeMatch();

	// オンライン対戦(リッスンサーバー)用。2人分の接続(PostLogin)とデッキ提出
	// (SubmitDeckForSide)が揃った時点で呼ばれ、InitializeMatch()のうち
	// 「デッキ初期化〜StartTurn」に相当する処理を行う(docs/online-play-design.md
	// 「`ACGGameMode`の変更」参照)。
	void InitializeOnlineMatch();

	// オンライン対戦: 接続してきたクライアントが自分のデッキを送ってきたときに
	// ACGPlayerController::ServerSubmitDeckから呼ばれる。25枚に満たない不正な
	// デッキはスターターデッキにフォールバックする。両陣営分揃ったら
	// InitializeOnlineMatch()を呼ぶ。
	void SubmitDeckForSide(int32 SideIndex, const TArray<FName>& DeckCardIds);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void StartTurn();

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool RequestPlayCard(int32 SideIndex, FName CardId, int32 TargetUnitIndex = -1);

	// MarketSlotIndexで指定する(CardIdではない)。次期ルールでは両者の山札が同じ
	// カードを含み得るため、複数の枠が同じCardIdを表示することがあり、CardId単体
	// では枠を一意に特定できない(docs/next-ruleset-design.md「マーケット」)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool RequestBuyCard(int32 SideIndex, int32 MarketSlotIndex);

	// 攻撃を仕掛ける(攻撃対象は指定しない)。相手に守護がいれば必ずそちらへ強制的に
	// 命中し、いなければ対象(敵ユニットまたは顔面)をプレイヤー/AIが選ぶ選択待ちに
	// 入る(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool RequestAttack(int32 AttackerSideIndex, int32 AttackerUnitIndex);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RequestEndTurn(int32 SideIndex);

	// 「対象を選ぶ操作を取りやめて戻れるようにしてほしい。カードを完全にプレイした/
	// 攻撃を完了した後は戻れない」というフィードバックへの対応。現在の選択待ちが
	// キャンセル可能(FCGPendingChoice::bCancellable)なときだけ、その選択を取りやめて
	// 手前の状態(攻撃対象選択なら何もしていない状態、カードプレイ由来の対象選択なら
	// プレイ直前の両陣営の状態)へ戻す。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool RequestCancelChoice(int32 SideIndex);

	// 選択式カード効果/攻撃対象選択(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	// 人間側の選択待ちを開始する。AI側は各効果ハンドラがこれを呼ばず、
	// AutoResolveChoiceIfAI()経由でその場即決する。
	void BeginChoice(const FCGPendingChoice& Choice);

	// 選択待ち(手札/墓地/マーケットからカードを1枚選ぶ場合)を、選んだCardIdで解決する。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool ResolvePendingChoiceWithCard(int32 SideIndex, FName ChosenCardId);

	// 選択待ち(敵ユニット/顔面を選ぶ場合)を、対象ユニットIndex(-1なら顔面)で解決する。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool ResolvePendingChoiceWithTarget(int32 SideIndex, int32 ChosenUnitIndex);

	// 選択待ち(山札の一番上を残すか送るか)を解決する。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool ResolvePendingChoiceKeepOrBury(int32 SideIndex, bool bKeepOnTop);

	// 選択待ち(購入したカードを手札に入れるか、山札の一番下に送るか)を解決する。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool ResolvePendingChoiceBuyDestination(int32 SideIndex, bool bToHand);

	// 選択待ち(EnemyUnitTarget: 敵ユニットを1体選ぶ、次期ルール)を解決する。
	// Choice.EffectIdで分岐し、断罪(SealSpell/SealOnPlay)なら対象ユニットを
	// 場から完全に取り除いて(墓地へは送らない)青パッシブ有効時は1ドローする、
	// 対象指定デバフ(OnPlayDebuffTarget、B02/B04)ならAtk/Hpへ
	// Choice.PendingBuffAmount(負の値)を適用して死亡処理まで行う
	// (docs/game-rules-minimum.md「青」)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool ResolvePendingChoiceSealTarget(int32 SideIndex, int32 ChosenUnitIndex);

	// 選択待ち(対象指定の味方強化: 自分の場のユニットを1体選ぶ、次期ルール)を
	// 解決する。Choice.PendingBuffAmount分だけAtk/Hpを増やす。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool ResolvePendingChoiceAllyTarget(int32 SideIndex, int32 ChosenUnitIndex);

	// 選択待ち(補充し直すマーケットの枠を1つ選ぶ、橙O04市場の噂話)を解決する。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool ResolvePendingChoiceMarketSlotTarget(int32 SideIndex, int32 ChosenSlotIndex);

	UFUNCTION(BlueprintPure, Category = "CardGame")
	ACGGameState* GetCGGameState() const;

	// バランス検証用: 色ごとの基本デッキ(`UCGCardDatabase::GetBasicColorDeckCardIds`)
	// 同士でAI対AIの対戦をNumMatches回繰り返し、色ごとの勝率・対面ごとの勝敗を
	// `LogCardGame`へ出力する(「緑が強すぎる」等のフィードバックを実際の対戦
	// データで検証するため)。起動時コマンドライン引数`-SimulateMatches=N`で
	// 呼ばれ、通常のHUD/InitializeMatchは行わない(headless)。
	void RunSelfPlaySimulation(int32 NumMatches);

	// バランス検証用: 色ごとの基本デッキを起点に、山登り法(1枚だけ別のカードに
	// 入れ替えて評価し、勝率が上がれば採用)で色ごとに勝率の高いデッキ構成を
	// 探索する。評価は毎回、他4色の「現時点のベストデッキ」と総当たりで
	// MatchesPerEvaluation回対戦させた勝率で行うため、色同士のデッキが
	// ラウンドを追うごとに互いに適応していく。起動時コマンドライン引数
	// `-OptimizeDecks=N`(色ごとの試行回数=IterationsPerColor)で呼ばれ、
	// `-OptimizeDeckMatches=N`(評価1回あたりの対戦数、既定200)、
	// `-OptimizeDeckRounds=N`(色を何周探索するか、既定3)も指定できる。
	// 結果(色ごとの最終デッキ構成・ベースラインとの勝率差)をLogCardGameへ
	// 出力して終わる(headless、RunSelfPlaySimulationと同じ位置づけ)。
	void RunDeckOptimization(int32 IterationsPerColor, int32 MatchesPerEvaluation, int32 Rounds);

	// 診断専用(一時的): 先攻/後攻の偏りの主因がマーケットの早い者勝ち購入か
	// 戦闘の先制かを切り分けるための、購入フェーズ無効化フラグ。コマンドライン
	// `-SimDisableBuy`でのみtrueになる。`UCGAIOpponent::RunTurn`が参照する。
	bool bDiagDisableBuyPhase = false;

	// UI更新のフック。WidgetTreeはPythonから編集できなかった(docs/automation-notes.md)ため、
	// UIバインドはBlueprintエディタ上で手動で行う想定。
	UFUNCTION(BlueprintImplementableEvent, Category = "CardGame")
	void OnCardGameStateChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "CardGame")
	void OnMatchEnded(int32 InWinnerPlayerIndex);

protected:
	// 対戦1回分(PlayerState再生成・両陣営のデッキ配布・マーケット公開・
	// StartTurn()呼び出し)を行う共通処理。RunSelfPlaySimulation()と
	// RunDeckOptimization()の両方から、両陣営のデッキを直接指定して呼ばれる。
	// 結果はCGState->WinnerPlayerIndex/TurnCount/Sides[]から読む。
	// 戻り値は先攻側のSideIndex(0か1)。呼び出し側が先攻/後攻の勝敗集計に使う。
	int32 PlaySimulatedMatch(const TArray<FName>& DeckSide0, const TArray<FName>& DeckSide1);

	void CheckWinLose();
	ACGPlayerState* GetOpponent(int32 SideIndex) const;

	// ダメージ・デバフ等を適用した後、両陣営分の死亡処理(墓地送り+死亡時効果+
	// 死亡時ドロー)をまとめて行う。死亡時効果の中には敵側のランダムなUnitを
	// 巻き込んで倒すものがある(例: R01黒鉄拾いの悪童「死亡時、敵のランダムな
	// Unit1体に1ダメージ」)。この巻き添え死は、死んだユニットの持ち主とは逆の
	// 陣営で起きるため、どちらか片方の陣営だけ死亡処理を呼ぶと、その巻き添え死が
	// 一切クリーンアップされず「HPが0以下なのに場に残り続ける」不具合になる
	// (「ランダムでダメージが飛んでタフネスが0になったのにカードが残っている」
	// というフィードバックへの対応)。そのため、どちらの陣営にもHp<=0のUnitが
	// 残らなくなるまで両陣営を交互に処理する。
	void ResolveDeathsForBothSides(ACGPlayerState* SideA, ACGPlayerState* SideB, ACGGameState* CGState);

	// 全アクション処理の最後に呼ぶ共通フック。UI再描画イベント(OnCardGameStateChanged)
	// の前に、公開情報用のHandCount/DeckCountを最新化する(docs/online-play-design.md
	// 「`ACGPlayerState`のレプリケーション」)。既存の全`OnCardGameStateChanged()`
	// 呼び出し箇所をこちらに置き換えている。
	void NotifyStateChanged();

	// 選択待ちを開始した側がAIだった場合、その場でUCGAIOpponentの判断関数を呼んで
	// 即座に選択を解決する(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	// BeginChoice()を呼びうる全ての箇所(RequestPlayCard/RequestEndTurn/
	// RequestAttack等)の最後で呼ぶ。
	void AutoResolveChoiceIfAI();

	// RequestEndTurn()から、選択待ちが無い場合はその場で、ある場合は選択解決後に
	// 呼ばれる、実際のターン交代処理。
	void CompleteEndTurn(int32 SideIndex);

	// 攻撃の実処理(ダメージ・反撃・死亡処理)。対象(TargetUnitIndex、-1なら顔面)は
	// 呼び出し側(RequestAttack: 守護がいる場合/ResolvePendingChoiceWithTarget:
	// 選択解決後)で確定させてから呼ぶ。
	bool ExecuteAttack(int32 AttackerSideIndex, int32 AttackerUnitIndex, int32 TargetUnitIndex);

	// Side 1は常にAI制御(一人二役の解消)。意思決定ロジック自体はUCGAIOpponentへ
	// 切り出してあり(docs/architecture.md「対戦ロジックのクラス責務」)、GameModeはそれを
	// 手番がSide 1になったStartTurnの終わりで呼ぶだけにしている。
	UPROPERTY()
	TObjectPtr<UCGAIOpponent> AIOpponent;

	// GameMode::BeginPlay時点ではローカルプレイヤーのビューポートがまだ描画準備完了
	// していないことがあり、その場でAddToViewport()しても画面に反映されないことがある。
	// そのため1フレーム遅延させてHUDを生成する。
	void SetupHUD();

	FTimerHandle HUDSetupTimerHandle;

	// RunSelfPlaySimulation実行中のみtrue。StartTurn()のAI自動進行判定
	// (通常はSide1のみ)とAutoResolveChoiceIfAI()の判定を、両陣営とも
	// AI扱いに切り替える(Side0側の選択待ちもその場で即決させる)。
	bool bSimulateBothSidesAsAI = false;

	// オンライン対戦中はtrue。Side1(AISideIndex)を無条件にAI扱いする既存の
	// StartTurn()/AutoResolveChoiceIfAI()の判定を無効化するために使う
	// (オンラインではSide1も人間のため。docs/online-play-design.md参照)。
	bool bOnlineMatch = false;

	// オンライン対戦: PostLogin()で接続した順に0, 1と割り振るためのカウンタ。
	int32 NumOnlineConnections = 0;

	// RequestCancelChoice用のスナップショット(カードプレイ直前の両陣営の状態)。
	// RequestPlayCardがPendingChoiceをbCancellable=trueにするときだけ書き込み、
	// 選択が解決される(通常解決/キャンセルのどちらでも)たびに読み捨てる想定
	// (FCGPlayerStateSnapshotのコメント参照)。攻撃対象選択のキャンセルは状態を
	// 何も変えていないため、これを使わない。
	FCGPlayerStateSnapshot CancelSnapshotSelf;
	FCGPlayerStateSnapshot CancelSnapshotOpponent;

	// オンライン対戦: SubmitDeckForSideで受け取った、まだ試合開始に使っていない
	// デッキ(SideIndex→デッキ)。両陣営分(2件)揃うとInitializeOnlineMatch()を呼ぶ。
	TMap<int32, TArray<FName>> PendingOnlineDecks;
};
