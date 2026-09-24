#include "CGDeckBuilderHUD.h"
#include "CGCardSlotWidget.h"
#include "CGCardDatabase.h"
#include "CGGameInstance.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Algo/Count.h"
#include "CardGame.h"

namespace
{
	// 次期ルール(docs/next-ruleset-design.md)でデッキ25枚・同名カード3枚まで
	// 重複可に変更。
	constexpr int32 RequiredDeckSize = 25;
	constexpr int32 MaxCopiesPerCard = 3;
	const TCHAR* LobbyLevelPath = TEXT("/Game/CardGame/Maps/L_Lobby");

	// カード一覧(下段)の表示縮小率。通常表示/コンパクト表示切り替え用
	// (「カードが増えたときの工夫」フィードバックのため。docs/architecture.md
	// 「カードUIの設計」)。通常表示でもUCGCardSlotWidget::WrapForCompactDisplay()で
	// 折り返しグリッドにするため、以前(等倍・横スクロール一列)より一度に見える枚数が増える。
	constexpr float PoolDisplayScaleNormal = 0.6f;
	constexpr float PoolDisplayScaleCompact = 0.4f;

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

	// 検索/フィルタ/並び替え/表示切り替え用の小さめのトグルボタン。選択中かどうかは
	// 呼び出し側がSetRenderOpacity()で示す(UCGDeckBuilderHUD::UpdateFilterSortButtonVisuals)。
	UButton* MakeSmallToggleButton(UWidgetTree* WidgetTree, const TCHAR* Name, const FString& Label, UHorizontalBox* Root)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sSizeBox"), Name));
		SizeBox->SetHeightOverride(34.f);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%sLabel"), Name));
		Text->SetText(FText::FromString(Label));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13));
		Text->SetJustification(ETextJustify::Center);

		if (UButtonSlot* TextSlot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			TextSlot->SetPadding(FMargin(12.f, 4.f));
		}

		if (USizeBoxSlot* ButtonSlot = Cast<USizeBoxSlot>(SizeBox->AddChild(Button)))
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		}

		if (UHorizontalBoxSlot* RowSlot = Root->AddChildToHorizontalBox(SizeBox))
		{
			RowSlot->SetPadding(FMargin(3.f, 0.f));
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}
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

	// 現在GameInstanceに入っているデッキを編集開始時の初期選択にする
	// (デッキ名も合わせて復元。同じ名前のまま保存すれば上書きになる)。
	SelectedCardIds.Reset();
	if (UCGGameInstance* GI = GetCGGameInstance())
	{
		SelectedCardIds = GI->PlayerDeckCardIds;
		DeckNameBox->SetText(FText::FromString(GI->ActiveDeckName));
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

	// WidgetTree->RootWidgetの設定(BuildRootOverlay呼び出し)は、保存/戻るボタンを
	// 右上に重ねるためのオーバーレイ(ButtonCornerOverlay)を組み立てた後、関数末尾で
	// まとめて行う(CGGameHUD.cppと同じ方針)。

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckBuilderTitle"));
	Title->SetText(FText::FromString(TEXT("デッキ構築")));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	Title->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = Root->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 4.f));
	}

	// デッキ名の入力欄。保存時にこの名前でGameInstanceのSavedDecksへ保存/上書きする
	// (docs/architecture.md「デッキの永続化」)。
	UHorizontalBox* DeckNameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DeckNameRow"));
	if (UVerticalBoxSlot* DeckNameRowSlot = Root->AddChildToVerticalBox(DeckNameRow))
	{
		DeckNameRowSlot->SetHorizontalAlignment(HAlign_Center);
		DeckNameRowSlot->SetPadding(FMargin(12.f, 0.f, 12.f, 4.f));
	}

	UTextBlock* DeckNameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckNameLabel"));
	DeckNameLabel->SetText(FText::FromString(TEXT("デッキ名:")));
	DeckNameLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15));
	if (UHorizontalBoxSlot* DeckNameLabelSlot = DeckNameRow->AddChildToHorizontalBox(DeckNameLabel))
	{
		DeckNameLabelSlot->SetVerticalAlignment(VAlign_Center);
		DeckNameLabelSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	USizeBox* DeckNameSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DeckNameSizeBox"));
	DeckNameSizeBox->SetWidthOverride(280.f);
	DeckNameSizeBox->SetHeightOverride(34.f);

	DeckNameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("DeckNameBox"));
	DeckNameBox->SetHintText(FText::FromString(TEXT("デッキ名を入力")));
	if (USizeBoxSlot* DeckNameBoxSlot = Cast<USizeBoxSlot>(DeckNameSizeBox->AddChild(DeckNameBox)))
	{
		DeckNameBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		DeckNameBoxSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UHorizontalBoxSlot* DeckNameBoxRowSlot = DeckNameRow->AddChildToHorizontalBox(DeckNameSizeBox))
	{
		DeckNameBoxRowSlot->SetVerticalAlignment(VAlign_Center);
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

	// カード一覧の検索/フィルタ/並び替え/表示切り替え。カードの種類・種族・タイプが
	// 今後増えていく前提で、一覧から目的のカードを見つけやすくするために用意している
	// (「カードが増えたときの工夫」というフィードバックのため)。
	UHorizontalBox* PoolControlsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PoolControlsRow"));
	if (UVerticalBoxSlot* ControlsRowSlot = Root->AddChildToVerticalBox(PoolControlsRow))
	{
		ControlsRowSlot->SetHorizontalAlignment(HAlign_Center);
		ControlsRowSlot->SetPadding(FMargin(12.f, 2.f));
	}

	USizeBox* SearchSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SearchBoxSizeBox"));
	SearchSizeBox->SetWidthOverride(200.f);
	SearchSizeBox->SetHeightOverride(34.f);

	SearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("SearchBox"));
	SearchBox->SetHintText(FText::FromString(TEXT("カード名で検索")));
	SearchBox->OnTextChanged.AddDynamic(this, &UCGDeckBuilderHUD::HandleSearchTextChanged);
	if (USizeBoxSlot* SearchSlot = Cast<USizeBoxSlot>(SearchSizeBox->AddChild(SearchBox)))
	{
		SearchSlot->SetHorizontalAlignment(HAlign_Fill);
		SearchSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UHorizontalBoxSlot* SearchRowSlot = PoolControlsRow->AddChildToHorizontalBox(SearchSizeBox))
	{
		SearchRowSlot->SetPadding(FMargin(3.f, 0.f, 14.f, 0.f));
		SearchRowSlot->SetVerticalAlignment(VAlign_Center);
	}

	FilterAllButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterAllButton"), TEXT("全て"), PoolControlsRow);
	FilterAllButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterAllClicked);
	FilterUnitButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterUnitButton"), TEXT("Unit"), PoolControlsRow);
	FilterUnitButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterUnitClicked);
	FilterSpellButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterSpellButton"), TEXT("Spell"), PoolControlsRow);
	FilterSpellButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterSpellClicked);

	SortCostButton = MakeSmallToggleButton(WidgetTree, TEXT("SortCostButton"), TEXT("コスト順"), PoolControlsRow);
	SortCostButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleSortCostClicked);
	SortNameButton = MakeSmallToggleButton(WidgetTree, TEXT("SortNameButton"), TEXT("名前順"), PoolControlsRow);
	SortNameButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleSortNameClicked);
	SortTribeButton = MakeSmallToggleButton(WidgetTree, TEXT("SortTribeButton"), TEXT("種族順"), PoolControlsRow);
	SortTribeButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleSortTribeClicked);

	CompactToggleButton = MakeSmallToggleButton(WidgetTree, TEXT("CompactToggleButton"), TEXT("コンパクト表示"), PoolControlsRow);
	CompactToggleButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleCompactToggleClicked);

	// 色フィルタは種別フィルタ等と同じ行に詰め込むと窮屈になるため、専用の行にする
	// (「デッキ構築で色でフィルターをかけれるようにしてほしい」というフィードバックへの対応)。
	UHorizontalBox* ColorFilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ColorFilterRow"));
	if (UVerticalBoxSlot* ColorFilterRowSlot = Root->AddChildToVerticalBox(ColorFilterRow))
	{
		ColorFilterRowSlot->SetHorizontalAlignment(HAlign_Center);
		ColorFilterRowSlot->SetPadding(FMargin(12.f, 0.f, 12.f, 2.f));
	}

	FilterColorAllButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterColorAllButton"), TEXT("全色"), ColorFilterRow);
	FilterColorAllButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterColorAllClicked);
	FilterColorRedButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterColorRedButton"), TEXT("赤"), ColorFilterRow);
	FilterColorRedButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterColorRedClicked);
	FilterColorOrangeButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterColorOrangeButton"), TEXT("橙"), ColorFilterRow);
	FilterColorOrangeButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterColorOrangeClicked);
	FilterColorGreenButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterColorGreenButton"), TEXT("緑"), ColorFilterRow);
	FilterColorGreenButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterColorGreenClicked);
	FilterColorBlueButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterColorBlueButton"), TEXT("青"), ColorFilterRow);
	FilterColorBlueButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterColorBlueClicked);
	FilterColorPurpleButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterColorPurpleButton"), TEXT("紫"), ColorFilterRow);
	FilterColorPurpleButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterColorPurpleClicked);
	FilterColorNoneButton = MakeSmallToggleButton(WidgetTree, TEXT("FilterColorNoneButton"), TEXT("無色"), ColorFilterRow);
	FilterColorNoneButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleFilterColorNoneClicked);

	// 一覧本体。カードが何行に折り返しても画面全体が伸びないよう内部スクロールに
	// している(docs/architecture.md「カードUIの設計」)。以前は固定高さ(440px)の
	// SizeBoxで囲んでいたが、画面の高さが足りない環境ではデッキ上段や上部の各行と
	// 合わせた合計の高さが画面を超え、下の「保存してロビーへ」/「保存せず戻る」
	// ボタン自体が画面外に押し出されて見えなくなる(「デッキ構築画面からロビーに
	// 戻れない」というフィードバックの実際の原因。CGGameHUDのバトル画面で
	// 同種の問題を直したのと同じ対策)ため、固定高さをやめてFill(残り高さを
	// 一覧が吸収し、それでも足りない分は内部スクロールで対応)にする。
	UScrollBox* PoolScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PoolScroll"));
	PoolScroll->SetOrientation(EOrientation::Orient_Vertical);

	PoolListContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PoolListContainer"));
	PoolScroll->AddChild(PoolListContainer);

	if (UVerticalBoxSlot* PoolAreaSlot = Root->AddChildToVerticalBox(PoolScroll))
	{
		PoolAreaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PoolAreaSlot->SetPadding(FMargin(12.f, 4.f));
	}

	// 保存/戻るボタンは、Rootの縦積みの最後(画面下部)に置くと、画面の高さが
	// 足りない環境でカード一覧より下が画面外へ押し出されて見えなくなってしまう
	// (「デッキ構築画面からロビーに戻れない」バグの原因になった)。常に画面内に
	// 見えるよう、Rootの縦積みには含めず画面右上にオーバーレイで重ねる方式に変更する
	// (「ボタンの位置を右上にしてほしい」というフィードバックへの対応)。
	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ButtonRow"));

	SaveButton = MakeActionButton(WidgetTree, TEXT("SaveButton"), TEXT("保存してロビーへ"), ButtonRow);
	SaveButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleSaveClicked);

	UButton* BackButton = MakeActionButton(WidgetTree, TEXT("BackButton"), TEXT("保存せず戻る"), ButtonRow);
	BackButton->OnClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandleBackClicked);

	UOverlay* ButtonCornerOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ButtonCornerOverlay"));
	if (UOverlaySlot* RootSlot = ButtonCornerOverlay->AddChildToOverlay(Root))
	{
		RootSlot->SetHorizontalAlignment(HAlign_Fill);
		RootSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* ButtonRowSlot = ButtonCornerOverlay->AddChildToOverlay(ButtonRow))
	{
		ButtonRowSlot->SetHorizontalAlignment(HAlign_Right);
		ButtonRowSlot->SetVerticalAlignment(VAlign_Top);
		ButtonRowSlot->SetPadding(FMargin(0.f, 12.f, 16.f, 0.f));
	}

	// カードのホバー拡大プレビューを最前面に出すための共通レイヤーを重ねる
	// (UCGCardHostWidget::BuildRootOverlay、docs/architecture.md「ホバー拡大とZ順序」)。
	WidgetTree->RootWidget = BuildRootOverlay(ButtonCornerOverlay);

	UpdateFilterSortButtonVisuals();
}

UCGGameInstance* UCGDeckBuilderHUD::GetCGGameInstance() const
{
	return GetWorld() ? Cast<UCGGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

void UCGDeckBuilderHUD::RefreshUI()
{
	// カード一覧を作り直す前に、ホバー中だったカードが消えて拡大プレビューだけ
	// 画面に残ってしまう問題を防ぐ(CGGameHUDと同じ対策)。
	HidePreview();

	// 「保存してロビーへ」は25枚ちょうどでないと押せない(SaveButton->SetIsEnabled、
	// 下記)が、それだけでは理由が伝わらず「ロビーに戻れない」という混乱を招くため
	// (フィードバックへの対応)、25枚に満たない/超える間はここに理由と、代わりに
	// 「保存せず戻る」ボタンが使えることを明記する。
	const FString DeckStatus = (SelectedCardIds.Num() == RequiredDeckSize)
		? FString::Printf(TEXT("あなたのデッキ: %d / %d"), SelectedCardIds.Num(), RequiredDeckSize)
		: FString::Printf(TEXT("あなたのデッキ: %d / %d (ちょうど%d枚で「保存してロビーへ」が押せます。保存せず戻る場合は右のボタンを使ってください)"),
			SelectedCardIds.Num(), RequiredDeckSize, RequiredDeckSize);
	StatusText->SetText(FText::FromString(DeckStatus));

	// 上段: 今のデッキ。カードを追加した順ではなく、コストの低い順(同コストなら
	// カード名順)に並べる方が一覧性が良いため(「コスト順に並ぶようにしてほしい」
	// というフィードバックへの対応)。CGCardFilterSort.cppのコストソートと同じ
	// 比較基準。
	DeckRowBox->ClearChildren();
	LastDeckCardIds = SelectedCardIds;
	LastDeckCardIds.Sort([](const FName& A, const FName& B)
	{
		FCGCardDef DefA, DefB;
		UCGCardDatabase::FindCard(A, DefA);
		UCGCardDatabase::FindCard(B, DefB);
		return DefA.Cost != DefB.Cost ? DefA.Cost < DefB.Cost : DefA.CardName < DefB.CardName;
	});
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

	// 下段: 検索/フィルタ/並び替え後のカード一覧。
	RefreshPoolList();

	SaveButton->SetIsEnabled(SelectedCardIds.Num() == RequiredDeckSize);
}

TArray<FCGCardDef> UCGDeckBuilderHUD::BuildFilteredSortedPool() const
{
	return CGCardFilterSort::BuildFilteredSortedList(UCGCardDatabase::GetBuildableCards(), FilterSortState);
}

void UCGDeckBuilderHUD::RefreshPoolList()
{
	PoolListContainer->ClearChildren();
	LastPoolCardIds.Reset();

	const TArray<FCGCardDef> Filtered = BuildFilteredSortedPool();
	const float DisplayScale = bCompactPoolDisplay ? PoolDisplayScaleCompact : PoolDisplayScaleNormal;

	if (Filtered.Num() == 0)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PoolEmptyText"));
		EmptyText->SetText(FText::FromString(TEXT("条件に合うカードがありません")));
		if (UVerticalBoxSlot* EmptyTextSlot = PoolListContainer->AddChildToVerticalBox(EmptyText))
		{
			EmptyTextSlot->SetHorizontalAlignment(HAlign_Center);
			EmptyTextSlot->SetPadding(FMargin(0.f, 16.f));
		}
		return;
	}

	// 種族順のときだけ見出し付きでグルーピングする(コスト順/名前順はグルーピングの
	// 単位として意味が薄いため、単一のWrapBoxのまま並べる)。
	if (FilterSortState.SortMode != ECGCardSortMode::Tribe)
	{
		UWrapBox* Wrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("PoolWrap"));
		Wrap->SetInnerSlotPadding(FVector2D(0.f, CardGap * 2.f));
		PoolListContainer->AddChildToVerticalBox(Wrap);

		for (const FCGCardDef& Def : Filtered)
		{
			LastPoolCardIds.Add(Def.CardId);
			AddPoolCardToWrap(Wrap, Def, LastPoolCardIds.Num() - 1);
		}
		return;
	}

	FString CurrentGroupKey;
	bool bHasCurrentGroup = false;
	UWrapBox* CurrentWrap = nullptr;
	int32 GroupIndex = 0;

	for (const FCGCardDef& Def : Filtered)
	{
		const FString GroupKey = Def.Tribe.IsEmpty() ? TEXT("(種族未設定)") : Def.Tribe;
		if (!bHasCurrentGroup || GroupKey != CurrentGroupKey)
		{
			CurrentGroupKey = GroupKey;
			bHasCurrentGroup = true;
			++GroupIndex;

			UTextBlock* GroupHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				*FString::Printf(TEXT("PoolGroupHeader_%d"), GroupIndex));
			GroupHeader->SetText(FText::FromString(GroupKey));
			GroupHeader->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
			GroupHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.8f, 0.6f)));
			if (UVerticalBoxSlot* HeaderSlot = PoolListContainer->AddChildToVerticalBox(GroupHeader))
			{
				HeaderSlot->SetPadding(FMargin(4.f, 10.f, 4.f, 2.f));
			}

			CurrentWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(),
				*FString::Printf(TEXT("PoolGroupWrap_%d"), GroupIndex));
			CurrentWrap->SetInnerSlotPadding(FVector2D(0.f, CardGap * 2.f));
			PoolListContainer->AddChildToVerticalBox(CurrentWrap);
		}

		LastPoolCardIds.Add(Def.CardId);
		AddPoolCardToWrap(CurrentWrap, Def, LastPoolCardIds.Num() - 1);
	}
}

void UCGDeckBuilderHUD::AddPoolCardToWrap(UWrapBox* Wrap, const FCGCardDef& Def, int32 SlotIndex)
{
	// 重複3枚まで可(次期ルール)なので、「デッキ入り済み」ではなく「上限に達したか」
	// で暗くするかどうかを判定する。
	const int32 CopiesInDeck = Algo::CountIf(SelectedCardIds, [&Def](const FName& Id) { return Id == Def.CardId; });
	const bool bAtMaxCopies = CopiesInDeck >= MaxCopiesPerCard;
	const float DisplayScale = bCompactPoolDisplay ? PoolDisplayScaleCompact : PoolDisplayScaleNormal;

	UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
	SlotWidget->SlotIndex = SlotIndex;
	SlotWidget->SetCardData(Def);
	// カードの情報自体は変えず、これ以上追加できない(3枚入っている)ものを
	// 少し暗くするだけに留める(文字での注記はどの画面でも同じ情報を表示するという
	// 方針にそぐわないため)。
	SlotWidget->SetRenderOpacity(bAtMaxCopies ? 0.5f : 1.f);
	RegisterCardHoverPreview(SlotWidget);
	SlotWidget->OnSlotClicked.AddDynamic(this, &UCGDeckBuilderHUD::HandlePoolSlotClicked);

	UWidget* WrapChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, SlotWidget, DisplayScale);
	if (UWrapBoxSlot* CardSlot = Wrap->AddChildToWrapBox(WrapChild))
	{
		CardSlot->SetPadding(FMargin(CardGap));
	}
}

void UCGDeckBuilderHUD::UpdateFilterSortButtonVisuals()
{
	auto SetActive = [](UButton* Button, bool bActive)
	{
		if (Button)
		{
			// 選択中のボタンだけ不透明度を上げて示す(バトル画面の「攻撃できないユニットを
			// 少し暗くする」等と同じ、状態表現に色を使う既存の方針に合わせている)。
			Button->SetRenderOpacity(bActive ? 1.f : 0.55f);
		}
	};

	SetActive(FilterAllButton, !FilterSortState.TypeFilter.IsSet());
	SetActive(FilterUnitButton, FilterSortState.TypeFilter.IsSet() && FilterSortState.TypeFilter.GetValue() == ECGCardType::Unit);
	SetActive(FilterSpellButton, FilterSortState.TypeFilter.IsSet() && FilterSortState.TypeFilter.GetValue() == ECGCardType::Spell);

	SetActive(FilterColorAllButton, !FilterSortState.ColorFilter.IsSet());
	SetActive(FilterColorRedButton, FilterSortState.ColorFilter == ECGColor::Red);
	SetActive(FilterColorOrangeButton, FilterSortState.ColorFilter == ECGColor::Orange);
	SetActive(FilterColorGreenButton, FilterSortState.ColorFilter == ECGColor::Green);
	SetActive(FilterColorBlueButton, FilterSortState.ColorFilter == ECGColor::Blue);
	SetActive(FilterColorPurpleButton, FilterSortState.ColorFilter == ECGColor::Purple);
	SetActive(FilterColorNoneButton, FilterSortState.ColorFilter == ECGColor::None);

	SetActive(SortCostButton, FilterSortState.SortMode == ECGCardSortMode::Cost);
	SetActive(SortNameButton, FilterSortState.SortMode == ECGCardSortMode::Name);
	SetActive(SortTribeButton, FilterSortState.SortMode == ECGCardSortMode::Tribe);

	SetActive(CompactToggleButton, bCompactPoolDisplay);
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
	// 同名カードは3枚まで重複可(次期ルール)。既に上限枚まで入っている場合は
	// タップしても何もしない。デッキが25枚に達している場合も同様。
	const int32 CopiesInDeck = Algo::CountIf(SelectedCardIds, [&CardId](const FName& Id) { return Id == CardId; });
	if (CopiesInDeck < MaxCopiesPerCard && SelectedCardIds.Num() < RequiredDeckSize)
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
		GI->SaveDeckAs(DeckNameBox->GetText().ToString(), SelectedCardIds);
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

void UCGDeckBuilderHUD::HandleSearchTextChanged(const FText& NewText)
{
	FilterSortState.SearchQuery = NewText.ToString();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterAllClicked()
{
	FilterSortState.TypeFilter.Reset();
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterUnitClicked()
{
	FilterSortState.TypeFilter = ECGCardType::Unit;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterSpellClicked()
{
	FilterSortState.TypeFilter = ECGCardType::Spell;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterColorAllClicked()
{
	FilterSortState.ColorFilter.Reset();
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterColorRedClicked()
{
	FilterSortState.ColorFilter = ECGColor::Red;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterColorOrangeClicked()
{
	FilterSortState.ColorFilter = ECGColor::Orange;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterColorGreenClicked()
{
	FilterSortState.ColorFilter = ECGColor::Green;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterColorBlueClicked()
{
	FilterSortState.ColorFilter = ECGColor::Blue;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterColorPurpleClicked()
{
	FilterSortState.ColorFilter = ECGColor::Purple;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleFilterColorNoneClicked()
{
	FilterSortState.ColorFilter = ECGColor::None;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleSortCostClicked()
{
	FilterSortState.SortMode = ECGCardSortMode::Cost;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleSortNameClicked()
{
	FilterSortState.SortMode = ECGCardSortMode::Name;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleSortTribeClicked()
{
	FilterSortState.SortMode = ECGCardSortMode::Tribe;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}

void UCGDeckBuilderHUD::HandleCompactToggleClicked()
{
	bCompactPoolDisplay = !bCompactPoolDisplay;
	UpdateFilterSortButtonVisuals();
	RefreshPoolList();
}
