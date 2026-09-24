#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/TimerHandle.h"
#include "CGLobbyGameMode.generated.h"

// ロビー画面用の軽量GameMode。ロビーは対戦ロジックを持たずUI表示のみのため、
// バトル進行を担うACGGameModeとは別クラスにしている
// (docs/architecture.md「レベルと画面遷移」)。
UCLASS()
class ACGLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACGLobbyGameMode();

	virtual void BeginPlay() override;

protected:
	// GameMode::BeginPlay時点ではローカルプレイヤーのビューポートがまだ描画準備完了
	// していないことがあり、その場でAddToViewport()しても画面に反映されないことが
	// ある(ACGGameMode::SetupHUDと同じ既知の落とし穴)ため1フレーム遅延させる。
	void SetupHUD();

	FTimerHandle HUDSetupTimerHandle;
};
