#include "CGGameHUD.h"
#include "CGCardSlotWidget.h"
#include "CGGameMode.h"
#include "CGGameState.h"
#include "CGPlayerState.h"
#include "CGPlayerController.h"
#include "CGCardDatabase.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"
#include "CardGame.h"

namespace
{
	// 勝敗確定後に「ロビーへ戻る」ボタンから遷移する先(docs/architecture.md「レベルと画面遷移」)。
	const TCHAR* LobbyLevelPath = TEXT("/Game/CardGame/Maps/L_Lobby");

	// 常時表示するカードの縮小率。マーケット/場は一覧性重視でやや小さく、自分の手札は
	// 操作対象なので少し大きめにする。カーソルを乗せれば実寸プレビューで確認できるため、
	// 小さくても情報の見落としは起きない(UCGCardSlotWidget::WrapForCompactDisplay、
	// docs/architecture.md「カードUIの設計」)。
	constexpr float EnemyHandDisplayScale = 0.42f;
	// 場は「ゾーンの残り高さをFillで専有する」方式をやめ、カード1段分の実寸(+最低保証の
	// 高さ)だけを使うようにしたため、以前(0.52)より少し大きくしても全体の余白は
	// むしろ減る(ユニット数によらず常に確保していた無駄な余白が無くなったぶん)。
	constexpr float BoardDisplayScale = 0.65f;
	constexpr float MarketDisplayScale = 0.6f;
	// 自分の手札は実際に読んで操作する対象のため、他より大きめにする
	// (0.62だと文字が小さすぎるというフィードバックのため引き上げた)。
	constexpr float SelfHandDisplayScale = 0.8f;
	// 選択ポップアップ(山札スクライ表示)は、内容をよく見て選ぶための専用画面なので
	// 基準サイズ(等倍)で表示する。WrapForCompactDisplayはScale>=1.0のときは縮小せず
	// カードをそのまま返す(見にくい/小さすぎるというフィードバックのため。
	// docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。スクライは常に1枚だけなのでこのままでよい。
	constexpr float ChoiceViewerDisplayScale = 1.f;

	// 墓地ビューアーは候補が複数枚になり得るため、スクライと違って等倍のままだと
	// 1行に収まらずすぐ折り返し/横スクロールが必要になってしまう。少し縮小して
	// 1行に複数枚並ぶ余地を作りつつ、盤面(0.65)よりは大きく保って見やすさを優先する
	// (「墓地ポップアップを2列で見たい」というフィードバックのため)。
	constexpr float GraveyardViewerDisplayScale = 0.72f;

	// 墓地ビューアーの折り返し幅。この幅を超えたカードは自動で次の行へ折り返される
	// (MakeScrollableWrap)。3枚(カード幅+左右余白ぶん)がちょうど収まる幅にしており、
	// 4枚目以降は次の行、さらに収まらない分は縦スクロールで見る。
	constexpr float GraveyardViewerWrapWidth =
		(UCGCardSlotWidget::CardWidth * GraveyardViewerDisplayScale + 12.f) * 3.f;

	// マーケットのサイドレールの幅。2列分のカード(MarketDisplayScale×CardWidth×2)+
	// カード間の余白+ゾーンパネル自体の左右パディングが収まる幅にしている。
	constexpr float MarketRailWidth = 360.f;

	// 場の行が最低限確保する高さ(カード1段の実寸+少し余裕)。ユニット0体でも
	// この高さは保つことで、ユニットの出入りでゾーンの高さがガクガク変わらないようにする。
	const float BoardRowMinHeight = UCGCardSlotWidget::CardHeight * BoardDisplayScale + 8.f;

	const float CardGap = 6.f;

	// カード枚数が増えても画面外へあふれないよう、各行を横スクロール可能にする。
	// 縦方向にスクロールが必要になるのを避けるため(バトル画面レイアウト再設計、
	// docs/architecture.md)、ここで作るのは横スクロールの行単体のみで、
	// どのパネルへ追加するかは呼び出し側に委ねる。
	UScrollBox* MakeScrollableRow(UWidgetTree* WidgetTree, const TCHAR* Name, UHorizontalBox*& OutInnerBox)
	{
		UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
			*FString::Printf(TEXT("%sScroll"), Name));
		ScrollBox->SetOrientation(EOrientation::Orient_Horizontal);

		OutInnerBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name);
		ScrollBox->AddChild(OutInnerBox);
		return ScrollBox;
	}

	// マーケットのサイドレールは縦長のため、横一列よりも縦積みのほうが見やすい
	// (横一列だと1〜2枚しか見えず横スクロールが必要という指摘のため)。
	UScrollBox* MakeScrollableColumn(UWidgetTree* WidgetTree, const TCHAR* Name, UVerticalBox*& OutInnerBox)
	{
		UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
			*FString::Printf(TEXT("%sScroll"), Name));
		ScrollBox->SetOrientation(EOrientation::Orient_Vertical);

		OutInnerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name);
		ScrollBox->AddChild(OutInnerBox);
		return ScrollBox;
	}

	// 墓地ビューアーのように「普段は1〜2枚だが、条件次第で候補が増えることがある」
	// 一覧を、横スクロールではなく折り返し(2列以上)で見せたい場合に使う。WrapBoxは
	// 自分に割り当てられた幅を超えたら自動で次の行へ折り返すため、割り当て幅を
	// MaxWidthで明示的に制限することで折り返す列数を制御している。折り返しても
	// なお収まらない分は縦スクロールで対応する。
	UScrollBox* MakeScrollableWrap(UWidgetTree* WidgetTree, const TCHAR* Name, float MaxWidth, UWrapBox*& OutInnerBox)
	{
		UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
			*FString::Printf(TEXT("%sScroll"), Name));
		ScrollBox->SetOrientation(EOrientation::Orient_Vertical);

		USizeBox* WidthLimiter = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sWidthLimiter"), Name));
		WidthLimiter->SetMaxDesiredWidth(MaxWidth);

		OutInnerBox = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), Name);
		OutInnerBox->SetInnerSlotPadding(FVector2D(0.f, CardGap * 2.f));

		if (USizeBoxSlot* WrapSlot = Cast<USizeBoxSlot>(WidthLimiter->AddChild(OutInnerBox)))
		{
			WrapSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		// ScrollBoxのスロットは既定でHAlign_Fillのため、そのままだと中のSizeBoxが
		// ビューポート幅いっぱいに引き伸ばされてしまい、SetMaxDesiredWidth()が
		// 効かない(SizeBox::OnArrangeChildren()は自分に割り当てられた幅をそのまま
		// 子へ渡すため)。Centerにすることで「割り当てられた幅」自体をSizeBoxの
		// 希望サイズ(=MaxDesiredWidthで制限された幅)に基づかせ、内側のWrapBoxが
		// 実際にその幅で折り返すようにする。
		if (UScrollBoxSlot* LimiterSlot = Cast<UScrollBoxSlot>(ScrollBox->AddChild(WidthLimiter)))
		{
			LimiterSlot->SetHorizontalAlignment(HAlign_Center);
		}
		return ScrollBox;
	}

	void AddSectionHeader(UWidgetTree* WidgetTree, UVerticalBox* Root, const TCHAR* Name, const FString& Text)
	{
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Header->SetText(FText::FromString(Text));
		Header->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12));
		if (UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(Header))
		{
			RowSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 0.f));
		}
	}

	// プレイヤー情報(付随情報テキスト)用の小さな石のチップ背景。バトルフィールド
	// 等の大きな枠(MakeStoneFramePanel)と同じ質感を、文字1〜2行ぶんの小さな
	// 領域に凝縮したもの(「プレイヤーの情報をもう少し見やすくできますか」という
	// フィードバックへの対応)。中身の分量に合わせて自然な大きさになる(Auto)。
	UBorder* WrapTextInInfoChip(UWidgetTree* WidgetTree, const TCHAR* Name, UWidget* Content)
	{
		UBorder* Chip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		FSlateBrush ChipBrush;
		ChipBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		ChipBrush.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.42f));
		ChipBrush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(8.f, 8.f, 8.f, 8.f), FSlateColor(FLinearColor(0.65f, 0.60f, 0.48f, 0.7f)), 1.5f);
		Chip->SetBrush(ChipBrush);
		Chip->SetPadding(FMargin(10.f, 5.f));
		Chip->AddChild(Content);
		return Chip;
	}

	// バトルフィールド/マーケット/手札、それぞれの区画を石造りの卓のような枠で
	// 囲むヘルパー(「この背景を基にバトルフィールド、マーケット、手札などの枠を
	// 作って見やすくしてください」というフィードバックへの対応)。以前は区画ごとに
	// 彩度の強い色分け(赤/金/青)をしていたが、MTG Arena風レイアウトへの移行時に
	// 撤去していた。今回は色分けではなく、背景の石畳と馴染む中立な石の質感の枠
	// (半透明の石色の地+明るい石の縁取り、角丸)で統一し、「どこからどこまでが
	// 1つの区画か」を分かりやすくすることだけを狙う。テクスチャ資産は使わず、
	// FSlateBrushのRoundedBox描画のみで表現する(MakeCircleBadge等と同じ手法)。
	// 親への追加方法(VerticalBoxへ積む/固定幅のサイドレールへ入れる等)は
	// 呼び出し側ごとに異なるため、ここでは作るだけにして追加は呼び出し側に任せる。
	// OutContentに内部コンテンツ用のVerticalBox(この中に見出し・行を追加していく)を返す。
	UBorder* MakeStoneFramePanel(UWidgetTree* WidgetTree, const TCHAR* Name, UVerticalBox*& OutContent)
	{
		UBorder* Zone = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		FSlateBrush StoneBrush;
		StoneBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		StoneBrush.TintColor = FSlateColor(FLinearColor(0.30f, 0.29f, 0.25f, 0.35f));
		StoneBrush.OutlineSettings = FSlateBrushOutlineSettings(
			FVector4(14.f, 14.f, 14.f, 14.f), FSlateColor(FLinearColor(0.68f, 0.63f, 0.50f, 0.9f)), 2.f);
		Zone->SetBrush(StoneBrush);
		Zone->SetHorizontalAlignment(HAlign_Fill);
		Zone->SetVerticalAlignment(VAlign_Fill);
		Zone->SetPadding(FMargin(14.f, 8.f));
		// 枠の外へ中身がはみ出して他のバー(自分の情報/ターン終了ボタン等)に
		// 重なって見えてしまうのを防ぐ(「かぶっていて見にくい」というフィードバックへの対応)。
		Zone->SetClipping(EWidgetClipping::ClipToBounds);

		OutContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
			*FString::Printf(TEXT("%sContent"), Name));
		Zone->AddChild(OutContent);
		return Zone;
	}

	// 敵ゾーンと自分ゾーンの間に置く、卓の中央のような中立な余白。ゾーン自体をAutoに
	// したことで浮いた高さの受け皿をここに集約する(ゾーンの背景色の中に余白が
	// 紛れ込んで間延びして見えるのを防ぐ)。
	void AddNeutralGapSpacer(UWidgetTree* WidgetTree, UVerticalBox* Column)
	{
		USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("NeutralGapSpacer"));
		if (UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Gap))
		{
			RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	// カードの行(横スクロール)は、枚数が少ないときに左端へ張り付かず、
	// ゾーンの中央に寄るようにする(枚数が多くスクロールが必要な場合も動作に支障はない)。
	void AddCenteredRow(UVerticalBox* Content, UScrollBox* ScrollBox, bool bGrow)
	{
		if (UVerticalBoxSlot* RowSlot = Content->AddChildToVerticalBox(ScrollBox))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Center);
			if (bGrow)
			{
				RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
	}

	// 場の行専用: ユニットが0体でもカード1段分の高さを最低保証しつつ(体数の増減で
	// ゾーンの高さがガクガク変わるのを防ぐ)、それ以上には伸ばさない(Fillにして
	// 残り高さを専有すると元の「余白だらけ」の状態に戻ってしまう)。
	void AddBoardRow(UWidgetTree* WidgetTree, UVerticalBox* Content, UScrollBox* ScrollBox, float MinRowHeight)
	{
		USizeBox* MinHeightBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		MinHeightBox->SetMinDesiredHeight(MinRowHeight);
		if (USizeBoxSlot* InnerSlot = Cast<USizeBoxSlot>(MinHeightBox->AddChild(ScrollBox)))
		{
			InnerSlot->SetHorizontalAlignment(HAlign_Center);
			InnerSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* RowSlot = Content->AddChildToVerticalBox(MinHeightBox))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	// MTG Arena風のライフオーブ(円形バッジ)を作る。テクスチャ資産を使わず、
	// FSlateBrushのRoundedBox描画(RoundingType=HalfHeightRadius、幅=高さの
	// 正方形なら自動的に真円になる)だけで表現する。OutNumberTextに、あとで
	// RefreshUIからHPの数字をSetTextするためのUTextBlockを返す。
	USizeBox* MakeCircleBadge(UWidgetTree* WidgetTree, const TCHAR* Name, float Diameter,
		const FLinearColor& FillColor, const FLinearColor& OutlineColor, float OutlineWidth,
		UTextBlock*& OutNumberText)
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("%sSize"), Name));
		SizeBox->SetWidthOverride(Diameter);
		SizeBox->SetHeightOverride(Diameter);

		UBorder* Circle = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FillColor);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FSlateColor(OutlineColor), OutlineWidth);
		Circle->SetBrush(Brush);
		Circle->SetHorizontalAlignment(HAlign_Center);
		Circle->SetVerticalAlignment(VAlign_Center);
		if (USizeBoxSlot* InnerSlot = Cast<USizeBoxSlot>(SizeBox->AddChild(Circle)))
		{
			InnerSlot->SetHorizontalAlignment(HAlign_Fill);
			InnerSlot->SetVerticalAlignment(VAlign_Fill);
		}

		OutNumberText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("%sText"), Name));
		OutNumberText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::RoundToInt(Diameter * 0.34f)));
		OutNumberText->SetJustification(ETextJustify::Center);
		Circle->AddChild(OutNumberText);

		return SizeBox;
	}

	// ボタンの見た目を、既定のUMGボタン(平坦な灰色の四角)から角丸+押下フィードバック
	// ありのものに差し替える(「見た目を良くしたい」フィードバックへの対応)。
	// テクスチャ資産は使わず、FSlateBrushのRoundedBox描画のみで表現する。
	void ApplyStyledButtonLook(UButton* Button, const FLinearColor& BaseColor, float Radius = 10.f)
	{
		if (!Button)
		{
			return;
		}
		auto MakeState = [Radius](const FLinearColor& Color)
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius);
			return Brush;
		};

		FButtonStyle Style = Button->GetStyle();
		Style.Normal = MakeState(BaseColor);
		Style.Hovered = MakeState(FLinearColor(FMath::Min(BaseColor.R * 1.2f, 1.f), FMath::Min(BaseColor.G * 1.2f, 1.f), FMath::Min(BaseColor.B * 1.2f, 1.f), BaseColor.A));
		Style.Pressed = MakeState(FLinearColor(BaseColor.R * 0.75f, BaseColor.G * 0.75f, BaseColor.B * 0.75f, BaseColor.A));
		Style.Disabled = MakeState(FLinearColor(BaseColor.R, BaseColor.G, BaseColor.B, 0.35f));
		Style.NormalPadding = FMargin(0.f);
		Style.PressedPadding = FMargin(0.f);
		Button->SetStyle(Style);
	}

	// 写実的な戦場背景の上に白文字を置くと、明るい部分に重なったときに読みにくく
	// なる(「プレイヤーの情報をもう少し見やすくできますか」というフィードバック
	// への対応)。黒い縁取り(FFontOutlineSettings、マテリアル不要)を付けて、
	// 背景の明暗によらず常にくっきり読めるようにする。
	void ApplyTextOutline(UTextBlock* Text, int32 OutlineSize = 1)
	{
		if (!Text)
		{
			return;
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.OutlineSettings.OutlineSize = OutlineSize;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		Text->SetFont(Font);
	}

	FString GetColorDisplayName(ECGColor Color)
	{
		switch (Color)
		{
		case ECGColor::Red:    return TEXT("赤");
		case ECGColor::Orange: return TEXT("橙");
		case ECGColor::Green:  return TEXT("緑");
		case ECGColor::Blue:   return TEXT("青");
		case ECGColor::Purple: return TEXT("紫");
		default:               return FString();
		}
	}

	// パッシブが「発動しているのは分かるが何をしているのか分からない」という
	// フィードバックへの対応。色名だけでなく発動条件の要約も表示する
	// (docs/game-rules-minimum.md「色ガイド」の各色パッシブ行と対応させること)。
	// 5色とも報酬は共通で「コイン+1、1ターン1回」のため、ここでは条件だけを返す。
	FString GetColorPassiveConditionText(ECGColor Color)
	{
		switch (Color)
		{
		case ECGColor::Red:    return TEXT("3枚目のプレイでコイン+1");
		case ECGColor::Orange: return TEXT("最初の購入でコイン+1");
		case ECGColor::Green:  return TEXT("ターン開始時Unit3体以上でコイン+1");
		case ECGColor::Blue:   return TEXT("2回目のドローでコイン+1");
		case ECGColor::Purple: return TEXT("変貌成功でコイン+1");
		default:               return FString();
		}
	}

	// フィニッシャー(docs/game-rules-minimum.md「フィニッシャー」)の発動条件の
	// 進行度合いを「現在値/必要値」の形でテキスト化する。「フィニッシャーが
	// 出る条件をどれだけ満たしているのか一目でわかるようにしてほしい」という
	// フィードバックへの対応。閾値・後手ハンデ(-2)は
	// ACGPlayerState::CheckAndSpawnFinisher()と必ず一致させること。
	FString GetFinisherProgressText(const ACGPlayerState* Player, ECGColor Color)
	{
		if (!Player)
		{
			return FString();
		}
		if (Player->FinishersSpawned.Contains(Color))
		{
			return TEXT("登場済み");
		}

		int32 Current = 0;
		int32 Threshold = 0;
		switch (Color)
		{
		case ECGColor::Red:    Current = Player->AlliesDiedThisMatch;       Threshold = 10; break;
		case ECGColor::Orange: Current = Player->CardsPurchasedThisMatch;   Threshold = 9;  break;
		case ECGColor::Blue:   Current = Player->CardsDrawnThisMatch;       Threshold = 18; break;
		case ECGColor::Green:  Current = Player->BoardUnits.Num();          Threshold = 8;  break;
		case ECGColor::Purple: Current = Player->UnitsTransformedThisMatch; Threshold = 5;  break;
		default:               return FString();
		}

		const int32 SecondPlayerBonus = Player->bWentSecond ? 2 : 0;
		Threshold = FMath::Max(0, Threshold - SecondPlayerBonus);
		Current = FMath::Min(Current, Threshold);
		return FString::Printf(TEXT("%d/%d"), Current, Threshold);
	}

	// HP以外の付随情報(マナ・山札・墓地・コイン・パッシブ・割引等)をまとめる。
	// HPはMTG Arena風のライフオーブ(円形バッジ、MakeCircleBadge/EnemyOrbText/
	// SelfOrbText)側で大きく表示するため、ここでは扱わない。敵は上、自分は下に
	// 分けて表示する(左右で見比べる必要がないよう、同じ形式の文字列にしている)。
	// 発動中の色パッシブ/現在有効な購入割引も表示する(「パッシブが発動している
	// か分からない」「割引が分かりにくい」というフィードバックへの対応)。
	// 1行に全部詰め込むと「プレイヤーの情報が見づらい」というフィードバックの
	// 原因になっていたため、常に出る基本情報(1行目)と、条件付きで出る経済/
	// パッシブ情報(2行目、何も無ければ出さない)に分けている。
	FString FormatPlayerSecondaryInfoText(const ACGPlayerState* Player)
	{
		if (!Player)
		{
			return FString();
		}
		// 山札はDeckCardIds.Num()ではなくDeckCount(公開複製用)を使う。相手の
		// DeckCardIdsは非公開情報のためオンライン対戦ではクライアントに複製
		// されず、Num()が常に0になってしまう(docs/online-play-design.md
		// 「`ACGPlayerState`のレプリケーション」)。
		const FString BaseLine = FString::Printf(TEXT("MP %d/%d   山札 %d   墓地 %d"),
			Player->CurrentMana, Player->MaxMana,
			Player->DeckCount, Player->DiscardCardIds.Num());

		// コイン・パッシブ・購入軽減は、以前は0/未発動のときに表示を省いていたが、
		// 「プレイヤーのステイタスとして常に表示してほしい」というフィードバックへの
		// 対応で、値が0や未発動でも常に表示したままにする。
		FString PassiveText = TEXT("-");
		if (Player->ActiveColors.Num() > 0)
		{
			TArray<FString> ColorLabels;
			for (const ECGColor Color : Player->ActiveColors)
			{
				const FString Condition = GetColorPassiveConditionText(Color);
				ColorLabels.Add(Condition.IsEmpty()
					? GetColorDisplayName(Color)
					: FString::Printf(TEXT("%s: %s"), *GetColorDisplayName(Color), *Condition));
			}
			PassiveText = FString::Join(ColorLabels, TEXT("/"));
		}

		// 今すぐ購入すれば実際に何点軽減されるか(荒野の物々交換人/橙パッシブ未使用/
		// 先物/O13の合計)。
		const int32 CurrentDiscount = Player->ComputeCurrentPurchaseDiscount();

		// フィニッシャーの進行度も、パッシブと同様にアクティブな色ごとに
		// 「現在値/必要値」を並べる(「登場済み」の色は達成済みとわかる表記にする)。
		FString FinisherText = TEXT("-");
		if (Player->ActiveColors.Num() > 0)
		{
			TArray<FString> FinisherLabels;
			for (const ECGColor Color : Player->ActiveColors)
			{
				const FString Progress = GetFinisherProgressText(Player, Color);
				if (!Progress.IsEmpty())
				{
					FinisherLabels.Add(FString::Printf(TEXT("%s: %s"), *GetColorDisplayName(Color), *Progress));
				}
			}
			if (FinisherLabels.Num() > 0)
			{
				FinisherText = FString::Join(FinisherLabels, TEXT("/"));
			}
		}

		TArray<FString> ExtraParts;
		ExtraParts.Add(FString::Printf(TEXT("コイン %d"), Player->PurchaseMana));
		ExtraParts.Add(FString::Printf(TEXT("購入割引 %d"), CurrentDiscount));
		ExtraParts.Add(FString::Printf(TEXT("パッシブ[%s]"), *PassiveText));
		ExtraParts.Add(FString::Printf(TEXT("フィニッシャー[%s]"), *FinisherText));

		if (Player->bBuffBoardOnPurchaseThisTurn)
		{
			ExtraParts.Add(TEXT("購入時+1/+0中"));
		}

		return FString::Printf(TEXT("%s\n%s"), *BaseLine, *FString::Join(ExtraParts, TEXT("   ")));
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

	// WidgetTree->RootWidgetの設定(BuildRootOverlay呼び出し)はChoiceModalLayerを
	// 組み立てた後、関数末尾でまとめて行う(ModalLayerとして渡す必要があるため)。

	// 最上部: ターン数/勝敗のみを表示する細いバナー。HP/マナ等は敵情報/自分情報へ分離した。
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16));
	StatusText->SetJustification(ETextJustify::Center);
	ApplyTextOutline(StatusText, 2);
	if (UVerticalBoxSlot* StatusSlot = Root->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
		StatusSlot->SetPadding(FMargin(16.f, 6.f, 16.f, 2.f));
	}

	// 選択式カード効果/攻撃対象選択の選択待ち中だけ表示するプロンプト
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。普段はCollapsed。
	ChoicePromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChoicePromptText"));
	ChoicePromptText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15));
	ChoicePromptText->SetJustification(ETextJustify::Center);
	ChoicePromptText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.3f)));
	ChoicePromptText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* PromptSlot = Root->AddChildToVerticalBox(ChoicePromptText))
	{
		PromptSlot->SetHorizontalAlignment(HAlign_Center);
		PromptSlot->SetPadding(FMargin(16.f, 0.f, 16.f, 2.f));
	}

	// EnemyOrFaceTarget選択待ち中だけ表示する「顔面を狙う」ボタン(docs/card-effect-
	// player-choice-plan.md「③敵ユニット/顔面を選ぶ」)。敵ユニットは盤面のカードを
	// 直接クリックして選ぶが、顔面はカードが無いためボタンで選べるようにする。
	TargetFaceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TargetFaceButton"));
	UTextBlock* TargetFaceLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TargetFaceLabel"));
	TargetFaceLabel->SetText(FText::FromString(TEXT("顔面を狙う")));
	TargetFaceLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	TargetFaceLabel->SetJustification(ETextJustify::Center);
	TargetFaceButton->AddChild(TargetFaceLabel);
	TargetFaceButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleTargetFaceClicked);
	TargetFaceButton->SetVisibility(ESlateVisibility::Collapsed);
	ApplyStyledButtonLook(TargetFaceButton, FLinearColor(0.55f, 0.12f, 0.10f, 1.f));
	if (UVerticalBoxSlot* TargetFaceSlot = Root->AddChildToVerticalBox(TargetFaceButton))
	{
		TargetFaceSlot->SetHorizontalAlignment(HAlign_Center);
		TargetFaceSlot->SetPadding(FMargin(16.f, 0.f, 16.f, 2.f));
	}

	// 墓地/山札スクライのように普段は画面に無いカードを選ぶときは、専用のポップアップを
	// 画面の一番手前に表示して選ぶ(docs/architecture.md「選択待ち(PendingChoice)の仕組み」、
	// 「墓地が見にくい」というフィードバックを受けて一時墓地ビューアーから移行)。
	// 背景を暗く覆うことで他の操作から切り離し、選んだ後は閉じて拡大カードが
	// 画面に残らないようにする(RefreshUI側でHidePreview()と合わせて解決)。
	ChoiceModalLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChoiceModalLayer"));
	ChoiceModalLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
	// このBorder自身のHAlign/VAlignは「自分の子(ModalPanel)をどう配置するか」を
	// 決めるものであり、画面全体を覆うかどうかとは別(画面全体を覆うのは、この
	// ウィジェットを追加するOverlaySlot側のFill指定で行う)。ここでは子を中央に置く。
	ChoiceModalLayer->SetHorizontalAlignment(HAlign_Center);
	ChoiceModalLayer->SetVerticalAlignment(VAlign_Center);
	ChoiceModalLayer->SetVisibility(ESlateVisibility::Collapsed);

	// ポップアップのサイズは中身(候補の枚数)によらず固定にする(選ぶたびに大きさが
	// 変わって見づらい/カード1枚ぶんだと小さすぎる、というフィードバックのため)。
	// 高さは、墓地ビューアーが2〜3行に折り返した場合でも(それを超える分は縦スクロール
	// で見る前提で)ちょうどよく収まるよう460から拡張している。
	USizeBox* ModalSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ChoiceModalSizeBox"));
	ModalSizeBox->SetWidthOverride(860.f);
	ModalSizeBox->SetHeightOverride(600.f);
	ChoiceModalLayer->AddChild(ModalSizeBox);

	UBorder* ModalPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChoiceModalPanel"));
	ModalPanel->SetBrushColor(FLinearColor(0.16f, 0.14f, 0.05f, 0.98f));
	ModalPanel->SetPadding(FMargin(24.f, 20.f));
	// 中身(ModalContent)は固定サイズいっぱいに引き伸ばさず、内容量(墓地の候補枚数/
	// スクライの1枚)によらず常にパネル中央に来るようにする(ModalPanel自身の
	// HAlign/VAlignは「自分の子をどう配置するか」を決めるものなのでCenterにする。
	// 背景色を固定サイズいっぱいに敷くのは下のModalPanelSlot側のFill指定で行う)。
	ModalPanel->SetHorizontalAlignment(HAlign_Center);
	ModalPanel->SetVerticalAlignment(VAlign_Center);
	// USizeBoxは子を明示的にFill指定しないと自然なサイズのまま配置してしまう
	// (docs/architecture.md「カードUIの設計」で扱った既知の落とし穴と同種)。
	if (USizeBoxSlot* ModalPanelSlot = Cast<USizeBoxSlot>(ModalSizeBox->AddChild(ModalPanel)))
	{
		ModalPanelSlot->SetHorizontalAlignment(HAlign_Fill);
		ModalPanelSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* ModalContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChoiceModalContent"));
	ModalPanel->AddChild(ModalContent);

	// GraveyardCard選択待ち中だけ表示する一時墓地ビューアー(docs/card-effect-
	// player-choice-plan.md「②墓地から1枚選ぶ」)。普段は枚数しか見せない墓地の
	// 中身を、選択が必要なときだけここに表示する。
	GraveyardModalSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GraveyardModalSection"));
	AddSectionHeader(WidgetTree, GraveyardModalSection, TEXT("GraveyardViewerHeader"), TEXT("墓地"));

	// 候補が多いときに横一列(横スクロール)ではなく複数行に折り返して見せる
	// (「墓地ポップアップを2列で見たい」というフィードバックのため。MakeScrollableWrap)。
	UWrapBox* GraveyardInner = nullptr;
	UScrollBox* GraveyardViewerScroll = MakeScrollableWrap(WidgetTree, TEXT("GraveyardViewerBox"), GraveyardViewerWrapWidth, GraveyardInner);
	GraveyardViewerBox = GraveyardInner;
	if (UVerticalBoxSlot* GraveyardRowSlot = GraveyardModalSection->AddChildToVerticalBox(GraveyardViewerScroll))
	{
		GraveyardRowSlot->SetHorizontalAlignment(HAlign_Center);
		GraveyardRowSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	GraveyardModalSection->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* GraveyardSectionSlot = ModalContent->AddChildToVerticalBox(GraveyardModalSection))
	{
		GraveyardSectionSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// KeepOrBury選択待ち中だけ表示する、山札の一番上のカード+2択ボタン
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	ScryBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ScryBox"));
	ScryBox->SetVisibility(ESlateVisibility::Collapsed);

	ScryCardContainer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ScryCardContainer"));
	if (UHorizontalBoxSlot* ScryCardSlot = ScryBox->AddChildToHorizontalBox(ScryCardContainer))
	{
		ScryCardSlot->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* ScryButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ScryButtonColumn"));

	KeepOnTopButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("KeepOnTopButton"));
	UTextBlock* KeepOnTopLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeepOnTopLabel"));
	KeepOnTopLabel->SetText(FText::FromString(TEXT("上に残す")));
	KeepOnTopLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	KeepOnTopLabel->SetJustification(ETextJustify::Center);
	KeepOnTopButton->AddChild(KeepOnTopLabel);
	KeepOnTopButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleKeepOnTopClicked);
	ApplyStyledButtonLook(KeepOnTopButton, FLinearColor(0.18f, 0.35f, 0.42f, 1.f));
	if (UVerticalBoxSlot* KeepOnTopSlot = ScryButtonColumn->AddChildToVerticalBox(KeepOnTopButton))
	{
		KeepOnTopSlot->SetPadding(FMargin(8.f, 2.f));
	}

	SendToBottomButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SendToBottomButton"));
	UTextBlock* SendToBottomLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SendToBottomLabel"));
	SendToBottomLabel->SetText(FText::FromString(TEXT("下に送る")));
	SendToBottomLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	SendToBottomLabel->SetJustification(ETextJustify::Center);
	SendToBottomButton->AddChild(SendToBottomLabel);
	SendToBottomButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleSendToBottomClicked);
	ApplyStyledButtonLook(SendToBottomButton, FLinearColor(0.18f, 0.35f, 0.42f, 1.f));
	if (UVerticalBoxSlot* SendToBottomSlot = ScryButtonColumn->AddChildToVerticalBox(SendToBottomButton))
	{
		SendToBottomSlot->SetPadding(FMargin(8.f, 2.f));
	}

	if (UHorizontalBoxSlot* ScryButtonColumnSlot = ScryBox->AddChildToHorizontalBox(ScryButtonColumn))
	{
		ScryButtonColumnSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* ScryRowSlot = ModalContent->AddChildToVerticalBox(ScryBox))
	{
		ScryRowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// BuyDestination選択待ち中だけ表示する、購入したカード+2択ボタン
	// (「購入時に行き先を選べるようにしてほしい」というフィードバックのため。
	// ScryBoxと全く同じ構造)。
	BuyDestinationBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuyDestinationBox"));
	BuyDestinationBox->SetVisibility(ESlateVisibility::Collapsed);

	BuyDestinationCardContainer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuyDestinationCardContainer"));
	if (UHorizontalBoxSlot* BuyDestinationCardSlot = BuyDestinationBox->AddChildToHorizontalBox(BuyDestinationCardContainer))
	{
		BuyDestinationCardSlot->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* BuyDestinationButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuyDestinationButtonColumn"));

	ToHandButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToHandButton"));
	UTextBlock* ToHandLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToHandLabel"));
	ToHandLabel->SetText(FText::FromString(TEXT("手札へ")));
	ToHandLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	ToHandLabel->SetJustification(ETextJustify::Center);
	ToHandButton->AddChild(ToHandLabel);
	ToHandButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleToHandClicked);
	ApplyStyledButtonLook(ToHandButton, FLinearColor(0.18f, 0.35f, 0.42f, 1.f));
	if (UVerticalBoxSlot* ToHandSlot = BuyDestinationButtonColumn->AddChildToVerticalBox(ToHandButton))
	{
		ToHandSlot->SetPadding(FMargin(8.f, 2.f));
	}

	ToDeckBottomButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToDeckBottomButton"));
	UTextBlock* ToDeckBottomLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ToDeckBottomLabel"));
	ToDeckBottomLabel->SetText(FText::FromString(TEXT("山札の下へ")));
	ToDeckBottomLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	ToDeckBottomLabel->SetJustification(ETextJustify::Center);
	ToDeckBottomButton->AddChild(ToDeckBottomLabel);
	ToDeckBottomButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleToDeckBottomClicked);
	ApplyStyledButtonLook(ToDeckBottomButton, FLinearColor(0.18f, 0.35f, 0.42f, 1.f));
	if (UVerticalBoxSlot* ToDeckBottomSlot = BuyDestinationButtonColumn->AddChildToVerticalBox(ToDeckBottomButton))
	{
		ToDeckBottomSlot->SetPadding(FMargin(8.f, 2.f));
	}

	if (UHorizontalBoxSlot* BuyDestinationButtonColumnSlot = BuyDestinationBox->AddChildToHorizontalBox(BuyDestinationButtonColumn))
	{
		BuyDestinationButtonColumnSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* BuyDestinationRowSlot = ModalContent->AddChildToVerticalBox(BuyDestinationBox))
	{
		BuyDestinationRowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// 行動ログ(次期ルール):「CPUと対戦したときに何をされたのか分からない」という
	// フィードバックへの対応。ChoiceModalLayerと違いPendingChoiceに連動せず、
	// ActionLogToggleButtonでいつでも開閉できる独立したポップアップにする
	// (docs/game-rules-minimum.md「行動ログ」参照)。
	ActionLogModalLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ActionLogModalLayer"));
	ActionLogModalLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
	ActionLogModalLayer->SetHorizontalAlignment(HAlign_Center);
	ActionLogModalLayer->SetVerticalAlignment(VAlign_Center);
	ActionLogModalLayer->SetVisibility(ESlateVisibility::Collapsed);

	USizeBox* ActionLogSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ActionLogSizeBox"));
	ActionLogSizeBox->SetWidthOverride(560.f);
	ActionLogSizeBox->SetHeightOverride(520.f);
	ActionLogModalLayer->AddChild(ActionLogSizeBox);

	UBorder* ActionLogPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ActionLogPanel"));
	ActionLogPanel->SetBrushColor(FLinearColor(0.16f, 0.14f, 0.05f, 0.98f));
	ActionLogPanel->SetPadding(FMargin(20.f, 16.f));
	if (USizeBoxSlot* ActionLogPanelSlot = Cast<USizeBoxSlot>(ActionLogSizeBox->AddChild(ActionLogPanel)))
	{
		ActionLogPanelSlot->SetHorizontalAlignment(HAlign_Fill);
		ActionLogPanelSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* ActionLogPanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ActionLogPanelContent"));
	ActionLogPanel->AddChild(ActionLogPanelContent);

	UHorizontalBox* ActionLogHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ActionLogHeaderRow"));
	UTextBlock* ActionLogHeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActionLogHeaderText"));
	ActionLogHeaderText->SetText(FText::FromString(TEXT("行動ログ")));
	ActionLogHeaderText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
	if (UHorizontalBoxSlot* ActionLogHeaderTextSlot = ActionLogHeaderRow->AddChildToHorizontalBox(ActionLogHeaderText))
	{
		ActionLogHeaderTextSlot->SetVerticalAlignment(VAlign_Center);
		ActionLogHeaderTextSlot->SetHorizontalAlignment(HAlign_Left);
		ActionLogHeaderTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UButton* ActionLogCloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ActionLogCloseButton"));
	UTextBlock* ActionLogCloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActionLogCloseLabel"));
	ActionLogCloseLabel->SetText(FText::FromString(TEXT("閉じる")));
	ActionLogCloseLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	ActionLogCloseLabel->SetJustification(ETextJustify::Center);
	ActionLogCloseButton->AddChild(ActionLogCloseLabel);
	ActionLogCloseButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleActionLogCloseClicked);
	ApplyStyledButtonLook(ActionLogCloseButton, FLinearColor(0.28f, 0.28f, 0.30f, 1.f));
	if (UHorizontalBoxSlot* ActionLogCloseSlot = ActionLogHeaderRow->AddChildToHorizontalBox(ActionLogCloseButton))
	{
		ActionLogCloseSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* ActionLogHeaderRowSlot = ActionLogPanelContent->AddChildToVerticalBox(ActionLogHeaderRow))
	{
		ActionLogHeaderRowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	UScrollBox* ActionLogScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ActionLogScroll"));
	ActionLogListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ActionLogListBox"));
	ActionLogScroll->AddChild(ActionLogListBox);
	if (UVerticalBoxSlot* ActionLogScrollSlot = ActionLogPanelContent->AddChildToVerticalBox(ActionLogScroll))
	{
		ActionLogScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// MTG Arena風のレイアウト(「MTGアリーナを参考にして画面を作ってください」への
	// 対応、docs/architecture.md「カードUIの設計」)。以前は敵/自分それぞれの場を
	// 彩度の強い色つきパネル+「敵の場」「自分の場」という文字見出しで囲んでいたが、
	// MTG Arenaは場をゾーン名で区切らず、盤面全体を1つの続いた卓として見せている。
	// これに合わせて場のパネル・見出しは一旦廃止し、HPは中央上下のライフオーブ
	// (円形バッジ、MakeCircleBadge)で目立たせ、細かい情報(マナ/山札/墓地等)は
	// その隣に小さく添える形に変更した。その後「枠が無いと逆に見づらい」という
	// フィードバックを受け、バトルフィールド/マーケット/手札それぞれを石造りの
	// 枠(MakeStoneFramePanel)で囲み直している(旧来の彩度の強い色分けには戻さない)。

	// 敵の情報バー(ライフオーブ+付随情報)。画面の一番上、卓全体を見渡す位置に置く。
	UHorizontalBox* EnemyTopBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EnemyTopBar"));
	UTextBlock* EnemyOrbTextRaw = nullptr;
	USizeBox* EnemyOrbBox = MakeCircleBadge(WidgetTree, TEXT("EnemyOrb"), 54.f,
		FLinearColor(0.12f, 0.05f, 0.05f, 0.95f), FLinearColor(0.55f, 0.22f, 0.2f, 1.f), 2.f, EnemyOrbTextRaw);
	EnemyOrbText = EnemyOrbTextRaw;
	if (UHorizontalBoxSlot* OrbSlot = EnemyTopBar->AddChildToHorizontalBox(EnemyOrbBox))
	{
		OrbSlot->SetVerticalAlignment(VAlign_Center);
		OrbSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}
	EnemySecondaryInfoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnemySecondaryInfoText"));
	EnemySecondaryInfoText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13));
	ApplyTextOutline(EnemySecondaryInfoText);
	if (UHorizontalBoxSlot* InfoSlot = EnemyTopBar->AddChildToHorizontalBox(
		WrapTextInInfoChip(WidgetTree, TEXT("EnemySecondaryInfoChip"), EnemySecondaryInfoText)))
	{
		InfoSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(EnemyTopBar))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetPadding(FMargin(16.f, 4.f, 16.f, 6.f));
	}

	// マーケットは敵/自分どちらの陣地でもない共有ゾーンだが、敵の場・自分の場と
	// 横一列に並べて縦に積むと、場の縦幅を圧迫してカードが窮屈になっていた。
	// マーケットだけ右側の縦長サイドレールへ独立させ、中央列(敵/自分の陣地)が
	// 画面の縦幅をより多く使えるようにする。
	UHorizontalBox* MainRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainRow"));
	if (UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(MainRow))
	{
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* CenterColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterColumn"));
	if (UHorizontalBoxSlot* ColSlot = MainRow->AddChildToHorizontalBox(CenterColumn))
	{
		ColSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ColSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 敵の手札(縮小、裏向き)。専用の石枠で囲み、盤面と区別する。
	UVerticalBox* EnemyHandZone = nullptr;
	UBorder* EnemyHandPanel = MakeStoneFramePanel(WidgetTree, TEXT("EnemyHandZone"), EnemyHandZone);
	UHorizontalBox* EnemyHandInner = nullptr;
	UScrollBox* EnemyHandScroll = MakeScrollableRow(WidgetTree, TEXT("EnemyHandBox"), EnemyHandInner);
	EnemyHandBox = EnemyHandInner;
	AddCenteredRow(EnemyHandZone, EnemyHandScroll, /*bGrow=*/false);
	if (UVerticalBoxSlot* EnemyHandRowSlot = CenterColumn->AddChildToVerticalBox(EnemyHandPanel))
	{
		EnemyHandRowSlot->SetHorizontalAlignment(HAlign_Center);
		EnemyHandRowSlot->SetPadding(FMargin(0.f, 2.f));
	}

	// バトルフィールド: 敵の場+中央線+自分の場をまとめて1つの石枠で囲む
	// (「バトルフィールドの枠を作ってほしい」というフィードバックへの対応)。
	UVerticalBox* BattlefieldZone = nullptr;
	UBorder* BattlefieldPanel = MakeStoneFramePanel(WidgetTree, TEXT("BattlefieldZone"), BattlefieldZone);

	UHorizontalBox* EnemyBoardInner = nullptr;
	UScrollBox* EnemyBoardScroll = MakeScrollableRow(WidgetTree, TEXT("EnemyBoardBox"), EnemyBoardInner);
	EnemyBoardBox = EnemyBoardInner;
	AddBoardRow(WidgetTree, BattlefieldZone, EnemyBoardScroll, BoardRowMinHeight);

	// 卓の中央線。敵陣/自陣の境目を、枠の中でさらに1本の細い線で示す
	// (MTG Arenaの盤面に実際の仕切り線は無いが、対戦相手との境目が全く無いと
	// 逆に分かりにくいため、控えめな線1本だけ残す妥協点)。線自体は固定の薄い
	// 高さ(Auto)にし、上下をAddNeutralGapSpacer(Fill)で挟むことで、枠内の
	// 余った高さの受け皿を保ちつつ線が引き伸ばされないようにする。
	AddNeutralGapSpacer(WidgetTree, BattlefieldZone);

	USizeBox* CenterLineSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CenterLineSize"));
	CenterLineSizeBox->SetHeightOverride(2.f);
	UBorder* CenterLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CenterLine"));
	CenterLine->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.12f));
	if (USizeBoxSlot* LineInnerSlot = Cast<USizeBoxSlot>(CenterLineSizeBox->AddChild(CenterLine)))
	{
		LineInnerSlot->SetHorizontalAlignment(HAlign_Fill);
		LineInnerSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UVerticalBoxSlot* LineSlot = BattlefieldZone->AddChildToVerticalBox(CenterLineSizeBox))
	{
		LineSlot->SetHorizontalAlignment(HAlign_Fill);
		LineSlot->SetPadding(FMargin(24.f, 0.f));
	}

	AddNeutralGapSpacer(WidgetTree, BattlefieldZone);

	UHorizontalBox* SelfBoardInner = nullptr;
	UScrollBox* SelfBoardScroll = MakeScrollableRow(WidgetTree, TEXT("SelfBoardBox"), SelfBoardInner);
	SelfBoardBox = SelfBoardInner;
	AddBoardRow(WidgetTree, BattlefieldZone, SelfBoardScroll, BoardRowMinHeight);

	if (UVerticalBoxSlot* BattlefieldRowSlot = CenterColumn->AddChildToVerticalBox(BattlefieldPanel))
	{
		// 手札行(敵/自分)・最下部バーは常に全体を表示したいので、画面の高さが
		// 足りないときはバトルフィールド側だけを縮める(Fill)。ここをAuto(既定)の
		// ままにしていると、バトルフィールドが最低高さぶん確保しきれない場合に
		// 最下部バー(自分の情報+ターン終了ボタン)が画面外へ押し出され、手札の
		// 下にかぶって見えてしまっていた。
		BattlefieldRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BattlefieldRowSlot->SetPadding(FMargin(0.f, 2.f));
	}

	// 自分の手札。専用の石枠で囲み、盤面と区別する。
	UVerticalBox* SelfHandZone = nullptr;
	UBorder* SelfHandPanel = MakeStoneFramePanel(WidgetTree, TEXT("SelfHandZone"), SelfHandZone);
	UHorizontalBox* SelfHandInner = nullptr;
	UScrollBox* SelfHandScroll = MakeScrollableRow(WidgetTree, TEXT("HandBox"), SelfHandInner);
	HandBox = SelfHandInner;
	AddCenteredRow(SelfHandZone, SelfHandScroll, /*bGrow=*/false);
	if (UVerticalBoxSlot* SelfHandRowSlot = CenterColumn->AddChildToVerticalBox(SelfHandPanel))
	{
		SelfHandRowSlot->SetHorizontalAlignment(HAlign_Center);
		SelfHandRowSlot->SetPadding(FMargin(0.f, 2.f));
	}

	// マーケットは中央列の右にサイドレールとして固定幅で表示する。
	USizeBox* MarketRailBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MarketRailBox"));
	MarketRailBox->SetWidthOverride(MarketRailWidth);

	UVerticalBox* MarketZone = nullptr;
	// USizeBoxは子を明示的にFill指定しないと自然なサイズのまま配置してしまう
	// (docs/architecture.md「カードUIの設計」で扱った既知の落とし穴と同種)。
	if (USizeBoxSlot* MarketZoneSlot = Cast<USizeBoxSlot>(
		MarketRailBox->AddChild(MakeStoneFramePanel(WidgetTree, TEXT("MarketZone"), MarketZone))))
	{
		MarketZoneSlot->SetHorizontalAlignment(HAlign_Fill);
		MarketZoneSlot->SetVerticalAlignment(VAlign_Fill);
	}

	AddSectionHeader(WidgetTree, MarketZone, TEXT("MarketHeader"), TEXT("マーケット"));

	// レール幅を活かして2列に折り返す(1列縦積みだと横に余白が余ってしまうため)。
	UScrollBox* MarketScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("MarketBoxScroll"));
	MarketScroll->SetOrientation(EOrientation::Orient_Vertical);

	UWrapBox* MarketWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("MarketBox"));
	MarketWrap->SetOrientation(EOrientation::Orient_Horizontal);
	MarketScroll->AddChild(MarketWrap);
	MarketBox = MarketWrap;

	if (UVerticalBoxSlot* RowSlot = MarketZone->AddChildToVerticalBox(MarketScroll))
	{
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	if (UHorizontalBoxSlot* ColSlot = MainRow->AddChildToHorizontalBox(MarketRailBox))
	{
		ColSlot->SetVerticalAlignment(VAlign_Fill);
		ColSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	}

	// 最下部バー: 自分の付随情報 + 自分のライフオーブ(大きめ、卓を見渡す自分の
	// 定位置) + ボタン列。自分の手札は既にCenterColumn側へ移したため、ここは
	// MTG Arenaの「自分のポートレート」に相当する行のみになる。
	UHorizontalBox* BottomBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BottomBar"));

	SelfSecondaryInfoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelfSecondaryInfoText"));
	SelfSecondaryInfoText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13));
	ApplyTextOutline(SelfSecondaryInfoText);
	if (UHorizontalBoxSlot* RowSlot = BottomBar->AddChildToHorizontalBox(
		WrapTextInInfoChip(WidgetTree, TEXT("SelfSecondaryInfoChip"), SelfSecondaryInfoText)))
	{
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RowSlot->SetHorizontalAlignment(HAlign_Right);
		RowSlot->SetVerticalAlignment(VAlign_Center);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	}

	UTextBlock* SelfOrbTextRaw = nullptr;
	USizeBox* SelfOrbBox = MakeCircleBadge(WidgetTree, TEXT("SelfOrb"), 92.f,
		FLinearColor(0.10f, 0.09f, 0.03f, 0.95f), FLinearColor(0.85f, 0.7f, 0.25f, 1.f), 3.f, SelfOrbTextRaw);
	SelfOrbText = SelfOrbTextRaw;
	if (UHorizontalBoxSlot* OrbSlot = BottomBar->AddChildToHorizontalBox(SelfOrbBox))
	{
		OrbSlot->SetVerticalAlignment(VAlign_Center);
		OrbSlot->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
	}

	UVerticalBox* ButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ButtonColumn"));

	USizeBox* EndTurnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EndTurnSizeBox"));
	EndTurnSizeBox->SetWidthOverride(150.f);
	EndTurnSizeBox->SetHeightOverride(52.f);

	EndTurnButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EndTurnButton"));
	UTextBlock* EndTurnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndTurnLabel"));
	EndTurnLabel->SetText(FText::FromString(TEXT("ターン終了")));
	EndTurnLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16));
	EndTurnLabel->SetJustification(ETextJustify::Center);
	EndTurnButton->AddChild(EndTurnLabel);
	EndTurnButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleEndTurnClicked);
	// 自分のライフオーブと同じ金色系にして、最も押す機会の多い主要アクションだと
	// 分かるようにする(MTG Arenaの「ターン終了」ボタンも自分のポートレート付近に
	// 目立つ色で置かれている)。
	ApplyStyledButtonLook(EndTurnButton, FLinearColor(0.55f, 0.42f, 0.12f, 1.f));
	EndTurnSizeBox->AddChild(EndTurnButton);

	if (UVerticalBoxSlot* RowSlot = ButtonColumn->AddChildToVerticalBox(EndTurnSizeBox))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetPadding(FMargin(8.f, 4.f));
	}

	// 行動ログを開くボタン。「CPUと対戦したときに何をされたのか分からない」という
	// フィードバックへの対応。ターン終了ボタンの下、常時表示(選択待ち中でも
	// 見返せるようにする。docs/game-rules-minimum.md「行動ログ」参照)。
	ActionLogToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ActionLogToggleButton"));
	UTextBlock* ActionLogToggleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActionLogToggleLabel"));
	ActionLogToggleLabel->SetText(FText::FromString(TEXT("行動ログ")));
	ActionLogToggleLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	ActionLogToggleLabel->SetJustification(ETextJustify::Center);
	ActionLogToggleButton->AddChild(ActionLogToggleLabel);
	ActionLogToggleButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleActionLogToggleClicked);
	ApplyStyledButtonLook(ActionLogToggleButton, FLinearColor(0.28f, 0.28f, 0.30f, 1.f));
	if (UVerticalBoxSlot* ActionLogToggleSlot = ButtonColumn->AddChildToVerticalBox(ActionLogToggleButton))
	{
		ActionLogToggleSlot->SetHorizontalAlignment(HAlign_Fill);
		ActionLogToggleSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 4.f));
	}

	// 勝敗確定後のみ表示する導線。それまではCollapsedにしておく(RefreshUIで切り替える)。
	// 高さ固定のSizeBoxで包まないのは、Collapsed時にVerticalBox上で実際にスペースごと
	// 消えるようにするため(SizeBoxはHeightOverrideを子の可視状態に関わらず親へ申告
	// してしまい、隠れているのに空白が残ってしまう)。
	BackToLobbyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BackToLobbyButton"));
	UTextBlock* BackToLobbyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BackToLobbyLabel"));
	BackToLobbyLabel->SetText(FText::FromString(TEXT("ロビーへ戻る")));
	BackToLobbyLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14));
	BackToLobbyLabel->SetJustification(ETextJustify::Center);
	BackToLobbyButton->AddChild(BackToLobbyLabel);
	BackToLobbyButton->OnClicked.AddDynamic(this, &UCGGameHUD::HandleBackToLobbyClicked);
	BackToLobbyButton->SetVisibility(ESlateVisibility::Collapsed);
	ApplyStyledButtonLook(BackToLobbyButton, FLinearColor(0.28f, 0.28f, 0.30f, 1.f));

	if (UVerticalBoxSlot* RowSlot = ButtonColumn->AddChildToVerticalBox(BackToLobbyButton))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 4.f));
	}

	if (UHorizontalBoxSlot* RowSlot = BottomBar->AddChildToHorizontalBox(ButtonColumn))
	{
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	if (UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(BottomBar))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetPadding(FMargin(12.f, 4.f, 12.f, 8.f));
	}

	// 画面の一番奥に、戦場らしい雰囲気を出すための背景を敷く(「フィールドをもっと
	// リアルな戦場をモチーフにしたものに変更してください」というフィードバックへの
	// 対応)。ユーザー提供の戦場イラスト(Content/CardGame/Textures/
	// T_BattlefieldBackground、取り込み元はCardGame/SourceArt/参照)があればそれを、
	// 無ければ(未取り込み環境向けの保険として)単色+光暈のプレースホルダーに
	// フォールバックする。
	UOverlay* ContentWithBackground = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ContentWithBackground"));

	UTexture2D* BattlefieldTexture = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/CardGame/Textures/T_BattlefieldBackground.T_BattlefieldBackground"));

	if (BattlefieldTexture)
	{
		// UScaleBoxのScaleToFillは、アスペクト比を保ったまま画面全体を覆うように
		// 拡大し、はみ出た分は切り取る(CSSのbackground-size: coverと同じ挙動)。
		// 画面の縦横比が画像と多少違っても、上下左右に余白が出ないようにするため。
		UScaleBox* BattlefieldScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("BattlefieldScaleBox"));
		BattlefieldScaleBox->SetStretch(EStretch::ScaleToFill);

		UImage* BattlefieldImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BattlefieldBackgroundImage"));
		BattlefieldImage->SetBrushFromTexture(BattlefieldTexture, /*bMatchSize=*/true);
		BattlefieldScaleBox->AddChild(BattlefieldImage);

		if (UOverlaySlot* BgSlot = ContentWithBackground->AddChildToOverlay(BattlefieldScaleBox))
		{
			BgSlot->SetHorizontalAlignment(HAlign_Fill);
			BgSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// 画像自体に十分な質感・陰影があるため、コード側の光暈演出は重ねない
		// (単色プレースホルダー時代の名残、二重に飾ると煩雑になるため省略)。

		// 情報量の多い写実的な背景の上に白文字を乗せると読みにくくなるため、
		// 背景と実際のUI(Root)の間に薄暗い半透明の幕を1枚挟んで、文字の
		// 可読性を保つ(背景の雰囲気自体はうっすら透けて見える程度に抑える)。
		UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattlefieldScrim"));
		Scrim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.32f));
		if (UOverlaySlot* ScrimSlot = ContentWithBackground->AddChildToOverlay(Scrim))
		{
			ScrimSlot->SetHorizontalAlignment(HAlign_Fill);
			ScrimSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	else
	{
		UE_LOG(LogCardGame, Warning, TEXT("UCGGameHUD::EnsureWidgetTreeBuilt: T_BattlefieldBackground not found, falling back to flat color background"));

		UBorder* BattlefieldBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattlefieldBackground"));
		BattlefieldBackground->SetBrushColor(FLinearColor(0.045f, 0.035f, 0.025f, 1.f));
		if (UOverlaySlot* BgSlot = ContentWithBackground->AddChildToOverlay(BattlefieldBackground))
		{
			BgSlot->SetHorizontalAlignment(HAlign_Fill);
			BgSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// 自分のライフオーブ付近から仄かに灯りが漏れているような、大きく淡い暖色の円。
		// テクスチャ不要のRoundedBox+HalfHeightRadiusのみで表現する。
		USizeBox* GlowSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BattlefieldGlowSize"));
		GlowSizeBox->SetWidthOverride(1100.f);
		GlowSizeBox->SetHeightOverride(1100.f);
		UBorder* Glow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattlefieldGlow"));
		{
			FSlateBrush GlowBrush;
			GlowBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			GlowBrush.TintColor = FSlateColor(FLinearColor(0.35f, 0.24f, 0.08f, 0.10f));
			Glow->SetBrush(GlowBrush);
		}
		if (USizeBoxSlot* GlowInnerSlot = Cast<USizeBoxSlot>(GlowSizeBox->AddChild(Glow)))
		{
			GlowInnerSlot->SetHorizontalAlignment(HAlign_Fill);
			GlowInnerSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* GlowSlot = ContentWithBackground->AddChildToOverlay(GlowSizeBox))
		{
			GlowSlot->SetHorizontalAlignment(HAlign_Center);
			GlowSlot->SetVerticalAlignment(VAlign_Bottom);
			GlowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, -500.f));
		}
	}

	if (UOverlaySlot* ContentSlot = ContentWithBackground->AddChildToOverlay(Root))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// カードのホバー拡大プレビューを最前面に出すための共通レイヤーと、選択ポップアップ
	// (ChoiceModalLayer)/行動ログポップアップ(ActionLogModalLayer)をRootに重ねる。
	// BuildRootOverlay()はModalLayerを1つしか受け取らないため、両方を1つのUOverlayに
	// まとめてから渡す(行動ログはActionLogToggleButtonでいつでも独立に開閉できる、
	// ChoiceModalLayerとは別の重ね方をしなくても両者が同時に表示されることは無い)。
	// MainContentとホバープレビュー層の間に挟むことで、ポップアップ内のカードを
	// ホバーしたときの拡大表示がポップアップの下に隠れないようにしている
	// (UCGCardHostWidget::BuildRootOverlay、docs/architecture.md「ホバー拡大とZ順序」、
	// docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	UOverlay* TopModalStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TopModalStack"));
	if (UOverlaySlot* ChoiceLayerSlot = TopModalStack->AddChildToOverlay(ChoiceModalLayer))
	{
		ChoiceLayerSlot->SetHorizontalAlignment(HAlign_Fill);
		ChoiceLayerSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* ActionLogLayerSlot = TopModalStack->AddChildToOverlay(ActionLogModalLayer))
	{
		ActionLogLayerSlot->SetHorizontalAlignment(HAlign_Fill);
		ActionLogLayerSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 勝敗確定演出(「勝利、敗北したときにもっと派手に演出を入れてほしい」という
	// フィードバックへの対応)。他のどのポップアップよりも手前(TopModalStackの
	// 最後の子)に重ね、全画面を暗く覆う背景+中央の大きなバナー文字で演出する。
	// 実際のスケール/透明度の変化はTickResultAnimation()がNativeTick()から
	// 毎フレーム更新する(ShowFloatingNumber()と同じ手動アニメーション方式)。
	ResultOverlayLayer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultOverlayLayer"));
	ResultOverlayLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	ResultOverlayLayer->SetHorizontalAlignment(HAlign_Fill);
	ResultOverlayLayer->SetVerticalAlignment(VAlign_Fill);

	ResultBannerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResultBannerText"));
	ResultBannerText->SetJustification(ETextJustify::Center);
	{
		FSlateFontInfo BannerFont = ResultBannerText->GetFont();
		BannerFont.Size = 96;
		BannerFont.TypefaceFontName = TEXT("Bold");
		BannerFont.OutlineSettings.OutlineSize = 4;
		BannerFont.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		ResultBannerText->SetFont(BannerFont);
	}
	ResultBannerText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ResultOverlayLayer->AddChild(ResultBannerText);

	// 「演出が出た後操作できなくなる」というフィードバックへの対応。以前は
	// ResultOverlayLayerをHitTestInvisibleにして下のBackToLobbyButtonへクリックを
	// 素通りさせる方式だったが、暗転演出の下で小さなボタンを探させるのは分かりにくい。
	// 全画面を覆う透明なボタンでResultOverlayLayerごと包み、どこをクリックしても
	// ロビーに戻れるようにする(見た目はResultOverlayLayer/ResultBannerTextの
	// ままで、ボタン自体は完全に透明)。
	ResultClickCatcher = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ResultClickCatcher"));
	ApplyStyledButtonLook(ResultClickCatcher, FLinearColor(0.f, 0.f, 0.f, 0.f));
	if (UButtonSlot* ClickCatcherContentSlot = Cast<UButtonSlot>(ResultClickCatcher->AddChild(ResultOverlayLayer)))
	{
		ClickCatcherContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ClickCatcherContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	ResultClickCatcher->OnClicked.AddDynamic(this, &UCGGameHUD::HandleBackToLobbyClicked);
	ResultClickCatcher->SetVisibility(ESlateVisibility::Collapsed);

	if (UOverlaySlot* ResultLayerSlot = TopModalStack->AddChildToOverlay(ResultClickCatcher))
	{
		ResultLayerSlot->SetHorizontalAlignment(HAlign_Fill);
		ResultLayerSlot->SetVerticalAlignment(VAlign_Fill);
	}

	WidgetTree->RootWidget = BuildRootOverlay(ContentWithBackground, TopModalStack);

	UE_LOG(LogCardGame, Log, TEXT("UCGGameHUD::EnsureWidgetTreeBuilt RootWidget=%s"),
		WidgetTree->RootWidget ? *WidgetTree->RootWidget->GetName() : TEXT("null"));
}

ACGGameMode* UCGGameHUD::GetCGGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
}

ACGPlayerController* UCGGameHUD::GetCGPlayerController() const
{
	return Cast<ACGPlayerController>(GetOwningPlayer());
}

int32 UCGGameHUD::GetMySideIndex() const
{
	if (ACGPlayerController* PC = GetCGPlayerController())
	{
		if (ACGPlayerState* PS = PC->GetPlayerState<ACGPlayerState>())
		{
			return PS->SideIndex;
		}
	}
	// オフライン(vs AI)、またはPlayerStateがまだ割り当てられていない場合の
	// フォールバック。オフラインでは常にSide0が人間のため0で問題ない。
	return 0;
}

void UCGGameHUD::RefreshUI()
{
	// カード一覧を作り直す前に、ホバー中だったカードが消えて拡大プレビューだけ
	// 画面に残ってしまう問題を防ぐ(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	HidePreview();

	ACGGameState* CGState = GetWorld() ? GetWorld()->GetGameState<ACGGameState>() : nullptr;
	UE_LOG(LogCardGame, Log, TEXT("UCGGameHUD::RefreshUI CGState=%s Sides=%d"),
		CGState ? TEXT("valid") : TEXT("NULL"), CGState ? CGState->Sides.Num() : -1);
	if (!CGState || CGState->Sides.Num() < 2)
	{
		return;
	}

	// 行動ログが開いている間は、対戦相手の操作(オンライン対戦時)や自分の操作で
	// 新しい行動が追加されるたびに中身も更新する(閉じている間は何もしない)。
	if (ActionLogModalLayer && ActionLogModalLayer->GetVisibility() == ESlateVisibility::Visible)
	{
		PopulateActionLog();
	}

	// 上段=敵/下段=自分の表示位置は手番に関わらず固定。オンラインではホストと
	// ゲストで実際のSideIndexが異なる(GetMySideIndex()、docs/online-play-
	// plan.md「フェーズ4」参照)。
	const int32 MySideIndex = GetMySideIndex();
	ACGPlayerState* SelfSide = CGState->Sides[MySideIndex];
	ACGPlayerState* EnemySide = CGState->Sides[MySideIndex == 0 ? 1 : 0];
	if (!SelfSide || !EnemySide)
	{
		return;
	}

	if (CGState->WinnerPlayerIndex != -1)
	{
		// オンライン対戦で相手が通信切断した場合は、通常の決着と区別して表示する
		// (docs/online-play-plan.md「フェーズ4」参照。負けた側の画面には
		// そもそも届かないため、実質的に切断していない側にしか表示されない)。
		if (CGState->bOpponentDisconnected)
		{
			StatusText->SetText(FText::FromString(
				CGState->WinnerPlayerIndex == MySideIndex
					? TEXT("相手が切断しました(あなたの勝ち)")
					: TEXT("通信が切断されました")));
		}
		else
		{
			StatusText->SetText(FText::FromString(
				CGState->WinnerPlayerIndex == MySideIndex ? TEXT("勝利!") : TEXT("敗北...")));

			// 勝敗確定演出(「勝利、敗北したときにもっと派手に演出を入れてほしい」
			// というフィードバックへの対応)。通信切断による不戦勝/不戦敗は対象外
			// (対戦の決着そのものではないため)。LastAnimatedWinnerで、同じ結果に
			// 対して何度もRefreshUI()のたびに再生し直さないようにする。
			if (LastAnimatedWinner != CGState->WinnerPlayerIndex)
			{
				LastAnimatedWinner = CGState->WinnerPlayerIndex;
				PlayResultAnimation(CGState->WinnerPlayerIndex == MySideIndex);
			}
		}
	}
	else
	{
		const bool bHumanTurn = (CGState->CurrentTurnPlayerIndex == MySideIndex);
		// 先攻1ターン目は攻撃禁止(先攻/後攻の偏り対策、docs/next-ruleset-
		// simulation-v1.md「第12回」参照)。疾駆Unitで攻撃しようとして反応が
		// 無いと混乱するため、番の説明に明記する。
		const FString TurnOneHint = (CGState->TurnCount == 1) ? TEXT("(1ターン目は攻撃不可)") : TEXT("");
		// 表示用ターン数は「両者のターンが終わって1増える」ラウンド数にする
		// (CGState->TurnCount自体は先攻1ターン目判定等の内部ロジック用に
		// プレイヤーのターンごと+1のまま維持し、ここでは表示だけ変換する)。
		const int32 DisplayRoundNumber = (CGState->TurnCount + 1) / 2;
		// 「自分と相手のどちらが先攻/後攻かをわかるようにしてほしい」という
		// フィードバックへの対応。bWentSecondはInitializeMatch等で試合開始時に
		// 一度だけ決まり、以後変わらないので毎ターン表示して問題ない。
		const FString SelfFirstOrSecondLabel = SelfSide->bWentSecond ? TEXT("後攻") : TEXT("先攻");
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("Turn %d [あなたは%s] - %s%s"), DisplayRoundNumber, *SelfFirstOrSecondLabel,
			bHumanTurn ? TEXT("あなたの番") : TEXT("相手の番"), *TurnOneHint)));
	}

	// 勝敗確定後のみ「ロビーへ戻る」導線を表示する(docs/architecture.md「レベルと画面遷移」)。
	BackToLobbyButton->SetVisibility(CGState->WinnerPlayerIndex != -1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// 選択待ち中はプロンプトを表示する(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	// 選ぶ側が自分のときだけ表示すればよい(AIはその場で即決するため選択待ちが残らない)。
	const bool bHumanIsChoosing = CGState->PendingChoice.IsActive() && CGState->PendingChoice.SideIndex == MySideIndex;
	ChoicePromptText->SetVisibility(bHumanIsChoosing ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bHumanIsChoosing)
	{
		ChoicePromptText->SetText(FText::FromString(CGState->PendingChoice.PromptText));
	}

	// GraveyardCard/KeepOrBury/BuyDestinationは、普段は画面に無いカードを見せる必要が
	// あるため、一番手前のポップアップ(ChoiceModalLayer)で表示する(docs/card-effect-
	// player-choice-plan.md、「墓地が見にくい」というフィードバックを受けて変更)。
	// 中身(GraveyardModalSection/ScryBox/BuyDestinationBox)はいずれか1つだけを表示する。
	const bool bShowGraveyardViewer = bHumanIsChoosing && CGState->PendingChoice.ChoiceType == ECGChoiceType::GraveyardCard;
	const bool bShowScry = bHumanIsChoosing && CGState->PendingChoice.ChoiceType == ECGChoiceType::KeepOrBury;
	const bool bShowBuyDestination = bHumanIsChoosing && CGState->PendingChoice.ChoiceType == ECGChoiceType::BuyDestination;
	ChoiceModalLayer->SetVisibility((bShowGraveyardViewer || bShowScry || bShowBuyDestination) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	GraveyardModalSection->SetVisibility(bShowGraveyardViewer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShowGraveyardViewer)
	{
		PopulateGraveyardViewer(*SelfSide, CGState->PendingChoice);
	}
	else
	{
		GraveyardViewerBox->ClearChildren();
		LastGraveyardCandidateCardIds.Reset();
	}

	// EnemyOrFaceTarget選択待ち中だけ「顔面を狙う」ボタンを表示する。敵に守護がいる
	// 間は顔面を選べない(必ず守護ユニットが対象になる)ため、そのときは隠す。
	// bRequireUnitTarget(R05黒鉄の抜き打ち等、Unit限定の効果)のときも同様に隠す
	// (「Unit限定/リーダー限定のカードがどちらも選べてしまうバグがある」という
	// フィードバックへの対応)。
	const bool bShowTargetFace = bHumanIsChoosing
		&& CGState->PendingChoice.ChoiceType == ECGChoiceType::EnemyOrFaceTarget
		&& !CGState->PendingChoice.bRequireUnitTarget
		&& !EnemySide->HasGuardUnit();
	TargetFaceButton->SetVisibility(bShowTargetFace ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	ScryBox->SetVisibility(bShowScry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShowScry)
	{
		PopulateScryViewer(CGState->PendingChoice.RevealedCardId);
	}
	else
	{
		ScryCardContainer->ClearChildren();
	}

	// 手札が上限(10枚)に達しているときは「手札へ」を選んでも意味がないため隠す
	// (TargetFaceButtonが守護持ちの相手のときに顔面を隠すのと同じ考え方)。
	BuyDestinationBox->SetVisibility(bShowBuyDestination ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShowBuyDestination)
	{
		PopulateBuyDestinationViewer(CGState->PendingChoice.RevealedCardId);
		ToHandButton->SetVisibility(SelfSide->HandCardIds.Num() < 10 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	else
	{
		BuyDestinationCardContainer->ClearChildren();
	}

	// HPはライフオーブ(円形バッジ)へ大きな数字で、それ以外の付随情報は隣の
	// テキストへ(MTG Arena風レイアウト、docs/architecture.md「カードUIの設計」)。
	EnemyOrbText->SetText(FText::AsNumber(EnemySide->CurrentHP));
	SelfOrbText->SetText(FText::AsNumber(SelfSide->CurrentHP));
	EnemySecondaryInfoText->SetText(FText::FromString(FormatPlayerSecondaryInfoText(EnemySide)));
	SelfSecondaryInfoText->SetText(FText::FromString(FormatPlayerSecondaryInfoText(SelfSide)));

	// リーダーHPの変化を検知して、原因(戦闘の顔面攻撃/バーンSpell/断罪連動ダメージ/
	// 回復など)を問わずライフオーブの位置にダメージ(赤)/回復(緑)のポップアップを
	// 表示する(「ダメージなどの表記を変更してほしい」というフィードバックへの
	// 対応。戦闘でユニットが受けるダメージは引き続きPlayAttackAnimation()側で
	// 個別に表示する)。LastSeenXHP==-1(まだ見ていない)のときは、初回表示分を
	// 演出扱いしないようスキップする。
	if (LastSeenEnemyHP != -1 && EnemySide->CurrentHP != LastSeenEnemyHP)
	{
		const FGeometry OrbGeo = EnemyOrbText->GetCachedGeometry();
		const FVector2D OrbCenter = OrbGeo.GetAbsolutePosition() + OrbGeo.GetAbsoluteSize() * 0.5f;
		const int32 Delta = EnemySide->CurrentHP - LastSeenEnemyHP;
		ShowFloatingNumber(OrbCenter, Delta, /*bIsHeal=*/Delta > 0);
	}
	LastSeenEnemyHP = EnemySide->CurrentHP;

	if (LastSeenSelfHP != -1 && SelfSide->CurrentHP != LastSeenSelfHP)
	{
		const FGeometry OrbGeo = SelfOrbText->GetCachedGeometry();
		const FVector2D OrbCenter = OrbGeo.GetAbsolutePosition() + OrbGeo.GetAbsoluteSize() * 0.5f;
		const int32 Delta = SelfSide->CurrentHP - LastSeenSelfHP;
		ShowFloatingNumber(OrbCenter, Delta, /*bIsHeal=*/Delta > 0);
	}
	LastSeenSelfHP = SelfSide->CurrentHP;

	// 相手の手札は中身を見せず、枚数分だけ裏向きカードを並べる(小さめサイズ)。
	// EnemySide->HandCardIds自体は非公開情報のため、オンライン対戦では相手の
	// クライアントに複製されず常に空になる。公開複製用のHandCountを使う
	// (docs/online-play-design.md「`ACGPlayerState`のレプリケーション」)。
	PopulateFaceDownHandRow(EnemyHandBox, EnemySide->HandCount, EnemyHandDisplayScale);
	PopulateBoardRow(EnemyBoardBox, EnemySide, /*bIsSelfSide=*/false, BoardDisplayScale);
	PopulateBoardRow(SelfBoardBox, SelfSide, /*bIsSelfSide=*/true, BoardDisplayScale);
	// 新しい攻撃があったかどうかの検知・演出再生はNativeTick()側で行う(このタイミングで
	// 今作ったばかりのカードウィジェットの座標を読むと、まだ一度もPaintされておらず
	// GetCachedGeometry()が(0,0)を返すため。NativeTickのコメント参照)。

	LastMarketSlots = CGState->MarketSlots;
	PopulateMarketColumn(MarketBox, LastMarketSlots, MarketDisplayScale, SelfSide);

	LastHandCardIds = SelfSide->HandCardIds;
	PopulateCardRow(HandBox, LastHandCardIds, SelfHandDisplayScale,
		GET_FUNCTION_NAME_CHECKED(UCGGameHUD, HandleHandSlotClicked));
}

void UCGGameHUD::PopulateGraveyardViewer(const ACGPlayerState& Self, const FCGPendingChoice& Choice)
{
	GraveyardViewerBox->ClearChildren();
	LastGraveyardCandidateCardIds.Reset();

	for (const FName& CardId : Self.DiscardCardIds)
	{
		FCGCardDef Def;
		if (!UCGCardDatabase::FindCard(CardId, Def))
		{
			continue;
		}
		// 選択の絞り込み条件(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		// 条件に合わないカードはそもそも一覧に出さない。
		if (Choice.MaxCost >= 0 && Def.Cost > Choice.MaxCost)
		{
			continue;
		}
		if (Choice.bRequireSpell && Def.CardType != ECGCardType::Spell)
		{
			continue;
		}

		LastGraveyardCandidateCardIds.Add(CardId);
		const int32 SlotIndex = LastGraveyardCandidateCardIds.Num() - 1;

		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = SlotIndex;
		SlotWidget->SetCardData(Def);
		RegisterCardHoverPreview(SlotWidget);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UCGGameHUD::HandleGraveyardSlotClicked);
		UWidget* RowChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, SlotWidget, GraveyardViewerDisplayScale);
		if (UWrapBoxSlot* CardSlot = GraveyardViewerBox->AddChildToWrapBox(RowChild))
		{
			CardSlot->SetVerticalAlignment(VAlign_Center);
			CardSlot->SetPadding(FMargin(CardGap, 0.f));
		}
	}
}

void UCGGameHUD::PopulateScryViewer(FName RevealedCardId)
{
	ScryCardContainer->ClearChildren();

	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(RevealedCardId, Def))
	{
		return;
	}

	UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
	SlotWidget->SlotIndex = -1;
	SlotWidget->SetCardData(Def);
	RegisterCardHoverPreview(SlotWidget);
	UWidget* RowChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, SlotWidget, ChoiceViewerDisplayScale);
	if (UHorizontalBoxSlot* CardSlot = ScryCardContainer->AddChildToHorizontalBox(RowChild))
	{
		CardSlot->SetVerticalAlignment(VAlign_Center);
		CardSlot->SetPadding(FMargin(CardGap, 0.f));
	}
}

void UCGGameHUD::PopulateBuyDestinationViewer(FName CardId)
{
	BuyDestinationCardContainer->ClearChildren();

	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return;
	}

	UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
	SlotWidget->SlotIndex = -1;
	SlotWidget->SetCardData(Def);
	RegisterCardHoverPreview(SlotWidget);
	UWidget* RowChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, SlotWidget, ChoiceViewerDisplayScale);
	if (UHorizontalBoxSlot* CardSlot = BuyDestinationCardContainer->AddChildToHorizontalBox(RowChild))
	{
		CardSlot->SetVerticalAlignment(VAlign_Center);
		CardSlot->SetPadding(FMargin(CardGap, 0.f));
	}
}

void UCGGameHUD::PopulateFaceDownHandRow(UHorizontalBox* Box, int32 CardCount, float DisplayScale)
{
	Box->ClearChildren();
	for (int32 i = 0; i < CardCount; ++i)
	{
		UCGCardSlotWidget* FaceDownCard = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		FaceDownCard->SlotIndex = -1;
		FaceDownCard->SetFaceDown();
		RegisterCardHoverPreview(FaceDownCard);
		UWidget* RowChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, FaceDownCard, DisplayScale);
		if (UHorizontalBoxSlot* CardSlot = Box->AddChildToHorizontalBox(RowChild))
		{
			CardSlot->SetVerticalAlignment(VAlign_Center);
			CardSlot->SetPadding(FMargin(CardGap, 0.f));
		}
	}
}

void UCGGameHUD::PopulateMarketColumn(UWrapBox* Box, const TArray<FCGMarketSlot>& Slots, float DisplayScale, ACGPlayerState* SelfSide)
{
	Box->ClearChildren();
	// 自分のコイン+マナで届かない枠は薄く表示する(「マーケットの買えない
	// カードを分かりやすくしてほしい」というフィードバックへの対応。選べない
	// 対象を薄灰色にする既存のPendingChoice向けグレーアウトと同じ考え方)。
	// クリック自体は禁止しない(RequestBuyCard側で従来通り拒否されるだけで、
	// 見た目だけ「今は買えない」と分かるようにする)。
	const int32 SelfDiscount = SelfSide ? SelfSide->ComputeCurrentPurchaseDiscount() : 0;
	const int32 SelfAffordable = SelfSide ? SelfSide->CurrentMana + SelfSide->PurchaseMana : 0;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		// 出どころの山札・捨て札とも尽きて空になっている枠は表示しない
		// (docs/next-ruleset-design.md「マーケット」)。SlotIndexは元のインデックスの
		// ままにしておくことで、クリック時にCGState->MarketSlotsと正しく対応させる。
		if (Slots[i].CardId.IsNone())
		{
			continue;
		}

		FCGCardDef Def;
		UCGCardDatabase::FindCard(Slots[i].CardId, Def);
		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		SlotWidget->SetCardData(Def);
		RegisterCardHoverPreview(SlotWidget);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UCGGameHUD::HandleMarketSlotClicked);
		const int32 EffectiveCost = FMath::Max(0, Def.Cost - SelfDiscount);
		SlotWidget->SetRenderOpacity(EffectiveCost <= SelfAffordable ? 1.f : 0.4f);
		UWidget* WrapChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, SlotWidget, DisplayScale);

		// マーケットの出どころ表示(次期ルール、docs/next-ruleset-design.md
		// 「マーケット」で決定済み)。買う側が「自分の山札を削るか、相手の山札を
		// 削るか」を判断するための必須情報。
		UVerticalBox* SlotColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* OriginLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		const bool bIsSelfOrigin = (Slots[i].OriginSideIndex == GetMySideIndex());
		OriginLabel->SetText(FText::FromString(bIsSelfOrigin ? TEXT("自分の山札") : TEXT("相手の山札")));
		OriginLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10));
		OriginLabel->SetJustification(ETextJustify::Center);
		OriginLabel->SetColorAndOpacity(FSlateColor(bIsSelfOrigin
			? FLinearColor(0.55f, 0.75f, 1.f)
			: FLinearColor(1.f, 0.55f, 0.55f)));
		if (UVerticalBoxSlot* LabelSlot = SlotColumn->AddChildToVerticalBox(OriginLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* CardSlotInColumn = SlotColumn->AddChildToVerticalBox(WrapChild))
		{
			CardSlotInColumn->SetHorizontalAlignment(HAlign_Center);
		}

		if (UWrapBoxSlot* CardSlot = Box->AddChildToWrapBox(SlotColumn))
		{
			CardSlot->SetPadding(FMargin(CardGap));
		}
	}
}

void UCGGameHUD::PopulateBoardRow(UHorizontalBox* Box, ACGPlayerState* Side, bool bIsSelfSide, float DisplayScale)
{
	Box->ClearChildren();

	// 対象指定の味方強化(次期ルール)選択待ち中は、攻撃可否に関わらず自分の場の
	// 全ユニットをクリック可能にする(docs/next-ruleset-cards-v1.md紫の対象指定
	// バフ系)。
	ACGGameState* CGState = GetWorld() ? GetWorld()->GetGameState<ACGGameState>() : nullptr;
	const bool bAllyTargetChoiceActive = bIsSelfSide && CGState
		&& CGState->PendingChoice.ChoiceType == ECGChoiceType::AllyUnitTarget
		&& CGState->PendingChoice.SideIndex == CGState->CurrentTurnPlayerIndex;

	// 敵の場に対する選択待ち(攻撃対象/カード効果ダメージ対象、断罪対象)の間、
	// 選べない相手ユニットを薄く・クリック不可にする(「選べない対象は選択できない
	// ように薄灰色にする工夫がほしい」というフィードバックへの対応)。以前は
	// クリックしても何も起きない、または守護のときは選択UIすら出ないため
	// 「選ぶ権利が無い」ように見えていた。
	const bool bEnemyOrFaceActive = !bIsSelfSide && CGState
		&& CGState->PendingChoice.ChoiceType == ECGChoiceType::EnemyOrFaceTarget
		&& CGState->PendingChoice.SideIndex == CGState->CurrentTurnPlayerIndex;
	const bool bEnemyUnitTargetActive = !bIsSelfSide && CGState
		&& CGState->PendingChoice.ChoiceType == ECGChoiceType::EnemyUnitTarget
		&& CGState->PendingChoice.SideIndex == CGState->CurrentTurnPlayerIndex;
	const bool bEnemyHasGuard = !bIsSelfSide && Side->HasGuardUnit();

	for (int32 i = 0; i < Side->BoardUnits.Num(); ++i)
	{
		const FCGBoardUnit& BoardUnit = Side->BoardUnits[i];
		FCGCardDef Def;
		UCGCardDatabase::FindCard(BoardUnit.CardId, Def);

		// 変貌先の"T"カードはコスト0で定義されている(購入・手札プレイ不可にする
		// ためだけの値、docs/next-ruleset-cards-v1.md「変貌先カード」)。この0を
		// そのままコスト表示に使うと「変貌後すべて0コストになる」ように見えてしまう
		// (フィードバックへの対応)ため、変貌済みなら変貌前カードのコストを表示する。
		if (BoardUnit.OriginalCardId != NAME_None)
		{
			FCGCardDef OriginalDef;
			if (UCGCardDatabase::FindCard(BoardUnit.OriginalCardId, OriginalDef))
			{
				Def.Cost = OriginalDef.Cost;
			}
		}

		// 自分の場だけ、攻撃可能かどうかを判定する(相手の場は常に攻撃できない)。
		// 守護は場のユニットだけの一時的な状態ではなくカード自体の常設能力のため、
		// SetCardData側で(部族欄に)常に表示している。
		const bool bCanAttack = bIsSelfSide && BoardUnit.bCanAttack;
		// P16(仮称、強制変貌)は対象を「変貌可能なUnit」に絞る必要がある(他の
		// AllyUnitTarget効果=強化は場のどのUnitでも対象になれるため絞り込み不要)。
		const bool bAllyEligible = !bAllyTargetChoiceActive
			|| CGState->PendingChoice.EffectId != FName(CGEffectId::OnPlayForceTransformAllyTarget)
			|| Side->CanUnitTransform(i);
		const bool bClickable = (bAllyTargetChoiceActive && bAllyEligible) || bCanAttack;

		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		// 場のユニットはバフ等で現在のAtk/HpがカードDefの基本値と異なることがあるため、
		// BoardUnit側の現在値で上書きする(Atkは緑パッシブ等の継続ボーナスを含む
		// 実効値。ACGPlayerState::GetEffectiveAtk参照)。
		SlotWidget->SetCardData(Def, Side->GetEffectiveAtk(i), BoardUnit.Hp);
		RegisterCardHoverPreview(SlotWidget);
		if (bIsSelfSide)
		{
			// カードの情報自体は変えず、今は攻撃できないユニットを少し暗くするだけに留める
			// (文字での注記はどの画面でも同じ情報を表示するという方針にそぐわないため)。
			SlotWidget->SetRenderOpacity(bClickable ? 1.f : 0.5f);
			if (bClickable)
			{
				SlotWidget->OnSlotClicked.AddDynamic(this, &UCGGameHUD::HandleBoardSlotClicked);
			}
		}
		else
		{
			// 敵ユニットは、攻撃対象/ダメージ対象の選択待ち中だけ意味を持つクリックを
			// 受け付ける(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
			// 選択待ちで無いときのクリックはハンドラ側で無視される。
			bool bEnemyEligible = true;
			if (bEnemyOrFaceActive)
			{
				// 守護がいれば、選べるのはそのユニットだけ(ACGGameMode::
				// ResolvePendingChoiceWithTargetのバリデーションと同じ規則)。
				bEnemyEligible = !bEnemyHasGuard || BoardUnit.bHasGuard;
			}
			else if (bEnemyUnitTargetActive)
			{
				// ACGPlayerState::HasSealCandidate/HasSealCandidateByPowerと同じ
				// 絞り込み条件を、ユニット単位で判定する。
				const FCGPendingChoice& Choice = CGState->PendingChoice;
				if (Choice.bFilterByCurrentAtk)
				{
					bEnemyEligible = Choice.MaxCost < 0 || BoardUnit.Atk <= Choice.MaxCost;
				}
				else
				{
					const FName EffectiveCardId = BoardUnit.OriginalCardId.IsNone() ? BoardUnit.CardId : BoardUnit.OriginalCardId;
					FCGCardDef EffectiveDef;
					bEnemyEligible = UCGCardDatabase::FindCard(EffectiveCardId, EffectiveDef)
						&& (Choice.MaxCost < 0 || EffectiveDef.Cost <= Choice.MaxCost);
				}
			}

			const bool bAnyEnemyChoiceActive = bEnemyOrFaceActive || bEnemyUnitTargetActive;
			SlotWidget->SetRenderOpacity((!bAnyEnemyChoiceActive || bEnemyEligible) ? 1.f : 0.35f);
			if (!bAnyEnemyChoiceActive || bEnemyEligible)
			{
				SlotWidget->OnSlotClicked.AddDynamic(this, &UCGGameHUD::HandleEnemyBoardSlotClicked);
			}
		}
		UWidget* RowChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, SlotWidget, DisplayScale);
		if (UHorizontalBoxSlot* CardSlot = Box->AddChildToHorizontalBox(RowChild))
		{
			CardSlot->SetVerticalAlignment(VAlign_Center);
			CardSlot->SetPadding(FMargin(CardGap, 0.f));
		}
	}
}

bool UCGGameHUD::PlayAttackAnimation(const FCGLastAttackResult& Result)
{
	if (Result.AttackerSideIndex < 0 || Result.AttackerUnitIndex < 0)
	{
		return true;
	}

	// 上段=敵/下段=自分の表示位置は手番に関わらず固定(GetMySideIndex()参照)。
	const bool bAttackerIsSelf = (Result.AttackerSideIndex == GetMySideIndex());
	UHorizontalBox* AttackerBox = bAttackerIsSelf ? SelfBoardBox : EnemyBoardBox;
	UHorizontalBox* DefenderBox = bAttackerIsSelf ? EnemyBoardBox : SelfBoardBox;
	if (!AttackerBox || Result.AttackerUnitIndex >= AttackerBox->GetChildrenCount())
	{
		// 攻撃したユニットが反撃等で既に場からいなくなっている等、演出のしようがない
		// ケース。再試行しても直らないので諦めたことにする。
		return true;
	}

	UCGCardSlotWidget* AttackerCard = UCGCardSlotWidget::UnwrapCompactDisplay(AttackerBox->GetChildAt(Result.AttackerUnitIndex));
	if (!AttackerCard)
	{
		return true;
	}

	const FGeometry AttackerGeo = AttackerCard->GetCachedGeometry();
	if (AttackerGeo.GetLocalSize().IsNearlyZero())
	{
		// 直前のRefreshUI()で作り直されたばかりで、まだ一度もPaintされておらず
		// 座標が取れていない。次のフレームで再試行する(NativeTickのコメント参照)。
		return false;
	}

	// 対象の絶対座標(ユニットが対象ならそのカード、顔面が対象ならInfoTextの位置)を求める。
	FVector2D TargetAbsoluteCenter = FVector2D::ZeroVector;
	bool bHasTarget = false;
	if (Result.TargetUnitIndex >= 0 && DefenderBox && Result.TargetUnitIndex < DefenderBox->GetChildrenCount())
	{
		UCGCardSlotWidget* DefenderCard = UCGCardSlotWidget::UnwrapCompactDisplay(DefenderBox->GetChildAt(Result.TargetUnitIndex));
		if (!DefenderCard)
		{
			return true;
		}
		const FGeometry DefenderGeo = DefenderCard->GetCachedGeometry();
		if (DefenderGeo.GetLocalSize().IsNearlyZero())
		{
			return false;
		}
		TargetAbsoluteCenter = DefenderGeo.GetAbsolutePosition() + DefenderGeo.GetAbsoluteSize() * 0.5f;
		bHasTarget = true;
		DefenderCard->PlayHitFlash();
	}
	else
	{
		// 顔面が対象のときは、防御側のライフオーブ(HPを表示する円形バッジ)を
		// 仮想的な「顔」の位置として使う。
		UTextBlock* FaceInfoText = bAttackerIsSelf ? EnemyOrbText : SelfOrbText;
		if (FaceInfoText)
		{
			const FGeometry FaceGeo = FaceInfoText->GetCachedGeometry();
			TargetAbsoluteCenter = FaceGeo.GetAbsolutePosition() + FaceGeo.GetAbsoluteSize() * 0.5f;
			bHasTarget = true;
		}
	}

	const FVector2D AttackerAbsoluteCenter = AttackerGeo.GetAbsolutePosition() + AttackerGeo.GetAbsoluteSize() * 0.5f;

	if (bHasTarget)
	{
		// 突進アニメーションは対象方向へ寄せるだけに留め、距離をそのまま使うと画面端の
		// カードでは動きすぎるため、最大80pxに丸める。
		FVector2D Direction = TargetAbsoluteCenter - AttackerAbsoluteCenter;
		const float Distance = Direction.Size();
		if (Distance > KINDA_SMALL_NUMBER)
		{
			Direction /= Distance;
		}
		const FVector2D LungeOffset = Direction * FMath::Min(Distance * 0.35f, 80.f);
		AttackerCard->PlayLungeTowards(LungeOffset);

		// 顔面(リーダー)へのダメージは、RefreshUI()側のライフオーブHP差分検知
		// (LastSeenSelfHP/LastSeenEnemyHP)で表示するため、ここではユニットが
		// 対象のときだけ表示する(二重表示防止)。
		if (Result.TargetUnitIndex >= 0)
		{
			ShowFloatingNumber(TargetAbsoluteCenter, Result.DamageToTarget, /*bIsHeal=*/false);
		}
	}

	// 反撃ダメージがあれば、攻撃したユニット自身にも被弾フラッシュ+数値を出す。
	if (Result.CounterDamageToAttacker > 0)
	{
		AttackerCard->PlayHitFlash();
		ShowFloatingNumber(AttackerAbsoluteCenter, Result.CounterDamageToAttacker, /*bIsHeal=*/false);
	}

	return true;
}

bool UCGGameHUD::PlayCardPlayAnimation(const FCGLastCardPlayResult& Result)
{
	if (Result.SideIndex < 0 || Result.CardId.IsNone())
	{
		return true;
	}

	// 上段=敵/下段=自分の表示位置は手番に関わらず固定(GetMySideIndex()参照)。
	const bool bIsSelf = (Result.SideIndex == GetMySideIndex());

	FCGCardDef PlayedDef;
	if (!UCGCardDatabase::FindCard(Result.CardId, PlayedDef))
	{
		return true;
	}

	if (Result.bIsUnit)
	{
		// Unitは着地した盤面のカードを、被弾フラッシュ(赤)とは違う金色で点滅させる
		// ことで「今プレイされたばかり」であることを示す。BoardUnitsは別Actor
		// (ACGPlayerState)の複製のため、CardPlaySequenceNumber(ACGGameState側)より
		// 届くのが遅れて、まだこのインデックスが場に存在しないことがある
		// (falseを返して次のフレームに再試行する)。
		UHorizontalBox* BoardBox = bIsSelf ? SelfBoardBox : EnemyBoardBox;
		if (!BoardBox || Result.BoardIndex < 0 || Result.BoardIndex >= BoardBox->GetChildrenCount())
		{
			return false;
		}
		UCGCardSlotWidget* PlayedCard = UCGCardSlotWidget::UnwrapCompactDisplay(BoardBox->GetChildAt(Result.BoardIndex));
		if (!PlayedCard)
		{
			return true;
		}
		const FGeometry CardGeo = PlayedCard->GetCachedGeometry();
		if (CardGeo.GetLocalSize().IsNearlyZero())
		{
			return false;
		}
		PlayedCard->PlayHitFlash(0.5f, FLinearColor(1.f, 0.82f, 0.15f));

		// 点滅だけだと、直後にStateVersionの追い更新(NativeTickのコメント参照)で
		// 盤面の行が丸ごと作り直されて演出が数フレームで打ち切られてしまうことがある
		// (「カードプレイの演出がない」というフィードバックへの対応。盤面ウィジェット
		// に依存しない浮遊テキストも合わせて出すことで、作り直しの影響を受けずに
		// 確実にカード名を見せる)。
		const FVector2D CardAbsoluteCenter = CardGeo.GetAbsolutePosition() + CardGeo.GetAbsoluteSize() * 0.5f;
		ShowFloatingText(CardAbsoluteCenter, PlayedDef.CardName, FLinearColor(1.f, 0.82f, 0.15f));
		return true;
	}

	// Spellは効果解決後すぐ場から消えるため、代わりにプレイヤーの顔(ライフオーブ)の
	// 位置にカード名をポップアップ表示する。OrbTextはEnsureWidgetTreeBuilt()で常に
	// 作られている(場の状況に依存しない)ため、再試行の必要はない。
	UTextBlock* OrbText = bIsSelf ? SelfOrbText : EnemyOrbText;
	if (!OrbText)
	{
		return true;
	}
	const FGeometry OrbGeo = OrbText->GetCachedGeometry();
	const FVector2D OrbAbsoluteCenter = OrbGeo.GetAbsolutePosition() + OrbGeo.GetAbsoluteSize() * 0.5f;
	ShowFloatingText(OrbAbsoluteCenter, PlayedDef.CardName, FLinearColor(1.f, 0.82f, 0.15f));
	return true;
}

void UCGGameHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ACGGameMode* GameMode = nullptr;
	ACGGameState* CGState = nullptr;
	if (!TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}

	// オンライン対戦: サーバー側の状態が変わったことをStateVersionの変化で検知し、
	// UIを再描画する(docs/online-play-design.md参照)。
	//
	// 要注意: GameStateのStateVersionと、変化の原因になった各ACGPlayerStateの
	// フィールド(HandCount/DeckCount/CurrentMana/BoardUnits等)は別々のActorの
	// レプリケーションであり、同じフレームで揃って届く保証が無い。StateVersion
	// の変化を検知した瞬間にまだ肝心のPlayerState側の複製が届いていない
	// ケースがあるため、変化検知時の即時RefreshUI()に加えて、少し後にもう
	// 1回だけ追いのRefreshUI()を行い、遅れて届いた複製を拾う
	// (`ACGGameMode::NotifyStateChanged()`のForceNetUpdate()と合わせて、
	// 通常はこの追い1回で揃う)。
	//
	// 過去の実装では「変化検知後、一定時間は毎フレームRefreshUI()し続ける」
	// 方式にしていたが、RefreshUI()は手札/盤面/マーケットを丸ごと作り直す
	// 重い処理のため、行動のたびに約1秒間・最大60回も再構築が走り、
	// 「操作するたびに画面がリロードされる」という体感の悪さにつながって
	// いた。ForceNetUpdate()で複製自体はほぼ同時に届くようになったため、
	// 追いのRefreshUI()は1回だけで十分になった。
	constexpr int32 FollowUpResyncDelayTicks = 10; // 約0.15秒後に1回だけ。
	if (LastSeenStateVersion == -1 || CGState->StateVersion != LastSeenStateVersion)
	{
		LastSeenStateVersion = CGState->StateVersion;
		RefreshUI();
		ResyncTicksRemaining = FollowUpResyncDelayTicks;
	}
	else if (ResyncTicksRemaining > 0)
	{
		--ResyncTicksRemaining;
		if (ResyncTicksRemaining == 0)
		{
			RefreshUI();
		}
	}

	// 勝敗確定演出の更新。以前はこの下の「新しい攻撃が無ければreturn」に
	// 巻き込まれて一切呼ばれていなかった(決着後は新しい攻撃が起きないため、
	// AttackSequenceNumberが変化せず毎回ここでreturnしてしまっていた)。
	// 「勝敗の演出が反映されていない」というフィードバックへの対応として、
	// 攻撃演出の判定より前に独立して呼ぶようにした。
	if (bResultAnimationActive)
	{
		TickResultAnimation(InDeltaTime);
	}

	// カードプレイ演出の検知。仕組みは下の攻撃演出検知と同じ
	// (対象がまだ複製で届いていない間は再試行する)。
	if (CGState->CardPlaySequenceNumber != LastAnimatedCardPlaySequence)
	{
		++CardPlayAnimationRetryCount;
		constexpr int32 MaxCardPlayAnimationRetries = 10;
		if (PlayCardPlayAnimation(CGState->LastCardPlayResult) || CardPlayAnimationRetryCount > MaxCardPlayAnimationRetries)
		{
			LastAnimatedCardPlaySequence = CGState->CardPlaySequenceNumber;
			CardPlayAnimationRetryCount = 0;
		}
	}

	if (CGState->AttackSequenceNumber == LastAnimatedAttackSequence)
	{
		return;
	}

	// 新しい攻撃を検知。座標がまだ取れていない間はPlayAttackAnimation()がfalseを
	// 返すので、一定回数までは何もせず次のフレームで再試行する(コメントは
	// このクラスのNativeTick宣言側を参照)。
	++AttackAnimationRetryCount;
	constexpr int32 MaxAttackAnimationRetries = 10;
	if (PlayAttackAnimation(CGState->LastAttackResult) || AttackAnimationRetryCount > MaxAttackAnimationRetries)
	{
		LastAnimatedAttackSequence = CGState->AttackSequenceNumber;
		AttackAnimationRetryCount = 0;
	}
}

void UCGGameHUD::PlayResultAnimation(bool bIsVictory)
{
	if (!ResultOverlayLayer || !ResultBannerText || !ResultClickCatcher)
	{
		return;
	}

	// 勝敗確定演出(「勝利、敗北したときにもっと派手に演出を入れてほしい」という
	// フィードバックへの対応)。勝ちは眩しい金色、負けは沈んだ暗赤色にして、
	// 見ただけで結果が分かるようにする。
	ResultBannerText->SetText(FText::FromString(bIsVictory ? TEXT("勝利!") : TEXT("敗北...")));
	ResultBannerText->SetColorAndOpacity(FSlateColor(bIsVictory
		? FLinearColor(1.f, 0.85f, 0.2f)
		: FLinearColor(0.75f, 0.2f, 0.2f)));

	// 「演出が出た後操作できなくなる」というフィードバックへの対応。ResultClickCatcher
	// をVisible(HitTest可能)にすることで、画面のどこをクリックしてもロビーに
	// 戻れるようにする(HandleBackToLobbyClickedへ直結。EnsureWidgetTreeBuilt参照)。
	ResultClickCatcher->SetVisibility(ESlateVisibility::Visible);
	ResultAnimationElapsed = 0.f;
	bResultAnimationActive = true;
	TickResultAnimation(0.f);
}

void UCGGameHUD::TickResultAnimation(float DeltaTime)
{
	if (!ResultOverlayLayer || !ResultBannerText)
	{
		bResultAnimationActive = false;
		return;
	}

	ResultAnimationElapsed += DeltaTime;

	// 背景は最初の0.4秒でゆっくり暗く覆う(全画面を急に暗転させると変化が
	// きつすぎるため)。
	constexpr float BackgroundFadeDuration = 0.4f;
	const float BackgroundAlpha = FMath::Clamp(ResultAnimationElapsed / BackgroundFadeDuration, 0.f, 1.f) * 0.72f;
	ResultOverlayLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, BackgroundAlpha));

	// バナー文字は0→1.25→1.0とオーバーシュートしながら飛び出してくる
	// (ease-out-backに近い簡易カーブ。派手さを出しつつ最終的に読みやすい
	// 等倍サイズへ収束させる)。0.6秒でこの導入アニメーションを終える。
	constexpr float ScaleInDuration = 0.6f;
	const float ScaleAlpha = FMath::Clamp(ResultAnimationElapsed / ScaleInDuration, 0.f, 1.f);
	// 3次のオーバーシュート近似: t=0→0, t=0.6→1.25, t=1→1.0。
	const float Overshoot = FMath::Sin(ScaleAlpha * PI * 0.9f) * 0.28f;
	const float IntroScale = ScaleAlpha + Overshoot * (1.f - ScaleAlpha * 0.3f);

	// 導入が終わった後も、常時ゆっくり明滅させて「派手さ」を持続させる
	// (完全に静止させると一瞬で目を引かなくなるため)。
	const float PulseTime = FMath::Max(0.f, ResultAnimationElapsed - ScaleInDuration);
	const float Pulse = 1.f + FMath::Sin(PulseTime * 2.2f) * 0.04f;
	const float FinalScale = (ScaleAlpha >= 1.f) ? Pulse : IntroScale;

	ResultBannerText->SetRenderScale(FVector2D(FinalScale, FinalScale));
	ResultBannerText->SetRenderOpacity(FMath::Clamp(ResultAnimationElapsed / 0.25f, 0.f, 1.f));
}

void UCGGameHUD::PopulateCardRow(UHorizontalBox* Box, const TArray<FName>& CardIds, float DisplayScale, FName ClickHandlerName)
{
	Box->ClearChildren();
	for (int32 i = 0; i < CardIds.Num(); ++i)
	{
		FCGCardDef Def;
		UCGCardDatabase::FindCard(CardIds[i], Def);
		UCGCardSlotWidget* SlotWidget = CreateWidget<UCGCardSlotWidget>(GetWorld(), UCGCardSlotWidget::StaticClass());
		SlotWidget->SlotIndex = i;
		SlotWidget->SetCardData(Def);
		RegisterCardHoverPreview(SlotWidget);
		// マーケット/手札はクリック時の処理だけが違うため、関数名指定で動的にバインドしている
		// (AddDynamicはコンパイル時に関数を1つに固定するマクロのため、ここでは使えない)。
		FScriptDelegate ClickDelegate;
		ClickDelegate.BindUFunction(this, ClickHandlerName);
		SlotWidget->OnSlotClicked.Add(ClickDelegate);
		UWidget* RowChild = UCGCardSlotWidget::WrapForCompactDisplay(WidgetTree, SlotWidget, DisplayScale);
		if (UHorizontalBoxSlot* CardSlot = Box->AddChildToHorizontalBox(RowChild))
		{
			CardSlot->SetVerticalAlignment(VAlign_Center);
			CardSlot->SetPadding(FMargin(CardGap, 0.f));
		}
	}
}

bool UCGGameHUD::TryGetGameModeAndState(ACGGameMode*& OutGameMode, ACGGameState*& OutCGState) const
{
	// OutGameModeはサーバー(スタンドアロン/リッスンサーバー)でのみ有効で、
	// リモートのクライアントでは常にnullptrになる(GetAuthGameMode()の仕様)。
	// GameStateはレプリケートされるActorのためクライアントでも取得できる。
	// 呼び出し側はゲームロジックの変更に`ACGPlayerController`経由のRPCを使う
	// ようになった(GameModeを直接呼ばない)ため、戻り値はCGStateの有無だけで
	// 判定する(docs/online-play-design.md「HUD側の変更点」)。
	OutGameMode = GetCGGameMode();
	OutCGState = GetWorld() ? GetWorld()->GetGameState<ACGGameState>() : nullptr;
	return OutCGState != nullptr;
}

void UCGGameHUD::HandleHandSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState) || !LastHandCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}

	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}

	// 手札から1枚選ぶ選択待ち中(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)は、
	// 通常のプレイではなく選択の解決として扱う。
	if (CGState->PendingChoice.ChoiceType == ECGChoiceType::HandCard
		&& CGState->PendingChoice.SideIndex == CGState->CurrentTurnPlayerIndex)
	{
		PC->ServerResolveChoiceWithCard(LastHandCardIds[SlotIndex]);
		RefreshUI();
		return;
	}

	PC->ServerRequestPlayCard(LastHandCardIds[SlotIndex], -1);
	RefreshUI();
}

void UCGGameHUD::HandleGraveyardSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState) || !LastGraveyardCandidateCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}
	if (CGState->PendingChoice.ChoiceType != ECGChoiceType::GraveyardCard
		|| CGState->PendingChoice.SideIndex != CGState->CurrentTurnPlayerIndex)
	{
		return;
	}
	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}
	PC->ServerResolveChoiceWithCard(LastGraveyardCandidateCardIds[SlotIndex]);
	RefreshUI();
}

void UCGGameHUD::HandleMarketSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState) || !LastMarketSlots.IsValidIndex(SlotIndex))
	{
		return;
	}

	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}

	// マーケットから1枚選ぶ選択待ち中(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)は、
	// 通常の購入ではなく選択の解決として扱う
	// (この経路はCardId単体で解決するため、Slot経由である必要はない)。
	if (CGState->PendingChoice.ChoiceType == ECGChoiceType::MarketCard
		&& CGState->PendingChoice.SideIndex == CGState->CurrentTurnPlayerIndex)
	{
		PC->ServerResolveChoiceWithCard(LastMarketSlots[SlotIndex].CardId);
		RefreshUI();
		return;
	}

	// 補充し直す枠を選ぶ選択待ち中(橙O04市場の噂話、次期ルール)は、通常の購入
	// ではなく選択の解決として扱う。コスト上限が無く6枠のどれでも選べるため、
	// 通常なら買えない枠をクリックしても解決できる。
	if (CGState->PendingChoice.ChoiceType == ECGChoiceType::MarketSlotTarget
		&& CGState->PendingChoice.SideIndex == CGState->CurrentTurnPlayerIndex)
	{
		PC->ServerResolveChoiceMarketSlotTarget(SlotIndex);
		RefreshUI();
		return;
	}

	// 次期ルール(docs/next-ruleset-design.md)ではCardIdではなく枠のインデックスで
	// 購入する(両者の山札が同じカードを含み得るため、CardIdでは枠を一意に特定できない)。
	PC->ServerRequestBuyCard(SlotIndex);
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

	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}

	// 対象指定の味方強化(次期ルール)選択待ち中は、通常の攻撃開始ではなく
	// 選択の解決として扱う(docs/next-ruleset-cards-v1.md紫の対象指定バフ系)。
	if (CGState->PendingChoice.ChoiceType == ECGChoiceType::AllyUnitTarget
		&& CGState->PendingChoice.SideIndex == CGState->CurrentTurnPlayerIndex)
	{
		PC->ServerResolveChoiceAllyTarget(SlotIndex);
		RefreshUI();
		return;
	}

	// 攻撃対象(敵ユニットまたは顔面)の選択は、相手に守護がいなければ選択待ちに
	// 入る(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	// この関数では攻撃を「開始」するだけで、対象の確定はACGGameMode側で行う。
	PC->ServerRequestAttack(SlotIndex);
	RefreshUI();
}

void UCGGameHUD::HandleEnemyBoardSlotClicked(int32 SlotIndex)
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}
	if (CGState->PendingChoice.SideIndex != CGState->CurrentTurnPlayerIndex)
	{
		return;
	}

	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}

	// 断罪(青、次期ルール)対象の選択。EnemyOrFaceTargetと異なり顔面は選べないため
	// 専用のChoiceTypeで扱う(docs/game-rules-minimum.md「青」)。
	if (CGState->PendingChoice.ChoiceType == ECGChoiceType::EnemyUnitTarget)
	{
		PC->ServerResolveChoiceSealTarget(SlotIndex);
		RefreshUI();
		return;
	}

	if (CGState->PendingChoice.ChoiceType != ECGChoiceType::EnemyOrFaceTarget)
	{
		return;
	}
	PC->ServerResolveChoiceWithTarget(SlotIndex);
	RefreshUI();
}

void UCGGameHUD::HandleTargetFaceClicked()
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}
	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}
	PC->ServerResolveChoiceWithTarget(-1);
	RefreshUI();
}

void UCGGameHUD::HandleKeepOnTopClicked()
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}
	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}
	PC->ServerResolveChoiceKeepOrBury(/*bKeepOnTop=*/true);
	RefreshUI();
}

void UCGGameHUD::HandleSendToBottomClicked()
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}
	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}
	PC->ServerResolveChoiceKeepOrBury(/*bKeepOnTop=*/false);
	RefreshUI();
}

void UCGGameHUD::HandleToHandClicked()
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}
	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}
	PC->ServerResolveChoiceBuyDestination(/*bToHand=*/true);
	RefreshUI();
}

void UCGGameHUD::HandleToDeckBottomClicked()
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}
	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}
	PC->ServerResolveChoiceBuyDestination(/*bToHand=*/false);
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
	ACGPlayerController* PC = GetCGPlayerController();
	if (!PC)
	{
		return;
	}
	PC->ServerRequestEndTurn();
	RefreshUI();
}

void UCGGameHUD::HandleBackToLobbyClicked()
{
	UGameplayStatics::OpenLevel(this, FName(LobbyLevelPath));
}

void UCGGameHUD::HandleActionLogToggleClicked()
{
	if (!ActionLogModalLayer)
	{
		return;
	}
	const bool bWillBeVisible = ActionLogModalLayer->GetVisibility() != ESlateVisibility::Visible;
	if (bWillBeVisible)
	{
		PopulateActionLog();
	}
	ActionLogModalLayer->SetVisibility(bWillBeVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UCGGameHUD::HandleActionLogCloseClicked()
{
	if (ActionLogModalLayer)
	{
		ActionLogModalLayer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCGGameHUD::PopulateActionLog()
{
	ACGGameMode* GameMode;
	ACGGameState* CGState;
	if (!ActionLogListBox || !TryGetGameModeAndState(GameMode, CGState))
	{
		return;
	}

	ActionLogListBox->ClearChildren();
	const int32 MySideIndex = GetMySideIndex();
	if (CGState->ActionLog.Num() == 0)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EmptyText->SetText(FText::FromString(TEXT("まだ行動はありません。")));
		EmptyText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14));
		ActionLogListBox->AddChildToVerticalBox(EmptyText);
		return;
	}

	// 古い順に並んでいるが、直近の行動を一番上に見せた方が「今何をされたか」を
	// すぐ確認できるため、新しい順(逆順)に表示する。
	for (int32 i = CGState->ActionLog.Num() - 1; i >= 0; --i)
	{
		const FCGActionLogEntry& Entry = CGState->ActionLog[i];
		const bool bIsSelf = (Entry.SideIndex == MySideIndex);
		UTextBlock* EntryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		EntryText->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"),
			bIsSelf ? TEXT("あなた") : TEXT("相手"), *Entry.Text)));
		EntryText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14));
		EntryText->SetAutoWrapText(true);
		EntryText->SetColorAndOpacity(FSlateColor(bIsSelf
			? FLinearColor(0.65f, 0.8f, 1.f)
			: FLinearColor(1.f, 0.7f, 0.65f)));
		if (UVerticalBoxSlot* EntrySlot = ActionLogListBox->AddChildToVerticalBox(EntryText))
		{
			EntrySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
	}
}
