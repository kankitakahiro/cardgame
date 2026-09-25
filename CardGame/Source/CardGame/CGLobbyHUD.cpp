#include "CGLobbyHUD.h"
#include "CardGame.h"
#include "CGCardSlotWidget.h"
#include "CGCardDatabase.h"
#include "CGGameInstance.h"
#include "CGDeckListRowWidget.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Engine/Engine.h"

namespace
{
	// バトル画面は既存の唯一の対戦レベルをそのまま使う(デッキ構築を経ない
	// 直接PIEも引き続きできるよう、レベル名・パスは変更していない)。
	const TCHAR* BattleLevelPath = TEXT("/Game/CardGame/Maps/L_Card_GamePrototype");
	const TCHAR* DeckBuilderLevelPath = TEXT("/Game/CardGame/Maps/L_DeckBuilder");

	const float LobbyCardGap = 6.f;

	// カード図鑑の一覧表示縮小率。全画面レイヤーで表示スペースに余裕があるため
	// デッキ構築画面の一覧と同じ値にしている(docs/architecture.md「カードUIの設計」)。
	constexpr float CodexDisplayScaleNormal = 0.6f;
	constexpr float CodexDisplayScaleCompact = 0.4f;

	// 色ごとの基本デッキ(docs/next-ruleset-cards-v1.md「サンプルデッキ」)の表示名。デッキ選択画面で
	// 「選択」すると、そのままGameInstance::SavedDecksへこの名前で保存される
	// (UCGGameInstance::SaveDeckAs、CGLobbyHUD::HandleDeckSelectRowSelected参照)。
	// 行のUCGDeckListRowWidgetにはSlotIndexとして負のインデックス(-1-配列番号)を
	// 持たせ、通常の保存済みデッキ(0以上)と区別する。
	const TArray<TPair<ECGColor, FString>>& GetBasicDeckColors()
	{
		static const TArray<TPair<ECGColor, FString>> Colors = {
			{ ECGColor::Red,    TEXT("赤(基本)") },
			{ ECGColor::Orange, TEXT("橙(基本)") },
			{ ECGColor::Green,  TEXT("緑(基本)") },
			{ ECGColor::Blue,   TEXT("青(基本)") },
			{ ECGColor::Purple, TEXT("紫(基本)") },
		};
		return Colors;
	}

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

	// カード図鑑の検索/フィルタ/並び替え/表示切り替え/閉じるボタン用の小さめの
	// ボタン(UCGDeckBuilderHUD::MakeSmallToggleButtonと同じ考え方)。
	UButton* MakeSmallButton(UWidgetTree* WidgetTree, const TCHAR* Name, const FString& Label, UHorizontalBox* Root)
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

	// 「見出し + (現在の選択の情報表示 + 選択ボタン)」の1セクションを作る
	// (docs/architecture.md「ロビーのモーダル構成」参照)。対戦準備画面の
	// 対戦相手/自分、オンライン対戦画面の自分、の3箇所で使う共通部品。
	// 情報表示のテキストは呼び出し側がRefreshXxxSummary()で都度更新する。
	UTextBlock* AddDeckSummarySection(UWidgetTree* WidgetTree, UVerticalBox* Root, const TCHAR* HeadingName,
		const FString& HeadingText, const TCHAR* RowName, const TCHAR* TextName, const TCHAR* ButtonName, UButton*& OutSelectButton)
	{
		UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), HeadingName);
		Heading->SetText(FText::FromString(HeadingText));
		Heading->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
		if (UVerticalBoxSlot* HeadingSlot = Root->AddChildToVerticalBox(Heading))
		{
			HeadingSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), RowName);
		if (UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}

		UTextBlock* SummaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
		SummaryText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16));
		if (UHorizontalBoxSlot* SummarySlot = Row->AddChildToHorizontalBox(SummaryText))
		{
			SummarySlot->SetVerticalAlignment(VAlign_Center);
			SummarySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		OutSelectButton = MakeSmallButton(WidgetTree, ButtonName, TEXT("選択"), Row);
		return SummaryText;
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

	// 「デッキ選択」「対戦相手デッキ」は単体ボタンとしては廃止し、「対戦開始」を
	// 押した後に開く`BattleSetupLayer`(デッキ選択画面)の中でまとめて選ぶ
	// (docs/architecture.md「ロビーのモーダル構成」参照)。
	UButton* StartBattleButton = MakeMenuButton(WidgetTree, TEXT("StartBattleButton"), TEXT("対戦開始"), Root);
	StartBattleButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleOpenBattleSetupClicked);

	UButton* CodexButton = MakeMenuButton(WidgetTree, TEXT("CodexButton"), TEXT("カード図鑑"), Root);
	CodexButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleOpenCodexClicked);

	UButton* HowToPlayButton = MakeMenuButton(WidgetTree, TEXT("HowToPlayButton"), TEXT("遊び方"), Root);
	HowToPlayButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleOpenHowToPlayClicked);

	UButton* OnlineButton = MakeMenuButton(WidgetTree, TEXT("OnlineButton"), TEXT("オンライン対戦"), Root);
	OnlineButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleOpenOnlineClicked);

	// カード図鑑: レベル遷移を伴わない全画面レイヤー(docs/architecture.md
	// 「カードUIの設計」)。カードが増えても検索/フィルタ/並び替えで見つけやすいよう、
	// デッキ構築画面と同じ仕組みをここでも使う(CGCardFilterSort)。
	CodexLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CodexLayer"));
	CodexLayer->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.98f));
	CodexLayer->SetHorizontalAlignment(HAlign_Fill);
	CodexLayer->SetVerticalAlignment(VAlign_Fill);
	CodexLayer->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* CodexRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CodexRoot"));
	CodexLayer->AddChild(CodexRoot);

	UHorizontalBox* CodexHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CodexHeaderRow"));
	if (UVerticalBoxSlot* HeaderRowSlot = CodexRoot->AddChildToVerticalBox(CodexHeaderRow))
	{
		HeaderRowSlot->SetPadding(FMargin(24.f, 16.f, 24.f, 4.f));
	}

	UTextBlock* CodexTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CodexTitle"));
	CodexTitle->SetText(FText::FromString(TEXT("カード図鑑")));
	CodexTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	if (UHorizontalBoxSlot* CodexTitleSlot = CodexHeaderRow->AddChildToHorizontalBox(CodexTitle))
	{
		CodexTitleSlot->SetVerticalAlignment(VAlign_Center);
		CodexTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UButton* CloseCodexButton = MakeSmallButton(WidgetTree, TEXT("CloseCodexButton"), TEXT("閉じる"), CodexHeaderRow);
	CloseCodexButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCloseCodexClicked);

	// 検索/フィルタ/並び替え/表示切り替えの操作列(UCGDeckBuilderHUDと同じ構成)。
	UHorizontalBox* CodexControlsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CodexControlsRow"));
	if (UVerticalBoxSlot* ControlsRowSlot = CodexRoot->AddChildToVerticalBox(CodexControlsRow))
	{
		ControlsRowSlot->SetPadding(FMargin(24.f, 2.f));
	}

	USizeBox* CodexSearchSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CodexSearchSizeBox"));
	CodexSearchSizeBox->SetWidthOverride(220.f);
	CodexSearchSizeBox->SetHeightOverride(34.f);

	CodexSearchBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("CodexSearchBox"));
	CodexSearchBox->SetHintText(FText::FromString(TEXT("カード名で検索")));
	CodexSearchBox->OnTextChanged.AddDynamic(this, &UCGLobbyHUD::HandleCodexSearchTextChanged);
	if (USizeBoxSlot* CodexSearchSlot = Cast<USizeBoxSlot>(CodexSearchSizeBox->AddChild(CodexSearchBox)))
	{
		CodexSearchSlot->SetHorizontalAlignment(HAlign_Fill);
		CodexSearchSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UHorizontalBoxSlot* CodexSearchRowSlot = CodexControlsRow->AddChildToHorizontalBox(CodexSearchSizeBox))
	{
		CodexSearchRowSlot->SetPadding(FMargin(3.f, 0.f, 14.f, 0.f));
		CodexSearchRowSlot->SetVerticalAlignment(VAlign_Center);
	}

	CodexFilterAllButton = MakeSmallButton(WidgetTree, TEXT("CodexFilterAllButton"), TEXT("全て"), CodexControlsRow);
	CodexFilterAllButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCodexFilterAllClicked);
	CodexFilterUnitButton = MakeSmallButton(WidgetTree, TEXT("CodexFilterUnitButton"), TEXT("Unit"), CodexControlsRow);
	CodexFilterUnitButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCodexFilterUnitClicked);
	CodexFilterSpellButton = MakeSmallButton(WidgetTree, TEXT("CodexFilterSpellButton"), TEXT("Spell"), CodexControlsRow);
	CodexFilterSpellButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCodexFilterSpellClicked);

	CodexSortCostButton = MakeSmallButton(WidgetTree, TEXT("CodexSortCostButton"), TEXT("コスト順"), CodexControlsRow);
	CodexSortCostButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCodexSortCostClicked);
	CodexSortNameButton = MakeSmallButton(WidgetTree, TEXT("CodexSortNameButton"), TEXT("名前順"), CodexControlsRow);
	CodexSortNameButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCodexSortNameClicked);
	CodexSortTribeButton = MakeSmallButton(WidgetTree, TEXT("CodexSortTribeButton"), TEXT("種族順"), CodexControlsRow);
	CodexSortTribeButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCodexSortTribeClicked);

	CodexCompactToggleButton = MakeSmallButton(WidgetTree, TEXT("CodexCompactToggleButton"), TEXT("コンパクト表示"), CodexControlsRow);
	CodexCompactToggleButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCodexCompactToggleClicked);

	// 一覧本体。全画面レイヤーなので、ヘッダー/操作列を除いた残りの縦スペースを
	// そのまま使い(Fill)、その中で縦スクロールする。
	UScrollBox* CodexScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CodexScroll"));
	CodexScroll->SetOrientation(EOrientation::Orient_Vertical);

	CodexListContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CodexListContainer"));
	CodexScroll->AddChild(CodexListContainer);

	if (UVerticalBoxSlot* CodexScrollSlot = CodexRoot->AddChildToVerticalBox(CodexScroll))
	{
		CodexScrollSlot->SetPadding(FMargin(24.f, 8.f));
		CodexScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 遊び方: 対戦ルールを説明する、レベル遷移を伴わない全画面レイヤー
	// (「ルールを説明するページ」が欲しいというフィードバックのため)。中身は
	// 固定テキストなので、カード図鑑と違い都度の再構築(RefreshXxxList相当)は不要。
	HowToPlayLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HowToPlayLayer"));
	HowToPlayLayer->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.98f));
	HowToPlayLayer->SetHorizontalAlignment(HAlign_Fill);
	HowToPlayLayer->SetVerticalAlignment(VAlign_Fill);
	HowToPlayLayer->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* HowToPlayRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HowToPlayRoot"));
	HowToPlayLayer->AddChild(HowToPlayRoot);

	UHorizontalBox* HowToPlayHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HowToPlayHeaderRow"));
	if (UVerticalBoxSlot* HowToPlayHeaderRowSlot = HowToPlayRoot->AddChildToVerticalBox(HowToPlayHeaderRow))
	{
		HowToPlayHeaderRowSlot->SetPadding(FMargin(24.f, 16.f, 24.f, 4.f));
	}

	UTextBlock* HowToPlayTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HowToPlayTitle"));
	HowToPlayTitle->SetText(FText::FromString(TEXT("遊び方")));
	HowToPlayTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	if (UHorizontalBoxSlot* HowToPlayTitleSlot = HowToPlayHeaderRow->AddChildToHorizontalBox(HowToPlayTitle))
	{
		HowToPlayTitleSlot->SetVerticalAlignment(VAlign_Center);
		HowToPlayTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UButton* CloseHowToPlayButton = MakeSmallButton(WidgetTree, TEXT("CloseHowToPlayButton"), TEXT("閉じる"), HowToPlayHeaderRow);
	CloseHowToPlayButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCloseHowToPlayClicked);

	// 本文はスクロール可能な固定幅の読み物として中央に置く(横幅いっぱいに文章が
	// 伸びると読みにくいため)。
	USizeBox* HowToPlayBodySizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HowToPlayBodySizeBox"));
	HowToPlayBodySizeBox->SetMaxDesiredWidth(820.f);

	UScrollBox* HowToPlayScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("HowToPlayScroll"));
	HowToPlayScroll->SetOrientation(EOrientation::Orient_Vertical);

	UVerticalBox* HowToPlayContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HowToPlayContent"));
	HowToPlayScroll->AddChild(HowToPlayContent);

	if (USizeBoxSlot* HowToPlayScrollSlot = Cast<USizeBoxSlot>(HowToPlayBodySizeBox->AddChild(HowToPlayScroll)))
	{
		HowToPlayScrollSlot->SetHorizontalAlignment(HAlign_Fill);
		HowToPlayScrollSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (UVerticalBoxSlot* HowToPlayBodySlot = HowToPlayRoot->AddChildToVerticalBox(HowToPlayBodySizeBox))
	{
		// ScrollBoxの既定はHAlign_Fillのため、そのままだと中のSizeBoxがビューポート幅
		// いっぱいに引き伸ばされ、SetMaxDesiredWidth()が効かない
		// (USizeBox::OnArrangeChildren()は自分に割り当てられた幅をそのまま子へ渡すため。
		// docs/architecture.md「カードUIの設計」で扱った落とし穴と同種)。Centerにする
		// ことで、割り当て幅自体をSizeBoxの希望サイズ(=MaxDesiredWidthで制限された幅)
		// に基づかせている。
		HowToPlayBodySlot->SetHorizontalAlignment(HAlign_Center);
		HowToPlayBodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	AddHowToPlaySection(HowToPlayContent, TEXT("対戦の基本"),
		TEXT("1対1で対戦します(あなたが先攻/後攻いずれかになるかはランダム)。"
			"初期ライフは20、初期手札は5枚、手札は10枚を超えるとドローした瞬間に捨て札へ送られます。"));

	AddHowToPlaySection(HowToPlayContent, TEXT("マナとマーケット"),
		TEXT("マナはカードのプレイと購入に共通で使うリソースです。ターン開始のたびに最大値が1増え(上限10)、その分だけ全回復します。"
			"マーケットには常に5枚のカードが公開されており、購入やプレイで減った分は自動で補充されます。"
			"カードを購入すると、そのカードを手札に加えるか、山札の一番下に送るかをその場で選べます。手札が上限(10枚)に近いときや、"
			"今すぐ使わないカードを後で引きたいときは山札の下へ送るのが有効です。"));

	AddHowToPlaySection(HowToPlayContent, TEXT("ターンの流れ"),
		TEXT("① ドロー: 1枚引き、マナが最大値+1され全回復し、自分の場のユニットが再び攻撃できるようになります。\n"
			"② メイン: カードのプレイ・マーケットからの購入・ユニットでの攻撃を、好きな順番・好きな回数だけ行えます。\n"
			"③ エンド: ターン終了時の効果を解決して相手の番に交代します。"));

	AddHowToPlaySection(HowToPlayContent, TEXT("カードの種類"),
		TEXT("Unit(ユニット): 場に残るカードです。登場時効果・場にいる間ずっと効く効果・死亡時効果を持つことがあります。\n"
			"Spell(スペル): プレイした瞬間に効果を発揮し、そのまま捨て札へ送られます。"));

	AddHowToPlaySection(HowToPlayContent, TEXT("キーワード"),
		TEXT("速攻: このユニットは登場したターンでも攻撃できます(通常は登場した次の自分の番からしか攻撃できません)。\n"
			"守護: このユニットが場にいる間、相手からの攻撃は必ずこのユニットが受けます。"));

	AddHowToPlaySection(HowToPlayContent, TEXT("選ぶ操作が必要な効果"),
		TEXT("「手札を1枚捨てる」「ダメージを与える相手を選ぶ」「山札の一番上を見て残すか送るか選ぶ」など、選択が必要なカード効果や攻撃対象の指定は、"
			"画面に表示されるカードやボタンを直接クリックして選びます(対戦相手は選択が必要な場面でもその場で自動的に決めます)。"));

	AddHowToPlaySection(HowToPlayContent, TEXT("勝敗"),
		TEXT("相手のライフを0以下にすると勝利です。自分のライフが0以下になった場合、またはカードを引こうとしたときに山札が0枚だった場合は敗北になります。"));

	// 自分のデッキ一覧: 保存済みデッキ(GameInstance::SavedDecks)+基本デッキから
	// 選ぶ全画面レイヤー(カード図鑑・遊び方と同じ方式)。「デッキ選択(親)」
	// (BattleSetupLayer)または「オンライン対戦」(OnlineLayer)の「選択」ボタンから
	// 開かれる共通画面で、この画面自体に「閉じる」は置かない。行を選ぶと
	// PendingReturnLayerへ自動的に戻る(docs/architecture.md「ロビーのモーダル構成」参照)。
	// 中身は開くたびに作り直す(RefreshDeckSelectList、カード図鑑と同じ考え方)。
	DeckSelectLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeckSelectLayer"));
	DeckSelectLayer->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.98f));
	DeckSelectLayer->SetHorizontalAlignment(HAlign_Fill);
	DeckSelectLayer->SetVerticalAlignment(VAlign_Fill);
	DeckSelectLayer->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* DeckSelectRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DeckSelectRoot"));
	DeckSelectLayer->AddChild(DeckSelectRoot);

	UHorizontalBox* DeckSelectHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DeckSelectHeaderRow"));
	if (UVerticalBoxSlot* DeckSelectHeaderRowSlot = DeckSelectRoot->AddChildToVerticalBox(DeckSelectHeaderRow))
	{
		DeckSelectHeaderRowSlot->SetPadding(FMargin(24.f, 16.f, 24.f, 4.f));
	}

	UTextBlock* DeckSelectTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckSelectTitle"));
	DeckSelectTitle->SetText(FText::FromString(TEXT("自分のデッキ")));
	DeckSelectTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	if (UHorizontalBoxSlot* DeckSelectTitleSlot = DeckSelectHeaderRow->AddChildToHorizontalBox(DeckSelectTitle))
	{
		DeckSelectTitleSlot->SetVerticalAlignment(VAlign_Center);
		DeckSelectTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 一覧本体は「遊び方」と同じく固定幅で中央に置く(1行がリスト全体の横幅
	// いっぱいに伸びると、名前とボタンの間が間延びして見づらいため)。
	USizeBox* DeckSelectBodySizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DeckSelectBodySizeBox"));
	DeckSelectBodySizeBox->SetMaxDesiredWidth(600.f);

	UScrollBox* DeckSelectScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DeckSelectScroll"));
	DeckSelectScroll->SetOrientation(EOrientation::Orient_Vertical);

	DeckSelectListContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DeckSelectListContainer"));
	DeckSelectScroll->AddChild(DeckSelectListContainer);

	if (USizeBoxSlot* DeckSelectScrollSlot = Cast<USizeBoxSlot>(DeckSelectBodySizeBox->AddChild(DeckSelectScroll)))
	{
		DeckSelectScrollSlot->SetHorizontalAlignment(HAlign_Fill);
		DeckSelectScrollSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (UVerticalBoxSlot* DeckSelectBodySlot = DeckSelectRoot->AddChildToVerticalBox(DeckSelectBodySizeBox))
	{
		// USizeBoxのMaxDesiredWidthを効かせるためHAlign_Center(HowToPlayBodySizeBox
		// と同じ理由。上記コメント参照)。
		DeckSelectBodySlot->SetHorizontalAlignment(HAlign_Center);
		DeckSelectBodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DeckSelectBodySlot->SetPadding(FMargin(24.f, 8.f));
	}

	// 対戦相手デッキ一覧: 自分のデッキ一覧と同じ全画面モーダルレイヤー方式
	// (「CPUと対戦するときに相手のデッキを選べるようにしてほしい」という
	// フィードバックへの対応)。保存済みデッキの概念が無い分、構造は自分のデッキ
	// 一覧の簡略版になっている。「デッキ選択(親)」からのみ開かれ、この画面自体に
	// 「閉じる」は置かない(行を選ぶとBattleSetupLayerへ自動的に戻る)。
	AIDeckSelectLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AIDeckSelectLayer"));
	AIDeckSelectLayer->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.98f));
	AIDeckSelectLayer->SetHorizontalAlignment(HAlign_Fill);
	AIDeckSelectLayer->SetVerticalAlignment(VAlign_Fill);
	AIDeckSelectLayer->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* AIDeckSelectRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AIDeckSelectRoot"));
	AIDeckSelectLayer->AddChild(AIDeckSelectRoot);

	UHorizontalBox* AIDeckSelectHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AIDeckSelectHeaderRow"));
	if (UVerticalBoxSlot* AIDeckSelectHeaderRowSlot = AIDeckSelectRoot->AddChildToVerticalBox(AIDeckSelectHeaderRow))
	{
		AIDeckSelectHeaderRowSlot->SetPadding(FMargin(24.f, 16.f, 24.f, 4.f));
	}

	UTextBlock* AIDeckSelectTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AIDeckSelectTitle"));
	AIDeckSelectTitle->SetText(FText::FromString(TEXT("対戦相手のデッキ")));
	AIDeckSelectTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	if (UHorizontalBoxSlot* AIDeckSelectTitleSlot = AIDeckSelectHeaderRow->AddChildToHorizontalBox(AIDeckSelectTitle))
	{
		AIDeckSelectTitleSlot->SetVerticalAlignment(VAlign_Center);
		AIDeckSelectTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	USizeBox* AIDeckSelectBodySizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AIDeckSelectBodySizeBox"));
	AIDeckSelectBodySizeBox->SetMaxDesiredWidth(600.f);

	UScrollBox* AIDeckSelectScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("AIDeckSelectScroll"));
	AIDeckSelectScroll->SetOrientation(EOrientation::Orient_Vertical);

	AIDeckSelectListContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AIDeckSelectListContainer"));
	AIDeckSelectScroll->AddChild(AIDeckSelectListContainer);

	if (USizeBoxSlot* AIDeckSelectScrollSlot = Cast<USizeBoxSlot>(AIDeckSelectBodySizeBox->AddChild(AIDeckSelectScroll)))
	{
		AIDeckSelectScrollSlot->SetHorizontalAlignment(HAlign_Fill);
		AIDeckSelectScrollSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (UVerticalBoxSlot* AIDeckSelectBodySlot = AIDeckSelectRoot->AddChildToVerticalBox(AIDeckSelectBodySizeBox))
	{
		AIDeckSelectBodySlot->SetHorizontalAlignment(HAlign_Center);
		AIDeckSelectBodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AIDeckSelectBodySlot->SetPadding(FMargin(24.f, 8.f));
	}

	// デッキ選択(親)画面: 「対戦開始」ボタンから開く。対戦相手・自分それぞれ
	// 「今選ばれているデッキ」の情報表示のみを持ち、一覧はここには出さない
	// (「選択」ボタンを押すとデッキを実際に選択できる画面(一覧)に遷移し、
	// 選択するとこの画面に戻ってきて選択されたデッキが分かるようにしてほしい、
	// というフィードバックへの対応。docs/architecture.md「ロビーのモーダル構成」参照)。
	BattleSetupLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattleSetupLayer"));
	BattleSetupLayer->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.98f));
	BattleSetupLayer->SetHorizontalAlignment(HAlign_Fill);
	BattleSetupLayer->SetVerticalAlignment(VAlign_Fill);
	BattleSetupLayer->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* BattleSetupRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BattleSetupRoot"));
	BattleSetupLayer->AddChild(BattleSetupRoot);

	UHorizontalBox* BattleSetupHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BattleSetupHeaderRow"));
	if (UVerticalBoxSlot* BattleSetupHeaderRowSlot = BattleSetupRoot->AddChildToVerticalBox(BattleSetupHeaderRow))
	{
		BattleSetupHeaderRowSlot->SetPadding(FMargin(24.f, 16.f, 24.f, 4.f));
	}

	UTextBlock* BattleSetupTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleSetupTitle"));
	BattleSetupTitle->SetText(FText::FromString(TEXT("デッキ選択")));
	BattleSetupTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	if (UHorizontalBoxSlot* BattleSetupTitleSlot = BattleSetupHeaderRow->AddChildToHorizontalBox(BattleSetupTitle))
	{
		BattleSetupTitleSlot->SetVerticalAlignment(VAlign_Center);
		BattleSetupTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UButton* CloseBattleSetupButton = MakeSmallButton(WidgetTree, TEXT("CloseBattleSetupButton"), TEXT("閉じる"), BattleSetupHeaderRow);
	CloseBattleSetupButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCloseBattleSetupClicked);

	// 本体は「遊び方」「一覧画面」と同じく固定幅で中央に置く。
	USizeBox* BattleSetupBodySizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BattleSetupBodySizeBox"));
	BattleSetupBodySizeBox->SetMaxDesiredWidth(600.f);

	UVerticalBox* BattleSetupBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BattleSetupBody"));
	if (USizeBoxSlot* BattleSetupBodyInnerSlot = Cast<USizeBoxSlot>(BattleSetupBodySizeBox->AddChild(BattleSetupBody)))
	{
		BattleSetupBodyInnerSlot->SetHorizontalAlignment(HAlign_Fill);
		BattleSetupBodyInnerSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (UVerticalBoxSlot* BattleSetupBodySlot = BattleSetupRoot->AddChildToVerticalBox(BattleSetupBodySizeBox))
	{
		BattleSetupBodySlot->SetHorizontalAlignment(HAlign_Center);
		BattleSetupBodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BattleSetupBodySlot->SetPadding(FMargin(24.f, 8.f));
	}

	UButton* BattleSetupSelectAIButton = nullptr;
	BattleSetupAIDeckSummaryText = AddDeckSummarySection(WidgetTree, BattleSetupBody, TEXT("BattleSetupAIHeading"),
		TEXT("対戦相手のデッキ"), TEXT("BattleSetupAIRow"), TEXT("BattleSetupAIDeckSummaryText"),
		TEXT("BattleSetupSelectAIButton"), BattleSetupSelectAIButton);
	BattleSetupSelectAIButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleSelectAIDeckClicked);

	UButton* BattleSetupSelectMyButton = nullptr;
	BattleSetupMyDeckSummaryText = AddDeckSummarySection(WidgetTree, BattleSetupBody, TEXT("BattleSetupMyHeading"),
		TEXT("自分のデッキ"), TEXT("BattleSetupMyRow"), TEXT("BattleSetupMyDeckSummaryText"),
		TEXT("BattleSetupSelectMyButton"), BattleSetupSelectMyButton);
	BattleSetupSelectMyButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleSelectMyDeckFromSummaryClicked);

	UButton* ConfirmBattleSetupButton = MakeMenuButton(WidgetTree, TEXT("ConfirmBattleSetupButton"), TEXT("決定"), BattleSetupBody);
	ConfirmBattleSetupButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleConfirmBattleSetupClicked);

	// オンライン対戦(ホスト/参加): カード図鑑・遊び方・デッキ選択と同じ全画面
	// モーダルレイヤー方式(docs/online-play-design.md「ロビー画面の変更点」)。
	OnlineLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OnlineLayer"));
	OnlineLayer->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.98f));
	OnlineLayer->SetHorizontalAlignment(HAlign_Fill);
	OnlineLayer->SetVerticalAlignment(VAlign_Fill);
	OnlineLayer->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* OnlineRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OnlineRoot"));
	OnlineLayer->AddChild(OnlineRoot);

	UHorizontalBox* OnlineHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("OnlineHeaderRow"));
	if (UVerticalBoxSlot* OnlineHeaderRowSlot = OnlineRoot->AddChildToVerticalBox(OnlineHeaderRow))
	{
		OnlineHeaderRowSlot->SetPadding(FMargin(24.f, 16.f, 24.f, 4.f));
	}

	UTextBlock* OnlineTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OnlineTitle"));
	OnlineTitle->SetText(FText::FromString(TEXT("オンライン対戦")));
	OnlineTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28));
	if (UHorizontalBoxSlot* OnlineTitleSlot = OnlineHeaderRow->AddChildToHorizontalBox(OnlineTitle))
	{
		OnlineTitleSlot->SetVerticalAlignment(VAlign_Center);
		OnlineTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UButton* CloseOnlineButton = MakeSmallButton(WidgetTree, TEXT("CloseOnlineButton"), TEXT("閉じる"), OnlineHeaderRow);
	CloseOnlineButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleCloseOnlineClicked);

	// 本体は「遊び方」「デッキ選択」と同じく固定幅で中央に置く。
	USizeBox* OnlineBodySizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("OnlineBodySizeBox"));
	OnlineBodySizeBox->SetMaxDesiredWidth(600.f);

	UVerticalBox* OnlineBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OnlineBody"));
	if (USizeBoxSlot* OnlineBodyInnerSlot = Cast<USizeBoxSlot>(OnlineBodySizeBox->AddChild(OnlineBody)))
	{
		OnlineBodyInnerSlot->SetHorizontalAlignment(HAlign_Fill);
		OnlineBodyInnerSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (UVerticalBoxSlot* OnlineBodySlot = OnlineRoot->AddChildToVerticalBox(OnlineBodySizeBox))
	{
		OnlineBodySlot->SetHorizontalAlignment(HAlign_Center);
		OnlineBodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		OnlineBodySlot->SetPadding(FMargin(24.f, 8.f));
	}

	// 自分のデッキ(「オンライン対戦でも同じような画面遷移がいい」という
	// フィードバックへの対応)。対戦相手は人間のためAI側の選択は無い。「選択」で
	// 自分のデッキ一覧(DeckSelectLayer、デッキ選択(親)と共通)へ遷移し、
	// 選ぶとこの画面へ戻る。
	UButton* OnlineSelectMyDeckButton = nullptr;
	OnlineMyDeckSummaryText = AddDeckSummarySection(WidgetTree, OnlineBody, TEXT("OnlineMyDeckHeading"),
		TEXT("自分のデッキ"), TEXT("OnlineMyDeckRow"), TEXT("OnlineMyDeckSummaryText"),
		TEXT("OnlineSelectMyDeckButton"), OnlineSelectMyDeckButton);
	OnlineSelectMyDeckButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleSelectMyDeckFromOnlineClicked);

	// ホストする: 対戦レベルをリッスンサーバーとして開く。
	UButton* HostButton = MakeMenuButton(WidgetTree, TEXT("HostGameButton"), TEXT("ホストする"), OnlineBody);
	HostButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleHostGameClicked);

	// 自分のSteamID(ホストする側が友達に伝える用。docs/online-play-design.md
	// 「SteamSocketsによるインターネット越し接続」参照)。Steamが使えない
	// 環境では空のまま表示しない。
	YourSteamIdLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("YourSteamIdLabel"));
	YourSteamIdLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13));
	YourSteamIdLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.f)));
	YourSteamIdLabel->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* YourSteamIdLabelSlot = OnlineBody->AddChildToVerticalBox(YourSteamIdLabel))
	{
		YourSteamIdLabelSlot->SetHorizontalAlignment(HAlign_Center);
		YourSteamIdLabelSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	// SteamID自体は選択してコピーできるよう、読み取り専用のEditableTextBoxに
	// 入れる(「SteamIDをコピペできるようにしてほしい」というフィードバックへの対応)。
	YourSteamIdText = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("YourSteamIdText"));
	YourSteamIdText->SetIsReadOnly(true);
	YourSteamIdText->SetJustification(ETextJustify::Center);
	YourSteamIdText->SetVisibility(ESlateVisibility::Collapsed);
	USizeBox* YourSteamIdSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("YourSteamIdSizeBox"));
	YourSteamIdSizeBox->SetWidthOverride(320.f);
	if (USizeBoxSlot* YourSteamIdInnerSlot = Cast<USizeBoxSlot>(YourSteamIdSizeBox->AddChild(YourSteamIdText)))
	{
		YourSteamIdInnerSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	if (UVerticalBoxSlot* YourSteamIdSlot = OnlineBody->AddChildToVerticalBox(YourSteamIdSizeBox))
	{
		YourSteamIdSlot->SetHorizontalAlignment(HAlign_Center);
		YourSteamIdSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	}

	UTextBlock* JoinLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("JoinLabel"));
	JoinLabel->SetText(FText::FromString(TEXT("参加する(ホストのIPアドレス、またはSteamIDを入力)")));
	JoinLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16));
	if (UVerticalBoxSlot* JoinLabelSlot = OnlineBody->AddChildToVerticalBox(JoinLabel))
	{
		JoinLabelSlot->SetHorizontalAlignment(HAlign_Center);
		JoinLabelSlot->SetPadding(FMargin(0.f, 24.f, 0.f, 4.f));
	}

	UHorizontalBox* JoinRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("JoinRow"));
	if (UVerticalBoxSlot* JoinRowSlot = OnlineBody->AddChildToVerticalBox(JoinRow))
	{
		JoinRowSlot->SetHorizontalAlignment(HAlign_Center);
		JoinRowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 12.f));
	}

	USizeBox* JoinIPSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("JoinIPSizeBox"));
	JoinIPSizeBox->SetWidthOverride(220.f);
	JoinIPSizeBox->SetHeightOverride(40.f);

	JoinIPBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("JoinIPBox"));
	JoinIPBox->SetHintText(FText::FromString(TEXT("例: 127.0.0.1 または 76561198012345678")));
	if (USizeBoxSlot* JoinIPBoxSlot = Cast<USizeBoxSlot>(JoinIPSizeBox->AddChild(JoinIPBox)))
	{
		JoinIPBoxSlot->SetHorizontalAlignment(HAlign_Fill);
		JoinIPBoxSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UHorizontalBoxSlot* JoinIPRowSlot = JoinRow->AddChildToHorizontalBox(JoinIPSizeBox))
	{
		JoinIPRowSlot->SetVerticalAlignment(VAlign_Center);
		JoinIPRowSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	}

	UButton* JoinButton = MakeSmallButton(WidgetTree, TEXT("JoinGameButton"), TEXT("参加する"), JoinRow);
	JoinButton->OnClicked.AddDynamic(this, &UCGLobbyHUD::HandleJoinGameClicked);

	OnlineStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OnlineStatusText"));
	OnlineStatusText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14));
	OnlineStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
	if (UVerticalBoxSlot* OnlineStatusSlot = OnlineBody->AddChildToVerticalBox(OnlineStatusText))
	{
		OnlineStatusSlot->SetHorizontalAlignment(HAlign_Center);
		OnlineStatusSlot->SetPadding(FMargin(0.f, 8.f));
	}

	// カードのホバー拡大プレビューを最前面に出すための共通レイヤーをRootに重ねる
	// (UCGCardHostWidget::BuildRootOverlay、docs/architecture.md「ホバー拡大とZ順序」)。
	// カード図鑑・遊び方・デッキ選択・オンライン対戦の4つの全画面レイヤーは1つの
	// UOverlayにまとめてModalLayerとして渡すことで、図鑑の中のカードをホバーした
	// ときの拡大表示がパネルの下に隠れないようにする(CGGameHUDのChoiceModalLayerと
	// 同じ対策)。
	UOverlay* ModalStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("LobbyModalStack"));
	if (UOverlaySlot* CodexStackSlot = ModalStack->AddChildToOverlay(CodexLayer))
	{
		CodexStackSlot->SetHorizontalAlignment(HAlign_Fill);
		CodexStackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* HowToPlayStackSlot = ModalStack->AddChildToOverlay(HowToPlayLayer))
	{
		HowToPlayStackSlot->SetHorizontalAlignment(HAlign_Fill);
		HowToPlayStackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* DeckSelectStackSlot = ModalStack->AddChildToOverlay(DeckSelectLayer))
	{
		DeckSelectStackSlot->SetHorizontalAlignment(HAlign_Fill);
		DeckSelectStackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* AIDeckSelectStackSlot = ModalStack->AddChildToOverlay(AIDeckSelectLayer))
	{
		AIDeckSelectStackSlot->SetHorizontalAlignment(HAlign_Fill);
		AIDeckSelectStackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* BattleSetupStackSlot = ModalStack->AddChildToOverlay(BattleSetupLayer))
	{
		BattleSetupStackSlot->SetHorizontalAlignment(HAlign_Fill);
		BattleSetupStackSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* OnlineStackSlot = ModalStack->AddChildToOverlay(OnlineLayer))
	{
		OnlineStackSlot->SetHorizontalAlignment(HAlign_Fill);
		OnlineStackSlot->SetVerticalAlignment(VAlign_Fill);
	}

	WidgetTree->RootWidget = BuildRootOverlay(Root, ModalStack);

	UpdateCodexButtonVisuals();
}

void UCGLobbyHUD::HandleDeckBuilderClicked()
{
	UGameplayStatics::OpenLevel(this, FName(DeckBuilderLevelPath));
}

void UCGLobbyHUD::HandleOpenBattleSetupClicked()
{
	BattleSetupLayer->SetVisibility(ESlateVisibility::Visible);
	RefreshBattleSetupSummary();
}

void UCGLobbyHUD::HandleCloseBattleSetupClicked()
{
	BattleSetupLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void UCGLobbyHUD::HandleConfirmBattleSetupClicked()
{
	UGameplayStatics::OpenLevel(this, FName(BattleLevelPath));
}

void UCGLobbyHUD::HandleSelectAIDeckClicked()
{
	// BattleSetupLayerはModalStack内でAIDeckSelectLayerより後に追加されており
	// (=手前に描画される)、Collapsedにせず一覧側をVisibleにするだけだと
	// 一覧がBattleSetupLayerの裏に隠れて何も変わって見えない
	// (「選択ボタンを押しても画面が遷移しない」というフィードバックへの対応)。
	PendingReturnLayer = BattleSetupLayer;
	BattleSetupLayer->SetVisibility(ESlateVisibility::Collapsed);
	AIDeckSelectLayer->SetVisibility(ESlateVisibility::Visible);
	RefreshAIDeckSelectList();
}

void UCGLobbyHUD::HandleSelectMyDeckFromSummaryClicked()
{
	PendingReturnLayer = BattleSetupLayer;
	BattleSetupLayer->SetVisibility(ESlateVisibility::Collapsed);
	DeckSelectLayer->SetVisibility(ESlateVisibility::Visible);
	RefreshDeckSelectList();
}

void UCGLobbyHUD::HandleSelectMyDeckFromOnlineClicked()
{
	PendingReturnLayer = OnlineLayer;
	OnlineLayer->SetVisibility(ESlateVisibility::Collapsed);
	DeckSelectLayer->SetVisibility(ESlateVisibility::Visible);
	RefreshDeckSelectList();
}

UCGGameInstance* UCGLobbyHUD::GetCGGameInstance() const
{
	return GetWorld() ? Cast<UCGGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

void UCGLobbyHUD::RefreshBattleSetupSummary()
{
	if (BattleSetupAIDeckSummaryText)
	{
		BattleSetupAIDeckSummaryText->SetText(FText::FromString(GetAIDeckSummaryDisplayText()));
	}
	if (BattleSetupMyDeckSummaryText)
	{
		BattleSetupMyDeckSummaryText->SetText(FText::FromString(GetMyDeckSummaryDisplayText()));
	}
}

void UCGLobbyHUD::RefreshOnlineMyDeckSummary()
{
	if (OnlineMyDeckSummaryText)
	{
		OnlineMyDeckSummaryText->SetText(FText::FromString(GetMyDeckSummaryDisplayText()));
	}
}

FString UCGLobbyHUD::GetAIDeckSummaryDisplayText() const
{
	UCGGameInstance* GI = GetCGGameInstance();
	if (!GI)
	{
		return FString();
	}
	if (GI->SelectedAIOpponentColor != ECGColor::None)
	{
		const TArray<TPair<ECGColor, FString>>& BasicColors = GetBasicDeckColors();
		for (const TPair<ECGColor, FString>& Entry : BasicColors)
		{
			if (Entry.Key == GI->SelectedAIOpponentColor)
			{
				return FString::Printf(TEXT("%s (%d枚)"), *Entry.Value, UCGCardDatabase::GetBasicColorDeckCardIds(Entry.Key).Num());
			}
		}
	}
	return TEXT("ランダム(5色から抽選) (25枚)");
}

FString UCGLobbyHUD::GetMyDeckSummaryDisplayText() const
{
	UCGGameInstance* GI = GetCGGameInstance();
	if (!GI)
	{
		return FString();
	}
	return FString::Printf(TEXT("%s (%d枚)"), *GI->ActiveDeckName, GI->PlayerDeckCardIds.Num());
}

void UCGLobbyHUD::RefreshDeckSelectList()
{
	DeckSelectListContainer->ClearChildren();

	UCGGameInstance* GI = GetCGGameInstance();
	if (!GI)
	{
		return;
	}

	UTextBlock* SavedHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SavedHeader->SetText(FText::FromString(TEXT("保存したデッキ")));
	SavedHeader->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16));
	if (UVerticalBoxSlot* SavedHeaderSlot = DeckSelectListContainer->AddChildToVerticalBox(SavedHeader))
	{
		SavedHeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	for (int32 i = 0; i < GI->SavedDecks.Num(); ++i)
	{
		const FCGSavedDeck& Deck = GI->SavedDecks[i];
		UCGDeckListRowWidget* Row = CreateWidget<UCGDeckListRowWidget>(GetWorld(), UCGDeckListRowWidget::StaticClass());
		Row->SetRowData(i, Deck.DeckName, Deck.CardIds.Num(), Deck.DeckName == GI->ActiveDeckName);
		Row->OnSelectClicked.AddDynamic(this, &UCGLobbyHUD::HandleDeckSelectRowSelected);
		Row->OnDeleteClicked.AddDynamic(this, &UCGLobbyHUD::HandleDeckSelectRowDeleted);
		if (UVerticalBoxSlot* RowSlot = DeckSelectListContainer->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 3.f));
		}
	}

	// 色ごとの基本デッキ(次期ルールの色を体感するための参考デッキ、
	// docs/next-ruleset-cards-v1.md「サンプルデッキ」)。選ぶとそのまま「保存したデッキ」へ
	// 追加される(UCGGameInstance::SaveDeckAs)。行のインデックスは
	// -1-配列番号にして、保存済みデッキ(0以上)と区別する
	// (HandleDeckSelectRowSelected参照)。
	UTextBlock* BasicHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BasicHeader->SetText(FText::FromString(TEXT("基本デッキ(選ぶと保存されます)")));
	BasicHeader->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16));
	if (UVerticalBoxSlot* BasicHeaderSlot = DeckSelectListContainer->AddChildToVerticalBox(BasicHeader))
	{
		BasicHeaderSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 4.f));
	}

	const TArray<TPair<ECGColor, FString>>& BasicColors = GetBasicDeckColors();
	for (int32 i = 0; i < BasicColors.Num(); ++i)
	{
		const FString& DisplayName = BasicColors[i].Value;
		const bool bIsActive = (DisplayName == GI->ActiveDeckName);
		UCGDeckListRowWidget* Row = CreateWidget<UCGDeckListRowWidget>(GetWorld(), UCGDeckListRowWidget::StaticClass());
		Row->SetRowData(-1 - i, DisplayName, UCGCardDatabase::GetBasicColorDeckCardIds(BasicColors[i].Key).Num(), bIsActive, /*bAllowDelete=*/false);
		Row->OnSelectClicked.AddDynamic(this, &UCGLobbyHUD::HandleDeckSelectRowSelected);
		if (UVerticalBoxSlot* RowSlot = DeckSelectListContainer->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 3.f));
		}
	}
}

void UCGLobbyHUD::HandleDeckSelectRowSelected(int32 SlotIndex)
{
	UCGGameInstance* GI = GetCGGameInstance();
	if (GI)
	{
		if (SlotIndex >= 0)
		{
			GI->SelectSavedDeck(SlotIndex);
		}
		else
		{
			// 基本デッキ行(負のインデックス)。選んだ色の基本デッキをそのまま
			// 保存済みデッキへ追加/上書きし、アクティブにする。
			const int32 ColorIndex = -1 - SlotIndex;
			const TArray<TPair<ECGColor, FString>>& BasicColors = GetBasicDeckColors();
			if (BasicColors.IsValidIndex(ColorIndex))
			{
				GI->SaveDeckAs(BasicColors[ColorIndex].Value, UCGCardDatabase::GetBasicColorDeckCardIds(BasicColors[ColorIndex].Key));
			}
		}
	}

	// 選んだ瞬間、呼び出し元(デッキ選択(親)またはオンライン対戦)へ自動的に戻る
	// (「選択ボタンを押すとデッキを実際に選択できる画面に遷移して選択すると
	// デッキ選択画面に戻ってきて選択されたデッキが選ばれている」という
	// フィードバックへの対応。docs/architecture.md「ロビーのモーダル構成」参照)。
	DeckSelectLayer->SetVisibility(ESlateVisibility::Collapsed);
	if (PendingReturnLayer == OnlineLayer)
	{
		RefreshOnlineMyDeckSummary();
	}
	else if (PendingReturnLayer == BattleSetupLayer)
	{
		RefreshBattleSetupSummary();
	}
	if (PendingReturnLayer)
	{
		PendingReturnLayer->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCGLobbyHUD::HandleDeckSelectRowDeleted(int32 SlotIndex)
{
	// 基本デッキ行(負のインデックス)は削除ボタン自体を表示していないため、
	// ここに来るのは保存済みデッキ(0以上)のみのはずだが念のため確認する。
	if (SlotIndex < 0)
	{
		return;
	}
	if (UCGGameInstance* GI = GetCGGameInstance())
	{
		GI->DeleteSavedDeck(SlotIndex);
	}
	RefreshDeckSelectList();
}

void UCGLobbyHUD::RefreshAIDeckSelectList()
{
	AIDeckSelectListContainer->ClearChildren();

	UCGGameInstance* GI = GetCGGameInstance();
	if (!GI)
	{
		return;
	}

	// 行インデックスは0=ランダム、1以上はGetBasicDeckColors()のインデックス+1
	// (HandleAIDeckSelectRowSelected参照)。「(25枚)」はランダムでも実際に選ばれる
	// デッキが必ず25枚になることを表すためにそのまま表示する。
	UCGDeckListRowWidget* RandomRow = CreateWidget<UCGDeckListRowWidget>(GetWorld(), UCGDeckListRowWidget::StaticClass());
	RandomRow->SetRowData(0, TEXT("ランダム(5色から抽選)"), 25, GI->SelectedAIOpponentColor == ECGColor::None, /*bAllowDelete=*/false);
	RandomRow->OnSelectClicked.AddDynamic(this, &UCGLobbyHUD::HandleAIDeckSelectRowSelected);
	if (UVerticalBoxSlot* RandomRowSlot = AIDeckSelectListContainer->AddChildToVerticalBox(RandomRow))
	{
		RandomRowSlot->SetPadding(FMargin(0.f, 3.f));
	}

	const TArray<TPair<ECGColor, FString>>& BasicColors = GetBasicDeckColors();
	for (int32 i = 0; i < BasicColors.Num(); ++i)
	{
		const ECGColor Color = BasicColors[i].Key;
		const bool bIsActive = (GI->SelectedAIOpponentColor == Color);
		UCGDeckListRowWidget* Row = CreateWidget<UCGDeckListRowWidget>(GetWorld(), UCGDeckListRowWidget::StaticClass());
		Row->SetRowData(i + 1, BasicColors[i].Value, UCGCardDatabase::GetBasicColorDeckCardIds(Color).Num(), bIsActive, /*bAllowDelete=*/false);
		Row->OnSelectClicked.AddDynamic(this, &UCGLobbyHUD::HandleAIDeckSelectRowSelected);
		if (UVerticalBoxSlot* RowSlot = AIDeckSelectListContainer->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 3.f));
		}
	}
}

void UCGLobbyHUD::HandleAIDeckSelectRowSelected(int32 SlotIndex)
{
	if (UCGGameInstance* GI = GetCGGameInstance())
	{
		const TArray<TPair<ECGColor, FString>>& BasicColors = GetBasicDeckColors();
		GI->SelectedAIOpponentColor = (SlotIndex == 0 || !BasicColors.IsValidIndex(SlotIndex - 1))
			? ECGColor::None
			: BasicColors[SlotIndex - 1].Key;
	}

	// 対戦相手デッキ一覧はデッキ選択(親)からのみ開かれるため、常にそこへ戻る。
	AIDeckSelectLayer->SetVisibility(ESlateVisibility::Collapsed);
	RefreshBattleSetupSummary();
	if (PendingReturnLayer)
	{
		PendingReturnLayer->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCGLobbyHUD::HandleOpenCodexClicked()
{
	CodexLayer->SetVisibility(ESlateVisibility::Visible);
	RefreshCodexList();
}

void UCGLobbyHUD::HandleCloseCodexClicked()
{
	// 拡大プレビューが表示されたまま図鑑を閉じると、閉じた後も画面に残ってしまう
	// (CGGameHUD::RefreshUI等と同じ対策)。
	HidePreview();
	CodexLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void UCGLobbyHUD::RefreshCodexList()
{
	CodexListContainer->ClearChildren();

	const TArray<FCGCardDef> Filtered = CGCardFilterSort::BuildFilteredSortedList(UCGCardDatabase::GetBuildableCards(), FilterSortState);

	if (Filtered.Num() == 0)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CodexEmptyText"));
		EmptyText->SetText(FText::FromString(TEXT("条件に合うカードがありません")));
		if (UVerticalBoxSlot* EmptyTextSlot = CodexListContainer->AddChildToVerticalBox(EmptyText))
		{
			EmptyTextSlot->SetHorizontalAlignment(HAlign_Center);
			EmptyTextSlot->SetPadding(FMargin(0.f, 16.f));
		}
		return;
	}

	// 種族順のときだけ見出し付きでグルーピングする(UCGDeckBuilderHUD::RefreshPoolListと同じ)。
	if (FilterSortState.SortMode != ECGCardSortMode::Tribe)
	{
		UWrapBox* Wrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("CodexWrap"));
		Wrap->SetInnerSlotPadding(FVector2D(0.f, LobbyCardGap * 2.f));
		CodexListContainer->AddChildToVerticalBox(Wrap);

		for (const FCGCardDef& Def : Filtered)
		{
			AddCodexCardToWrap(Wrap, Def);
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
				*FString::Printf(TEXT("CodexGroupHeader_%d"), GroupIndex));
			GroupHeader->SetText(FText::FromString(GroupKey));
			GroupHeader->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
			GroupHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.8f, 0.6f)));
			if (UVerticalBoxSlot* HeaderSlot = CodexListContainer->AddChildToVerticalBox(GroupHeader))
			{
				HeaderSlot->SetPadding(FMargin(4.f, 10.f, 4.f, 2.f));
			}

			CurrentWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(),
				*FString::Printf(TEXT("CodexGroupWrap_%d"), GroupIndex));
			CurrentWrap->SetInnerSlotPadding(FVector2D(0.f, LobbyCardGap * 2.f));
			CodexListContainer->AddChildToVerticalBox(CurrentWrap);
		}

		AddCodexCardToWrap(CurrentWrap, Def);
	}
}

void UCGLobbyHUD::AddCodexCardToWrap(UWrapBox* Wrap, const FCGCardDef& Def)
{
	const float DisplayScale = bCompactDisplay ? CodexDisplayScaleCompact : CodexDisplayScaleNormal;

	// 図鑑は眺めるだけの画面のため、クリックハンドラは登録しない
	// (ホバーでの拡大プレビューのみ有効。RegisterCardHoverPreview)。
	UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
	SlotWidget->SetCardData(Def);
	RegisterCardHoverPreview(SlotWidget);

	UWidget* WrapChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, SlotWidget, DisplayScale);
	if (UWrapBoxSlot* CardSlot = Wrap->AddChildToWrapBox(WrapChild))
	{
		CardSlot->SetPadding(FMargin(LobbyCardGap));
	}
}

void UCGLobbyHUD::UpdateCodexButtonVisuals()
{
	auto SetActive = [](UButton* Button, bool bActive)
	{
		if (Button)
		{
			Button->SetRenderOpacity(bActive ? 1.f : 0.55f);
		}
	};

	SetActive(CodexFilterAllButton, !FilterSortState.TypeFilter.IsSet());
	SetActive(CodexFilterUnitButton, FilterSortState.TypeFilter.IsSet() && FilterSortState.TypeFilter.GetValue() == ECGCardType::Unit);
	SetActive(CodexFilterSpellButton, FilterSortState.TypeFilter.IsSet() && FilterSortState.TypeFilter.GetValue() == ECGCardType::Spell);

	SetActive(CodexSortCostButton, FilterSortState.SortMode == ECGCardSortMode::Cost);
	SetActive(CodexSortNameButton, FilterSortState.SortMode == ECGCardSortMode::Name);
	SetActive(CodexSortTribeButton, FilterSortState.SortMode == ECGCardSortMode::Tribe);

	SetActive(CodexCompactToggleButton, bCompactDisplay);
}

void UCGLobbyHUD::HandleOpenHowToPlayClicked()
{
	HowToPlayLayer->SetVisibility(ESlateVisibility::Visible);
}

void UCGLobbyHUD::HandleCloseHowToPlayClicked()
{
	HowToPlayLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void UCGLobbyHUD::HandleOpenOnlineClicked()
{
	RefreshOnlineMyDeckSummary();
	if (OnlineStatusText)
	{
		OnlineStatusText->SetText(FText::GetEmpty());
	}
	if (YourSteamIdText && YourSteamIdLabel)
	{
		const FString SteamId = GetLocalSteamIdString();
		if (SteamId.IsEmpty())
		{
			YourSteamIdLabel->SetText(FText::FromString(TEXT("(SteamIDを取得できません。Steamクライアントが起動しているか確認してください)")));
			YourSteamIdLabel->SetVisibility(ESlateVisibility::Visible);
			YourSteamIdText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			YourSteamIdLabel->SetText(FText::FromString(TEXT("あなたのSteamID(ホストする際に友達へ伝えてください。クリックして選択・コピーできます)")));
			YourSteamIdLabel->SetVisibility(ESlateVisibility::Visible);
			// SteamIDだけを入れることで、そのままCtrl+A→Ctrl+Cでコピーできるようにする。
			YourSteamIdText->SetText(FText::FromString(SteamId));
			YourSteamIdText->SetVisibility(ESlateVisibility::Visible);
		}
	}
	OnlineLayer->SetVisibility(ESlateVisibility::Visible);
}

void UCGLobbyHUD::HandleCloseOnlineClicked()
{
	OnlineLayer->SetVisibility(ESlateVisibility::Collapsed);
}

FString UCGLobbyHUD::GetLocalSteamIdString() const
{
	// SteamSocketsプラグイン経由でインターネット越しに参加してもらうため、
	// 自分のSteamID64(10進数文字列)を取得する(docs/online-play-design.md
	// 「SteamSocketsによるインターネット越し接続」参照)。Steamが無効/未起動
	// (Nullサブシステムにフォールバック等)の場合は空文字を返す。
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	if (!OSS)
	{
		return FString();
	}
	IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		return FString();
	}
	const TSharedPtr<const FUniqueNetId> UserId = Identity->GetUniquePlayerId(0);
	return UserId.IsValid() ? UserId->ToString() : FString();
}

void UCGLobbyHUD::HandleHostGameClicked()
{
	// 対戦レベルをリッスンサーバーとして開く(docs/online-play-design.md
	// 「全体アーキテクチャ」)。ホスト自身もこの後PostLogin経由でSide0として
	// 参加し、自分のデッキをACGPlayerController::ServerSubmitDeckで送る。
	UGameplayStatics::OpenLevel(this, FName(BattleLevelPath), true, TEXT("listen"));
}

void UCGLobbyHUD::HandleJoinGameClicked()
{
	const FString IP = JoinIPBox ? JoinIPBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (IP.IsEmpty())
	{
		if (OnlineStatusText)
		{
			OnlineStatusText->SetText(FText::FromString(TEXT("IPアドレスまたはSteamIDを入力してください")));
		}
		return;
	}

	if (OnlineStatusText)
	{
		OnlineStatusText->SetText(FText::FromString(FString::Printf(TEXT("接続中: %s ..."), *IP)));
	}

	// 当初はConsoleCommand("open <文字列>")で接続していたが、実地テストで
	// SteamIDのような「ドットを含まない純粋な数字だけの文字列」を渡すと、
	// UEのURLテキスト解析が接続先ホストとして認識できず「読み込むマップ名」だと
	// 誤認識してしまい(LogLongPackageNames: Can't Find URL → InvalidURL)、
	// ポート番号を付けても改善しないことが判明した(IPアドレスのようにドットを
	// 含む文字列は正しく解釈される)。この文字列解析の曖昧さを避けるため、
	// FURLのHost/Portフィールドへ直接値を設定してGEngine->Browse()を呼ぶ方式に
	// 変更する。この経路はテキスト解析を経由しないため、SteamID(10進数)・
	// IPアドレスのどちらでも正しく接続できる(docs/online-play-design.md
	// 「SteamSocketsによるインターネット越し接続」で説明している、この文字列が
	// SteamIDならポート開放不要のP2P、IPアドレスならLAN直接接続として解決される
	// という仕組み自体はSteamSockets側の処理なので変わらない)。
	FString HostPart = IP;
	int32 PortPart = 7777;
	FString HostToken, PortToken;
	if (IP.Split(TEXT(":"), &HostToken, &PortToken))
	{
		HostPart = HostToken;
		PortPart = FCString::Atoi(*PortToken);
	}

	// GEngine->GetWorldContextFromWorld(World)によるWorld総当たり検索ではなく、
	// GameInstanceが直接保持しているWorldContextを使う(こちらの方が確実に
	// 取得できる。スタンドアロン/クライアントでは常に1つだけ存在する)。
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	FWorldContext* WorldContext = GameInstance ? GameInstance->GetWorldContext() : nullptr;
	if (!WorldContext)
	{
		UE_LOG(LogCardGame, Warning, TEXT("HandleJoinGameClicked: WorldContext取得失敗、接続を開始できません"));
		if (OnlineStatusText)
		{
			OnlineStatusText->SetText(FText::FromString(TEXT("接続を開始できませんでした(内部エラー)")));
		}
		return;
	}

	FURL URL;
	URL.Host = HostPart;
	URL.Port = PortPart;
	FString Error;
	const EBrowseReturnVal::Type Result = GEngine->Browse(*WorldContext, URL, Error);
	UE_LOG(LogCardGame, Log, TEXT("HandleJoinGameClicked: Browse(Host=%s Port=%d) Result=%d Error=%s"),
		*HostPart, PortPart, static_cast<int32>(Result), *Error);
	if (Result == EBrowseReturnVal::Failure && OnlineStatusText)
	{
		OnlineStatusText->SetText(FText::FromString(FString::Printf(TEXT("接続に失敗しました: %s"), *Error)));
	}
}

void UCGLobbyHUD::AddHowToPlaySection(UVerticalBox* Root, const FString& Heading, const FString& Body)
{
	UTextBlock* HeadingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HeadingText->SetText(FText::FromString(Heading));
	HeadingText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
	HeadingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.8f, 0.6f)));
	if (UVerticalBoxSlot* HeadingSlot = Root->AddChildToVerticalBox(HeadingText))
	{
		HeadingSlot->SetPadding(FMargin(4.f, 16.f, 4.f, 4.f));
	}

	UTextBlock* BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BodyText->SetText(FText::FromString(Body));
	BodyText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15));
	// 固定幅の読み物のため、長い説明文は折り返して表示する。
	BodyText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* BodySlot = Root->AddChildToVerticalBox(BodyText))
	{
		BodySlot->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));
	}
}

void UCGLobbyHUD::HandleCodexSearchTextChanged(const FText& NewText)
{
	FilterSortState.SearchQuery = NewText.ToString();
	RefreshCodexList();
}

void UCGLobbyHUD::HandleCodexFilterAllClicked()
{
	FilterSortState.TypeFilter.Reset();
	UpdateCodexButtonVisuals();
	RefreshCodexList();
}

void UCGLobbyHUD::HandleCodexFilterUnitClicked()
{
	FilterSortState.TypeFilter = ECGCardType::Unit;
	UpdateCodexButtonVisuals();
	RefreshCodexList();
}

void UCGLobbyHUD::HandleCodexFilterSpellClicked()
{
	FilterSortState.TypeFilter = ECGCardType::Spell;
	UpdateCodexButtonVisuals();
	RefreshCodexList();
}

void UCGLobbyHUD::HandleCodexSortCostClicked()
{
	FilterSortState.SortMode = ECGCardSortMode::Cost;
	UpdateCodexButtonVisuals();
	RefreshCodexList();
}

void UCGLobbyHUD::HandleCodexSortNameClicked()
{
	FilterSortState.SortMode = ECGCardSortMode::Name;
	UpdateCodexButtonVisuals();
	RefreshCodexList();
}

void UCGLobbyHUD::HandleCodexSortTribeClicked()
{
	FilterSortState.SortMode = ECGCardSortMode::Tribe;
	UpdateCodexButtonVisuals();
	RefreshCodexList();
}

void UCGLobbyHUD::HandleCodexCompactToggleClicked()
{
	bCompactDisplay = !bCompactDisplay;
	UpdateCodexButtonVisuals();
	RefreshCodexList();
}
