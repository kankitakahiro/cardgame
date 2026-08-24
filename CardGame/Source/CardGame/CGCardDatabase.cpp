#include "CGCardDatabase.h"

namespace
{
	FCGCardDef MakeCard(const TCHAR* CardId, const TCHAR* CardName, ECGCardType CardType, int32 Cost,
		int32 Atk, int32 Hp, const TCHAR* EffectId, int32 EffectValue, float Ratio, const TCHAR* Tags)
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
		return Card;
	}

	const TArray<FCGCardDef>& BuildAllCards()
	{
		static const TArray<FCGCardDef> Cards = {
			MakeCard(TEXT("C001"), TEXT("先駆けの斥候"), ECGCardType::Unit, 1, 1, 1, TEXT("TODO_ScoutTop1"), 0, 1.00f, TEXT("")),
			MakeCard(TEXT("C002"), TEXT("炎走りの小鬼"), ECGCardType::Unit, 1, 2, 1, TEXT("None"), 0, 1.50f, TEXT("")),
			MakeCard(TEXT("C003"), TEXT("盾持ち見習い"), ECGCardType::Unit, 1, 1, 2, TEXT("None"), 0, 2.00f, TEXT("Guard")),
			MakeCard(TEXT("C004"), TEXT("小さな研究者"), ECGCardType::Unit, 1, 1, 1, TEXT("OnDeathDraw"), 1, 1.75f, TEXT("")),
			MakeCard(TEXT("C005"), TEXT("切り込み隊長"), ECGCardType::Unit, 2, 2, 2, TEXT("None"), 0, 1.25f, TEXT("Haste")),
			MakeCard(TEXT("C006"), TEXT("墓場あさり"), ECGCardType::Unit, 2, 2, 2, TEXT("TODO_GraveyardToDeckBottomDraw1"), 1, 1.50f, TEXT("")),
			MakeCard(TEXT("C007"), TEXT("市場の仲買人"), ECGCardType::Unit, 2, 1, 3, TEXT("TODO_OnBuyEndTurnDiscardDraw"), 1, 1.25f, TEXT("")),
			MakeCard(TEXT("C008"), TEXT("錆びた巨兵"), ECGCardType::Unit, 2, 3, 2, TEXT("TODO_OnPlayDiscard1"), 1, 1.13f, TEXT("")),
			MakeCard(TEXT("C009"), TEXT("街道の突撃兵"), ECGCardType::Unit, 3, 3, 3, TEXT("TODO_SecondPlayBuff"), 1, 1.08f, TEXT("")),
			MakeCard(TEXT("C010"), TEXT("追撃の射手"), ECGCardType::Unit, 3, 2, 3, TEXT("TODO_OnAllySpellPing1"), 1, 1.08f, TEXT("")),
			MakeCard(TEXT("C011"), TEXT("再誕の司祭"), ECGCardType::Unit, 3, 2, 4, TEXT("TODO_OnPlayReturnGraveyardCheapCard"), 1, 1.17f, TEXT("")),
			MakeCard(TEXT("C012"), TEXT("市場監督官"), ECGCardType::Unit, 3, 3, 4, TEXT("TODO_BuyCostReductionThisTurn"), 1, 1.33f, TEXT("")),
			MakeCard(TEXT("C013"), TEXT("戦場の旗手"), ECGCardType::Unit, 4, 4, 4, TEXT("TODO_AllyBuffAtkThisTurn"), 1, 1.13f, TEXT("")),
			MakeCard(TEXT("C014"), TEXT("霊廟の守り手"), ECGCardType::Unit, 4, 3, 5, TEXT("TODO_OnDeathReturnRandomGraveyardUnit"), 0, 1.13f, TEXT("Guard")),
			MakeCard(TEXT("C015"), TEXT("隕鉄の突進獣"), ECGCardType::Unit, 4, 5, 4, TEXT("None"), 0, 1.13f, TEXT("")),
			MakeCard(TEXT("C016"), TEXT("連鎖術の教授"), ECGCardType::Unit, 4, 3, 4, TEXT("TODO_FirstSpellBonusDamage"), 1, 1.06f, TEXT("")),
			MakeCard(TEXT("C017"), TEXT("火花の一撃"), ECGCardType::Spell, 1, 0, 0, TEXT("OnPlayDamageTarget"), 2, 1.00f, TEXT("")),
			MakeCard(TEXT("C018"), TEXT("応急手当"), ECGCardType::Spell, 1, 0, 0, TEXT("OnPlayHealSelf"), 3, 1.20f, TEXT("")),
			MakeCard(TEXT("C019"), TEXT("手札の選別"), ECGCardType::Spell, 1, 0, 0, TEXT("TODO_Discard1Draw2"), 0, 1.25f, TEXT("")),
			MakeCard(TEXT("C020"), TEXT("見習い召集"), ECGCardType::Spell, 2, 0, 0, TEXT("TODO_Summon2x1_1Unit"), 2, 1.00f, TEXT("")),
			MakeCard(TEXT("C021"), TEXT("墓地再点火"), ECGCardType::Spell, 2, 0, 0, TEXT("TODO_ReturnGraveyardSpellSelfDamage1"), 1, 0.88f, TEXT("")),
			MakeCard(TEXT("C022"), TEXT("市場調達"), ECGCardType::Spell, 2, 0, 0, TEXT("TODO_BuyFromMarketCostUnder3ToHand"), 3, 1.13f, TEXT("")),
			MakeCard(TEXT("C023"), TEXT("連弾の雨"), ECGCardType::Spell, 3, 0, 0, TEXT("TODO_RandomEnemyDamage1x4"), 1, 0.67f, TEXT("")),
			MakeCard(TEXT("C024"), TEXT("逆転の号令"), ECGCardType::Spell, 3, 0, 0, TEXT("TODO_ConditionalDamage3or2"), 0, 0.45f, TEXT("")),
		};
		return Cards;
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
