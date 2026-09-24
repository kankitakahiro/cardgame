#include "CGPlayerController.h"
#include "CGGameMode.h"
#include "CGGameState.h"
#include "CGPlayerState.h"
#include "CGGameHUD.h"
#include "CGGameInstance.h"
#include "CardGame.h"
#include "Engine/World.h"
#include "TimerManager.h"

void ACGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// オンライン対戦時のみ、自分のデッキをサーバーへ送る(オフラインは
	// ACGGameMode::InitializeMatch()が直接GameInstanceを読むため不要)。
	if (IsLocalController() && GetNetMode() != NM_Standalone)
	{
		if (UCGGameInstance* GameInstance = GetGameInstance<UCGGameInstance>())
		{
			ServerSubmitDeck(GameInstance->PlayerDeckCardIds);
		}
	}

	SetupHUDIfOnline();
}

void ACGPlayerController::SetupHUDIfOnline()
{
	// オフライン(スタンドアロン)ではACGGameMode::BeginPlay()側が引き続き
	// HUDを作る(docs/online-play-design.md「フェーズ1」参照。二重生成を避ける)。
	if (!IsLocalController() || GetNetMode() == NM_Standalone)
	{
		return;
	}

	// BeginPlay時点だとローカルプレイヤーのビューポートがまだ準備できておらず
	// AddToViewport()が画面に反映されないことがあるため、1フレーム遅延させる
	// (元ACGGameMode::SetupHUDと同じ理由)。
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UCGGameHUD* HUD = CreateWidget<UCGGameHUD>(this, UCGGameHUD::StaticClass());
		if (!HUD)
		{
			UE_LOG(LogCardGame, Error, TEXT("ACGPlayerController::SetupHUDIfOnline: CreateWidget<UCGGameHUD> failed"));
			return;
		}

		HUD->AddToViewport();
		bShowMouseCursor = true;
		SetInputMode(FInputModeUIOnly());
		UE_LOG(LogCardGame, Log, TEXT("ACGPlayerController::SetupHUDIfOnline: HUD created for %s"), *GetName());
	}));
}

void ACGPlayerController::ServerSubmitDeck_Implementation(const TArray<FName>& DeckCardIds)
{
	if (ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>())
	{
		if (ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr)
		{
			GameMode->SubmitDeckForSide(MyState->SideIndex, DeckCardIds);
		}
	}
}

void ACGPlayerController::ServerRequestPlayCard_Implementation(FName CardId, int32 TargetUnitIndex)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->RequestPlayCard(MyState->SideIndex, CardId, TargetUnitIndex);
	}
}
bool ACGPlayerController::ServerRequestPlayCard_Validate(FName CardId, int32 TargetUnitIndex)
{
	return true;
}

void ACGPlayerController::ServerRequestBuyCard_Implementation(int32 MarketSlotIndex)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->RequestBuyCard(MyState->SideIndex, MarketSlotIndex);
	}
}
bool ACGPlayerController::ServerRequestBuyCard_Validate(int32 MarketSlotIndex)
{
	return true;
}

void ACGPlayerController::ServerRequestAttack_Implementation(int32 AttackerUnitIndex)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->RequestAttack(MyState->SideIndex, AttackerUnitIndex);
	}
}
bool ACGPlayerController::ServerRequestAttack_Validate(int32 AttackerUnitIndex)
{
	return true;
}

void ACGPlayerController::ServerRequestEndTurn_Implementation()
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->RequestEndTurn(MyState->SideIndex);
	}
}
bool ACGPlayerController::ServerRequestEndTurn_Validate()
{
	return true;
}

void ACGPlayerController::ServerResolveChoiceWithCard_Implementation(FName ChosenCardId)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->ResolvePendingChoiceWithCard(MyState->SideIndex, ChosenCardId);
	}
}
bool ACGPlayerController::ServerResolveChoiceWithCard_Validate(FName ChosenCardId)
{
	return true;
}

void ACGPlayerController::ServerResolveChoiceWithTarget_Implementation(int32 ChosenUnitIndex)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->ResolvePendingChoiceWithTarget(MyState->SideIndex, ChosenUnitIndex);
	}
}
bool ACGPlayerController::ServerResolveChoiceWithTarget_Validate(int32 ChosenUnitIndex)
{
	return true;
}

void ACGPlayerController::ServerResolveChoiceKeepOrBury_Implementation(bool bKeepOnTop)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->ResolvePendingChoiceKeepOrBury(MyState->SideIndex, bKeepOnTop);
	}
}
bool ACGPlayerController::ServerResolveChoiceKeepOrBury_Validate(bool bKeepOnTop)
{
	return true;
}

void ACGPlayerController::ServerResolveChoiceBuyDestination_Implementation(bool bToHand)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->ResolvePendingChoiceBuyDestination(MyState->SideIndex, bToHand);
	}
}
bool ACGPlayerController::ServerResolveChoiceBuyDestination_Validate(bool bToHand)
{
	return true;
}

void ACGPlayerController::ServerResolveChoiceSealTarget_Implementation(int32 ChosenUnitIndex)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->ResolvePendingChoiceSealTarget(MyState->SideIndex, ChosenUnitIndex);
	}
}
bool ACGPlayerController::ServerResolveChoiceSealTarget_Validate(int32 ChosenUnitIndex)
{
	return true;
}

void ACGPlayerController::ServerResolveChoiceAllyTarget_Implementation(int32 ChosenUnitIndex)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->ResolvePendingChoiceAllyTarget(MyState->SideIndex, ChosenUnitIndex);
	}
}
bool ACGPlayerController::ServerResolveChoiceAllyTarget_Validate(int32 ChosenUnitIndex)
{
	return true;
}

void ACGPlayerController::ServerResolveChoiceMarketSlotTarget_Implementation(int32 ChosenSlotIndex)
{
	ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
	ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
	if (MyState && GameMode)
	{
		GameMode->ResolvePendingChoiceMarketSlotTarget(MyState->SideIndex, ChosenSlotIndex);
	}
}
bool ACGPlayerController::ServerResolveChoiceMarketSlotTarget_Validate(int32 ChosenSlotIndex)
{
	return true;
}
