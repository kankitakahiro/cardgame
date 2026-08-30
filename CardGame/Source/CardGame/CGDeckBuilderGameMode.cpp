#include "CGDeckBuilderGameMode.h"
#include "CGDeckBuilderHUD.h"
#include "CardGame.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

ACGDeckBuilderGameMode::ACGDeckBuilderGameMode()
{
	// 3Dの操作対象を持たないUI主体の画面のため、デフォルトPawnは不要。
	DefaultPawnClass = nullptr;
}

void ACGDeckBuilderGameMode::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimerForNextTick(this, &ACGDeckBuilderGameMode::SetupHUD);
}

void ACGDeckBuilderGameMode::SetupHUD()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		UE_LOG(LogCardGame, Error, TEXT("ACGDeckBuilderGameMode::SetupHUD: No PlayerController(0) found"));
		return;
	}

	UCGDeckBuilderHUD* HUD = CreateWidget<UCGDeckBuilderHUD>(PC, UCGDeckBuilderHUD::StaticClass());
	if (!HUD)
	{
		UE_LOG(LogCardGame, Error, TEXT("ACGDeckBuilderGameMode::SetupHUD: CreateWidget<UCGDeckBuilderHUD> failed"));
		return;
	}

	HUD->AddToViewport();
	PC->bShowMouseCursor = true;
	PC->SetInputMode(FInputModeUIOnly());
}
