#include "CGCardDatabase.h"

namespace
{
	FCGCardDef MakeCard(const TCHAR* CardId, const TCHAR* CardName, ECGCardType CardType, ECGColor Color, int32 Cost,
		int32 Atk, int32 Hp, const TCHAR* EffectId, int32 EffectValue, float Ratio, const TCHAR* Tags,
		const TCHAR* Description, const TCHAR* Tribe = TEXT(""), const TCHAR* FlavorText = TEXT(""),
		const TCHAR* TransformTargetCardId = TEXT(""), const TCHAR* TransformConditionId = TEXT(""),
		int32 TransformConditionValue = 0, const TCHAR* CloneConditionId = TEXT(""),
		int32 CloneConditionValue = 0)
	{
		FCGCardDef Card;
		Card.CardId = FName(CardId);
		Card.CardName = CardName;
		Card.CardType = CardType;
		Card.Color = Color;
		Card.Cost = Cost;
		Card.Atk = Atk;
		Card.Hp = Hp;
		Card.EffectId = FName(EffectId);
		Card.EffectValue = EffectValue;
		Card.Ratio = Ratio;
		Card.Tags = Tags;
		Card.Description = Description;
		Card.Tribe = Tribe;
		Card.FlavorText = FlavorText;
		if (FCString::Strlen(TransformTargetCardId) > 0)
		{
			Card.TransformTargetCardId = FName(TransformTargetCardId);
			Card.TransformConditionId = FName(TransformConditionId);
			Card.TransformConditionValue = TransformConditionValue;
		}
		if (FCString::Strlen(CloneConditionId) > 0)
		{
			Card.CloneConditionId = FName(CloneConditionId);
			Card.CloneConditionValue = CloneConditionValue;
		}
		return Card;
	}

	// 旧24種(無色)。次期ルール移行後もフォールバック用途で残置している
	// (docs/architecture.md参照)。
	const TArray<FCGCardDef>& BuildLegacyCards()
	{
		static const TArray<FCGCardDef> Cards = {
			MakeCard(TEXT("C001"), TEXT("廃墟を渡る斥候"), ECGCardType::Unit, ECGColor::None, 1, 1, 1, CGEffectId::ScoutTop1, 0, 1.00f, TEXT(""),
				TEXT("登場時にデッキ上1枚を見て上下選択"), TEXT("斥候"), TEXT("地図にない道ほど、彼女の得意分野だった。")),
			MakeCard(TEXT("C002"), TEXT("結晶に導かれし小鬼"), ECGCardType::Unit, ECGColor::None, 1, 2, 1, CGEffectId::None, 0, 1.50f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("C003"), TEXT("石廃墟の盾持ち"), ECGCardType::Unit, ECGColor::None, 1, 1, 2, CGEffectId::None, 0, 2.00f, TEXT("Guard"),
				TEXT("")),
			MakeCard(TEXT("C004"), TEXT("結晶を覗いた子"), ECGCardType::Unit, ECGColor::None, 1, 1, 1, CGEffectId::OnDeathDraw, 1, 1.75f, TEXT(""),
				TEXT("死亡時1ドロー")),
			MakeCard(TEXT("C005"), TEXT("黒鉄柱を越える先兵"), ECGCardType::Unit, ECGColor::None, 2, 2, 2, CGEffectId::None, 0, 1.25f, TEXT("Haste"),
				TEXT(""), TEXT("戦士"), TEXT("号令を待たず、彼はすでに駆けだしている。")),
			MakeCard(TEXT("C006"), TEXT("廃墟あさりの拾い屋"), ECGCardType::Unit, ECGColor::None, 2, 2, 2, CGEffectId::GraveyardToDeckBottomDraw1, 1, 1.50f, TEXT(""),
				TEXT("登場時に墓地1枚を山札下へ、1ドロー")),
			MakeCard(TEXT("C007"), TEXT("荒野の行商人"), ECGCardType::Unit, ECGColor::None, 2, 1, 3, CGEffectId::OnBuyEndTurnDiscardDraw, 1, 1.25f, TEXT(""),
				TEXT("あなたが購入したターン終了時、手札を1枚捨てると1ドロー")),
			MakeCard(TEXT("C008"), TEXT("錆びた黒鉄の巨兵"), ECGCardType::Unit, ECGColor::None, 2, 3, 2, CGEffectId::OnPlayDiscard1, 1, 1.13f, TEXT(""),
				TEXT("登場時に手札1枚捨てる")),
			MakeCard(TEXT("C009"), TEXT("廃墟街道の突撃兵"), ECGCardType::Unit, ECGColor::None, 3, 3, 3, CGEffectId::SecondPlayBuff, 1, 1.08f, TEXT(""),
				TEXT("あなたがこのターン2枚目をプレイしていたら+1/+0"), TEXT("戦士"), TEXT("一度きりの突撃に、すべてを賭ける。")),
			MakeCard(TEXT("C010"), TEXT("結晶の谺を読む射手"), ECGCardType::Unit, ECGColor::None, 3, 2, 3, CGEffectId::OnAllySpellPing1, 1, 1.08f, TEXT(""),
				TEXT("味方Spell使用時に敵リーダーへ1ダメージ(各ターン1回)")),
			MakeCard(TEXT("C011"), TEXT("廃墟の蘇生司祭"), ECGCardType::Unit, ECGColor::None, 3, 2, 4, CGEffectId::OnPlayReturnGraveyardCheapCard, 1, 1.17f, TEXT(""),
				TEXT("登場時に墓地コスト1以下を手札へ")),
			MakeCard(TEXT("C012"), TEXT("荒野の物々交換人"), ECGCardType::Unit, ECGColor::None, 3, 3, 4, CGEffectId::BuyCostReductionThisTurn, 1, 1.33f, TEXT(""),
				TEXT("購入時コストをこのターンだけ1軽減(最小1)")),
			MakeCard(TEXT("C013"), TEXT("国無き旗手"), ECGCardType::Unit, ECGColor::None, 4, 4, 4, CGEffectId::AllyBuffAtkThisTurn, 1, 1.13f, TEXT(""),
				TEXT("味方他Unit全て+1/+0(ターン中)"), TEXT("指揮官"), TEXT("旗が揺れるたび、味方の士気も揺れる。")),
			MakeCard(TEXT("C014"), TEXT("廃墟の霊廟守り"), ECGCardType::Unit, ECGColor::None, 4, 3, 5, CGEffectId::OnDeathReturnRandomGraveyardUnit, 0, 1.13f, TEXT("Guard"),
				TEXT("死亡時にランダムで墓地のUnit1枚を山札へ"), TEXT("アンデッド"), TEXT("死してなお、その務めを忘れない。")),
			MakeCard(TEXT("C015"), TEXT("隕鉄に憑かれし獣"), ECGCardType::Unit, ECGColor::None, 4, 5, 4, CGEffectId::None, 0, 1.13f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("C016"), TEXT("第六界を記す学者"), ECGCardType::Unit, ECGColor::None, 4, 3, 4, CGEffectId::FirstSpellBonusDamage, 1, 1.06f, TEXT(""),
				TEXT("あなたの毎ターン最初のSpellが+1ダメージ")),
			// R05と同様の理由(「敵Unitへ」という説明にもかかわらず顔面も選べて
			// しまうバグ)でOnPlayDamageUnitTargetに差し替え。
			MakeCard(TEXT("C017"), TEXT("結晶の欠片"), ECGCardType::Spell, ECGColor::None, 1, 0, 0, CGEffectId::OnPlayDamageUnitTarget, 2, 1.00f, TEXT(""),
				TEXT("敵Unitへ2ダメージ"), TEXT(""), TEXT("小さな火花が、戦局を変えることもある。")),
			MakeCard(TEXT("C018"), TEXT("冷たい結晶への祈り"), ECGCardType::Spell, ECGColor::None, 1, 0, 0, CGEffectId::OnPlayHealSelf, 3, 1.20f, TEXT(""),
				TEXT("味方リーダー3回復")),
			MakeCard(TEXT("C019"), TEXT("荒野の取捨選択"), ECGCardType::Spell, ECGColor::None, 1, 0, 0, CGEffectId::Discard1Draw2, 0, 1.25f, TEXT(""),
				TEXT("1枚捨てて2枚引く")),
			MakeCard(TEXT("C020"), TEXT("彷徨う者たちの合流"), ECGCardType::Spell, ECGColor::None, 2, 0, 0, CGEffectId::SummonApprenticeTokens, 2, 1.00f, TEXT(""),
				TEXT("1/1 Unitを2体出す")),
			MakeCard(TEXT("C021"), TEXT("結晶に灯る記憶"), ECGCardType::Spell, ECGColor::None, 2, 0, 0, CGEffectId::ReturnGraveyardSpellSelfDamage1, 1, 0.88f, TEXT(""),
				TEXT("墓地のSpell1枚を手札へ、1点自傷")),
			MakeCard(TEXT("C022"), TEXT("廃墟に転がる戦利品"), ECGCardType::Spell, ECGColor::None, 2, 0, 0, CGEffectId::BuyFromMarketCostUnder3ToHand, 3, 1.13f, TEXT(""),
				TEXT("マーケットからコスト3以下を購入し即手札へ")),
			MakeCard(TEXT("C023"), TEXT("結晶が弾く雨"), ECGCardType::Spell, ECGColor::None, 3, 0, 0, CGEffectId::RandomEnemyDamage1x4, 1, 0.67f, TEXT(""),
				TEXT("ランダムな敵に1ダメージを4回")),
			MakeCard(TEXT("C024"), TEXT("土壇場の号令"), ECGCardType::Spell, ECGColor::None, 3, 0, 0, CGEffectId::ConditionalDamage3or2, 0, 0.45f, TEXT(""),
				TEXT("自HPが相手以下なら3ダメージ、そうでなければ2ダメージ")),
		};
		return Cards;
	}

	// 次期ルール(docs/next-ruleset-design.md)の5色×15種、計75種
	// (docs/next-ruleset-cards-v1.md)。効果が未実装のものは説明文にその旨を残し、
	// EffectId=Noneのままにしている(要フォローアップ、docs/architecture.md
	// 「次期ルール移行時の実装メモ」参照)。
	const TArray<FCGCardDef>& BuildNextRulesetCards()
	{
		static const TArray<FCGCardDef> Cards = {
			// --- 赤(速攻/バーン)— 疾駆 ---
			// 500戦シミュレーションで赤の15種がほぼ全て50%超えとなり(色全体の
			// 底上げ)、中でも1コストのバニラとしては効率が良すぎたためAtk2→1に
			// 調整した。さらに「バニラを無くしたい」というフィードバックを受けて
			// 死亡時効果を追加した(docs/next-ruleset-cards-v1.md「赤」参照)。
			// 「赤の死亡時の効果を減らしてほしい。強いユニットから取り除くように
			// してほしい」というフィードバックを受け、死亡時効果持ちの中で
			// 勝率上位だったR03/R04/R01(3000戦で59.4%/55.4%/54.7%)から順に
			// 死亡時効果を取り除いた(docs/next-ruleset-cards-v1.md「赤」参照)。
			MakeCard(TEXT("R01"), TEXT("黒鉄拾いの悪童"), ECGCardType::Unit, ECGColor::Red, 1, 1, 1, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("R02"), TEXT("先の先"), ECGCardType::Spell, ECGColor::Red, 1, 0, 0, CGEffectId::OnPlayDamageTarget, 1, 0.f, TEXT(""),
				TEXT("敵Unit1体、または敵リーダーへ1ダメージ")),
			MakeCard(TEXT("R03"), TEXT("抜き打ちの若武者"), ECGCardType::Unit, ECGColor::Red, 1, 1, 1, CGEffectId::None, 0, 0.f, TEXT("Haste"),
				TEXT("疾駆")),
			MakeCard(TEXT("R04"), TEXT("音を追う剣士"), ECGCardType::Unit, ECGColor::Red, 2, 2, 1, CGEffectId::None, 0, 0.f, TEXT("Haste"),
				TEXT("疾駆")),
			// 「Unit限定/リーダー限定のカードがどちらも選べてしまうバグがある」という
			// フィードバックへの対応。以前はR02(先の先、Unitまたはリーダー選択可)と
			// 同じOnPlayDamageTargetを使っていたため、「敵Unit1体へ」という説明にも
			// かかわらず顔面も選べてしまっていた。専用のOnPlayDamageUnitTarget
			// (顔面選択不可)に差し替えて修正した。
			MakeCard(TEXT("R05"), TEXT("黒鉄の抜き打ち"), ECGCardType::Spell, ECGColor::Red, 2, 0, 0, CGEffectId::OnPlayDamageUnitTarget, 3, 0.f, TEXT(""),
				TEXT("敵Unit1体へ3ダメージ")),
			MakeCard(TEXT("R06"), TEXT("捨て身の道場破り"), ECGCardType::Unit, ECGColor::Red, 2, 2, 2, CGEffectId::OnDeathDamageFace1, 1, 0.f, TEXT(""),
				TEXT("死亡時、敵リーダーへ1ダメージ")),
			// 複数回のシミュレーションで一貫して赤の中でも上位の勝率だったため、
			// 「生け贄のキーワードを持つユニットを増やしてほしい、基本的に赤の
			// 強すぎたカードに着けてほしい」というフィードバックを受け、
			// 生け贄(Sacrifice)を追加した。その後、生け贄3枚により赤全体が
			// 弱くなりすぎたため、「死亡時に発生する効果を増やしてほしい」という
			// フィードバックを受け、死亡時コイン+1を追加した(生け贄で失ったり、
			// 疾駆で先に攻め込んで倒れたりしても損切りしにくくする狙い)。
			MakeCard(TEXT("R07"), TEXT("疾風の抜き手"), ECGCardType::Unit, ECGColor::Red, 3, 3, 2, CGEffectId::OnDeathGrantPurchaseMana, 1, 0.f, TEXT("Haste,Sacrifice"),
				TEXT("疾駆 生け贄。死亡時、コインを1増加")),
			// 「敵リーダーへ4ダメージ」から、死亡時に敵リーダーへ1ダメージを与える
			// 0/1のUnit(TK_EMBER 赤熱の残り火)を2体出す効果に変更(フィードバック
			// への対応)。単発火力から、即座には減らないが場に残り続ける形の
			// 継続火力へ性質が変わる。
			MakeCard(TEXT("R08"), TEXT("赤熱の一閃"), ECGCardType::Spell, ECGColor::Red, 3, 0, 0, CGEffectId::SummonEmberTokens, 2, 0.f, TEXT(""),
				TEXT("死亡時、敵リーダーへ1ダメージを与える0/1のUnitを2体出す")),
			MakeCard(TEXT("R09"), TEXT("連携の立会人"), ECGCardType::Unit, ECGColor::Red, 3, 2, 3, CGEffectId::SecondPlayBuff, 2, 0.f, TEXT("Haste"),
				TEXT("疾駆。このターン他のUnitをプレイ済みなら+2/+0")),
			// 「R10が強すぎる」というフィードバックを受け、3000戦シミュレーションで
			// カード単体61.6%(赤で最上位)だったステータスを4/3→3/2に調整。
			// その後、赤全体の勝率が32.5%まで下がりすぎたため、Atkを3→4に再調整。
			// さらに赤全体が60%超と強すぎたため、疾駆を外し3/3に変更、代わりに
			// 登場時に敵Unit1体へ2ダメージを与える除去効果を追加した(即攻撃力を
			// 失う代わりに、盤面干渉できる汎用性を持たせる方針転換)。
			MakeCard(TEXT("R10"), TEXT("灼熱の一番槍"), ECGCardType::Unit, ECGColor::Red, 4, 3, 3, CGEffectId::OnPlayDamageUnitTarget, 2, 0.f, TEXT(""),
				TEXT("登場時、敵Unit1体へ2ダメージ")),
			MakeCard(TEXT("R11"), TEXT("廃墟の剣戟"), ECGCardType::Spell, ECGColor::Red, 4, 0, 0, CGEffectId::RandomEnemyDamage2x3, 2, 0.f, TEXT(""),
				TEXT("ランダムな敵Unit3体へ各2ダメージ(いなければ敵リーダーへ)")),
			// コスト4→2、ステータス3/3→1/1に調整(赤全体の勝率が下がりすぎたことを
			// 受け、低コスト帯を厚くする狙い)。
			MakeCard(TEXT("R12"), TEXT("黒鉄の刀鍛冶"), ECGCardType::Unit, ECGColor::Red, 2, 1, 1, CGEffectId::OnAllyDeathDamageFace1, 1, 0.f, TEXT(""),
				TEXT("自分のUnit死亡時、敵リーダーへ1ダメージ")),
			// 赤全体が60%超と強すぎたため、5/3→4/2に調整。その後もシミュレーションで
			// 赤の中で最上位の勝率が続いたため、生け贄(Sacrifice)を追加した
			// (「赤の強すぎたカードに生け贄を着けてほしい」というフィードバックへの対応)。
			// その後、生け贄3枚により赤全体が弱くなりすぎたため、死亡時コイン+1を追加した。
			MakeCard(TEXT("R13"), TEXT("一太刀の剣聖"), ECGCardType::Unit, ECGColor::Red, 5, 4, 2, CGEffectId::OnDeathGrantPurchaseMana, 1, 0.f, TEXT("Haste,Sacrifice"),
				TEXT("疾駆 生け贄。死亡時、コインを1増加")),
			MakeCard(TEXT("R14"), TEXT("決闘状"), ECGCardType::Spell, ECGColor::Red, 5, 0, 0, CGEffectId::DamageFaceByUnitCount, 2, 0.f, TEXT(""),
				TEXT("敵リーダーへ、自分の場のUnit数×2ダメージ")),
			// 「R15が強すぎる」というフィードバックを受け、3000戦シミュレーションで
			// カード単体61.3%(赤で2位)だったステータスを7/4→5/2に調整。さらに
			// 赤全体が60%超と強すぎたため、新キーワード「生け贄(Sacrifice)」を
			// 追加した。プレイするには自分の場のUnitを1体、道連れに死亡させる
			// 必要がある(場にUnitが1体もいなければプレイ自体できない)。生け贄に
			// するユニットはプレイヤー(AIは現在HPが一番低いもの、同点なら
			// Atkが一番低いものを自動選択)が選ぶ(docs/keywords.md「生け贄
			// (Sacrifice)」、ACGGameMode::ResolvePendingChoiceAllyTarget参照)。
			MakeCard(TEXT("R15"), TEXT("黒鉄を継ぐ大剣士"), ECGCardType::Unit, ECGColor::Red, 6, 5, 2, CGEffectId::OnPlayDamageFace, 2, 0.f, TEXT("Haste,Sacrifice"),
				TEXT("疾駆 生け贄。登場時、敵リーダーへ2ダメージ")),
			// 「戦場全体のユニットが死亡するたびに敵リーダーへ1ダメージ与える、
			// 3コスト1/2のユニットを追加してほしい」というフィードバックへの対応。
			// OnAllyDeathDamageFace1(R12、自分のUnit死亡限定)と異なり、自分・相手
			// どちらのUnitが死んでも発動する常在アウラ版(このユニット自身の死亡では
			// 発動しない。docs/keywords.md参照)。
			MakeCard(TEXT("R16"), TEXT("骸数えの剣客"), ECGCardType::Unit, ECGColor::Red, 3, 1, 2, CGEffectId::OnAnyUnitDeathDamageFace1, 1, 0.f, TEXT(""),
				TEXT("場のUnitが1体死亡するたび(敵味方問わず)、敵リーダーへ1ダメージ")),

			// --- 橙(購入加速)— 先物 ---
			MakeCard(TEXT("O01"), TEXT("風の相場"), ECGCardType::Spell, ECGColor::Orange, 1, 0, 0, CGEffectId::GrantPurchaseMana, 1, 0.f, TEXT(""),
				TEXT("コインを1増加")),
			// バニラを無くしたいというフィードバックで先物(Discount)を付与。
			// 先物キーワード自体を「次の購入コストを1軽減」から「登場時コインを1増加」に
			// 変更したため(docs/keywords.md「先物」参照)、この変更でO03/O06/O11/O14の
			// 効果も同様に変わる。ステータスはキーワード追加分を相殺して1/2→1/1に調整。
			MakeCard(TEXT("O02"), TEXT("砂海の仲買人"), ECGCardType::Unit, ECGColor::Orange, 1, 1, 1, CGEffectId::None, 0, 0.f, TEXT("Discount"),
				TEXT("先物")),
			MakeCard(TEXT("O03"), TEXT("先物師の弟子"), ECGCardType::Unit, ECGColor::Orange, 2, 2, 2, CGEffectId::None, 0, 0.f, TEXT("Discount"),
				TEXT("先物: 登場時、コインを1増やす")),
			MakeCard(TEXT("O04"), TEXT("市場の噂話"), ECGCardType::Spell, ECGColor::Orange, 2, 0, 0, CGEffectId::RerollMarketSlot, 0, 0.f, TEXT(""),
				TEXT("マーケットの好きな1枠を即座に補充し直す")),
			MakeCard(TEXT("O05"), TEXT("市場の設営係"), ECGCardType::Unit, ECGColor::Orange, 2, 1, 2, CGEffectId::OnBuyBuffSelfAtk, 1, 0.f, TEXT(""),
				TEXT("自分が購入するたび、このユニットは+1/+0(「このターン中」は簡略化して永続にしている)")),
			MakeCard(TEXT("O06"), TEXT("相場読みの商人"), ECGCardType::Unit, ECGColor::Orange, 3, 3, 3, CGEffectId::None, 0, 0.f, TEXT("Discount"),
				TEXT("先物: 登場時、コインを1増やす")),
			MakeCard(TEXT("O07"), TEXT("買い付け"), ECGCardType::Spell, ECGColor::Orange, 3, 0, 0, CGEffectId::BuyFromMarketCostUnder3ToHand, 5, 0.f, TEXT(""),
				TEXT("コスト5以下のマーケットカードを1枚購入し、即座に手札へ")),
			MakeCard(TEXT("O08"), TEXT("風の航路士"), ECGCardType::Unit, ECGColor::Orange, 3, 3, 2, CGEffectId::OnTurnStartGrantPurchaseMana, 1, 0.f, TEXT(""),
				TEXT("自分のターン開始時、コインを1増加")),
			// 「橙(黄色)と紫で強すぎるユニットのステータスを下げてほしい」という
			// フィードバックを受け、O09/O11/O12/O15(いずれも3000戦で55%超)の
			// HPをそれぞれ1下げた。
			MakeCard(TEXT("O09"), TEXT("船団長グラナ"), ECGCardType::Unit, ECGColor::Orange, 4, 4, 3, CGEffectId::GrantPurchaseMana, 2, 0.f, TEXT(""),
				TEXT("登場時、コインが2増加")),
			MakeCard(TEXT("O10"), TEXT("黄金の商機"), ECGCardType::Spell, ECGColor::Orange, 2, 0, 0, CGEffectId::GrantBoardBuffOnPurchaseThisTurn, 0, 0.f, TEXT(""),
				TEXT("このターン、購入するたびに場のユニットのステータスを+1/+0")),
			MakeCard(TEXT("O11"), TEXT("先物師の元締め"), ECGCardType::Unit, ECGColor::Orange, 4, 4, 3, CGEffectId::OnPlayRerollMarketRandom, 0, 0.f, TEXT("Discount"),
				TEXT("先物。登場時、マーケットをランダムな1枚補充し直す")),
			MakeCard(TEXT("O12"), TEXT("錨を抱く老船長"), ECGCardType::Unit, ECGColor::Orange, 5, 5, 4, CGEffectId::OnBuyEndTurnGrantPurchaseMana, 0, 0.f, TEXT(""),
				TEXT("自分が購入したターン終了時、コインを1増加")),
			MakeCard(TEXT("O13"), TEXT("市場開放の号令"), ECGCardType::Spell, ECGColor::Orange, 4, 0, 0, CGEffectId::GrantAllPurchasesDiscountThisTurn, 0, 0.f, TEXT(""),
				TEXT("このターン、購入コストが全て1軽減される")),
			// 「O14 先物を消して2/4にしてほしい」「O15 3/3にしてほしい」という
			// フィードバックを受け調整。
			MakeCard(TEXT("O14"), TEXT("市場の守り主"), ECGCardType::Unit, ECGColor::Orange, 5, 2, 4, CGEffectId::None, 0, 0.f, TEXT("Guard,Reinforce"),
				TEXT("庇護 増強")),
			MakeCard(TEXT("O15"), TEXT("黒鉄の買い占め屋"), ECGCardType::Unit, ECGColor::Orange, 6, 3, 3, CGEffectId::OnPlayDeployFromMarketFree, -1, 0.f, TEXT(""),
				TEXT("登場時、マーケットから1枚、コストを支払わず場に出す")),
			// 橙の補強用に追加(16枚目、紫のP16と同様に他4色より1枚多い16種構成)。
			// 「購入するたびに敵ユニットへダメージを与える」カードを追加してほしい
			// というフィードバックへの対応。実際のダメージ適用はACGGameMode::
			// RequestBuyCard側で行う(ACGPlayerState::BuyCard()はOpponentを
			// 持たないため。docs/next-ruleset-cards-v1.md「橙」参照)。
			MakeCard(TEXT("O16"), TEXT("強奪の商人"), ECGCardType::Unit, ECGColor::Orange, 3, 1, 1, CGEffectId::OnBuyDamageRandomEnemyUnit, 2, 0.f, TEXT(""),
				TEXT("自分が購入するたび、敵のランダムなUnit1体へ2ダメージ")),
			// 橙の補強用に追加(17枚目)。新キーワード「増強(Reinforce)」の低コスト版。
			// 「赤の速攻に対して橙の庇護持ちが高コスト(O14が5)すぎて間に合わない」
			// という課題への対応として、序盤から出せる庇護持ちの壁を用意した
			// (docs/keywords.md「増強(Reinforce)」参照)。
			MakeCard(TEXT("O17"), TEXT("見張り番の樽詰め"), ECGCardType::Unit, ECGColor::Orange, 2, 1, 2, CGEffectId::None, 0, 0.f, TEXT("Guard,Reinforce"),
				TEXT("庇護 増強")),

			// --- 緑(物量/分身/回復)— 分身 ---
			// 500戦シミュレーションで緑が59.9%と突出したため分身持ち7種を1段階
			// 弱体化したところ、今度は40.4%まで下がりすぎたため、その1段階分を
			// 巻き戻した(G11の差し替えはそのまま維持。docs/next-ruleset-cards-v1.md
			// 「緑」参照)。
			// 「緑の分身の条件を難しくしてほしい。ユニット数が関係するものが対象」
			// というフィードバックを受け、Unit数条件(AllyUnitCountAtLeast)を持つ
			// 4種(G01/G06/G09/G15)のしきい値をそれぞれ+1した。その後「ユニット数の
			// 条件を1つ減らしてほしい。ユニット数に自分は含まない」というフィード
			// バックを受け、しきい値を-1した上で「自分以外の味方Unit数」で判定する
			// ように変更した(Predicate_AllyUnitCountAtLeast参照)。数値は下がったが
			// 自分を含めなくなった分、実際に必要な他Unit数はこの変更の前後で
			// 変わっていない(以前: 場の合計3体以上=自分以外に2体。変更後: 自分
			// 以外に2体以上、と表現を揃えただけ)。
			MakeCard(TEXT("G01"), TEXT("種を携える使徒"), ECGCardType::Unit, ECGColor::Green, 1, 1, 1, CGEffectId::None, 0, 0.f, TEXT("Clone"),
				TEXT("分身: 自分以外に味方Unitが2体以上いるとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AllyUnitCountAtLeast, 2),
			MakeCard(TEXT("G02"), TEXT("列に加わった旅人"), ECGCardType::Spell, ECGColor::Green, 1, 0, 0, CGEffectId::SummonApprenticeTokens, 2, 0.f, TEXT(""),
				TEXT("0/1のUnitを2体出す")),
			MakeCard(TEXT("G03"), TEXT("熊の盾持ち"), ECGCardType::Unit, ECGColor::Green, 2, 1, 2, CGEffectId::None, 0, 0.f, TEXT("Clone"),
				TEXT("分身: このユニットが攻撃を受けて生き残ったとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::SurvivedAttack, 1),
			// 3000戦シミュレーションでキーワード無しのバニラながら64.0%と緑の中でも
			// 最上位だったため、素のステータスを下げる代わりに条件付きバフを持たせた
			// (docs/next-ruleset-cards-v1.md「緑」参照)。
			MakeCard(TEXT("G04"), TEXT("森の猪"), ECGCardType::Unit, ECGColor::Green, 2, 1, 2, CGEffectId::OnPlaySelfBuffAtkIfAlliesPresent, 1, 0.f, TEXT(""),
				TEXT("登場時、場に他の味方Unitが3体以上いるなら自身が+1/+0")),
			MakeCard(TEXT("G05"), TEXT("巡礼の呼び声"), ECGCardType::Spell, ECGColor::Green, 2, 0, 0, CGEffectId::SummonToughApprenticeTokens, 2, 0.f, TEXT(""),
				TEXT("1/1のUnitを2体出す")),
			// 「緑に回復できる要素を少し増やしてほしい」というフィードバックを受け、
			// 登場時回復を1→2に調整(赤の常時顔面狙いレース戦略への対抗策)。
			MakeCard(TEXT("G06"), TEXT("世界樹の巫女"), ECGCardType::Unit, ECGColor::Green, 3, 1, 3, CGEffectId::OnPlayHealSelf, 2, 0.f, TEXT("Clone"),
				TEXT("登場時、味方リーダーを2回復。分身: 自分以外に味方Unitが2体以上いるとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AllyUnitCountAtLeast, 2),
			// 3000戦シミュレーションでキーワード無しのバニラながら65.3%と緑最強格
			// だったため、素のステータスを大きく下げて分身+庇護を持たせた。庇護は
			// 「紫に多いが他色にも例外的に付いてよい」という方針のもと、緑にも
			// 意図的に1枚だけ残している(docs/keywords.md「庇護」参照)。
			MakeCard(TEXT("G07"), TEXT("猛る大鹿"), ECGCardType::Unit, ECGColor::Green, 3, 1, 2, CGEffectId::None, 0, 0.f, TEXT("Clone,Guard"),
				TEXT("庇護。分身: 自分のリーダーの体力が10以下のとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::LeaderHpAtMost, 10),
			MakeCard(TEXT("G08"), TEXT("祈りの輪"), ECGCardType::Spell, ECGColor::Green, 3, 0, 0, CGEffectId::BuffAllAlliesAtkOnly, 1, 0.f, TEXT(""),
				TEXT("場のUnitすべてのステータスを+1/+0")),
			MakeCard(TEXT("G09"), TEXT("行列を守る巨躯"), ECGCardType::Unit, ECGColor::Green, 4, 2, 4, CGEffectId::None, 0, 0.f, TEXT("Clone"),
				TEXT("分身: 自分以外に味方Unitが3体以上いるとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AllyUnitCountAtLeast, 3),
			MakeCard(TEXT("G10"), TEXT("群像の雄叫び"), ECGCardType::Unit, ECGColor::Green, 4, 3, 4, CGEffectId::OnPlayBuffSelfIfAlliesPresent, 2, 0.f, TEXT(""),
				TEXT("登場時、場に他の味方Unitが3体以上いるなら自身が+2/+2")),
			// トークン生成スペルが強すぎるというフィードバックへの対応で「大群の号令」を
			// 削除し、自分の場のUnit数を参照する除去スペルに差し替えた。物量(Unit数)を
			// 攻撃力に転換する役割にすることで、分身と組み合わせたときの物量シナジーを
			// 保ちつつ、トークン展開そのものは増やさない。
			MakeCard(TEXT("G11"), TEXT("群れの猛攻"), ECGCardType::Spell, ECGColor::Green, 3, 0, 0, CGEffectId::OnPlayDamageTargetByAllyUnitCount, 0, 0.f, TEXT(""),
				TEXT("自分の場にいるUnitの数だけ、敵Unit1体にダメージを与える")),
			// 「緑に回復できる要素を少し増やしてほしい」というフィードバックを受け、
			// 被弾時回復を1→2に調整(実際の回復量はACGGameMode::ExecuteAttack側の
			// ハードコード値、CGGameMode.cpp参照。EffectValueは未使用)。
			// 「回復が条件のものはキーワードではなくそういうカードということに
			// してほしい」というフィードバックを受け、Cloneタグを外した(◇分身の
			// バッジは表示されなくなる)。CloneConditionId=LeaderHealedによる
			// 実際の複製処理そのものは変更していない(CGCloneConditionIdはTagsとは
			// 独立に判定されるため、タグを外しても動作は変わらない)。
			MakeCard(TEXT("G12"), TEXT("不屈の大樹"), ECGCardType::Unit, ECGColor::Green, 5, 3, 5, CGEffectId::OnDefendHealSelf1, 0, 0.f, TEXT(""),
				TEXT("このユニットが攻撃を受けるたび、味方リーダーを2回復。味方リーダーが回復するたび、このユニットの素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::LeaderHealed, 1),
			// 「G13とG03はダメージを受けて生き残ったときにしてください」という
			// フィードバックを受け、AttackedAndSurvived(攻撃して生き残った)から
			// SurvivedAttack(攻撃を受けて生き残った、G03と同じ条件)に変更した。
			MakeCard(TEXT("G13"), TEXT("森の巨人"), ECGCardType::Unit, ECGColor::Green, 5, 4, 4, CGEffectId::None, 0, 0.f, TEXT("Clone"),
				TEXT("分身: このユニットが攻撃を受けて生き残ったとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::SurvivedAttack, 1),
			// 500戦シミュレーションで66.2%と緑の中でも最強だったため+2/+2→+1/+1に
			// 調整した(分身/トークンで横に並べる緑と全体バフの相性が良すぎた。
			// docs/next-ruleset-cards-v1.md「緑」参照)。
			MakeCard(TEXT("G14"), TEXT("世界樹の加護"), ECGCardType::Spell, ECGColor::Green, 5, 0, 0, CGEffectId::BuffAllAlliesFlat, 1, 0.f, TEXT(""),
				TEXT("場の味方Unit全てを+1/+1")),
			MakeCard(TEXT("G15"), TEXT("世界樹に選ばれし者"), ECGCardType::Unit, ECGColor::Green, 6, 4, 5, CGEffectId::OnPlayHealSelf, 2, 0.f, TEXT("Clone"),
				TEXT("登場時、味方リーダーを2回復。分身: 自分以外に味方Unitが4体以上いるとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AllyUnitCountAtLeast, 4),

			// --- 青(除去)— 断罪 ---
			MakeCard(TEXT("B01"), TEXT("封書の術"), ECGCardType::Spell, ECGColor::Blue, 1, 0, 0, CGEffectId::SealSpellByPower, 1, 0.f, TEXT("Seal"),
				TEXT("断罪。パワー1以下の敵Unit1体を断罪する")),
			// 「青色の断罪できるユニットのステータスを下げてほしい。B02を1/1に
			// してほしい」というフィードバックを受け、1/2→1/1に調整。
			MakeCard(TEXT("B02"), TEXT("一年生の書架番"), ECGCardType::Unit, ECGColor::Blue, 1, 1, 1, CGEffectId::OnPlayDebuffTarget, -1, 0.f, TEXT(""),
				TEXT("登場時、敵Unit1体を-1/-1する")),
			MakeCard(TEXT("B03"), TEXT("禁書の封印"), ECGCardType::Spell, ECGColor::Blue, 2, 0, 0, CGEffectId::SealSpell, 2, 0.f, TEXT("Seal"),
				TEXT("断罪。コスト2以下の敵Unit1体を断罪する")),
			// 「断罪するたびの効果は自分が断罪するたびにしてほしい」という
			// フィードバックを受け、説明文を明確化(実装(NotifySealSucceededが
			// ACGGameMode::ResolvePendingChoiceSealTarget等から`Side`=断罪を
			// 行った側でのみ呼ばれる)は元々自分の断罪のみに反応していたが、
			// 説明文が曖昧だったため「自分が」を明記した)。
			MakeCard(TEXT("B04"), TEXT("頁繰りの魔道士"), ECGCardType::Unit, ECGColor::Blue, 2, 2, 2, CGEffectId::OnSealDamageFace1, 0, 0.f, TEXT(""),
				TEXT("自分が断罪するたびに敵リーダーに1ダメージ")),
			MakeCard(TEXT("B05"), TEXT("荒野の記録係"), ECGCardType::Unit, ECGColor::Blue, 2, 1, 1, CGEffectId::OnTurnStartDraw, 0, 0.f, TEXT(""),
				TEXT("自分のターン開始時、このユニットが場にいれば1ドロー")),
			// 「B06を4コスト0/3、このユニットがいる限り敵のユニットの能力は
			// 発動しないという効果にしたい」というフィードバックを受け、断罪(Seal)
			// Spellから常在アウラ持ちUnitへ全面的に作り替えた。以前は
			// CGEffectId::SealSpell(コスト制限なし版)を使うSpellだったが、B03の
			// SealSpellとは独立の効果だったため、この変更はB03に影響しない。
			// 実装はACGPlayerState::AreUnitAbilitiesSuppressedByEnemy参照
			// (登場時/死亡時/常在アウラ等、Opponentを扱える発動箇所すべてで判定)。
			MakeCard(TEXT("B06"), TEXT("深淵の封印"), ECGCardType::Unit, ECGColor::Blue, 4, 0, 3, CGEffectId::SuppressEnemyUnitAbilities, 0, 0.f, TEXT(""),
				TEXT("このUnitが場にいる限り、敵Unitの能力は発動しない")),
			// 「青色の断罪できるユニットのステータスを下げてほしい」というフィード
			// バックを受け、断罪キーワード持ちUnit(B07/B11/B13)とB14(常在断罪)の
			// HPをそれぞれ1下げた。
			MakeCard(TEXT("B07"), TEXT("書庫の門番"), ECGCardType::Unit, ECGColor::Blue, 3, 1, 2, CGEffectId::SealOnPlay, 3, 0.f, TEXT("Seal"),
				TEXT("断罪。登場時、コスト3以下の敵Unit1体を断罪する")),
			// ドキュメントには元々「0/4、断罪するたびに1ドロー」と設計されていたが
			// 実装が漏れてバニラ(2/5)のままだった。「バニラを無くしたい」という
			// フィードバックを機に、ドキュメント通りの効果を実装した。その後、
			// 3000戦シミュレーションで赤vs青が78%まで偏った(Atk0で反撃できない
			// ことがアグロに弱い一因と推測)ため、Atkだけ0→1に戻した。
			// 「B08を1/3にしてほしい」というフィードバックを受け、1/4→1/3に調整。
			MakeCard(TEXT("B08"), TEXT("万年七年生"), ECGCardType::Unit, ECGColor::Blue, 3, 1, 3, CGEffectId::OnSealDraw1, 0, 0.f, TEXT(""),
				TEXT("自分が断罪するたびに1ドロー")),
			MakeCard(TEXT("B09"), TEXT("叡智の追放"), ECGCardType::Spell, ECGColor::Blue, 4, 0, 0, CGEffectId::SealSpellGrantPurchaseMana, -1, 0.f, TEXT("Seal"),
				TEXT("断罪。敵Unit1体を断罪し、コインを1増加")),
			// 「B10 2/3」「B11 2/3」というフィードバックを受け、それぞれ調整。
			MakeCard(TEXT("B10"), TEXT("弱点の考察官"), ECGCardType::Unit, ECGColor::Blue, 4, 2, 3, CGEffectId::EndTurnDebuffHighestCostEnemy, -2, 0.f, TEXT(""),
				TEXT("自分のターン終了時、敵Unitの中で最もコストが高いものを-2/-2")),
			MakeCard(TEXT("B11"), TEXT("追放の執行者"), ECGCardType::Unit, ECGColor::Blue, 4, 2, 3, CGEffectId::SealOnPlayByPower, 2, 0.f, TEXT("Seal"),
				TEXT("断罪。登場時、パワー(現在の攻撃力)2以下の敵Unit1体を断罪する")),
			MakeCard(TEXT("B12"), TEXT("禁書一斉開帳"), ECGCardType::Spell, ECGColor::Blue, 5, 0, 0, CGEffectId::MassDebuffEnemies, 2, 0.f, TEXT(""),
				TEXT("敵Unit全てを-2/-2する")),
			// 「B13 3/3」というフィードバックを受け、4/3→3/3に調整。
			MakeCard(TEXT("B13"), TEXT("叡智の大魔導"), ECGCardType::Unit, ECGColor::Blue, 5, 3, 3, CGEffectId::SealOnPlayByPower, 3, 0.f, TEXT("Seal"),
				TEXT("断罪。登場時、パワー(現在の攻撃力)3以下の敵Unit1体を断罪する")),
			// 「B14の上限をなくしてほしい。1/3にしてほしい」というフィードバックを
			// 受け、コスト上限(EffectValue)を2→-1(無条件)にし、3/2→1/3に調整。
			MakeCard(TEXT("B14"), TEXT("終焉の裁定者"), ECGCardType::Unit, ECGColor::Blue, 5, 1, 3, CGEffectId::OnTurnStartSealHighestCostEnemy, -1, 0.f, TEXT(""),
				TEXT("自分のターン開始時1回、敵Unitの中で最もコストが高いものを断罪する(コスト制限なし)")),
			MakeCard(TEXT("B15"), TEXT("双つの追放"), ECGCardType::Spell, ECGColor::Blue, 6, 0, 0, CGEffectId::SealSpellTwo, -1, 0.f, TEXT("Seal"),
				TEXT("断罪。敵Unit2体を断罪する")),

			// --- 紫(強化/変容)— 変貌 ---
			// 8000戦超のシミュレーションで紫15種(P16追加後16種)全てが単独で
			// 勝率50%を下回っており(33〜44%)、色全体が構造的に弱いと判明した
			// (docs/next-ruleset-simulation-v1.md「第19回」「第20回」参照)。
			// 変貌が完了する前に除去されると投資が無駄になるという弱点に対して、
			// 変貌元Unitの素のHPを+1して除去耐性を上げ、変貌先Unitのステータスを
			// +1/+1して「変貌が完了した後の見返り」自体も底上げする、という
			// 2段構えの底上げを全体に適用した(第20回)。
			// 「橙(黄色)と紫で強すぎるユニットのステータスを下げてほしい」という
			// フィードバックを受け、P01/P05/P14(いずれも3000戦で55%超)の
			// HPをそれぞれ1下げた。
			MakeCard(TEXT("P01"), TEXT("末席の令嬢"), ECGCardType::Unit, ECGColor::Purple, 1, 1, 1, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 自分のターンを2回経過すると「叙爵された令嬢」に変貌"),
				TEXT(""), TEXT(""), TEXT("P01T"), CGTransformConditionId::TurnsInPlay, 2),
			MakeCard(TEXT("P02"), TEXT("叙爵"), ECGCardType::Spell, ECGColor::Purple, 1, 0, 0, CGEffectId::BuffAllyTarget, 1, 0.f, TEXT(""),
				TEXT("味方Unit1体を+1/+1する")),
			MakeCard(TEXT("P03"), TEXT("香り売りの侍従"), ECGCardType::Unit, ECGColor::Purple, 2, 0, 3, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 対象指定の味方強化を2回受けると「紋章を継ぐ侍従」に変貌"),
				TEXT(""), TEXT(""), TEXT("P03T"), CGTransformConditionId::BuffedCount, 2),
			MakeCard(TEXT("P04"), TEXT("宝飾の重ね着け"), ECGCardType::Spell, ECGColor::Purple, 2, 0, 0, CGEffectId::BuffAllyTarget, 2, 0.f, TEXT(""),
				TEXT("味方Unit1体を+2/+2する")),
			// 護衛(庇護): 変貌が完了するまでの投資(P01/P03等)を守るための紫の護衛ユニット
			// (docs/next-ruleset-cards-v1.md「紫」。「変貌した後に庇護を付けるというより、
			// 変貌前のカードを守るために庇護を持ったカードを用意する」というフィードバックへの対応)。
			MakeCard(TEXT("P05"), TEXT("荒野を測る廷臣"), ECGCardType::Unit, ECGColor::Purple, 2, 2, 1, CGEffectId::OnPlayBuffAllyTarget, 1, 0.f, TEXT("Guard"),
				TEXT("庇護。登場時、味方Unit1体を+1/+1する(元の設計は+1/+2だが対称なバフに簡略化)")),
			MakeCard(TEXT("P06"), TEXT("秘めた野心の廷臣"), ECGCardType::Unit, ECGColor::Purple, 3, 2, 3, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 手札が4枚以下になると「野望を解き放った者」に変貌"),
				TEXT(""), TEXT(""), TEXT("P06T"), CGTransformConditionId::HandSizeAtMost, 4),
			MakeCard(TEXT("P07"), TEXT("重ねすぎた装い"), ECGCardType::Spell, ECGColor::Purple, 3, 0, 0, CGEffectId::BuffAllyTarget, 3, 0.f, TEXT(""),
				TEXT("味方Unit1体を+3/+3する")),
			MakeCard(TEXT("P08"), TEXT("仮面の廷臣"), ECGCardType::Unit, ECGColor::Purple, 3, 2, 5, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 場に2ターンいると「仮面を脱いだ者」に変貌"),
				TEXT(""), TEXT(""), TEXT("P08T"), CGTransformConditionId::TurnsInPlay, 2),
			MakeCard(TEXT("P09"), TEXT("喪に服す貴族"), ECGCardType::Unit, ECGColor::Purple, 4, 3, 5, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 味方Unitが2体死亡すると「怒りに呑まれし貴族」に変貌"),
				TEXT(""), TEXT(""), TEXT("P09T"), CGTransformConditionId::AlliesDiedSinceSummon, 2),
			MakeCard(TEXT("P10"), TEXT("秘宝の授与"), ECGCardType::Spell, ECGColor::Purple, 4, 0, 0, CGEffectId::BuffAllyTargetAndDraw, 3, 0.f, TEXT(""),
				TEXT("味方Unit1体を+3/+3し、1ドローする")),
			MakeCard(TEXT("P11"), TEXT("取り次ぎの女官"), ECGCardType::Unit, ECGColor::Purple, 4, 3, 4, CGEffectId::OnPlayGrantNextUnitPlayDiscount, 2, 0.f, TEXT("Guard"),
				TEXT("庇護。登場時、次にプレイするUnit1体のコストを2軽減する(元の設計は「手札の特定カードを軽減」だが、同名重複カードを区別する仕組みが無いため簡略化)")),
			MakeCard(TEXT("P12"), TEXT("王座を窺う者"), ECGCardType::Unit, ECGColor::Purple, 5, 3, 6, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: このターン中に味方の変貌が2回成功していれば、即座に「王座に迫る者」に変貌"),
				TEXT(""), TEXT(""), TEXT("P12T"), CGTransformConditionId::TransformsThisTurnCount, 2),
			MakeCard(TEXT("P13"), TEXT("玉座の噂"), ECGCardType::Spell, ECGColor::Purple, 5, 0, 0, CGEffectId::ForceTransformAllAllies, 0, 0.f, TEXT(""),
				TEXT("変貌条件を無視して、変貌可能な味方Unitをすべて変貌させる")),
			// 「P14が強すぎる」というフィードバックを受け、ステータスを5/5→3/3に調整
			// (庇護+登場時+2/+2+次Unit1コスト軽減という効果の強さに対してコストが
			// 5では割に合いすぎていたため)。
			MakeCard(TEXT("P14"), TEXT("宮廷の采配官"), ECGCardType::Unit, ECGColor::Purple, 5, 3, 2, CGEffectId::OnPlayBuffAllyTargetAndUnitDiscount, 2, 0.f, TEXT("Guard"),
				TEXT("庇護。登場時、味方Unit1体を+2/+2し、次にプレイするUnit1体のコストを1軽減する(手札の特定カードを軽減する原設計をP11と同じ方針で簡略化)")),
			MakeCard(TEXT("P15"), TEXT("空位の座を望む者"), ECGCardType::Unit, ECGColor::Purple, 6, 6, 3, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 自分のターン開始時、50%の確率で「紫冠の女王」に変貌"),
				TEXT(""), TEXT(""), TEXT("P15T"), CGTransformConditionId::RandomChancePerTurn, 50),
			// 紫の補強用に追加(16枚目、docs/next-ruleset-simulation-v1.md「第19回」)。
			// 他の変貌元Unit(P01/P03/P06/P08/P09/P12/P15)が自力で条件を満たすまで
			// 待つ必要があるのに対し、この1枚は「登場した瞬間に他の1体を変貌させる」
			// ことで変貌のタイミングを前倒しできる、紫のテンポ不足を補う狙いのカード。
			MakeCard(TEXT("P16"), TEXT("野心を焚きつける密使"), ECGCardType::Unit, ECGColor::Purple, 2, 1, 1, CGEffectId::OnPlayForceTransformAllyTarget, 0, 0.f, TEXT(""),
				TEXT("登場時、変貌条件を無視して味方Unit1体を選んで変貌させる(対象がいなければ何も起きない)")),
		};
		return Cards;
	}

	// 変貌先カード(P01T〜P15T)。マーケットには並ばず、購入・手札からのプレイも
	// 不可(コスト0)。対応する変貌元カードの変貌効果によってのみ場に出現する
	// (docs/next-ruleset-cards-v1.md「変貌先カード」)。BuildTokenCards()に含める
	// ことで、名もなき随行者と同じ「実在カードプールには含まれないが参照はできる」
	// 扱いにしている。
	const TArray<FCGCardDef>& BuildTransformTargetCards()
	{
		static const TArray<FCGCardDef> Cards = {
			// 紫のステータス全体ダウン(P05/P11/P14が庇護持ちの護衛になった分の底上げを
			// 打ち消すため、第20回で乗せた+1/+1のうちHP側を巻き戻した。docs/next-ruleset-
			// cards-v1.md「紫」)。P15Tだけは元々HP4と紙耐久な設計のため据え置き。
			MakeCard(TEXT("P01T"), TEXT("叙爵された令嬢"), ECGCardType::Unit, ECGColor::Purple, 1, 3, 3, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("P03T"), TEXT("紋章を継ぐ侍従"), ECGCardType::Unit, ECGColor::Purple, 2, 3, 4, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("P06T"), TEXT("野望を解き放った者"), ECGCardType::Unit, ECGColor::Purple, 3, 5, 4, CGEffectId::OnPlayHealSelf, 1, 0.f, TEXT(""),
				TEXT("場に出た時に味方リーダーを1回復")),
			MakeCard(TEXT("P08T"), TEXT("仮面を脱いだ者"), ECGCardType::Unit, ECGColor::Purple, 3, 6, 6, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("P09T"), TEXT("怒りに呑まれし貴族"), ECGCardType::Unit, ECGColor::Purple, 4, 7, 6, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("P12T"), TEXT("王座に迫る者"), ECGCardType::Unit, ECGColor::Purple, 5, 8, 7, CGEffectId::None, 0, 0.f, TEXT("Guard"),
				TEXT("庇護")),
			MakeCard(TEXT("P15T"), TEXT("紫冠の女王"), ECGCardType::Unit, ECGColor::Purple, 6, 11, 4, CGEffectId::OnPlayDamageFace, 2, 0.f, TEXT(""),
				TEXT("登場時、敵リーダーへ2ダメージ")),
		};
		return Cards;
	}

	// マーケット/初期デッキには含まれない、効果生成専用のトークン。
	// (彷徨う者たちの合流 C020が「1/1 Unitを出す」、G05巡礼の呼び声が
	// 「1/2 Unitを出す」ために使う)
	const TArray<FCGCardDef>& BuildTokenCards()
	{
		static const TArray<FCGCardDef> Tokens = {
			MakeCard(TEXT("TK_APPRENTICE"), TEXT("名もなき随行者"), ECGCardType::Unit, ECGColor::None, 0, 1, 1, CGEffectId::None, 0, 0.f, TEXT(""), TEXT("")),
			MakeCard(TEXT("TK_APPRENTICE_TOUGH"), TEXT("屈強な随行者"), ECGCardType::Unit, ECGColor::None, 0, 1, 2, CGEffectId::None, 0, 0.f, TEXT(""), TEXT("")),
			// R08赤熱の一閃が場に出すトークン。死亡時、敵リーダーへ1ダメージ
			// (docs/next-ruleset-cards-v1.md「赤」参照)。
			MakeCard(TEXT("TK_EMBER"), TEXT("赤熱の残り火"), ECGCardType::Unit, ECGColor::None, 0, 0, 1, CGEffectId::OnDeathDamageFace1, 1, 0.f, TEXT(""), TEXT("死亡時、敵リーダーへ1ダメージ")),
		};
		return Tokens;
	}

	// フィニッシャー(各色固有の条件を達成すると、手札・コストを介さず自動で
	// 場に駆けつける専用Unit。ACGPlayerState::CheckAndSpawnFinisher参照)。
	// 変貌先カード・トークンと同じく、マーケット/デッキ構築には出さない
	// (GetAllCards()/GetBuildableCards()には含めず、FindCard()経由でのみ参照する)。
	// コストは表示上の目安(10)で、実際には支払わず自動召喚される。
	const TArray<FCGCardDef>& BuildFinisherCards()
	{
		static const TArray<FCGCardDef> Cards = {
			// 赤: 味方Unitが10体死亡すると駆けつける。専用効果は無く、疾駆持ちの
			// 大きなステータスだけで即座に勝負を決めに行く。
			MakeCard(TEXT("FIN_RED"), TEXT("夜半に斬り結ぶ者"), ECGCardType::Unit, ECGColor::Red, 10, 8, 8, CGEffectId::None, 0, 0.f, TEXT("Haste,Finisher"),
				TEXT("フィニッシャー。味方Unitが10体死亡すると戦場に駆けつける。疾駆。"),
				TEXT(""), TEXT("屍を越えてなお、彼だけは燃え続けている。")),
			// 橙: マーケットから10枚購入すると駆けつける。登場時、マーケットの
			// Unitを3枚、プレイヤーが1枚ずつ選んでコストを支払わずそのまま場に出す
			// (O15黒鉄の買い占め屋と同じ選択の仕組みを3回繰り返す。「プレイヤーがマーケット
			// のカードを選択して無料でプレイできるカードを選びたい」という
			// フィードバックへの対応。以前はプレイヤーに選ばせず自動で3枚出していた)。
			MakeCard(TEXT("FIN_ORANGE"), TEXT("港を興す者"), ECGCardType::Unit, ECGColor::Orange, 10, 5, 5, CGEffectId::OnPlayFreeMarketCards, 3, 0.f, TEXT("Finisher"),
				TEXT("フィニッシャー。マーケットから10枚購入すると戦場に駆けつける。登場時、マーケットのUnitを3枚、コストを支払わず選んで場に出す。"),
				TEXT(""), TEXT("市場の全てが、彼一人のために動いていた。")),
			// 青: カードを20枚ドローすると駆けつける。登場時、敵の場を全て追放する。
			MakeCard(TEXT("FIN_BLUE"), TEXT("書庫の大賢者"), ECGCardType::Unit, ECGColor::Blue, 10, 6, 6, CGEffectId::OnPlayExileAllEnemyUnits, 0, 0.f, TEXT("Finisher"),
				TEXT("フィニッシャー。カードを20枚ドローすると戦場に駆けつける。登場時、敵の場のUnitをすべて追放する。"),
				TEXT(""), TEXT("知り尽くした者だけが、すべてを裁く権利を持つ。")),
			// 緑: 場にUnitが6体並ぶと駆けつける。登場時、味方全体を+1/+1する。
			MakeCard(TEXT("FIN_GREEN"), TEXT("灰の地に立つ世界樹"), ECGCardType::Unit, ECGColor::Green, 10, 4, 4, CGEffectId::OnPlayBuffAllAlliesFlat, 1, 0.f, TEXT("Finisher"),
				TEXT("フィニッシャー。場にUnitが6体並ぶと戦場に駆けつける。登場時、味方Unitすべてを+1/+1する。"),
				TEXT(""), TEXT("大地そのものが、群れに応えて姿を成した。")),
			// 紫: ユニットが5体変貌すると駆けつける。常在効果(EffectIdは登場時
			// ハンドラには登録せずHasBoardUnitWithEffectで判定、PlayCardFromHand参照)。
			MakeCard(TEXT("FIN_PURPLE"), TEXT("玉座に憑く者"), ECGCardType::Unit, ECGColor::Purple, 10, 5, 5, CGEffectId::FinisherTransformAura, 0, 0.f, TEXT("Finisher"),
				TEXT("フィニッシャー。Unitが5体変貌すると戦場に駆けつける。このユニットがいる限り、新しく登場した味方Unitは変貌条件を無視して即座に変貌する。"),
				TEXT(""), TEXT("形あるものすべては、彼女の前で仮初めに過ぎない。")),
		};
		return Cards;
	}
}

const TArray<FCGCardDef>& UCGCardDatabase::GetAllCards()
{
	// 次期ルール(docs/next-ruleset-design.md)の5色75種+旧24種(無色、
	// BuildLegacyCards)を合わせた全カードプール。旧24種は元々は下位互換の
	// フォールバック用途だったが、「無色」という6つ目のグループとしてデッキ
	// 構築画面・カード図鑑にも表示するようにした(GetBuildableCards()参照。
	// docs/game-rules-minimum.md「色ガイド」参照)。
	static const TArray<FCGCardDef> All = []()
	{
		TArray<FCGCardDef> Combined = BuildNextRulesetCards();
		Combined.Append(BuildLegacyCards());
		return Combined;
	}();
	return All;
}

const TArray<FCGCardDef>& UCGCardDatabase::GetBuildableCards()
{
	// デッキ構築画面・カード図鑑の一覧に表示する全カード(5色75種+無色24種、
	// 計99種)。無色カードはどの色のデッキにも入れられ、色のパッシブしきい値
	// (17/25)にはカウントされない(ComputeActiveColors()がColor::Noneを
	// 除外している)。
	return GetAllCards();
}

bool UCGCardDatabase::FindCard(FName CardId, FCGCardDef& OutCard)
{
	for (const FCGCardDef& Card : GetAllCards())
	{
		if (Card.CardId == CardId)
		{
			OutCard = Card;
			return true;
		}
	}
	for (const FCGCardDef& Card : BuildTransformTargetCards())
	{
		if (Card.CardId == CardId)
		{
			OutCard = Card;
			return true;
		}
	}
	for (const FCGCardDef& Card : BuildTokenCards())
	{
		if (Card.CardId == CardId)
		{
			OutCard = Card;
			return true;
		}
	}
	for (const FCGCardDef& Card : BuildFinisherCards())
	{
		if (Card.CardId == CardId)
		{
			OutCard = Card;
			return true;
		}
	}
	return false;
}

TArray<FName> UCGCardDatabase::GetAllCardIds()
{
	TArray<FName> Ids;
	Ids.Reserve(GetAllCards().Num());
	for (const FCGCardDef& Card : GetAllCards())
	{
		Ids.Add(Card.CardId);
	}
	return Ids;
}

TArray<FName> UCGCardDatabase::GetStarterDeckCardIds()
{
	// 次期ルール(docs/next-ruleset-design.md)のフォールバック初期デッキ(25枚)。
	// デッキ構築を経ずに直接バトル画面へ入った場合(AI側および未構築時の人間側)用。
	// 特定の色のしきい値(17/25)を満たさない、複数色混在の標準的な構成にしている
	// (単色デッキの検証はデッキ構築画面から個別に組んで行う想定)。
	return {
		FName(TEXT("R01")), FName(TEXT("R02")), FName(TEXT("R04")), FName(TEXT("R05")),
		FName(TEXT("O01")), FName(TEXT("O03")), FName(TEXT("O07")),
		FName(TEXT("G01")), FName(TEXT("G02")), FName(TEXT("G04")), FName(TEXT("G05")), FName(TEXT("G07")),
		FName(TEXT("B01")), FName(TEXT("B02")), FName(TEXT("B03")), FName(TEXT("B08")),
		FName(TEXT("P01")), FName(TEXT("P02")), FName(TEXT("P04")), FName(TEXT("P05")),
		FName(TEXT("R07")), FName(TEXT("G09")), FName(TEXT("B07")), FName(TEXT("O06")), FName(TEXT("P08")),
	};
}

TArray<FName> UCGCardDatabase::GetBasicColorDeckCardIds(ECGColor Color)
{
	// 各色15種類のうち5種類を3枚、残り10種類を1枚にした25枚の純色構成
	// (5×3+10×1=25)。しきい値17/25を余裕を持って超える(25/25)ため、必ず
	// パッシブが発動する。3枚採用する5種類は、その色のキーワード(疾駆/先物/
	// 庇護/断罪/変貌)を持つ、または軸となる効果を持つカードの中から低〜中コスト
	// を中心に選んでいる(docs/next-ruleset-cards-v1.md「サンプルデッキ」参照)。
	//
	// フィニッシャー導入・調整後、この構成を何段階かに分けて見直したが
	// (docs/next-ruleset-simulation-v1.md「第17回」「第18回」参照)、
	// ①勝率上位5枚への全面差し替え(全色)、②勝率+コストカーブ+色の軸を
	// 加味した1色1〜2枚だけの入れ替え(赤/緑/青個別)のどちらを試しても、
	// その色単体は改善しても他色(特に橙)が巻き添えで悪化し、5色トータルの
	// 勝率のばらつきは常にこの構成(無調整)より悪化した。5色は互いに
	// 影響し合う閉じた環境のため、1色を強化すると必ずどこかの対面バランスが
	// 崩れる。この構成が現時点で最もばらつきが小さい(紫を除き52.5〜55.3%)
	// ことを複数回の検証で確認済みのため、変更しないことに決めている
	// (ただし赤に新規カードR16を追加した際は、既存10種の入れ替えではなく
	// 単純に11種目の1枚採用として追加した。26枚デッキになるが、パッシブの
	// しきい値17は固定値でありデッキ総数に依存しないため問題ない)。
	switch (Color)
	{
	case ECGColor::Red:
		return {
			FName(TEXT("R01")), FName(TEXT("R01")), FName(TEXT("R01")),
			FName(TEXT("R02")), FName(TEXT("R02")), FName(TEXT("R02")),
			FName(TEXT("R04")), FName(TEXT("R04")), FName(TEXT("R04")),
			FName(TEXT("R07")), FName(TEXT("R07")), FName(TEXT("R07")),
			FName(TEXT("R10")), FName(TEXT("R10")), FName(TEXT("R10")),
			FName(TEXT("R03")), FName(TEXT("R05")), FName(TEXT("R06")), FName(TEXT("R08")), FName(TEXT("R09")),
			FName(TEXT("R11")), FName(TEXT("R12")), FName(TEXT("R13")), FName(TEXT("R14")), FName(TEXT("R15")),
			FName(TEXT("R16")),
		};
	case ECGColor::Orange:
		// 3000戦シミュレーション(実測)で橙は44.3%と全色最弱、特に紫に25%(58-172)
		// しか勝てないことが判明した(橙は盤面に干渉する手段が無く、経済が育つ前に
		// 紫のバフ済みユニットに押し切られるため)。純粋な補助(O01の使い切りマナ、
		// O11の市場リロール)を減らし、実際に盤面で戦える大きめの体(O09: 4/4+即時
		// マナ2、O14: 3/6庇護+購入毎に成長)を3枚に増やした(「各色で強いと思える
		// デッキを作ってほしい」というフィードバックへの対応)。
		return {
			FName(TEXT("O03")), FName(TEXT("O03")), FName(TEXT("O03")),
			FName(TEXT("O06")), FName(TEXT("O06")), FName(TEXT("O06")),
			FName(TEXT("O08")), FName(TEXT("O08")), FName(TEXT("O08")),
			FName(TEXT("O09")), FName(TEXT("O09")), FName(TEXT("O09")),
			FName(TEXT("O14")), FName(TEXT("O14")), FName(TEXT("O14")),
			// O16(強奪の商人)追加に伴い、3000戦シミュレーションで橙最弱だった
			// O04(市場の噂話)をO16に入れ替えた(紫がP16追加時にP11をP16へ
			// 入れ替えたのと同じ方針)。O17(見張り番の樽詰め)追加に伴い、同様に
			// O01(風の相場)をO17に入れ替えた(赤の速攻に序盤から耐える庇護持ちを
			// 組み込む狙い)。
			FName(TEXT("O17")), FName(TEXT("O02")), FName(TEXT("O16")), FName(TEXT("O05")), FName(TEXT("O07")),
			FName(TEXT("O10")), FName(TEXT("O11")), FName(TEXT("O12")), FName(TEXT("O13")), FName(TEXT("O15")),
		};
	case ECGColor::Green:
		return {
			FName(TEXT("G01")), FName(TEXT("G01")), FName(TEXT("G01")),
			FName(TEXT("G03")), FName(TEXT("G03")), FName(TEXT("G03")),
			FName(TEXT("G05")), FName(TEXT("G05")), FName(TEXT("G05")),
			FName(TEXT("G06")), FName(TEXT("G06")), FName(TEXT("G06")),
			FName(TEXT("G09")), FName(TEXT("G09")), FName(TEXT("G09")),
			FName(TEXT("G02")), FName(TEXT("G04")), FName(TEXT("G07")), FName(TEXT("G08")), FName(TEXT("G10")),
			FName(TEXT("G11")), FName(TEXT("G12")), FName(TEXT("G13")), FName(TEXT("G14")), FName(TEXT("G15")),
		};
	case ECGColor::Blue:
		return {
			FName(TEXT("B01")), FName(TEXT("B01")), FName(TEXT("B01")),
			FName(TEXT("B03")), FName(TEXT("B03")), FName(TEXT("B03")),
			FName(TEXT("B07")), FName(TEXT("B07")), FName(TEXT("B07")),
			FName(TEXT("B08")), FName(TEXT("B08")), FName(TEXT("B08")),
			FName(TEXT("B11")), FName(TEXT("B11")), FName(TEXT("B11")),
			FName(TEXT("B02")), FName(TEXT("B04")), FName(TEXT("B05")), FName(TEXT("B06")), FName(TEXT("B09")),
			FName(TEXT("B10")), FName(TEXT("B12")), FName(TEXT("B13")), FName(TEXT("B14")), FName(TEXT("B15")),
		};
	case ECGColor::Purple:
		// P16(野心を焚きつける密使)を1枚組み込むため、既存の11枚目(P11取り次ぎの女官、
		// 変貌軸との噛み合いが薄く勝率も低め)と入れ替えた。P16は他の変貌元
		// Unit(P01/P03/P08)を早期に変貌させ、除去される前に投資を回収する
		// 狙い(docs/next-ruleset-simulation-v1.md「第19回」参照)。
		return {
			FName(TEXT("P01")), FName(TEXT("P01")), FName(TEXT("P01")),
			FName(TEXT("P02")), FName(TEXT("P02")), FName(TEXT("P02")),
			FName(TEXT("P03")), FName(TEXT("P03")), FName(TEXT("P03")),
			FName(TEXT("P05")), FName(TEXT("P05")), FName(TEXT("P05")),
			FName(TEXT("P08")), FName(TEXT("P08")), FName(TEXT("P08")),
			FName(TEXT("P04")), FName(TEXT("P06")), FName(TEXT("P07")), FName(TEXT("P09")), FName(TEXT("P10")),
			FName(TEXT("P16")), FName(TEXT("P12")), FName(TEXT("P13")), FName(TEXT("P14")), FName(TEXT("P15")),
		};
	case ECGColor::None:
	default:
		return {};
	}
}

FName UCGCardDatabase::GetFinisherCardId(ECGColor Color)
{
	switch (Color)
	{
	case ECGColor::Red:    return FName(TEXT("FIN_RED"));
	case ECGColor::Orange: return FName(TEXT("FIN_ORANGE"));
	case ECGColor::Blue:   return FName(TEXT("FIN_BLUE"));
	case ECGColor::Green:  return FName(TEXT("FIN_GREEN"));
	case ECGColor::Purple: return FName(TEXT("FIN_PURPLE"));
	case ECGColor::None:
	default:
		return NAME_None;
	}
}
