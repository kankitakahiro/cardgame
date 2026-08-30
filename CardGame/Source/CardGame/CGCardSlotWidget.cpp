#include "CGCardSlotWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

namespace
{
	// 多層フレームの太さ。外側から境界線→種別色→レアリティ枠。
	constexpr float OuterBorderThickness = 4.f;
	constexpr float TypeBorderThickness = 6.f;
	constexpr float RarityBorderThickness = 3.f;

	constexpr float ImagePlaceholderHeight = 90.f;

	// 効果文/フレーバーテキストはカードによって長さがかなり違うため、固定フォントサイズだと
	// 短いカードでは余白が目立ち、長いカードでは枠からはみ出してクリップされてしまう。
	// 文字数に応じて段階的にフォントサイズを落とし、常にカード内に収まるようにする
	// (厳密なテキスト計測ではなく文字数ベースの簡易ヒューリスティック)。
	int32 ComputeFitFontSize(const FString& Text, int32 MaxSize, int32 MinSize)
	{
		const int32 Steps = MaxSize - MinSize;
		if (Steps <= 0 || Text.Len() <= 20)
		{
			return MaxSize;
		}
		const int32 Reduction = FMath::Min(Steps, (Text.Len() - 20) / 15);
		return MaxSize - Reduction;
	}

	// 種別未設定(SetCardTypeが実質呼ばれていない=裏向きカード等)のときの無地の枠色。
	const FLinearColor CardTypeDefaultColor(0.35f, 0.35f, 0.35f, 1.f);
	// カード全体の境界線(ほぼ黒)。
	const FLinearColor CardBorderColor(0.05f, 0.05f, 0.05f, 1.f);
	// レアリティ概念導入前の、当面固定の中間色の飾り枠。
	const FLinearColor RarityPlaceholderColor(0.55f, 0.50f, 0.38f, 1.f);
	// カード面(イラストが無い部分の背景)。
	const FLinearColor CardFaceColor(0.12f, 0.11f, 0.10f, 1.f);
	const FLinearColor ImagePlaceholderColor(0.35f, 0.35f, 0.35f, 1.f);
}

TSharedRef<SWidget> UCGCardSlotWidget::RebuildWidget()
{
	EnsureBuilt();
	return Super::RebuildWidget();
}

void UCGCardSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureBuilt();
}

void UCGCardSlotWidget::EnsureBuilt()
{
	if (Button)
	{
		return;
	}

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSizeBox"));
	SizeBox->SetWidthOverride(CardWidth);
	SizeBox->SetHeightOverride(CardHeight);
	// 効果文が長いカードで中身が枠からはみ出すと、カードごとに見た目の大きさが
	// バラバラに見えてしまうため、必ずこのサイズで切り取る。
	SizeBox->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
	// UButtonの既定スタイルには、ボタンの背景画像の縁を考慮した内側余白
	// (NormalPadding/PressedPadding)が標準で入っており、これが原因で子(OuterBorder)が
	// Button全体を埋めきれず、隙間からButton自身の既定の背景色(薄いグレー)が
	// 見えてしまっていた。ここでは枠自体をカードのデザインとして描いているため、
	// ボタン側の余白は0にする。
	// 既定のボタン背景画像(枠線や余白を含む見た目)を完全に消し、こちらで用意した
	// 多層フレームだけが見えるようにする(Padding=0だけでは背景画像自体の縁が
	// わずかに残っていた)。
	FButtonStyle NoChromeButtonStyle = Button->GetStyle();
	NoChromeButtonStyle.NormalPadding = FMargin(0.f);
	NoChromeButtonStyle.PressedPadding = FMargin(0.f);
	NoChromeButtonStyle.Normal = FSlateNoResource();
	NoChromeButtonStyle.Hovered = FSlateNoResource();
	NoChromeButtonStyle.Pressed = FSlateNoResource();
	NoChromeButtonStyle.Disabled = FSlateNoResource();
	Button->SetStyle(NoChromeButtonStyle);

	// 多層フレーム: 境界線→種別色→レアリティ枠(現状は飾りのみ)。それぞれPaddingぶんだけ
	// 内側に次の要素を配置することで、リング状の縁取りに見せている。
	// UBorderは「自分の子を自分の内側でどう配置するか」をHorizontalAlignment/
	// VerticalAlignmentで持っており、これを明示しないと子(=次の内側の枠)が中身の量に
	// 応じた自然なサイズになってしまい、カードごとに内側の枠の大きさがバラバラに見える
	// バグになっていた。全階層で明示的に親いっぱいへ広げる。
	OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterBorder"));
	OuterBorder->SetPadding(FMargin(OuterBorderThickness));
	OuterBorder->SetBrushColor(CardBorderColor);
	OuterBorder->SetHorizontalAlignment(HAlign_Fill);
	OuterBorder->SetVerticalAlignment(VAlign_Fill);

	TypeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TypeBorder"));
	TypeBorder->SetPadding(FMargin(TypeBorderThickness));
	TypeBorder->SetBrushColor(CardTypeDefaultColor);
	TypeBorder->SetHorizontalAlignment(HAlign_Fill);
	TypeBorder->SetVerticalAlignment(VAlign_Fill);

	RarityBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RarityBorder"));
	RarityBorder->SetPadding(FMargin(RarityBorderThickness));
	RarityBorder->SetBrushColor(RarityPlaceholderColor);
	RarityBorder->SetHorizontalAlignment(HAlign_Fill);
	RarityBorder->SetVerticalAlignment(VAlign_Fill);

	UBorder* FaceBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FaceBackground"));
	FaceBackground->SetPadding(FMargin(0.f));
	FaceBackground->SetBrushColor(CardFaceColor);
	FaceBackground->SetHorizontalAlignment(HAlign_Fill);
	FaceBackground->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));

	// --- ヘッダー: コストバッジ + カード名 ---
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));

	USizeBox* CostSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CostSizeBox"));
	CostSizeBox->SetWidthOverride(30.f);
	CostSizeBox->SetHeightOverride(30.f);

	UBorder* CostBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CostBadge"));
	CostBadge->SetBrushColor(FLinearColor(0.15f, 0.15f, 0.55f, 1.f));
	CostBadge->SetHorizontalAlignment(HAlign_Center);
	CostBadge->SetVerticalAlignment(VAlign_Center);

	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CostText"));
	CostText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12));
	CostText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CostText->SetJustification(ETextJustify::Center);
	CostBadge->AddChild(CostText);
	if (USizeBoxSlot* CostBadgeSlot = Cast<USizeBoxSlot>(CostSizeBox->AddChild(CostBadge)))
	{
		CostBadgeSlot->SetHorizontalAlignment(HAlign_Fill);
		CostBadgeSlot->SetVerticalAlignment(VAlign_Fill);
	}
	HeaderRow->AddChildToHorizontalBox(CostSizeBox);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13));
	NameText->SetAutoWrapText(true);
	if (UHorizontalBoxSlot* NameSlot = HeaderRow->AddChildToHorizontalBox(NameText))
	{
		NameSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* HeaderSlot = ContentBox->AddChildToVerticalBox(HeaderRow))
	{
		HeaderSlot->SetPadding(FMargin(8.f, 8.f, 8.f, 2.f));
	}

	// --- イラスト欄(画像アセット未用意のため場所だけ確保) ---
	USizeBox* ImageSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ImageSizeBox"));
	ImageSizeBox->SetHeightOverride(ImagePlaceholderHeight);
	UBorder* ImagePlaceholder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ImagePlaceholder"));
	ImagePlaceholder->SetBrushColor(ImagePlaceholderColor);
	if (USizeBoxSlot* ImagePlaceholderSlot = Cast<USizeBoxSlot>(ImageSizeBox->AddChild(ImagePlaceholder)))
	{
		ImagePlaceholderSlot->SetHorizontalAlignment(HAlign_Fill);
		ImagePlaceholderSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UVerticalBoxSlot* ImageSlot = ContentBox->AddChildToVerticalBox(ImageSizeBox))
	{
		ImageSlot->SetPadding(FMargin(8.f, 2.f));
	}

	// --- 部族/キーワード行(部族は現状一部のカードのみ値あり、Guard/Hasteはカードの
	// Tagsから常時表示。場・手札・マーケットなど、どこで表示していても同じ内容になる) ---
	TribeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TribeText"));
	TribeText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 10));
	TribeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.8f, 0.6f)));
	TribeText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* TribeSlot = ContentBox->AddChildToVerticalBox(TribeText))
	{
		TribeSlot->SetPadding(FMargin(8.f, 2.f));
	}

	// --- 効果テキスト ---
	EffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EffectText"));
	EffectText->SetAutoWrapText(true);
	EffectText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10));
	EffectText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* EffectSlot = ContentBox->AddChildToVerticalBox(EffectText))
	{
		EffectSlot->SetPadding(FMargin(8.f, 2.f));
		EffectSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// --- フレーバーテキスト(現状は一部のカードのみ値あり) ---
	FlavorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FlavorText"));
	FlavorText->SetAutoWrapText(true);
	FlavorText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 9));
	FlavorText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.72f, 0.65f)));
	FlavorText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* FlavorSlot = ContentBox->AddChildToVerticalBox(FlavorText))
	{
		FlavorSlot->SetPadding(FMargin(8.f, 2.f, 8.f, 8.f));
	}

	FaceBackground->AddChild(ContentBox);

	// --- 右下のATK/HP(Unitのみ。カード面にOverlayで重ねる) ---
	Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("SlotOverlay"));
	if (UOverlaySlot* FaceSlot = Overlay->AddChildToOverlay(FaceBackground))
	{
		FaceSlot->SetHorizontalAlignment(HAlign_Fill);
		FaceSlot->SetVerticalAlignment(VAlign_Fill);
	}

	StatBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatBadgeBorder"));
	StatBadgeBorder->SetPadding(FMargin(7.f, 3.f));
	StatBadgeBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f));
	StatBadgeBorder->SetVisibility(ESlateVisibility::Collapsed);

	StatText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatText"));
	StatText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13));
	StatText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StatBadgeBorder->AddChild(StatText);

	if (UOverlaySlot* StatSlot = Overlay->AddChildToOverlay(StatBadgeBorder))
	{
		StatSlot->SetHorizontalAlignment(HAlign_Right);
		StatSlot->SetVerticalAlignment(VAlign_Bottom);
		StatSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 8.f));
	}

	RarityBorder->AddChild(Overlay);
	TypeBorder->AddChild(RarityBorder);
	OuterBorder->AddChild(TypeBorder);

	// UButton/USizeBoxは(UBorderと違って)自分自身にHorizontalAlignment/
	// VerticalAlignmentを持たず、子の配置は「スロット」オブジェクト
	// (UButtonSlot/USizeBoxSlot)側で管理している。ここを明示しないと既定で
	// 子が中央に自然なサイズのまま置かれてしまい、外側は240x336ぴったりなのに
	// 中の枠だけ中身の分量に応じて縮んで見える(=枠と中身がズレる)バグになっていた。
	if (UButtonSlot* ButtonContentSlot = Cast<UButtonSlot>(Button->AddChild(OuterBorder)))
	{
		ButtonContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (USizeBoxSlot* SizeBoxContentSlot = Cast<USizeBoxSlot>(SizeBox->AddChild(Button)))
	{
		SizeBoxContentSlot->SetHorizontalAlignment(HAlign_Fill);
		SizeBoxContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	WidgetTree->RootWidget = SizeBox;
	Button->OnClicked.AddDynamic(this, &UCGCardSlotWidget::HandleClicked);

	ApplyCardTypeColor();
	RefreshDisplay();
}

void UCGCardSlotWidget::SetCardData(const FCGCardDef& Def, TOptional<int32> OverrideAtk, TOptional<int32> OverrideHp)
{
	EnsureBuilt();

	PendingCostText = FString::FromInt(Def.Cost);
	PendingNameText = Def.CardName;
	PendingEffectText = Def.Description;
	PendingFlavorText = Def.FlavorText;

	// 部族とGuard/Hasteのようなキーワード能力を1行にまとめる。状況によって出したり
	// 消したりする一言(旧StatusNote)ではなく、カード自体が持つ情報として常に表示する
	// (場・手札・マーケットどこで見ても同じ内容になるようにするため)。
	TArray<FString> TribeLineParts;
	if (!Def.Tribe.IsEmpty())
	{
		TribeLineParts.Add(Def.Tribe);
	}
	if (Def.HasTag(TEXT("Guard")))
	{
		TribeLineParts.Add(TEXT("守護"));
	}
	if (Def.HasTag(TEXT("Haste")))
	{
		TribeLineParts.Add(TEXT("速攻"));
	}
	PendingTribeText = FString::Join(TribeLineParts, TEXT(" ・ "));

	if (Def.CardType == ECGCardType::Unit)
	{
		PendingStatText = FString::Printf(TEXT("%d/%d"), OverrideAtk.Get(Def.Atk), OverrideHp.Get(Def.Hp));
	}
	else
	{
		PendingStatText.Empty();
	}

	CardType = Def.CardType;
	ApplyCardTypeColor();
	RefreshDisplay();

	bHasCardData = true;
	bLastWasFaceDown = false;
	LastCardDef = Def;
	LastOverrideAtk = OverrideAtk;
	LastOverrideHp = OverrideHp;
}

void UCGCardSlotWidget::SetFaceDown()
{
	EnsureBuilt();

	PendingCostText.Empty();
	PendingNameText = TEXT("?");
	PendingTribeText.Empty();
	PendingEffectText.Empty();
	PendingFlavorText.Empty();
	PendingStatText.Empty();

	// CardTypeは未設定のままにしておく(=種別枠は無地のまま、中身を明かさない)。
	RefreshDisplay();

	bHasCardData = true;
	bLastWasFaceDown = true;
}

void UCGCardSlotWidget::CopyCardDataTo(UCGCardSlotWidget* Target) const
{
	if (!Target || !bHasCardData)
	{
		return;
	}
	if (bLastWasFaceDown)
	{
		Target->SetFaceDown();
	}
	else
	{
		Target->SetCardData(LastCardDef, LastOverrideAtk, LastOverrideHp);
	}
}

void UCGCardSlotWidget::RefreshDisplay()
{
	if (!Button)
	{
		// EnsureBuilt()未実行。ビルドされた時点でEnsureBuilt()側から改めて呼ばれる。
		return;
	}

	CostText->SetText(FText::FromString(PendingCostText));
	NameText->SetText(FText::FromString(PendingNameText));

	TribeText->SetText(FText::FromString(PendingTribeText));
	TribeText->SetVisibility(PendingTribeText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	EffectText->SetText(FText::FromString(PendingEffectText));
	EffectText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), ComputeFitFontSize(PendingEffectText, 12, 7)));

	FlavorText->SetText(FText::FromString(PendingFlavorText));
	FlavorText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), ComputeFitFontSize(PendingFlavorText, 10, 7)));
	FlavorText->SetVisibility(PendingFlavorText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	StatText->SetText(FText::FromString(PendingStatText));
	StatBadgeBorder->SetVisibility(PendingStatText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UCGCardSlotWidget::ApplyCardTypeColor()
{
	if (!TypeBorder || !CardType.IsSet())
	{
		return;
	}
	// Unitは青系、Spellは紫系の枠にし、一覧で種別をひと目で見分けられるようにする。
	const FLinearColor Color = (*CardType == ECGCardType::Unit)
		? FLinearColor(0.30f, 0.55f, 0.95f)
		: FLinearColor(0.75f, 0.35f, 0.85f);
	TypeBorder->SetBrushColor(Color);
}

void UCGCardSlotWidget::HandleClicked()
{
	OnSlotClicked.Broadcast(SlotIndex);
}

void UCGCardSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	OnHoverChanged.Broadcast(this, true);
}

void UCGCardSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	OnHoverChanged.Broadcast(this, false);
}
