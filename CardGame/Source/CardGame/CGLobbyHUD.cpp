#include "CGLobbyHUD.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// バトル画面は既存の唯一の対戦レベルをそのまま使う(デッキ構築を経ない
	// 直接PIEも引き続きできるよう、レベル名・パスは変更していない)。
	const TCHAR* BattleLevelPath = TEXT("/Game/CardGame/Maps/L_Card_GamePrototype");
	const TCHAR* DeckBuilderLevelPath = TEXT("/Game/CardGame/Maps/L_DeckBuilder");

	UButton* MakeMenuButton(UWidgetTree* WidgetTree, const TCHAR* Name, const FString& Label, UVerticalBox* Root)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sSizeBox"), Name));
		SizeBox->SetWidthOverride(280.f);
		SizeBox->SetHeightOverride(64.f);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%sLabel"), Name));
		Text->SetText(FText::FromString(Label));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20));
		Text->SetJustification(ETextJustify::Center);

		Button->AddChild(Text);
		SizeBox->AddChild(Button);
		if (UVerticalBoxSlot* Slot = Root->AddChildToVerticalBox(SizeBox))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetPadding(FMargin(0.f, 12.f));
		}
		return Button;
	}
}

TSharedRef<SWidget> UCGLobbyHUD::RebuildWidget()
{
	EnsureWidgetTreeBuilt();
	return Super::RebuildWidget();
}

void UCGLobbyHUD::EnsureWidgetTreeBuilt()
{
	if (bWidgetTreeBuilt)
	{
		return;
	}
	bWidgetTreeBuilt = true;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LobbyRoot"));
	WidgetTree->RootWidget = Root;

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	Title->SetText(FText::FromString(TEXT("Card Game")));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 36));
	Title->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 48.f, 0.f, 24.f));
	}

	UButton* DeckBuilderButton = MakeMenuButton(WidgetTree, TEXT("DeckBuilderButton"), TEXT("デッキ構築"), Root);
	DeckBuilderButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleDeckBuilderClicked);

	UButton* StartBattleButton = MakeMenuButton(WidgetTree, TEXT("StartBattleButton"), TEXT("バトル開始"), Root);
	StartBattleButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleStartBattleClicked);
}

void UCGLobbyHUD::HandleDeckBuilderClicked()
{
	UGameplayStatics::OpenLevel(this, FName(DeckBuilderLevelPath));
}

void UCGLobbyHUD::HandleStartBattleClicked()
{
	UGameplayStatics::OpenLevel(this, FName(BattleLevelPath));
}
