#include "CGGameMode.h"
#include "CGGameState.h"
#include "CGPlayerState.h"
#include "CGCardDatabase.h"
#include "CGGameHUD.h"
#include "CardGame.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ACGGameMode::ACGGameMode()
{
	// GameStateClass / PlayerStateClass は、Blueprint再生成スクリプト側で
	// BP_CG_GameState / BP_CG_PlayerState を指すよう上書き設定される。
	GameStateClass = ACGGameState::StaticClass();
	PlayerStateClass = ACGPlayerState::StaticClass();

	// 3Dの操作対象を持たないUI主体のカードゲームのため、デフォルトPawnは不要。
	DefaultPawnClass = nullptr;
}

void ACGGameMode::BeginPlay()
{
	Super::BeginPlay();
	InitializeMatch();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (UCGGameHUD* HUD = CreateWidget<UCGGameHUD>(PC, UCGGameHUD::StaticClass()))
		{
			HUD->AddToViewport();
			PC->bShowMouseCursor = true;
			PC->SetInputMode(FInputModeUIOnly());
			UE_LOG(LogCardGame, Log, TEXT("HUD created and added to viewport for PC=%s"), *PC->GetName());
		}
		else
		{
			UE_LOG(LogCardGame, Error, TEXT("CreateWidget<UCGGameHUD> failed"));
		}
	}
	else
	{
		UE_LOG(LogCardGame, Error, TEXT("No PlayerController(0) found at BeginPlay time"));
	}
}

ACGGameState* ACGGameMode::GetCGGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ACGGameState>() : nullptr;
}

void ACGGameMode::InitializeMatch()
{
	UWorld* World = GetWorld();
	ACGGameState* CGState = GetCGGameState();
	if (!World || !CGState)
	{
		return;
	}

	CGState->Sides.Reset();
	const TArray<FName> Starter = UCGCardDatabase::GetStarterDeckCardIds();

	const TSubclassOf<APlayerState> SideClass = PlayerStateClass ? *PlayerStateClass : ACGPlayerState::StaticClass();

	for (int32 i = 0; i < 2; ++i)
	{
		ACGPlayerState* Side = World->SpawnActor<ACGPlayerState>(SideClass);
		if (!Side)
		{
			continue;
		}
		Side->SideIndex = i;
		Side->MaxHP = 20;
		Side->CurrentHP = 20;
		Side->MaxMana = 0;
		Side->CurrentMana = 0;
		Side->bIsDefeated = false;
		Side->InitializeStartingDeck(Starter);
		for (int32 d = 0; d < 5; ++d)
		{
			Side->DrawCard();
		}
		CGState->Sides.Add(Side);
	}

	CGState->MarketDeckCardIds = UCGCardDatabase::GetAllCardIds();
	CGState->MarketCardIds.Reset();
	CGState->RefillMarket();

	CGState->TurnCount = 0;
	CGState->WinnerPlayerIndex = -1;
	CGState->CurrentTurnPlayerIndex = FMath::RandBool() ? 0 : 1;

	UE_LOG(LogCardGame, Log, TEXT("InitializeMatch: Sides=%d Side0 HP=%d Hand=%d Deck=%d / Side1 HP=%d Hand=%d Deck=%d / Market=%d FirstPlayer=%d"),
		CGState->Sides.Num(),
		CGState->Sides.IsValidIndex(0) ? CGState->Sides[0]->CurrentHP : -1,
		CGState->Sides.IsValidIndex(0) ? CGState->Sides[0]->HandCardIds.Num() : -1,
		CGState->Sides.IsValidIndex(0) ? CGState->Sides[0]->DeckCardIds.Num() : -1,
		CGState->Sides.IsValidIndex(1) ? CGState->Sides[1]->CurrentHP : -1,
		CGState->Sides.IsValidIndex(1) ? CGState->Sides[1]->HandCardIds.Num() : -1,
		CGState->Sides.IsValidIndex(1) ? CGState->Sides[1]->DeckCardIds.Num() : -1,
		CGState->MarketCardIds.Num(),
		CGState->CurrentTurnPlayerIndex);

	StartTurn();
}

void ACGGameMode::StartTurn()
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->Sides.Num() < 2)
	{
		return;
	}

	CGState->TurnCount++;
	CGState->CurrentPhase = ECGPhase::Draw;

	ACGPlayerState* Active = CGState->Sides[CGState->CurrentTurnPlayerIndex];
	if (Active)
	{
		Active->RefreshManaForNewTurn();
		Active->DrawCard();
		for (int32 i = 0; i < Active->BoardUnitCanAttack.Num(); ++i)
		{
			Active->BoardUnitCanAttack[i] = true;
		}
	}

	CGState->CurrentPhase = ECGPhase::Main;

	UE_LOG(LogCardGame, Log, TEXT("StartTurn: TurnCount=%d ActiveSide=%d Mana=%d/%d HandSize=%d"),
		CGState->TurnCount, CGState->CurrentTurnPlayerIndex,
		Active ? Active->CurrentMana : -1, Active ? Active->MaxMana : -1,
		Active ? Active->HandCardIds.Num() : -1);

	CheckWinLose();
	OnCardGameStateChanged();
}

ACGPlayerState* ACGGameMode::GetOpponent(int32 SideIndex) const
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->Sides.Num() < 2)
	{
		return nullptr;
	}
	return CGState->Sides[SideIndex == 0 ? 1 : 0];
}

bool ACGGameMode::RequestPlayCard(int32 SideIndex, FName CardId, int32 TargetUnitIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->Sides.IsValidIndex(SideIndex))
	{
		return false;
	}
	if (CGState->CurrentTurnPlayerIndex != SideIndex || CGState->CurrentPhase != ECGPhase::Main)
	{
		return false;
	}

	ACGPlayerState* Side = CGState->Sides[SideIndex];
	ACGPlayerState* Opponent = GetOpponent(SideIndex);
	const bool bResult = Side && Side->PlayCardFromHand(CardId, Opponent, TargetUnitIndex);

	if (bResult)
	{
		if (Opponent)
		{
			const int32 DeathDraws = Opponent->RemoveDeadUnitsAndGetDeathDrawCount();
			for (int32 i = 0; i < DeathDraws; ++i)
			{
				Opponent->DrawCard();
			}
		}
		CheckWinLose();
		OnCardGameStateChanged();
	}
	UE_LOG(LogCardGame, Log, TEXT("RequestPlayCard: Side=%d Card=%s Target=%d -> %s"),
		SideIndex, *CardId.ToString(), TargetUnitIndex, bResult ? TEXT("OK") : TEXT("REJECTED"));
	return bResult;
}

bool ACGGameMode::RequestBuyCard(int32 SideIndex, FName CardId)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->Sides.IsValidIndex(SideIndex))
	{
		return false;
	}
	if (CGState->CurrentTurnPlayerIndex != SideIndex || CGState->CurrentPhase != ECGPhase::Main)
	{
		return false;
	}
	if (!CGState->MarketCardIds.Contains(CardId))
	{
		return false;
	}

	ACGPlayerState* Side = CGState->Sides[SideIndex];
	if (Side && Side->BuyCard(CardId))
	{
		CGState->MarketCardIds.RemoveSingle(CardId);
		CGState->RefillMarket();
		OnCardGameStateChanged();
		UE_LOG(LogCardGame, Log, TEXT("RequestBuyCard: Side=%d Card=%s -> OK"), SideIndex, *CardId.ToString());
		return true;
	}
	UE_LOG(LogCardGame, Log, TEXT("RequestBuyCard: Side=%d Card=%s -> REJECTED"), SideIndex, *CardId.ToString());
	return false;
}

bool ACGGameMode::RequestAttack(int32 AttackerSideIndex, int32 AttackerUnitIndex, int32 TargetUnitIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->Sides.IsValidIndex(AttackerSideIndex))
	{
		return false;
	}
	if (CGState->CurrentTurnPlayerIndex != AttackerSideIndex || CGState->CurrentPhase != ECGPhase::Main)
	{
		return false;
	}

	ACGPlayerState* Attacker = CGState->Sides[AttackerSideIndex];
	ACGPlayerState* Defender = GetOpponent(AttackerSideIndex);
	if (!Attacker || !Defender)
	{
		return false;
	}
	if (!Attacker->BoardUnitCardIds.IsValidIndex(AttackerUnitIndex) || !Attacker->BoardUnitCanAttack[AttackerUnitIndex])
	{
		return false;
	}

	// 守護(Guard)がいる場合は必ずそちらを対象にしなければならない。
	if (Defender->HasGuardUnit())
	{
		if (!Defender->BoardUnitHasGuard.IsValidIndex(TargetUnitIndex) || !Defender->BoardUnitHasGuard[TargetUnitIndex])
		{
			return false;
		}
	}

	const int32 Damage = Attacker->BoardUnitAtk[AttackerUnitIndex];
	Attacker->BoardUnitCanAttack[AttackerUnitIndex] = false;

	if (Defender->BoardUnitCardIds.IsValidIndex(TargetUnitIndex))
	{
		const int32 CounterDamage = Defender->BoardUnitAtk[TargetUnitIndex];
		Defender->ApplyDamageToUnit(TargetUnitIndex, Damage);
		Attacker->ApplyDamageToUnit(AttackerUnitIndex, CounterDamage);
	}
	else
	{
		Defender->ApplyDamage(Damage);
	}

	int32 DeathDraws = Attacker->RemoveDeadUnitsAndGetDeathDrawCount();
	for (int32 i = 0; i < DeathDraws; ++i)
	{
		Attacker->DrawCard();
	}
	DeathDraws = Defender->RemoveDeadUnitsAndGetDeathDrawCount();
	for (int32 i = 0; i < DeathDraws; ++i)
	{
		Defender->DrawCard();
	}

	CheckWinLose();
	OnCardGameStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("RequestAttack: Attacker=%d Unit=%d Target=%d Damage=%d -> OK"),
		AttackerSideIndex, AttackerUnitIndex, TargetUnitIndex, Damage);
	return true;
}

void ACGGameMode::RequestEndTurn(int32 SideIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->CurrentTurnPlayerIndex != SideIndex || CGState->WinnerPlayerIndex != -1)
	{
		return;
	}

	CGState->CurrentPhase = ECGPhase::End;
	CGState->CurrentTurnPlayerIndex = (SideIndex == 0) ? 1 : 0;
	StartTurn();
}

void ACGGameMode::CheckWinLose()
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->Sides.Num() < 2 || CGState->WinnerPlayerIndex != -1)
	{
		return;
	}

	const bool bSide0Lost = CGState->Sides[0] && (CGState->Sides[0]->bIsDefeated || CGState->Sides[0]->CurrentHP <= 0);
	const bool bSide1Lost = CGState->Sides[1] && (CGState->Sides[1]->bIsDefeated || CGState->Sides[1]->CurrentHP <= 0);

	if (bSide0Lost && bSide1Lost)
	{
		CGState->WinnerPlayerIndex = -1;
		UE_LOG(LogCardGame, Log, TEXT("Match ended: Draw"));
		OnMatchEnded(-1);
	}
	else if (bSide0Lost)
	{
		CGState->WinnerPlayerIndex = 1;
		UE_LOG(LogCardGame, Log, TEXT("Match ended: Winner=Side1"));
		OnMatchEnded(1);
	}
	else if (bSide1Lost)
	{
		CGState->WinnerPlayerIndex = 0;
		UE_LOG(LogCardGame, Log, TEXT("Match ended: Winner=Side0"));
		OnMatchEnded(0);
	}
}
