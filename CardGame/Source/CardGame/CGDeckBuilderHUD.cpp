#include "CGDeckBuilderHUD.h"
#include "CGCardSlotWidget.h"
#include "CGCardDatabase.h"
#include "CGGameInstance.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Kismet/GameplayStatics.h"
#include "CardGame.h"

namespace
{
	constexpr int32 RequiredDeckSize = 12;
	const TCHAR* LobbyLevelPath = TEXT("/Game/CardGame/Maps/L_Lobby");

	void AddSectionHeader(UWidgetTree* WidgetTree, UVerticalBox* Root, const TCHAR* Name, const FString& Text)
	{
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Header->SetText(FText::FromString(Text));
		Header->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16));
		if (UVerticalBoxSlot* Slot = Root->AddChildToVerticalBox(Header))
		{
			Slot->SetPadding(FMargin(12.f, 4.f, 12.f, 0.f));
		}
	}

	// カードはカーソルを乗せると拡大プレビューが表示されるが、それは最前面の専用レイヤーに
	// 複製されて描かれるため(UCGCardHostWidget)、行のレイアウト自体は拡大分の余白を
	// 確保する必要がない。カード同士の間隔は見た目用の小さな固定値だけで良い。
	const float CardGap = 6.f;

	UHorizontalBox* MakeScrollableRow(UWidgetTree* WidgetTree, UVerticalBox* Root, const TCHAR* Name)
	{
		UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
			*FString::Printf(TEXT("%sScroll"), Name));
		ScrollBox->SetOrientation(EOrientation::Orient_Horizontal);

		UHorizontalBox* InnerBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name);

		ScrollBox->AddChild(InnerBox);

		if (UVerticalBoxSlot* Slot = Root->AddChildToVerticalBox(ScrollBox))
		{
			Slot->SetPadding(FMargin(12.f, 4.f));
		}
		return InnerBox;
	}

	UButton* MakeActionButton(UWidgetTree* WidgetTree, const TCHAR* Name, const FString& Label, UHorizontalBox* Root)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sSizeBox"), Name));
		SizeBox->SetWidthOverride(220.f);
		SizeBox->SetHeightOverride(56.f);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%sLabel"), Name));
		Text->SetText(FText::FromString(Label));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
		Text->SetJustification(ETextJustify::Center);

		Button->AddChild(Text);
		SizeBox->AddChild(Button);
		Root->AddChildToHorizontalBox(SizeBox);
		return Button;
	}
}

TSharedRef<SWidget> UCGDeckBuilderHUD::RebuildWidget()
{
	EnsureWidgetTreeBuilt();
	return Super::RebuildWidget();
}

void UCGDeckBuilderHUD::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTreeBuilt();

	// 現在GameInstanceに入っているデッキを編集開始時の初期選択にする。
	SelectedCardIds.Reset();
	if (UCGGameInstance* GI = GetCGGameInstance())
	{
		SelectedCardIds = GI->PlayerDeckCardIds;
	}

	RefreshUI();
}

void UCGDeckBuilderHUD::EnsureWidgetTreeBuilt()
{
	if (bWidgetTreeBuilt)
	{
		return;
	}
	bWidgetTreeBuilt = true;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DeckBuilderRoot"));

	// カードのホバー拡大プレビューを最前面に出すための共通レイヤーをRootに重ねる
	// (UCGCardHostWidget::BuildRootOverlay、docs/architecture.md「ホバー拡大とZ順序」)。
	WidgetTree->RootWidget = BuildRootOverlay(Root);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckBuilderTitle"));
	Title->SetText(FText::FromString(TEXT("デッキ構築")));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	Title->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 4.f));
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckBuilderStatus"));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
	StatusText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* StatusSlot = Root->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
		StatusSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	// シャドウバースのデッキ編成画面を参考に、上段=今のデッキ、下段=選べるカード一覧、
	// という2段構成にしている(タップで上下を行き来する)。
	AddSectionHeader(WidgetTree, Root, TEXT("DeckRowHeader"), TEXT("あなたのデッキ(タップで外す)"));
	DeckRowBox = MakeScrollableRow(WidgetTree, Root, TEXT("DeckRowBox"));

	AddSectionHeader(WidgetTree, Root, TEXT("PoolRowHeader"), TEXT("カード一覧(タップで追加)"));
	PoolRowBox = MakeScrollableRow(WidgetTree, Root, TEXT("PoolRowBox"));

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));
	if (UVerticalBoxSlot* ButtonRowSlot = Root->AddChildToVerticalBox(ButtonRow))
	{
		ButtonRowSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonRowSlot->SetPadding(FMargin(12.f));
	}

	SaveButton = MakeActionButton(WidgetTree, TEXT("SaveButton"), TEXT("保存してロビーへ"), ButtonRow);
	SaveButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleSaveClicked);

	UButton* BackButton = MakeActionButton(WidgetTree, TEXT("BackButton"), TEXT("保存せず戻る"), ButtonRow);
	BackButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleBackClicked);
}

UCGGameInstance* UCGDeckBuilderHUD::GetCGGameInstance() const
{
	return GetWorld() ? Cast<UCGGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

void UCGDeckBuilderHUD::RefreshUI()
{
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("あなたのデッキ: %d / %d"), SelectedCardIds.Num(), RequiredDeckSize)));

	// 上段: 今のデッキ(選んだ順)。クリックで外す。
	DeckRowBox->ClearChildren();
	LastDeckCardIds = SelectedCardIds;
	for (int32 i = 0; i < LastDeckCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		UCGCardDatabase::FindCard(LastDeckCardIds[i], Def);

		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		SlotWidget->SetCardData(Def);
		RegisterCardHoverPreview(SlotWidget);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleDeckSlotClicked);
		if (UHorizontalBoxSlot* CardSlot = DeckRowBox->AddChildToHorizontalBox(SlotWidget))
		{
			CardSlot->SetVerticalAlignment(VAlign_Center);
			CardSlot->SetPadding(FMargin(CardGap, 0.f));
		}
	}

	// 下段: カード一覧(24種固定)。既にデッキに入っているものは少し暗く表示するだけで、
	// 外すのは上段側で行う(上下で役割を分けてわかりやすくする)。
	PoolRowBox->ClearChildren();
	LastPoolCardIds.Reset();
	for (const FCGCardDef& Def : UCGCardDatabase::GetAllCards())
	{
		LastPoolCardIds.Add(Def.CardId);
		const int32 SlotIndex = LastPoolCardIds.Num() - 1;
		const bool bInDeck = SelectedCardIds.Contains(Def.CardId);

		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = SlotIndex;
		SlotWidget->SetCardData(Def);
		// カードの情報自体は変えず、既にデッキ入りしているものを少し暗くするだけに留める
		// (文字での注記はどの画面でも同じ情報を表示するという方針にそぐわないため)。
		SlotWidget->SetRenderOpacity(bInDeck ? 0.5f : 1.f);
		RegisterCardHoverPreview(SlotWidget);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandlePoolSlotClicked);
		if (UHorizontalBoxSlot* CardSlot = PoolRowBox->AddChildToHorizontalBox(SlotWidget))
		{
			CardSlot->SetVerticalAlignment(VAlign_Center);
			CardSlot->SetPadding(FMargin(CardGap, 0.f));
		}
	}

	SaveButton->SetIsEnabled(SelectedCardIds.Num() == RequiredDeckSize);
}

void UCGDeckBuilderHUD::HandleDeckSlotClicked(int32 SlotIndex)
{
	if (!LastDeckCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}
	SelectedCardIds.RemoveSingle(LastDeckCardIds[SlotIndex]);
	RefreshUI();
}

void UCGDeckBuilderHUD::HandlePoolSlotClicked(int32 SlotIndex)
{
	if (!LastPoolCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}

	const FName CardId = LastPoolCardIds[SlotIndex];
	// 既にデッキ内のカードをタップしても何もしない(外すのは上段側の役割)。
	// デッキが12枚に達している場合は、先に上段側で何かを外してもらう必要がある。
	if (!SelectedCardIds.Contains(CardId) && SelectedCardIds.Num() < RequiredDeckSize)
	{
		SelectedCardIds.Add(CardId);
		RefreshUI();
	}
}

void UCGDeckBuilderHUD::HandleSaveClicked()
{
	if (SelectedCardIds.Num() != RequiredDeckSize)
	{
		return;
	}

	if (UCGGameInstance* GI = GetCGGameInstance())
	{
		GI->PlayerDeckCardIds = SelectedCardIds;
		GI->SaveDeckToDisk();
	}
	else
	{
		UE_LOG(LogCardGame, Error, TEXT("UCGDeckBuilderHUD::HandleSaveClicked: GameInstance is not UCGGameInstance"));
	}

	UGameplayStatics::OpenLevel(this, FName(LobbyLevelPath));
}

void UCGDeckBuilderHUD::HandleBackClicked()
{
	UGameplayStatics::OpenLevel(this, FName(LobbyLevelPath));
}
