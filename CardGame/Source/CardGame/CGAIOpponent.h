#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CGAIOpponent.generated.h"

class ACGGameMode;

// 対戦相手側(AI制御)の意思決定ロジック。以前は進行管理を担う ACGGameMode に
// RunAITurn() として同居していたが、AIをもっと賢くする/難易度を分けるといった
// 将来の拡張時にGameMode本体を肥大化させないよう、専用クラスへ切り出している
// (docs/refactor-plan-architecture.md Step 4)。
// GameModeの公開APIのみを呼んで進行させるため、進行ルール自体はGameMode側に残る。
UCLASS(BlueprintType)
class UCGAIOpponent : public UObject
{
	GENERATED_BODY()

public:
	// 手番がSideIndexに回ってきた直後に呼ぶ。購入→プレイ→攻撃→EndTurnまでを
	// 1回の呼び出し内で同期的に完結させる(HUDのRefreshUI()が呼ばれる頃には
	// 手番は必ず人間側に戻っている)。
	UFUNCTION(BlueprintCallable, Category = "CardGame|AI")
	void RunTurn(ACGGameMode* GameMode, int32 SideIndex);
};
