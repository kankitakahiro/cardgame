#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CGTypes.h"
#include "CGGameState.generated.h"

class ACGPlayerState;

// 対戦全体の公開状態(docs/architecture.md「対戦ロジックのクラス責務」)。
// 全フィールドが非公開情報を含まない公開情報のため、オンライン対戦では
// 全クライアントへ複製する(docs/online-play-design.md「`ACGGameState`の
// レプリケーション」)。
UCLASS()
class ACGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 CurrentTurnPlayerIndex = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 TurnCount = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	ECGPhase CurrentPhase = ECGPhase::Draw;

	// -1 = 未決着。docs/game-rules-minimum.mdの勝敗条件を満たすと0または1になる。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 WinnerPlayerIndex = -1;

	// オンライン対戦中、相手の通信切断によって試合が中断された場合にtrueになる
	// (docs/online-play-plan.md「フェーズ4」参照)。このとき`WinnerPlayerIndex`は
	// 切断していない側の勝ちとして扱うが、通常の決着とは表示を分けるためのフラグ。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bOpponentDisconnected = false;

	// 共通マーケット6枠(次期ルール、docs/next-ruleset-design.md「マーケット」)。
	// 各枠は出どころ(元々並べた側)を保持する(FCGMarketSlot参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	TArray<FCGMarketSlot> MarketSlots;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	TArray<TObjectPtr<ACGPlayerState>> Sides;

	// カード効果/攻撃でプレイヤーの選択待ちになっているときの状態。ChoiceTypeが
	// Noneなら選択待ちなし(docs/architecture.md「選択待ち(PendingChoice)の仕組み」参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	FCGPendingChoice PendingChoice;

	// 攻撃演出用。ACGGameMode::ExecuteAttack()のたびに内容が更新され、
	// AttackSequenceNumberがインクリメントされる。HUD側はこの番号の変化を見て
	// 新しい攻撃を検知し、演出(突進/フラッシュ/ダメージ数値)を1回だけ再生する
	// (docs/architecture.md「カードUIの設計」)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	FCGLastAttackResult LastAttackResult;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 AttackSequenceNumber = 0;

	// カードプレイ演出用。ACGGameMode::RequestPlayCard()のたびに内容が更新され、
	// CardPlaySequenceNumberがインクリメントされる。仕組みはAttackSequenceNumberと同じ
	// (FCGLastCardPlayResultのコメント参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	FCGLastCardPlayResult LastCardPlayResult;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 CardPlaySequenceNumber = 0;

	// `ACGGameMode::NotifyStateChanged()`が呼ばれるたびに+1される。オンライン
	// 対戦では、クライアント側はこの値の変化を見て「サーバー側で何か状態が
	// 変わった(≒再描画が必要)」ことを検知する(`UCGGameHUD::NativeTick`が
	// 既存の攻撃演出検知と同じポーリングの仕組みで見る。docs/online-play-design.md
	// 「PendingChoiceの非同期化について」の追加メモ)。オフラインでは各操作の
	// 直後に明示的にRefreshUI()しているため実質的に使わない。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 StateVersion = 0;

	// 行動ログ(次期ルール)。「CPUと対戦したときに何をされたのか分からない」
	// というフィードバックへの対応。プレイ/購入/攻撃/フィニッシャー登場のたびに
	// `AppendActionLog()`で追記され、直近`MaxActionLogEntries`件だけ保持する。
	// HUD側はいつでもボタンで開いて見返せる(docs/game-rules-minimum.md
	// 「行動ログ」参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	TArray<FCGActionLogEntry> ActionLog;

	// ActionLogへ1件追記する。上限(30件)を超えた古い分は切り捨てる。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void AppendActionLog(int32 SideIndex, const FString& Text);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 試合開始時、両者の山札トップから3枚ずつ(計6枚)を公開してマーケットを作る
	// (docs/next-ruleset-design.md「マーケット」)。Sidesが設定済みの状態で呼ぶこと。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void InitializeMarket();

	// 指定枠が購入された後に呼ぶ。その枠の出どころの山札トップで補充する
	// (出どころの山札が尽きていれば、通常のドローと同じくリシャッフル+ライフ減少。
	// それでも尽きていればその枠は空(NAME_None)のままになる)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RefillMarketSlot(int32 SlotIndex);

	// 購入を介さずに、指定枠を即座に補充し直す(橙、O04市場の噂話/O11市場の
	// 目付役)。それまで枠にあったカードは無くならず、出どころの山札の一番下へ
	// 戻してからRefillMarketSlot()と同じ処理で新しい1枚を引く。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RerollMarketSlot(int32 SlotIndex);

	// 表示/選択用に、マーケットの中身をCardIdだけの配列として取り出す
	// (空枠のNAME_Noneも含む。呼び出し側でスキップすること)。
	UFUNCTION(BlueprintPure, Category = "CardGame")
	TArray<FName> GetMarketCardIds() const;
};
