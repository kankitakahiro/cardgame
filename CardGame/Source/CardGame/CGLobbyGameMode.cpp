#include "CGLobbyGameMode.h"
#include "CGLobbyHUD.h"
#include "CardGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ACGLobbyGameMode::ACGLobbyGameMode()
{
	// 3Dの操作対象を持たないUI主体の画面のため、デフォルトPawnは不要。
	DefaultPawnClass = nullptr;
}

void ACGLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimerForNextTick(this, &ACGLobbyGameMode::SetupHUD);
}

void ACGLobbyGameMode::SetupHUD()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		UE_LOG(LogCardGame, Error, TEXT("ACGLobbyGameMode::SetupHUD: No PlayerController(0) found"));
		return;
	}

	UCGLobbyHUD* HUD = CreateWidget<UCGLobbyHUD>(PC, UCGLobbyHUD::StaticClass());
	if (!HUD)
	{
		UE_LOG(LogCardGame, Error, TEXT("ACGLobbyGameMode::SetupHUD: CreateWidget<UCGLobbyHUD> failed"));
		return;
	}

	HUD->AddToViewport();
	PC->bShowMouseCursor = true;
	PC->SetInputMode(FInputModeUIOnly());
}
