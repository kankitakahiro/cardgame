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
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "CardGame.h"

namespace
{
	// 場のユニット/相手の裏向き手札で使う固定カードサイズ。マーケット/自分の手札は
	// CGCardSlotWidgetのデフォルトサイズ(170x190、説明文を表示するため大きめ)を使う。
	constexpr float FaceDownCardWidth = 70.f;
	constexpr float FaceDownCardHeight = 90.f;
	constexpr float BoardCardWidth = 120.f;
	constexpr float BoardCardHeight = 110.f;

	FString FormatCard(const FCGCardDef& Def)
	{
		FString Base;
		if (Def.CardType == ECGCardType::Unit)
		{
			Base = FString::Printf(TEXT("%s [%d]\n%d/%d"), *Def.CardName, Def.Cost, Def.Atk, Def.Hp);
		}
		else
		{
			Base = FString::Printf(TEXT("%s [%d]"), *Def.CardName, Def.Cost);
		}

		if (!Def.Description.IsEmpty())
		{
			Base += FString::Printf(TEXT("\n%s"), *Def.Description);
		}
		return Base;
	}

	// カード枚数が増えても画面外へあふれないよう、各行を横スクロール可能にする。
	// 戻り値は中身を詰めるHorizontalBox(RefreshUIから直接ClearChildren/Add...で使う)。
	UHorizontalBox* MakeScrollableRow(UWidgetTree* WidgetTree, UVerticalBox* Root, const TCHAR* Name)
	{
		UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
			*FString::Printf(TEXT("%sScroll"), Name));
		ScrollBox->SetOrientation(EOrientation::Orient_Horizontal);

		UHorizontalBox* InnerBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name);
		ScrollBox->AddChild(InnerBox);

		if (UVerticalBoxSlot* Slot = Root->AddChildToVerticalBox(ScrollBox))
		{
			Slot->SetPadding(FMargin(12.f, 2.f));
		}
		return InnerBox;
	}

	void AddSectionHeader(UWidgetTree* WidgetTree, UVerticalBox* Root, const TCHAR* Name, const FString& Text)
	{
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Header->SetText(FText::FromString(Text));
		Header->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13));
		if (UVerticalBoxSlot* Slot = Root->AddChildToVerticalBox(Header))
		{
			Slot->SetPadding(FMargin(12.f, 0.f, 12.f, 0.f));
		}
	}
}

TSharedRef<SWidget> UCGGameHUD::RebuildWidget()
{
	EnsureWidgetTreeBuilt();
	return Super::RebuildWidget();
}

void UCGGameHUD::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTreeBuilt();
	RefreshUI();
}

void UCGGameHUD::EnsureWidgetTreeBuilt()
{
	if (bWidgetTreeBuilt)
	{
		return;
	}
	bWidgetTreeBuilt = true;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HUDRoot"));
	WidgetTree->RootWidget = Root;

	// ステータス〜手札までを縦スクロール領域に入れる。画面が狭くても中身が画面外に
	// あふれてEndTurnボタンごと見えなくなる、という事態を避けるため。
	// SizeBoxでMaxDesiredHeightを明示的に上限指定することで、親パネルのサイズ計算に
	// 依存せず確実にEndTurn分の高さを残す(Fill指定だけだと親の高さが不定な場合に
	// 効かないことがあった)。
	USizeBox* ContentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ContentSizeBox"));
	ContentSizeBox->SetMaxDesiredHeight(620.f);
	if (UVerticalBoxSlot* ContentSlot = Root->AddChildToVerticalBox(ContentSizeBox))
	{
		ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UScrollBox* ContentScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ContentScroll"));
	ContentScroll->SetOrientation(EOrientation::Orient_Vertical);
	ContentSizeBox->AddChild(ContentScroll);

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	ContentScroll->AddChild(ContentBox);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
	if (UVerticalBoxSlot* StatusSlot = ContentBox->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(16.f, 8.f, 16.f, 4.f));
	}

	AddSectionHeader(WidgetTree, ContentBox, TEXT("EnemyHandHeader"), TEXT("Enemy Hand"));
	EnemyHandBox = MakeScrollableRow(WidgetTree, ContentBox, TEXT("EnemyHandBox"));

	AddSectionHeader(WidgetTree, ContentBox, TEXT("EnemyBoardHeader"), TEXT("Enemy Board"));
	EnemyBoardBox = MakeScrollableRow(WidgetTree, ContentBox, TEXT("EnemyBoardBox"));

	AddSectionHeader(WidgetTree, ContentBox, TEXT("SelfBoardHeader"), TEXT("Your Board"));
	SelfBoardBox = MakeScrollableRow(WidgetTree, ContentBox, TEXT("SelfBoardBox"));

	AddSectionHeader(WidgetTree, ContentBox, TEXT("MarketHeader"), TEXT("Market"));
	MarketBox = MakeScrollableRow(WidgetTree, ContentBox, TEXT("MarketBox"));

	AddSectionHeader(WidgetTree, ContentBox, TEXT("HandHeader"), TEXT("Your Hand"));
	HandBox = MakeScrollableRow(WidgetTree, ContentBox, TEXT("HandBox"));

	USizeBox* EndTurnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EndTurnSizeBox"));
	EndTurnSizeBox->SetHeightOverride(64.f);

	EndTurnButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EndTurnButton"));
	UTextBlock* EndTurnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndTurnLabel"));
	EndTurnLabel->SetText(FText::FromString(TEXT("End Turn")));
	EndTurnLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20));
	EndTurnLabel->SetJustification(ETextJustify::Center);
	EndTurnButton->AddChild(EndTurnLabel);
	EndTurnButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleEndTurnClicked);
	EndTurnSizeBox->AddChild(EndTurnButton);

	if (UVerticalBoxSlot* EndTurnSlot = Root->AddChildToVerticalBox(EndTurnSizeBox))
	{
		EndTurnSlot->SetPadding(FMargin(12.f));
	}

	UE_LOG(LogCardGame, Log, TEXT("UCGGameHUD::EnsureWidgetTreeBuilt RootWidget=%s"),
		WidgetTree->RootWidget ? *WidgetTree->RootWidget->GetName() : TEXT("null"));
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
			TEXT("Turn %d - Active: Side %d   |   You HP:%d Mana:%d/%d Deck:%d   |   Enemy HP:%d Mana:%d/%d Deck:%d"),
			CGState->TurnCount, ActiveIndex,
			Active->CurrentHP, Active->CurrentMana, Active->MaxMana, Active->DeckCardIds.Num(),
			Other->CurrentHP, Other->CurrentMana, Other->MaxMana, Other->DeckCardIds.Num())));
	}

	// 相手の手札は中身を見せず、枚数分だけ裏向きカードを並べる(小さめサイズ)。
	PopulateFaceDownHandRow(EnemyHandBox, Other->HandCardIds.Num());
	PopulateBoardRow(EnemyBoardBox, Other, /*bIsSelfSide=*/false);
	PopulateBoardRow(SelfBoardBox, Active, /*bIsSelfSide=*/true);

	LastMarketCardIds = CGState->MarketCardIds;
	PopulateCardRow(MarketBox, LastMarketCardIds, /*bIsMarketRow=*/true,
		GET_FUNCTION_NAME_CHECKED(UCGGameHUD, HandleMarketSlotClicked));

	LastHandCardIds = Active->HandCardIds;
	PopulateCardRow(HandBox, LastHandCardIds, /*bIsMarketRow=*/false,
		GET_FUNCTION_NAME_CHECKED(UCGGameHUD, HandleHandSlotClicked));
}

void UCGGameHUD::PopulateFaceDownHandRow(UHorizontalBox* Box, int32 CardCount)
{
	Box->ClearChildren();
	for (int32 i = 0; i < CardCount; ++i)
	{
		UCGCardSlotWidget* FaceDownCard = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		FaceDownCard->SetCardSize(FaceDownCardWidth, FaceDownCardHeight);
		FaceDownCard->SlotIndex = -1;
		FaceDownCard->SetLabel(TEXT("?"));
		Box->AddChildToHorizontalBox(FaceDownCard);
	}
}

void UCGGameHUD::PopulateBoardRow(UHorizontalBox* Box, ACGPlayerState* Side, bool bIsSelfSide)
{
	Box->ClearChildren();
	for (int32 i = 0; i < Side->BoardUnitCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		UCGCardDatabase::FindCard(Side->BoardUnitCardIds[i], Def);

		// 自分の場は攻撃可能かどうか、相手の場は守護持ちかどうかだけを添えて表示する。
		const bool bCanAttack = bIsSelfSide && Side->BoardUnitCanAttack.IsValidIndex(i) && Side->BoardUnitCanAttack[i];
		const bool bHasGuard = !bIsSelfSide && Side->BoardUnitHasGuard.IsValidIndex(i) && Side->BoardUnitHasGuard[i];
		const TCHAR* Suffix = bCanAttack ? TEXT("\n(Attack)") : (bHasGuard ? TEXT("\n[Guard]") : TEXT(""));

		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SetCardSize(BoardCardWidth, BoardCardHeight);
		SlotWidget->SlotIndex = i;
		SlotWidget->SetLabel(FString::Printf(TEXT("%s\n%d/%d%s"), *Def.CardName, Side->BoardUnitAtk[i], Side->BoardUnitHp[i], Suffix));
		if (!Def.Description.IsEmpty())
		{
			SlotWidget->SetToolTipText(FText::FromString(Def.Description));
		}
		if (bCanAttack)
		{
			SlotWidget->OnSlotClicked.AddDynamic(this, &UCGGameHUD::HandleBoardSlotClicked);
		}
		Box->AddChildToHorizontalBox(SlotWidget);
	}
}

void UCGGameHUD::PopulateCardRow(UHorizontalBox* Box, const TArray<FName>& CardIds, bool bIsMarketRow, FName ClickHandlerName)
{
	Box->ClearChildren();
	for (int32 i = 0; i < CardIds.Num(); ++i)
	{
		FCGCardDef Def;
		UCGCardDatabase::FindCard(CardIds[i], Def);
		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		SlotWidget->SetLabel(bIsMarketRow ? FString::Printf(TEXT("Buy\n%s"), *FormatCard(Def)) : FormatCard(Def));
		if (!Def.Description.IsEmpty())
		{
			SlotWidget->SetToolTipText(FText::FromString(Def.Description));
		}
		// マーケット/手札はクリック時の処理だけが違うため、関数名指定で動的にバインドしている
		// (AddDynamicはコンパイル時に関数を1つに固定するマクロのため、ここでは使えない)。
		FScriptDelegate ClickDelegate;
		ClickDelegate.BindUFunction(this, ClickHandlerName);
		SlotWidget->OnSlotClicked.Add(ClickDelegate);
		Box->AddChildToHorizontalBox(SlotWidget);
	}
}

bool UCGGameHUD::TryGetGameModeAndState(ACGGameMode*& OutGameMode, ACGGameState*& OutCGState) const
{
	OutGameMode = GetCGGameMode();
	OutCGState = OutGameMode ? OutGameMode->GetCGGameState() : nullptr;
	return OutGameMode != nullptr && OutCGState != nullptr;
}

void UCGGameHUD::HandleHandSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState) || !LastHandCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}
	GameMode->RequestPlayCard(CGState->CurrentTurnPlayerIndex, LastHandCardIds[SlotIndex]);
	RefreshUI();
}

void UCGGameHUD::HandleMarketSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState) || !LastMarketCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}
	GameMode->RequestBuyCard(CGState->CurrentTurnPlayerIndex, LastMarketCardIds[SlotIndex]);
	RefreshUI();
}

void UCGGameHUD::HandleBoardSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState) || CGState->Sides.Num() < 2)
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
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}
	GameMode->RequestEndTurn(CGState->CurrentTurnPlayerIndex);
	RefreshUI();
}
