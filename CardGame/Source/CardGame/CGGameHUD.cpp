#include "CGGameHUD.h"
#include "CGCardSlotWidget.h"
#include "CGGameMode.h"
#include "CGGameState.h"
#include "CGPlayerState.h"
#include "CGCardDatabase.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "CardGame.h"

namespace
{
	FString FormatCard(const FCGCardDef& Def)
	{
		if (Def.CardType == ECGCardType::Unit)
		{
			return FString::Printf(TEXT("%s [%d]\n%d/%d"), *Def.CardName, Def.Cost, Def.Atk, Def.Hp);
		}
		return FString::Printf(TEXT("%s [%d]"), *Def.CardName, Def.Cost);
	}
}

void UCGGameHUD::NativeConstruct()
{
	Super::NativeConstruct();

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HUDRoot"));
	WidgetTree->RootWidget = Root;

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	Root->AddChildToVerticalBox(StatusText);

	EnemyBoardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EnemyBoardBox"));
	Root->AddChildToVerticalBox(EnemyBoardBox);

	SelfBoardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SelfBoardBox"));
	Root->AddChildToVerticalBox(SelfBoardBox);

	MarketBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MarketBox"));
	Root->AddChildToVerticalBox(MarketBox);

	HandBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HandBox"));
	Root->AddChildToVerticalBox(HandBox);

	EndTurnButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EndTurnButton"));
	UTextBlock* EndTurnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndTurnLabel"));
	EndTurnLabel->SetText(FText::FromString(TEXT("End Turn")));
	EndTurnButton->AddChild(EndTurnLabel);
	EndTurnButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleEndTurnClicked);
	Root->AddChildToVerticalBox(EndTurnButton);

	UE_LOG(LogCardGame, Log, TEXT("UCGGameHUD::NativeConstruct built widget tree, RootWidget=%s"),
		WidgetTree->RootWidget ? *WidgetTree->RootWidget->GetName() : TEXT("null"));

	RefreshUI();
}

ACGGameMode* UCGGameHUD::GetCGGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
}

void UCGGameHUD::RefreshUI()
{
	ACGGameMode* GameMode = GetCGGameMode();
	ACGGameState* CGState = GameMode ? GameMode->GetCGGameState() : nullptr;
	UE_LOG(LogCardGame, Log, TEXT("UCGGameHUD::RefreshUI GameMode=%s CGState=%s Sides=%d"),
		GameMode ? TEXT("valid") : TEXT("NULL"), CGState ? TEXT("valid") : TEXT("NULL"),
		CGState ? CGState->Sides.Num() : -1);
	if (!CGState || CGState->Sides.Num() < 2)
	{
		return;
	}

	const int32 ActiveIndex = CGState->CurrentTurnPlayerIndex;
	const int32 OtherIndex = ActiveIndex == 0 ? 1 : 0;
	ACGPlayerState* Active = CGState->Sides[ActiveIndex];
	ACGPlayerState* Other = CGState->Sides[OtherIndex];
	if (!Active || !Other)
	{
		return;
	}

	if (CGState->WinnerPlayerIndex != -1)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("Match Over - Winner: Side %d"), CGState->WinnerPlayerIndex)));
	}
	else
	{
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("Turn %d - Active: Side %d | You HP:%d Mana:%d/%d | Enemy HP:%d"),
			CGState->TurnCount, ActiveIndex, Active->CurrentHP, Active->CurrentMana, Active->MaxMana, Other->CurrentHP)));
	}

	EnemyBoardBox->ClearChildren();
	for (int32 i = 0; i < Other->BoardUnitCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		UCGCardDatabase::FindCard(Other->BoardUnitCardIds[i], Def);
		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		SlotWidget->SetLabel(FString::Printf(TEXT("%s\n%d/%d%s"), *Def.CardName, Other->BoardUnitAtk[i], Other->BoardUnitHp[i],
			(Other->BoardUnitHasGuard.IsValidIndex(i) && Other->BoardUnitHasGuard[i]) ? TEXT("\n[Guard]") : TEXT("")));
		EnemyBoardBox->AddChildToHorizontalBox(SlotWidget);
	}

	SelfBoardBox->ClearChildren();
	for (int32 i = 0; i < Active->BoardUnitCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		UCGCardDatabase::FindCard(Active->BoardUnitCardIds[i], Def);
		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		const bool bCanAttack = Active->BoardUnitCanAttack.IsValidIndex(i) && Active->BoardUnitCanAttack[i];
		SlotWidget->SetLabel(FString::Printf(TEXT("%s\n%d/%d%s"), *Def.CardName, Active->BoardUnitAtk[i], Active->BoardUnitHp[i],
			bCanAttack ? TEXT("\n(Attack)") : TEXT("")));
		if (bCanAttack)
		{
			SlotWidget->OnSlotClicked.AddDynamic(this, &UCGGameHUD::HandleBoardSlotClicked);
		}
		SelfBoardBox->AddChildToHorizontalBox(SlotWidget);
	}

	MarketBox->ClearChildren();
	LastMarketCardIds = CGState->MarketCardIds;
	for (int32 i = 0; i < LastMarketCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		UCGCardDatabase::FindCard(LastMarketCardIds[i], Def);
		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		SlotWidget->SetLabel(FString::Printf(TEXT("Buy\n%s"), *FormatCard(Def)));
		SlotWidget->OnSlotClicked.AddDynamic(this, &UCGGameHUD::HandleMarketSlotClicked);
		MarketBox->AddChildToHorizontalBox(SlotWidget);
	}

	HandBox->ClearChildren();
	LastHandCardIds = Active->HandCardIds;
	for (int32 i = 0; i < LastHandCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		UCGCardDatabase::FindCard(LastHandCardIds[i], Def);
		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		SlotWidget->SetLabel(FormatCard(Def));
		SlotWidget->OnSlotClicked.AddDynamic(this, &UCGGameHUD::HandleHandSlotClicked);
		HandBox->AddChildToHorizontalBox(SlotWidget);
	}
}

void UCGGameHUD::HandleHandSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode = GetCGGameMode();
	ACGGameState* CGState = GameMode ? GameMode->GetCGGameState() : nullptr;
	if (!GameMode || !CGState || !LastHandCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}
	GameMode->RequestPlayCard(CGState->CurrentTurnPlayerIndex, LastHandCardIds[SlotIndex]);
	RefreshUI();
}

void UCGGameHUD::HandleMarketSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode = GetCGGameMode();
	ACGGameState* CGState = GameMode ? GameMode->GetCGGameState() : nullptr;
	if (!GameMode || !CGState || !LastMarketCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}
	GameMode->RequestBuyCard(CGState->CurrentTurnPlayerIndex, LastMarketCardIds[SlotIndex]);
	RefreshUI();
}

void UCGGameHUD::HandleBoardSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode = GetCGGameMode();
	ACGGameState* CGState = GameMode ? GameMode->GetCGGameState() : nullptr;
	if (!GameMode || !CGState || CGState->Sides.Num() < 2)
	{
		return;
	}

	const int32 ActiveIndex = CGState->CurrentTurnPlayerIndex;
	const int32 OtherIndex = ActiveIndex == 0 ? 1 : 0;
	ACGPlayerState* Defender = CGState->Sides[OtherIndex];

	// 対象は自動選択(単一対象、docs/game-rules-minimum.md): 相手に守護がいれば必ずそちら、
	// いなければ顔面攻撃。手動ターゲット選択UIは未実装。
	int32 TargetUnitIndex = -1;
	if (Defender && Defender->HasGuardUnit())
	{
		for (int32 i = 0; i < Defender->BoardUnitHasGuard.Num(); ++i)
		{
			if (Defender->BoardUnitHasGuard[i])
			{
				TargetUnitIndex = i;
				break;
			}
		}
	}

	GameMode->RequestAttack(ActiveIndex, SlotIndex, TargetUnitIndex);
	RefreshUI();
}

void UCGGameHUD::HandleEndTurnClicked()
{
	ACGGameMode* GameMode = GetCGGameMode();
	ACGGameState* CGState = GameMode ? GameMode->GetCGGameState() : nullptr;
	if (!GameMode || !CGState)
	{
		return;
	}
	GameMode->RequestEndTurn(CGState->CurrentTurnPlayerIndex);
	RefreshUI();
}
