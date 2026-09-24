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
	// Choice.EffectIdで分岐し、封印(SealSpell/SealOnPlay)なら対象ユニットを
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
	void CheckWinLose();
	ACGPlayerState* GetOpponent(int32 SideIndex) const;

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

	// オンライン対戦: SubmitDeckForSideで受け取った、まだ試合開始に使っていない
	// デッキ(SideIndex→デッキ)。両陣営分(2件)揃うとInitializeOnlineMatch()を呼ぶ。
	TMap<int32, TArray<FName>> PendingOnlineDecks;
};
