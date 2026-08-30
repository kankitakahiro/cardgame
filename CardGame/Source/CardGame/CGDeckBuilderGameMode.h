#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/TimerHandle.h"
#include "CGDeckBuilderGameMode.generated.h"

// デッキ構築画面用の軽量GameMode。ACGLobbyGameModeと同じ構成で、
// 表示するウィジェットだけが異なる(docs/architecture.md「レベルと画面遷移」)。
UCLASS()
class ACGDeckBuilderGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACGDeckBuilderGameMode();

	virtual void BeginPlay() override;

protected:
	// GameMode::BeginPlay時点ではローカルプレイヤーのビューポートがまだ描画準備完了
	// していないことがあり、その場でAddToViewport()しても画面に反映されないことが
	// ある(ACGGameMode::SetupHUDと同じ既知の落とし穴)ため1フレーム遅延させる。
	void SetupHUD();

	FTimerHandle HUDSetupTimerHandle;
};
