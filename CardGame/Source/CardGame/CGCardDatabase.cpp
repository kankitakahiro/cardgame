#include "CGCardDatabase.h"

namespace
{
	FCGCardDef MakeCard(const TCHAR* CardId, const TCHAR* CardName, ECGCardType CardType, int32 Cost,
		int32 Atk, int32 Hp, const TCHAR* EffectId, int32 EffectValue, float Ratio, const TCHAR* Tags,
		const TCHAR* Description)
	{
		FCGCardDef Card;
		Card.CardId = FName(CardId);
		Card.CardName = CardName;
		Card.CardType = CardType;
		Card.Cost = Cost;
		Card.Atk = Atk;
		Card.Hp = Hp;
		Card.EffectId = FName(EffectId);
		Card.EffectValue = EffectValue;
		Card.Ratio = Ratio;
		Card.Tags = Tags;
		Card.Description = Description;
		return Card;
	}

	const TArray<FCGCardDef>& BuildAllCards()
	{
		static const TArray<FCGCardDef> Cards = {
			MakeCard(TEXT("C001"), TEXT("先駆けの斥候"), ECGCardType::Unit, 1, 1, 1, CGEffectId::ScoutTop1, 0, 1.00f, TEXT(""),
				TEXT("登場時にデッキ上1枚を見て上下選択")),
			MakeCard(TEXT("C002"), TEXT("炎走りの小鬼"), ECGCardType::Unit, 1, 2, 1, CGEffectId::None, 0, 1.50f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("C003"), TEXT("盾持ち見習い"), ECGCardType::Unit, 1, 1, 2, CGEffectId::None, 0, 2.00f, TEXT("Guard"),
				TEXT("")),
			MakeCard(TEXT("C004"), TEXT("小さな研究者"), ECGCardType::Unit, 1, 1, 1, CGEffectId::OnDeathDraw, 1, 1.75f, TEXT(""),
				TEXT("死亡時1ドロー")),
			MakeCard(TEXT("C005"), TEXT("切り込み隊長"), ECGCardType::Unit, 2, 2, 2, CGEffectId::None, 0, 1.25f, TEXT("Haste"),
				TEXT("")),
			MakeCard(TEXT("C006"), TEXT("墓場あさり"), ECGCardType::Unit, 2, 2, 2, CGEffectId::GraveyardToDeckBottomDraw1, 1, 1.50f, TEXT(""),
				TEXT("登場時に墓地1枚を山札下へ、1ドロー")),
			MakeCard(TEXT("C007"), TEXT("市場の仲買人"), ECGCardType::Unit, 2, 1, 3, CGEffectId::OnBuyEndTurnDiscardDraw, 1, 1.25f, TEXT(""),
				TEXT("あなたが購入したターン終了時、手札を1枚捨てると1ドロー")),
			MakeCard(TEXT("C008"), TEXT("錆びた巨兵"), ECGCardType::Unit, 2, 3, 2, CGEffectId::OnPlayDiscard1, 1, 1.13f, TEXT(""),
				TEXT("登場時に手札1枚捨てる")),
			MakeCard(TEXT("C009"), TEXT("街道の突撃兵"), ECGCardType::Unit, 3, 3, 3, CGEffectId::SecondPlayBuff, 1, 1.08f, TEXT(""),
				TEXT("あなたがこのターン2枚目をプレイしていたら+1/+0")),
			MakeCard(TEXT("C010"), TEXT("追撃の射手"), ECGCardType::Unit, 3, 2, 3, CGEffectId::OnAllySpellPing1, 1, 1.08f, TEXT(""),
				TEXT("味方Spell使用時に敵リーダーへ1ダメージ(各ターン1回)")),
			MakeCard(TEXT("C011"), TEXT("再誕の司祭"), ECGCardType::Unit, 3, 2, 4, CGEffectId::OnPlayReturnGraveyardCheapCard, 1, 1.17f, TEXT(""),
				TEXT("登場時に墓地コスト1以下を手札へ")),
			MakeCard(TEXT("C012"), TEXT("市場監督官"), ECGCardType::Unit, 3, 3, 4, CGEffectId::BuyCostReductionThisTurn, 1, 1.33f, TEXT(""),
				TEXT("購入時コストをこのターンだけ1軽減(最小1)")),
			MakeCard(TEXT("C013"), TEXT("戦場の旗手"), ECGCardType::Unit, 4, 4, 4, CGEffectId::AllyBuffAtkThisTurn, 1, 1.13f, TEXT(""),
				TEXT("味方他Unit全て+1/+0(ターン中)")),
			MakeCard(TEXT("C014"), TEXT("霊廟の守り手"), ECGCardType::Unit, 4, 3, 5, CGEffectId::OnDeathReturnRandomGraveyardUnit, 0, 1.13f, TEXT("Guard"),
				TEXT("死亡時にランダムで墓地のUnit1枚を山札へ")),
			MakeCard(TEXT("C015"), TEXT("隕鉄の突進獣"), ECGCardType::Unit, 4, 5, 4, CGEffectId::None, 0, 1.13f, TEXT(""),
				TEXT("")),
			MakeCard(TEXT("C016"), TEXT("連鎖術の教授"), ECGCardType::Unit, 4, 3, 4, CGEffectId::FirstSpellBonusDamage, 1, 1.06f, TEXT(""),
				TEXT("あなたの毎ターン最初のSpellが+1ダメージ")),
			MakeCard(TEXT("C017"), TEXT("火花の一撃"), ECGCardType::Spell, 1, 0, 0, CGEffectId::OnPlayDamageTarget, 2, 1.00f, TEXT(""),
				TEXT("敵Unitへ2ダメージ")),
			MakeCard(TEXT("C018"), TEXT("応急手当"), ECGCardType::Spell, 1, 0, 0, CGEffectId::OnPlayHealSelf, 3, 1.20f, TEXT(""),
				TEXT("味方リーダー3回復")),
			MakeCard(TEXT("C019"), TEXT("手札の選別"), ECGCardType::Spell, 1, 0, 0, CGEffectId::Discard1Draw2, 0, 1.25f, TEXT(""),
				TEXT("1枚捨てて2枚引く")),
			MakeCard(TEXT("C020"), TEXT("見習い召集"), ECGCardType::Spell, 2, 0, 0, CGEffectId::Summon2x1_1Unit, 2, 1.00f, TEXT(""),
				TEXT("1/1 Unitを2体出す")),
			MakeCard(TEXT("C021"), TEXT("墓地再点火"), ECGCardType::Spell, 2, 0, 0, CGEffectId::ReturnGraveyardSpellSelfDamage1, 1, 0.88f, TEXT(""),
				TEXT("墓地のSpell1枚を手札へ、1点自傷")),
			MakeCard(TEXT("C022"), TEXT("市場調達"), ECGCardType::Spell, 2, 0, 0, CGEffectId::BuyFromMarketCostUnder3ToHand, 3, 1.13f, TEXT(""),
				TEXT("マーケットからコスト3以下を購入し即手札へ")),
			MakeCard(TEXT("C023"), TEXT("連弾の雨"), ECGCardType::Spell, 3, 0, 0, CGEffectId::RandomEnemyDamage1x4, 1, 0.67f, TEXT(""),
				TEXT("ランダムな敵に1ダメージを4回")),
			MakeCard(TEXT("C024"), TEXT("逆転の号令"), ECGCardType::Spell, 3, 0, 0, CGEffectId::ConditionalDamage3or2, 0, 0.45f, TEXT(""),
				TEXT("自HPが相手以下なら3ダメージ、そうでなければ2ダメージ")),
		};
		return Cards;
	}

	// マーケット/初期デッキには含まれない、効果生成専用のトークン。
	// (見習い召集 C020 が「1/1 Unitを2体出す」ために使う)
	const TArray<FCGCardDef>& BuildTokenCards()
	{
		static const TArray<FCGCardDef> Tokens = {
			MakeCard(TEXT("TK_APPRENTICE"), TEXT("見習い兵"), ECGCardType::Unit, 0, 1, 1, CGEffectId::None, 0, 0.f, TEXT(""), TEXT("")),
		};
		return Tokens;
	}
}

const TArray<FCGCardDef>& UCGCardDatabase::GetAllCards()
{
	return BuildAllCards();
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
	for (const FCGCardDef& Card : BuildTokenCards())
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
	return {
		FName(TEXT("C001")), FName(TEXT("C002")), FName(TEXT("C003")), FName(TEXT("C004")),
		FName(TEXT("C005")), FName(TEXT("C006")), FName(TEXT("C007")), FName(TEXT("C008")),
		FName(TEXT("C009")), FName(TEXT("C010")), FName(TEXT("C011")), FName(TEXT("C012")),
	};
}
