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
			MakeCard(TEXT("C001"), TEXT("先駆けの斥候"), ECGCardType::Unit, ECGColor::None, 1, 1, 1, CGEffectId::ScoutTop1, 0, 1.00f, TEXT(""),
				TEXT("登場時にデッキ上1枚を見て上下選択"), TEXT("斥候"), TEXT("地図にない道ほど、彼女の得意分野だった。")),
			MakeCard(TEXT("C002"), TEXT("炎走りの小鬼"), ECGCardType::Unit, ECGColor::None, 1, 2, 1, CGEffectId::None, 0, 1.50f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("C003"), TEXT("盾持ち見習い"), ECGCardType::Unit, ECGColor::None, 1, 1, 2, CGEffectId::None, 0, 2.00f, TEXT("Guard"),
				TEXT("")),
			MakeCard(TEXT("C004"), TEXT("小さな研究者"), ECGCardType::Unit, ECGColor::None, 1, 1, 1, CGEffectId::OnDeathDraw, 1, 1.75f, TEXT(""),
				TEXT("死亡時1ドロー")),
			MakeCard(TEXT("C005"), TEXT("切り込み隊長"), ECGCardType::Unit, ECGColor::None, 2, 2, 2, CGEffectId::None, 0, 1.25f, TEXT("Haste"),
				TEXT(""), TEXT("戦士"), TEXT("号令を待たず、彼はすでに駆けだしている。")),
			MakeCard(TEXT("C006"), TEXT("墓場あさり"), ECGCardType::Unit, ECGColor::None, 2, 2, 2, CGEffectId::GraveyardToDeckBottomDraw1, 1, 1.50f, TEXT(""),
				TEXT("登場時に墓地1枚を山札下へ、1ドロー")),
			MakeCard(TEXT("C007"), TEXT("市場の仲買人"), ECGCardType::Unit, ECGColor::None, 2, 1, 3, CGEffectId::OnBuyEndTurnDiscardDraw, 1, 1.25f, TEXT(""),
				TEXT("あなたが購入したターン終了時、手札を1枚捨てると1ドロー")),
			MakeCard(TEXT("C008"), TEXT("錆びた巨兵"), ECGCardType::Unit, ECGColor::None, 2, 3, 2, CGEffectId::OnPlayDiscard1, 1, 1.13f, TEXT(""),
				TEXT("登場時に手札1枚捨てる")),
			MakeCard(TEXT("C009"), TEXT("街道の突撃兵"), ECGCardType::Unit, ECGColor::None, 3, 3, 3, CGEffectId::SecondPlayBuff, 1, 1.08f, TEXT(""),
				TEXT("あなたがこのターン2枚目をプレイしていたら+1/+0"), TEXT("戦士"), TEXT("一度きりの突撃に、すべてを賭ける。")),
			MakeCard(TEXT("C010"), TEXT("追撃の射手"), ECGCardType::Unit, ECGColor::None, 3, 2, 3, CGEffectId::OnAllySpellPing1, 1, 1.08f, TEXT(""),
				TEXT("味方Spell使用時に敵リーダーへ1ダメージ(各ターン1回)")),
			MakeCard(TEXT("C011"), TEXT("再誕の司祭"), ECGCardType::Unit, ECGColor::None, 3, 2, 4, CGEffectId::OnPlayReturnGraveyardCheapCard, 1, 1.17f, TEXT(""),
				TEXT("登場時に墓地コスト1以下を手札へ")),
			MakeCard(TEXT("C012"), TEXT("市場監督官"), ECGCardType::Unit, ECGColor::None, 3, 3, 4, CGEffectId::BuyCostReductionThisTurn, 1, 1.33f, TEXT(""),
				TEXT("購入時コストをこのターンだけ1軽減(最小1)")),
			MakeCard(TEXT("C013"), TEXT("戦場の旗手"), ECGCardType::Unit, ECGColor::None, 4, 4, 4, CGEffectId::AllyBuffAtkThisTurn, 1, 1.13f, TEXT(""),
				TEXT("味方他Unit全て+1/+0(ターン中)"), TEXT("指揮官"), TEXT("旗が揺れるたび、味方の士気も揺れる。")),
			MakeCard(TEXT("C014"), TEXT("霊廟の守り手"), ECGCardType::Unit, ECGColor::None, 4, 3, 5, CGEffectId::OnDeathReturnRandomGraveyardUnit, 0, 1.13f, TEXT("Guard"),
				TEXT("死亡時にランダムで墓地のUnit1枚を山札へ"), TEXT("アンデッド"), TEXT("死してなお、その務めを忘れない。")),
			MakeCard(TEXT("C015"), TEXT("隕鉄の突進獣"), ECGCardType::Unit, ECGColor::None, 4, 5, 4, CGEffectId::None, 0, 1.13f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("C016"), TEXT("連鎖術の教授"), ECGCardType::Unit, ECGColor::None, 4, 3, 4, CGEffectId::FirstSpellBonusDamage, 1, 1.06f, TEXT(""),
				TEXT("あなたの毎ターン最初のSpellが+1ダメージ")),
			MakeCard(TEXT("C017"), TEXT("火花の一撃"), ECGCardType::Spell, ECGColor::None, 1, 0, 0, CGEffectId::OnPlayDamageTarget, 2, 1.00f, TEXT(""),
				TEXT("敵Unitへ2ダメージ"), TEXT(""), TEXT("小さな火花が、戦局を変えることもある。")),
			MakeCard(TEXT("C018"), TEXT("応急手当"), ECGCardType::Spell, ECGColor::None, 1, 0, 0, CGEffectId::OnPlayHealSelf, 3, 1.20f, TEXT(""),
				TEXT("味方リーダー3回復")),
			MakeCard(TEXT("C019"), TEXT("手札の選別"), ECGCardType::Spell, ECGColor::None, 1, 0, 0, CGEffectId::Discard1Draw2, 0, 1.25f, TEXT(""),
				TEXT("1枚捨てて2枚引く")),
			MakeCard(TEXT("C020"), TEXT("見習い召集"), ECGCardType::Spell, ECGColor::None, 2, 0, 0, CGEffectId::SummonApprenticeTokens, 2, 1.00f, TEXT(""),
				TEXT("1/1 Unitを2体出す")),
			MakeCard(TEXT("C021"), TEXT("墓地再点火"), ECGCardType::Spell, ECGColor::None, 2, 0, 0, CGEffectId::ReturnGraveyardSpellSelfDamage1, 1, 0.88f, TEXT(""),
				TEXT("墓地のSpell1枚を手札へ、1点自傷")),
			MakeCard(TEXT("C022"), TEXT("市場調達"), ECGCardType::Spell, ECGColor::None, 2, 0, 0, CGEffectId::BuyFromMarketCostUnder3ToHand, 3, 1.13f, TEXT(""),
				TEXT("マーケットからコスト3以下を購入し即手札へ")),
			MakeCard(TEXT("C023"), TEXT("連弾の雨"), ECGCardType::Spell, ECGColor::None, 3, 0, 0, CGEffectId::RandomEnemyDamage1x4, 1, 0.67f, TEXT(""),
				TEXT("ランダムな敵に1ダメージを4回")),
			MakeCard(TEXT("C024"), TEXT("逆転の号令"), ECGCardType::Spell, ECGColor::None, 3, 0, 0, CGEffectId::ConditionalDamage3or2, 0, 0.45f, TEXT(""),
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
			MakeCard(TEXT("R01"), TEXT("火口の悪童"), ECGCardType::Unit, ECGColor::Red, 1, 1, 1, CGEffectId::OnDeathDamageRandomEnemyUnit1, 1, 0.f, TEXT(""),
				TEXT("死亡時、敵のランダムなUnit1体に1ダメージ")),
			MakeCard(TEXT("R02"), TEXT("導火線"), ECGCardType::Spell, ECGColor::Red, 1, 0, 0, CGEffectId::OnPlayDamageTarget, 1, 0.f, TEXT(""),
				TEXT("敵Unit1体、または敵リーダーへ1ダメージ")),
			MakeCard(TEXT("R03"), TEXT("焼印の斥候"), ECGCardType::Unit, ECGColor::Red, 1, 1, 1, CGEffectId::None, 0, 0.f, TEXT("Haste"),
				TEXT("疾駆")),
			MakeCard(TEXT("R04"), TEXT("猛る野伏"), ECGCardType::Unit, ECGColor::Red, 2, 2, 1, CGEffectId::None, 0, 0.f, TEXT("Haste"),
				TEXT("疾駆")),
			MakeCard(TEXT("R05"), TEXT("爆ぜる火矢"), ECGCardType::Spell, ECGColor::Red, 2, 0, 0, CGEffectId::OnPlayDamageTarget, 3, 0.f, TEXT(""),
				TEXT("敵Unit1体へ3ダメージ")),
			MakeCard(TEXT("R06"), TEXT("自爆の火薬兵"), ECGCardType::Unit, ECGColor::Red, 2, 2, 2, CGEffectId::OnDeathDamageFace1, 1, 0.f, TEXT(""),
				TEXT("死亡時、敵リーダーへ1ダメージ")),
			MakeCard(TEXT("R07"), TEXT("疾風の剣士"), ECGCardType::Unit, ECGColor::Red, 3, 3, 2, CGEffectId::None, 0, 0.f, TEXT("Haste"),
				TEXT("疾駆")),
			MakeCard(TEXT("R08"), TEXT("業火の一撃"), ECGCardType::Spell, ECGColor::Red, 3, 0, 0, CGEffectId::OnPlayDamageFace, 4, 0.f, TEXT(""),
				TEXT("敵リーダーへ4ダメージ")),
			MakeCard(TEXT("R09"), TEXT("連携の火術師"), ECGCardType::Unit, ECGColor::Red, 3, 2, 3, CGEffectId::SecondPlayBuff, 2, 0.f, TEXT(""),
				TEXT("このターン他のUnitをプレイ済みなら+2/+0")),
			MakeCard(TEXT("R10"), TEXT("灼熱の突撃兵"), ECGCardType::Unit, ECGColor::Red, 4, 4, 3, CGEffectId::None, 0, 0.f, TEXT("Haste"),
				TEXT("疾駆")),
			MakeCard(TEXT("R11"), TEXT("業火の乱舞"), ECGCardType::Spell, ECGColor::Red, 4, 0, 0, CGEffectId::RandomEnemyDamage2x3, 2, 0.f, TEXT(""),
				TEXT("ランダムな敵Unit3体へ各2ダメージ(いなければ敵リーダーへ)")),
			MakeCard(TEXT("R12"), TEXT("怨嗟の炎術師"), ECGCardType::Unit, ECGColor::Red, 4, 3, 3, CGEffectId::OnAllyDeathDamageFace1, 1, 0.f, TEXT(""),
				TEXT("自分のUnit死亡時、敵リーダーへ1ダメージ")),
			MakeCard(TEXT("R13"), TEXT("猛火の巨人"), ECGCardType::Unit, ECGColor::Red, 5, 5, 3, CGEffectId::None, 0, 0.f, TEXT("Haste"),
				TEXT("疾駆")),
			MakeCard(TEXT("R14"), TEXT("総攻めの狼煙"), ECGCardType::Spell, ECGColor::Red, 5, 0, 0, CGEffectId::DamageFaceByUnitCount, 2, 0.f, TEXT(""),
				TEXT("敵リーダーへ、自分の場のUnit数×2ダメージ")),
			MakeCard(TEXT("R15"), TEXT("終末の火竜"), ECGCardType::Unit, ECGColor::Red, 6, 7, 4, CGEffectId::OnPlayDamageFace, 2, 0.f, TEXT("Haste"),
				TEXT("疾駆。登場時、敵リーダーへ2ダメージ")),

			// --- 橙(購入加速)— 先物 ---
			MakeCard(TEXT("O01"), TEXT("目利きの一手"), ECGCardType::Spell, ECGColor::Orange, 1, 0, 0, CGEffectId::GrantPurchaseMana, 1, 0.f, TEXT(""),
				TEXT("コインを1増加")),
			// バニラを無くしたいというフィードバックで先物(Discount)を付与。
			// 先物キーワード自体を「次の購入コストを1軽減」から「登場時コインを1増加」に
			// 変更したため(docs/keywords.md「先物」参照)、この変更でO03/O06/O11/O14の
			// 効果も同様に変わる。ステータスはキーワード追加分を相殺して1/2→1/1に調整。
			MakeCard(TEXT("O02"), TEXT("露店の商人"), ECGCardType::Unit, ECGColor::Orange, 1, 1, 1, CGEffectId::None, 0, 0.f, TEXT("Discount"),
				TEXT("先物")),
			MakeCard(TEXT("O03"), TEXT("先物師の弟子"), ECGCardType::Unit, ECGColor::Orange, 2, 2, 2, CGEffectId::None, 0, 0.f, TEXT("Discount"),
				TEXT("先物: 登場時、コインを1増やす")),
			MakeCard(TEXT("O04"), TEXT("市場の噂話"), ECGCardType::Spell, ECGColor::Orange, 2, 0, 0, CGEffectId::RerollMarketSlot, 0, 0.f, TEXT(""),
				TEXT("マーケットの好きな1枠を即座に補充し直す")),
			MakeCard(TEXT("O05"), TEXT("値切りの番人"), ECGCardType::Unit, ECGColor::Orange, 2, 1, 2, CGEffectId::OnBuyBuffSelfAtk, 1, 0.f, TEXT(""),
				TEXT("自分が購入するたび、このユニットは+1/+0(「このターン中」は簡略化して永続にしている)")),
			MakeCard(TEXT("O06"), TEXT("相場読みの商人"), ECGCardType::Unit, ECGColor::Orange, 3, 3, 3, CGEffectId::None, 0, 0.f, TEXT("Discount"),
				TEXT("先物: 登場時、コインを1増やす")),
			MakeCard(TEXT("O07"), TEXT("即断の商談"), ECGCardType::Spell, ECGColor::Orange, 3, 0, 0, CGEffectId::BuyFromMarketCostUnder3ToHand, 5, 0.f, TEXT(""),
				TEXT("コスト5以下のマーケットカードを1枚購入し、即座に手札へ")),
			MakeCard(TEXT("O08"), TEXT("先読みの相場師"), ECGCardType::Unit, ECGColor::Orange, 3, 3, 2, CGEffectId::OnTurnStartGrantPurchaseMana, 1, 0.f, TEXT(""),
				TEXT("自分のターン開始時、コインを1増加")),
			MakeCard(TEXT("O09"), TEXT("大商人の護衛"), ECGCardType::Unit, ECGColor::Orange, 4, 4, 4, CGEffectId::GrantPurchaseMana, 2, 0.f, TEXT(""),
				TEXT("登場時、コインが2増加")),
			MakeCard(TEXT("O10"), TEXT("黄金の商機"), ECGCardType::Spell, ECGColor::Orange, 2, 0, 0, CGEffectId::GrantBoardBuffOnPurchaseThisTurn, 0, 0.f, TEXT(""),
				TEXT("このターン、購入するたびに場のユニットのステータスを+1/+0")),
			MakeCard(TEXT("O11"), TEXT("市場の目付役"), ECGCardType::Unit, ECGColor::Orange, 4, 4, 4, CGEffectId::OnPlayRerollMarketRandom, 0, 0.f, TEXT("Discount"),
				TEXT("先物。登場時、マーケットをランダムな1枚補充し直す")),
			MakeCard(TEXT("O12"), TEXT("豪商の後継者"), ECGCardType::Unit, ECGColor::Orange, 5, 5, 5, CGEffectId::OnBuyEndTurnGrantPurchaseMana, 0, 0.f, TEXT(""),
				TEXT("自分が購入したターン終了時、コインを1増加")),
			MakeCard(TEXT("O13"), TEXT("市場開放の号令"), ECGCardType::Spell, ECGColor::Orange, 4, 0, 0, CGEffectId::GrantAllPurchasesDiscountThisTurn, 0, 0.f, TEXT(""),
				TEXT("このターン、購入コストが全て1軽減される")),
			MakeCard(TEXT("O14"), TEXT("市場の守り主"), ECGCardType::Unit, ECGColor::Orange, 5, 3, 6, CGEffectId::OnBuyBuffSelfHp, 1, 0.f, TEXT("Discount,Guard"),
				TEXT("先物 庇護。自分が購入するたび、このユニットは+0/+1")),
			MakeCard(TEXT("O15"), TEXT("独占商人"), ECGCardType::Unit, ECGColor::Orange, 6, 6, 6, CGEffectId::OnPlayDeployFromMarketFree, -1, 0.f, TEXT(""),
				TEXT("登場時、マーケットから1枚、コストを支払わず場に出す")),

			// --- 緑(物量/分身/回復)— 分身 ---
			// 500戦シミュレーションで緑が59.9%と突出したため分身持ち7種を1段階
			// 弱体化したところ、今度は40.4%まで下がりすぎたため、その1段階分を
			// 巻き戻した(G11の差し替えはそのまま維持。docs/next-ruleset-cards-v1.md
			// 「緑」参照)。
			MakeCard(TEXT("G01"), TEXT("若木の番人"), ECGCardType::Unit, ECGColor::Green, 1, 1, 1, CGEffectId::None, 0, 0.f, TEXT("Clone"),
				TEXT("分身: 場に味方Unitが2体以上いるとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AllyUnitCountAtLeast, 2),
			MakeCard(TEXT("G02"), TEXT("癒しの若葉"), ECGCardType::Spell, ECGColor::Green, 1, 0, 0, CGEffectId::SummonApprenticeTokens, 2, 0.f, TEXT(""),
				TEXT("0/1のUnitを2体出す")),
			MakeCard(TEXT("G03"), TEXT("熊の盾持ち"), ECGCardType::Unit, ECGColor::Green, 2, 1, 2, CGEffectId::None, 0, 0.f, TEXT("Clone"),
				TEXT("分身: このユニットが攻撃を受けて生き残ったとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::SurvivedAttack, 1),
			// 3000戦シミュレーションでキーワード無しのバニラながら64.0%と緑の中でも
			// 最上位だったため、素のステータスを下げる代わりに条件付きバフを持たせた
			// (docs/next-ruleset-cards-v1.md「緑」参照)。
			MakeCard(TEXT("G04"), TEXT("森の猪"), ECGCardType::Unit, ECGColor::Green, 2, 1, 2, CGEffectId::OnPlaySelfBuffAtkIfAlliesPresent, 1, 0.f, TEXT(""),
				TEXT("登場時、場に他の味方Unitが3体以上いるなら自身が+1/+0")),
			MakeCard(TEXT("G05"), TEXT("群れの誕生"), ECGCardType::Spell, ECGColor::Green, 2, 0, 0, CGEffectId::SummonToughApprenticeTokens, 2, 0.f, TEXT(""),
				TEXT("1/1のUnitを2体出す")),
			MakeCard(TEXT("G06"), TEXT("聖樹の守護者"), ECGCardType::Unit, ECGColor::Green, 3, 1, 3, CGEffectId::OnPlayHealSelf, 1, 0.f, TEXT("Clone"),
				TEXT("登場時、味方リーダーを1回復。分身: 場に味方Unitが2体以上いるとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AllyUnitCountAtLeast, 2),
			// 3000戦シミュレーションでキーワード無しのバニラながら65.3%と緑最強格
			// だったため、素のステータスを大きく下げて分身+庇護を持たせた。庇護は
			// 「紫に多いが他色にも例外的に付いてよい」という方針のもと、緑にも
			// 意図的に1枚だけ残している(docs/keywords.md「庇護」参照)。
			MakeCard(TEXT("G07"), TEXT("猛る大鹿"), ECGCardType::Unit, ECGColor::Green, 3, 1, 2, CGEffectId::None, 0, 0.f, TEXT("Clone,Guard"),
				TEXT("庇護。分身: 自分のリーダーの体力が10以下のとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::LeaderHpAtMost, 10),
			MakeCard(TEXT("G08"), TEXT("森の恵み"), ECGCardType::Spell, ECGColor::Green, 3, 0, 0, CGEffectId::BuffAllAlliesAtkOnly, 1, 0.f, TEXT(""),
				TEXT("場のUnitすべてのステータスを+1/+0")),
			MakeCard(TEXT("G09"), TEXT("巨石の壁役"), ECGCardType::Unit, ECGColor::Green, 4, 2, 4, CGEffectId::None, 0, 0.f, TEXT("Clone"),
				TEXT("分身: 場に味方Unitが3体以上いるとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AllyUnitCountAtLeast, 3),
			MakeCard(TEXT("G10"), TEXT("群像の雄叫び"), ECGCardType::Unit, ECGColor::Green, 4, 3, 4, CGEffectId::OnPlayBuffSelfIfAlliesPresent, 2, 0.f, TEXT(""),
				TEXT("登場時、場に他の味方Unitが3体以上いるなら自身が+2/+2")),
			// トークン生成スペルが強すぎるというフィードバックへの対応で「大群の号令」を
			// 削除し、自分の場のUnit数を参照する除去スペルに差し替えた。物量(Unit数)を
			// 攻撃力に転換する役割にすることで、分身と組み合わせたときの物量シナジーを
			// 保ちつつ、トークン展開そのものは増やさない。
			MakeCard(TEXT("G11"), TEXT("群れの猛攻"), ECGCardType::Spell, ECGColor::Green, 3, 0, 0, CGEffectId::OnPlayDamageTargetByAllyUnitCount, 0, 0.f, TEXT(""),
				TEXT("自分の場にいるUnitの数だけ、敵Unit1体にダメージを与える")),
			MakeCard(TEXT("G12"), TEXT("不屈の大樹"), ECGCardType::Unit, ECGColor::Green, 5, 3, 5, CGEffectId::OnDefendHealSelf1, 0, 0.f, TEXT("Clone"),
				TEXT("このユニットが攻撃を受けるたび、味方リーダーを1回復。分身: 味方リーダーが回復したとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::LeaderHealed, 1),
			MakeCard(TEXT("G13"), TEXT("森の巨人"), ECGCardType::Unit, ECGColor::Green, 5, 4, 4, CGEffectId::None, 0, 0.f, TEXT("Clone"),
				TEXT("分身: このユニットが攻撃して生き残ったとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AttackedAndSurvived, 1),
			// 500戦シミュレーションで66.2%と緑の中でも最強だったため+2/+2→+1/+1に
			// 調整した(分身/トークンで横に並べる緑と全体バフの相性が良すぎた。
			// docs/next-ruleset-cards-v1.md「緑」参照)。
			MakeCard(TEXT("G14"), TEXT("大地の祝福"), ECGCardType::Spell, ECGColor::Green, 5, 0, 0, CGEffectId::BuffAllAlliesFlat, 1, 0.f, TEXT(""),
				TEXT("場の味方Unit全てを+1/+1")),
			MakeCard(TEXT("G15"), TEXT("万象の守り神"), ECGCardType::Unit, ECGColor::Green, 6, 4, 5, CGEffectId::OnPlayHealSelf, 2, 0.f, TEXT("Clone"),
				TEXT("登場時、味方リーダーを2回復。分身: 場に味方Unitが4体以上いるとき、素の状態のコピーを1体出す(このユニットにつき1回)"),
				TEXT(""), TEXT(""), TEXT(""), TEXT(""), 0, CGCloneConditionId::AllyUnitCountAtLeast, 4),

			// --- 青(除去/追放)— 封印 ---
			MakeCard(TEXT("B01"), TEXT("小さな封緘"), ECGCardType::Spell, ECGColor::Blue, 1, 0, 0, CGEffectId::SealSpell, 1, 0.f, TEXT("Seal"),
				TEXT("封印。コスト1以下の敵Unit1体を封印する")),
			MakeCard(TEXT("B02"), TEXT("氷刃の見習い"), ECGCardType::Unit, ECGColor::Blue, 1, 1, 2, CGEffectId::OnPlayDebuffTarget, -1, 0.f, TEXT(""),
				TEXT("登場時、敵Unit1体を-1/-1する")),
			MakeCard(TEXT("B03"), TEXT("氷結の封印"), ECGCardType::Spell, ECGColor::Blue, 2, 0, 0, CGEffectId::SealSpell, 2, 0.f, TEXT("Seal"),
				TEXT("封印。コスト2以下の敵Unit1体を封印する")),
			MakeCard(TEXT("B04"), TEXT("幻惑の魔道士"), ECGCardType::Unit, ECGColor::Blue, 2, 2, 2, CGEffectId::OnSealDamageFace1, 0, 0.f, TEXT(""),
				TEXT("封印するたびに敵リーダーに1ダメージ")),
			MakeCard(TEXT("B05"), TEXT("知識の番人"), ECGCardType::Unit, ECGColor::Blue, 2, 1, 1, CGEffectId::OnTurnStartDraw, 0, 0.f, TEXT(""),
				TEXT("自分のターン開始時、このユニットが場にいれば1ドロー")),
			MakeCard(TEXT("B06"), TEXT("深淵の封印"), ECGCardType::Spell, ECGColor::Blue, 4, 0, 0, CGEffectId::SealSpell, -1, 0.f, TEXT("Seal"),
				TEXT("封印。敵Unit1体を封印する(コスト制限なし)")),
			MakeCard(TEXT("B07"), TEXT("追放の審判官"), ECGCardType::Unit, ECGColor::Blue, 3, 1, 3, CGEffectId::SealOnPlay, 3, 0.f, TEXT("Seal"),
				TEXT("封印。登場時、コスト3以下の敵Unit1体を封印する")),
			// ドキュメントには元々「0/4、封印するたびに1ドロー」と設計されていたが
			// 実装が漏れてバニラ(2/5)のままだった。「バニラを無くしたい」という
			// フィードバックを機に、ドキュメント通りの効果を実装した。その後、
			// 3000戦シミュレーションで赤vs青が78%まで偏った(Atk0で反撃できない
			// ことがアグロに弱い一因と推測)ため、Atkだけ0→1に戻した。
			MakeCard(TEXT("B08"), TEXT("霧の壁"), ECGCardType::Unit, ECGColor::Blue, 3, 1, 4, CGEffectId::OnSealDraw1, 0, 0.f, TEXT(""),
				TEXT("封印するたびに1ドロー")),
			MakeCard(TEXT("B09"), TEXT("叡智の追放"), ECGCardType::Spell, ECGColor::Blue, 4, 0, 0, CGEffectId::SealSpellGrantPurchaseMana, -1, 0.f, TEXT("Seal"),
				TEXT("封印。敵Unit1体を封印し、コインを1増加")),
			MakeCard(TEXT("B10"), TEXT("衰弱の監視者"), ECGCardType::Unit, ECGColor::Blue, 4, 3, 4, CGEffectId::EndTurnDebuffHighestCostEnemy, -2, 0.f, TEXT(""),
				TEXT("自分のターン終了時、敵Unitの中で最もコストが高いものを-2/-2")),
			MakeCard(TEXT("B11"), TEXT("追放の執行者"), ECGCardType::Unit, ECGColor::Blue, 4, 3, 4, CGEffectId::SealOnPlayByPower, 2, 0.f, TEXT("Seal"),
				TEXT("封印。登場時、パワー(現在の攻撃力)2以下の敵Unit1体を封印する")),
			MakeCard(TEXT("B12"), TEXT("氷結の嵐"), ECGCardType::Spell, ECGColor::Blue, 5, 0, 0, CGEffectId::MassDebuffEnemies, 2, 0.f, TEXT(""),
				TEXT("敵Unit全てを-2/-2する")),
			MakeCard(TEXT("B13"), TEXT("叡智の大魔導"), ECGCardType::Unit, ECGColor::Blue, 5, 4, 4, CGEffectId::SealOnPlayByPower, 3, 0.f, TEXT("Seal"),
				TEXT("封印。登場時、パワー(現在の攻撃力)3以下の敵Unit1体を封印する")),
			MakeCard(TEXT("B14"), TEXT("終焉の裁定者"), ECGCardType::Unit, ECGColor::Blue, 5, 3, 3, CGEffectId::OnTurnStartSealHighestCostEnemy, 2, 0.f, TEXT(""),
				TEXT("自分のターン開始時1回、敵Unitの中で最もコストが高いもの(コスト2以下)を封印する")),
			MakeCard(TEXT("B15"), TEXT("双つの追放"), ECGCardType::Spell, ECGColor::Blue, 6, 0, 0, CGEffectId::SealSpellTwo, -1, 0.f, TEXT("Seal"),
				TEXT("封印。敵Unit2体を封印する")),

			// --- 紫(強化/変容)— 変貌 ---
			// 8000戦超のシミュレーションで紫15種(P16追加後16種)全てが単独で
			// 勝率50%を下回っており(33〜44%)、色全体が構造的に弱いと判明した
			// (docs/next-ruleset-simulation-v1.md「第19回」「第20回」参照)。
			// 変貌が完了する前に除去されると投資が無駄になるという弱点に対して、
			// 変貌元Unitの素のHPを+1して除去耐性を上げ、変貌先Unitのステータスを
			// +1/+1して「変貌が完了した後の見返り」自体も底上げする、という
			// 2段構えの底上げを全体に適用した(第20回)。
			MakeCard(TEXT("P01"), TEXT("無垢の使い魔"), ECGCardType::Unit, ECGColor::Purple, 1, 1, 2, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 自分のターンを2回経過すると「若き賢者」に変貌"),
				TEXT(""), TEXT(""), TEXT("P01T"), CGTransformConditionId::TurnsInPlay, 2),
			MakeCard(TEXT("P02"), TEXT("秘めた才能"), ECGCardType::Spell, ECGColor::Purple, 1, 0, 0, CGEffectId::BuffAllyTarget, 1, 0.f, TEXT(""),
				TEXT("味方Unit1体を+1/+1する")),
			MakeCard(TEXT("P03"), TEXT("若き見習い"), ECGCardType::Unit, ECGColor::Purple, 2, 0, 3, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 対象指定の味方強化を2回受けると「歴戦の使い魔」に変貌"),
				TEXT(""), TEXT(""), TEXT("P03T"), CGTransformConditionId::BuffedCount, 2),
			MakeCard(TEXT("P04"), TEXT("祝福の詠唱"), ECGCardType::Spell, ECGColor::Purple, 2, 0, 0, CGEffectId::BuffAllyTarget, 2, 0.f, TEXT(""),
				TEXT("味方Unit1体を+2/+2する")),
			// 護衛(庇護): 変貌が完了するまでの投資(P01/P03等)を守るための紫の護衛ユニット
			// (docs/next-ruleset-cards-v1.md「紫」。「変貌した後に庇護を付けるというより、
			// 変貌前のカードを守るために庇護を持ったカードを用意する」というフィードバックへの対応)。
			MakeCard(TEXT("P05"), TEXT("導きの魔女"), ECGCardType::Unit, ECGColor::Purple, 2, 2, 2, CGEffectId::OnPlayBuffAllyTarget, 1, 0.f, TEXT("Guard"),
				TEXT("庇護。登場時、味方Unit1体を+1/+1する(元の設計は+1/+2だが対称なバフに簡略化)")),
			MakeCard(TEXT("P06"), TEXT("封じられし魔物"), ECGCardType::Unit, ECGColor::Purple, 3, 2, 3, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 手札が4枚以下になると「解放された魔物」に変貌"),
				TEXT(""), TEXT(""), TEXT("P06T"), CGTransformConditionId::HandSizeAtMost, 4),
			MakeCard(TEXT("P07"), TEXT("大いなる変容"), ECGCardType::Spell, ECGColor::Purple, 3, 0, 0, CGEffectId::BuffAllyTarget, 3, 0.f, TEXT(""),
				TEXT("味方Unit1体を+3/+3する")),
			MakeCard(TEXT("P08"), TEXT("静かなる怪物"), ECGCardType::Unit, ECGColor::Purple, 3, 2, 5, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 場に2ターンいると「目覚めた怪物」に変貌"),
				TEXT(""), TEXT(""), TEXT("P08T"), CGTransformConditionId::TurnsInPlay, 2),
			MakeCard(TEXT("P09"), TEXT("眠れる怒り"), ECGCardType::Unit, ECGColor::Purple, 4, 3, 5, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 味方Unitが2体死亡すると「憤怒の権化」に変貌"),
				TEXT(""), TEXT(""), TEXT("P09T"), CGTransformConditionId::AlliesDiedSinceSummon, 2),
			MakeCard(TEXT("P10"), TEXT("深淵の契約"), ECGCardType::Spell, ECGColor::Purple, 4, 0, 0, CGEffectId::BuffAllyTargetAndDraw, 3, 0.f, TEXT(""),
				TEXT("味方Unit1体を+3/+3し、1ドローする")),
			MakeCard(TEXT("P11"), TEXT("予見の魔導師"), ECGCardType::Unit, ECGColor::Purple, 4, 3, 4, CGEffectId::OnPlayGrantNextUnitPlayDiscount, 2, 0.f, TEXT("Guard"),
				TEXT("庇護。登場時、次にプレイするUnit1体のコストを2軽減する(元の設計は「手札の特定カードを軽減」だが、同名重複カードを区別する仕組みが無いため簡略化)")),
			MakeCard(TEXT("P12"), TEXT("胎動する秘術"), ECGCardType::Unit, ECGColor::Purple, 5, 3, 6, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: このターン中に味方の変貌が2回成功していれば、即座に「万物の頂点」に変貌"),
				TEXT(""), TEXT(""), TEXT("P12T"), CGTransformConditionId::TransformsThisTurnCount, 2),
			MakeCard(TEXT("P13"), TEXT("解放の詠唱"), ECGCardType::Spell, ECGColor::Purple, 5, 0, 0, CGEffectId::ForceTransformAllAllies, 0, 0.f, TEXT(""),
				TEXT("変貌条件を無視して、変貌可能な味方Unitをすべて変貌させる")),
			MakeCard(TEXT("P14"), TEXT("調和の賢者"), ECGCardType::Unit, ECGColor::Purple, 5, 5, 5, CGEffectId::OnPlayBuffAllyTargetAndUnitDiscount, 2, 0.f, TEXT("Guard"),
				TEXT("庇護。登場時、味方Unit1体を+2/+2し、次にプレイするUnit1体のコストを1軽減する(手札の特定カードを軽減する原設計をP11と同じ方針で簡略化)")),
			MakeCard(TEXT("P15"), TEXT("不完全な神"), ECGCardType::Unit, ECGColor::Purple, 6, 6, 3, CGEffectId::None, 0, 0.f, TEXT("Transform"),
				TEXT("変貌: 自分のターン開始時、50%の確率で「真なる姿」に変貌"),
				TEXT(""), TEXT(""), TEXT("P15T"), CGTransformConditionId::RandomChancePerTurn, 50),
			// 紫の補強用に追加(16枚目、docs/next-ruleset-simulation-v1.md「第19回」)。
			// 他の変貌元Unit(P01/P03/P06/P08/P09/P12/P15)が自力で条件を満たすまで
			// 待つ必要があるのに対し、この1枚は「登場した瞬間に他の1体を変貌させる」
			// ことで変貌のタイミングを前倒しできる、紫のテンポ不足を補う狙いのカード。
			MakeCard(TEXT("P16"), TEXT("刻を歪める者"), ECGCardType::Unit, ECGColor::Purple, 2, 1, 1, CGEffectId::OnPlayForceTransformAllyTarget, 0, 0.f, TEXT(""),
				TEXT("登場時、変貌条件を無視して味方Unit1体を選んで変貌させる(対象がいなければ何も起きない)")),
		};
		return Cards;
	}

	// 変貌先カード(P01T〜P15T)。マーケットには並ばず、購入・手札からのプレイも
	// 不可(コスト0)。対応する変貌元カードの変貌効果によってのみ場に出現する
	// (docs/next-ruleset-cards-v1.md「変貌先カード」)。BuildTokenCards()に含める
	// ことで、見習い兵と同じ「実在カードプールには含まれないが参照はできる」
	// 扱いにしている。
	const TArray<FCGCardDef>& BuildTransformTargetCards()
	{
		static const TArray<FCGCardDef> Cards = {
			// 紫のステータス全体ダウン(P05/P11/P14が庇護持ちの護衛になった分の底上げを
			// 打ち消すため、第20回で乗せた+1/+1のうちHP側を巻き戻した。docs/next-ruleset-
			// cards-v1.md「紫」)。P15Tだけは元々HP4と紙耐久な設計のため据え置き。
			MakeCard(TEXT("P01T"), TEXT("若き賢者"), ECGCardType::Unit, ECGColor::Purple, 0, 3, 3, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("P03T"), TEXT("歴戦の使い魔"), ECGCardType::Unit, ECGColor::Purple, 0, 3, 4, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("P06T"), TEXT("解放された魔物"), ECGCardType::Unit, ECGColor::Purple, 0, 5, 4, CGEffectId::OnPlayHealSelf, 1, 0.f, TEXT(""),
				TEXT("場に出た時に味方リーダーを1回復")),
			MakeCard(TEXT("P08T"), TEXT("目覚めた怪物"), ECGCardType::Unit, ECGColor::Purple, 0, 6, 6, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("P09T"), TEXT("憤怒の権化"), ECGCardType::Unit, ECGColor::Purple, 0, 7, 6, CGEffectId::None, 0, 0.f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("P12T"), TEXT("万物の頂点"), ECGCardType::Unit, ECGColor::Purple, 0, 8, 7, CGEffectId::None, 0, 0.f, TEXT("Guard"),
				TEXT("庇護")),
			MakeCard(TEXT("P15T"), TEXT("真なる姿"), ECGCardType::Unit, ECGColor::Purple, 0, 11, 4, CGEffectId::OnPlayDamageFace, 2, 0.f, TEXT(""),
				TEXT("登場時、敵リーダーへ2ダメージ")),
		};
		return Cards;
	}

	// マーケット/初期デッキには含まれない、効果生成専用のトークン。
	// (見習い召集 C020/G11大群の号令 が「1/1 Unitを出す」、G05群れの誕生が
	// 「1/2 Unitを出す」ために使う)
	const TArray<FCGCardDef>& BuildTokenCards()
	{
		static const TArray<FCGCardDef> Tokens = {
			MakeCard(TEXT("TK_APPRENTICE"), TEXT("見習い兵"), ECGCardType::Unit, ECGColor::None, 0, 1, 1, CGEffectId::None, 0, 0.f, TEXT(""), TEXT("")),
			MakeCard(TEXT("TK_APPRENTICE_TOUGH"), TEXT("屈強な見習い兵"), ECGCardType::Unit, ECGColor::None, 0, 1, 2, CGEffectId::None, 0, 0.f, TEXT(""), TEXT("")),
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
			MakeCard(TEXT("FIN_RED"), TEXT("紅蓮の覇王"), ECGCardType::Unit, ECGColor::Red, 10, 8, 8, CGEffectId::None, 0, 0.f, TEXT("Haste,Finisher"),
				TEXT("フィニッシャー。味方Unitが10体死亡すると戦場に駆けつける。疾駆。"),
				TEXT(""), TEXT("屍を越えてなお、彼だけは燃え続けている。")),
			// 橙: マーケットから10枚購入すると駆けつける。登場時、マーケットの
			// Unitを3枚、プレイヤーが1枚ずつ選んでコストを支払わずそのまま場に出す
			// (O15独占商人と同じ選択の仕組みを3回繰り返す。「プレイヤーがマーケット
			// のカードを選択して無料でプレイできるカードを選びたい」という
			// フィードバックへの対応。以前はプレイヤーに選ばせず自動で3枚出していた)。
			MakeCard(TEXT("FIN_ORANGE"), TEXT("黄金の帝王"), ECGCardType::Unit, ECGColor::Orange, 10, 5, 5, CGEffectId::OnPlayFreeMarketCards, 3, 0.f, TEXT("Finisher"),
				TEXT("フィニッシャー。マーケットから10枚購入すると戦場に駆けつける。登場時、マーケットのUnitを3枚、コストを支払わず選んで場に出す。"),
				TEXT(""), TEXT("市場の全てが、彼一人のために動いていた。")),
			// 青: カードを20枚ドローすると駆けつける。登場時、敵の場を全て追放する。
			MakeCard(TEXT("FIN_BLUE"), TEXT("深淵の裁定者"), ECGCardType::Unit, ECGColor::Blue, 10, 6, 6, CGEffectId::OnPlayExileAllEnemyUnits, 0, 0.f, TEXT("Finisher"),
				TEXT("フィニッシャー。カードを20枚ドローすると戦場に駆けつける。登場時、敵の場のUnitをすべて追放する。"),
				TEXT(""), TEXT("知り尽くした者だけが、すべてを裁く権利を持つ。")),
			// 緑: 場にUnitが6体並ぶと駆けつける。登場時、味方全体を+1/+1する。
			MakeCard(TEXT("FIN_GREEN"), TEXT("大地の化身"), ECGCardType::Unit, ECGColor::Green, 10, 4, 4, CGEffectId::OnPlayBuffAllAlliesFlat, 1, 0.f, TEXT("Finisher"),
				TEXT("フィニッシャー。場にUnitが6体並ぶと戦場に駆けつける。登場時、味方Unitすべてを+1/+1する。"),
				TEXT(""), TEXT("大地そのものが、群れに応えて姿を成した。")),
			// 紫: ユニットが5体変貌すると駆けつける。常在効果(EffectIdは登場時
			// ハンドラには登録せずHasBoardUnitWithEffectで判定、PlayCardFromHand参照)。
			MakeCard(TEXT("FIN_PURPLE"), TEXT("変貌の女神"), ECGCardType::Unit, ECGColor::Purple, 10, 5, 5, CGEffectId::FinisherTransformAura, 0, 0.f, TEXT("Finisher"),
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
	// 庇護/封印/変貌)を持つ、または軸となる効果を持つカードの中から低〜中コスト
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
	// ことを複数回の検証で確認済みのため、変更しないことに決めている。
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
			FName(TEXT("O01")), FName(TEXT("O02")), FName(TEXT("O04")), FName(TEXT("O05")), FName(TEXT("O07")),
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
		// P16(刻を歪める者)を1枚組み込むため、既存の11枚目(P11予見の魔導師、
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
