#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/TimerHandle.h"
#include "CGGameMode.generated.h"

class ACGGameState;
class ACGPlayerState;

// 対戦進行の中核。docs/blueprint-architecture.md の BP_CG_GameMode に相当。
// docs/ue58-implementation-checklist.md の「実装順」1〜9を満たす最小ゲームループを実装する。
UCLASS()
class ACGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACGGameMode();

	virtual void BeginPlay() override;

	// 先攻後攻ランダム決定、両プレイヤーの初期デッキ構築・シャッフル・初期手札5枚配布、
	// マーケット5枚公開までを行う(docs/game-rules-minimum.md「初期設定」)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void InitializeMatch();

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void StartTurn();

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool RequestPlayCard(int32 SideIndex, FName CardId, int32 TargetUnitIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool RequestBuyCard(int32 SideIndex, FName CardId);

	// TargetUnitIndex = -1 は相手リーダーへの直接攻撃。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool RequestAttack(int32 AttackerSideIndex, int32 AttackerUnitIndex, int32 TargetUnitIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RequestEndTurn(int32 SideIndex);

	UFUNCTION(BlueprintPure, Category = "CardGame")
	ACGGameState* GetCGGameState() const;

	// UI更新のフック。WidgetTreeはPythonから編集できなかった(docs/automation-notes.md)ため、
	// UIバインドはBlueprintエディタ上で手動で行う想定。
	UFUNCTION(BlueprintImplementableEvent, Category = "CardGame")
	void OnCardGameStateChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "CardGame")
	void OnMatchEnded(int32 InWinnerPlayerIndex);

protected:
	void CheckWinLose();
	ACGPlayerState* GetOpponent(int32 SideIndex) const;

	// Side 1は常にAI制御(一人二役の解消)。手番がSide 1になったStartTurnの終わりで
	// 呼ばれ、購入→プレイ→攻撃→EndTurnまでを1回の呼び出し内で同期的に完結させる。
	// これにより、HUDのRefreshUI()が呼ばれる頃には手番は必ず人間(Side 0)に戻っている。
	void RunAITurn(int32 SideIndex);

	// GameMode::BeginPlay時点ではローカルプレイヤーのビューポートがまだ描画準備完了
	// していないことがあり、その場でAddToViewport()しても画面に反映されないことがある。
	// そのため1フレーム遅延させてHUDを生成する。
	void SetupHUD();

	FTimerHandle HUDSetupTimerHandle;
};
