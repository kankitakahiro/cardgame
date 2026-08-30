#include "CGPlayerState.h"
#include "CGCardDatabase.h"
#include "CGGameState.h"

void ACGPlayerState::InitializeStartingDeck(const TArray<FName>& StarterCardIds)
{
	DeckCardIds = StarterCardIds;
	HandCardIds.Reset();
	DiscardCardIds.Reset();
	BoardUnitCardIds.Reset();
	BoardUnitAtk.Reset();
	BoardUnitHp.Reset();
	BoardUnitCanAttack.Reset();
	BoardUnitHasGuard.Reset();
	ShuffleDeck();
}

void ACGPlayerState::ShuffleDeck()
{
	const int32 LastIndex = DeckCardIds.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		const int32 SwapIndex = FMath::RandRange(i, LastIndex);
		DeckCardIds.Swap(i, SwapIndex);
	}
}

bool ACGPlayerState::DrawCard()
{
	if (DeckCardIds.Num() == 0)
	{
		// 山札切れ負け(docs/game-rules-minimum.md)
		bIsDefeated = true;
		return false;
	}

	const FName Top = DeckCardIds[0];
	DeckCardIds.RemoveAt(0);

	// 手札上限10を超える分は、ルール未規定のため引いた瞬間に捨て札へ送る運用としている。
	if (HandCardIds.Num() < 10)
	{
		HandCardIds.Add(Top);
	}
	else
	{
		DiscardCardIds.Add(Top);
	}
	return true;
}

bool ACGPlayerState::PlayCardFromHand(FName CardId, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState)
{
	if (!HandCardIds.Contains(CardId))
	{
		return false;
	}

	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return false;
	}

	if (CurrentMana < Def.Cost)
	{
		return false;
	}

	HandCardIds.RemoveSingle(CardId);
	CurrentMana -= Def.Cost;

	// 街道の突撃兵(C009)判定用: このカードが「今ターン2枚目以降」かどうかを、
	// カウンタをインクリメントする前に確定させる。
	const bool bIsSecondOrLaterPlayThisTurn = (CardsPlayedThisTurn >= 1);
	CardsPlayedThisTurn++;

	if (Def.CardType == ECGCardType::Unit)
	{
		int32 EnterAtk = Def.Atk;
		if (Def.EffectId == FName(TEXT("SecondPlayBuff")) && bIsSecondOrLaterPlayThisTurn) // C009 街道の突撃兵
		{
			EnterAtk += 1;
		}

		BoardUnitCardIds.Add(CardId);
		BoardUnitAtk.Add(EnterAtk);
		BoardUnitHp.Add(Def.Hp);
		BoardUnitCanAttack.Add(Def.HasTag(TEXT("Haste")));
		BoardUnitHasGuard.Add(Def.HasTag(TEXT("Guard")));
		ResolveUnitOnPlayEffect(Def, Opponent);

		if (Def.EffectId == FName(TEXT("AllyBuffAtkThisTurn"))) // C013 戦場の旗手
		{
			// 簡易実装: 本来は「ターン中のみ」の一時バフだが、一時バフ管理の仕組みを
			// 新設するコストを避けるため、登場時点にいる味方(このユニット自身を除く)へ
			// 永続的に+1/+0を付与する形に簡略化している(docs/automation-notes.md参照)。
			for (int32 i = 0; i < BoardUnitAtk.Num() - 1; ++i)
			{
				BoardUnitAtk[i] += 1;
			}
		}
	}
	else
	{
		SpellsPlayedThisTurn++;
		ResolveSpellEffect(Def, Opponent, TargetUnitIndex, CGState);
		DiscardCardIds.Add(CardId);

		// 追撃の射手(C010): 味方Spell使用時、1ターンに1回だけ敵リーダーへ1ダメージ。
		if (Opponent && !bAllySpellPingUsedThisTurn && HasBoardUnitWithEffect(FName(TEXT("OnAllySpellPing1"))))
		{
			Opponent->ApplyDamage(1);
			bAllySpellPingUsedThisTurn = true;
		}
	}

	return true;
}

void ACGPlayerState::ResolveSpellEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState)
{
	// 実装済みの効果のみ処理する。TODO_ 接頭辞のEffectIdはデータのみ保持し、ここでは何もしない
	// (docs/automation-notes.md の実装ロードマップ参照)。

	// 連鎖術の教授(C016)判定用: このSpellが「今ターン最初のSpell」かどうか
	// (呼び出し元でSpellsPlayedThisTurnをインクリメント済みのため、1なら初回)。
	const int32 FirstSpellDamageBonus =
		(SpellsPlayedThisTurn == 1 && HasBoardUnitWithEffect(FName(TEXT("FirstSpellBonusDamage")))) ? 1 : 0;

	if (Def.EffectId == FName(TEXT("OnPlayDamageTarget")))
	{
		if (!Opponent)
		{
			return;
		}
		const int32 Damage = Def.EffectValue + FirstSpellDamageBonus;
		if (Opponent->BoardUnitCardIds.IsValidIndex(TargetUnitIndex))
		{
			Opponent->ApplyDamageToUnit(TargetUnitIndex, Damage);
		}
		else if (Opponent->BoardUnitCardIds.Num() > 0)
		{
			// 対象未指定時は先頭ユニットへ自動着弾(単一対象、docs/game-rules-minimum.md)。
			Opponent->ApplyDamageToUnit(0, Damage);
		}
		else
		{
			Opponent->ApplyDamage(Damage);
		}
	}
	else if (Def.EffectId == FName(TEXT("OnPlayHealSelf")))
	{
		Heal(Def.EffectValue);
	}
	else if (Def.EffectId == FName(TEXT("Discard1Draw2"))) // C019 手札の選別
	{
		DiscardRandomFromHand();
		DrawCard();
		DrawCard();
	}
	else if (Def.EffectId == FName(TEXT("Summon2x1_1Unit"))) // C020 見習い召集
	{
		AddBoardUnitDirect(FName(TEXT("TK_APPRENTICE")), 1, 1, false, false);
		AddBoardUnitDirect(FName(TEXT("TK_APPRENTICE")), 1, 1, false, false);
	}
	else if (Def.EffectId == FName(TEXT("ReturnGraveyardSpellSelfDamage1"))) // C021 墓地再点火
	{
		if (TryReturnRandomSpellFromDiscardToHand())
		{
			ApplyDamage(1);
		}
	}
	else if (Def.EffectId == FName(TEXT("BuyFromMarketCostUnder3ToHand"))) // C022 市場調達
	{
		if (CGState && HandCardIds.Num() < 10)
		{
			for (int32 i = 0; i < CGState->MarketCardIds.Num(); ++i)
			{
				FCGCardDef MarketDef;
				if (UCGCardDatabase::FindCard(CGState->MarketCardIds[i], MarketDef) && MarketDef.Cost <= 3)
				{
					HandCardIds.Add(CGState->MarketCardIds[i]);
					CGState->MarketCardIds.RemoveAt(i);
					CGState->RefillMarket();
					break;
				}
			}
		}
	}
	else if (Def.EffectId == FName(TEXT("RandomEnemyDamage1x4"))) // C023 連弾の雨
	{
		if (!Opponent)
		{
			return;
		}
		for (int32 Hit = 0; Hit < 4; ++Hit)
		{
			if (Opponent->BoardUnitCardIds.Num() > 0)
			{
				const int32 RandomUnitIndex = FMath::RandRange(0, Opponent->BoardUnitCardIds.Num() - 1);
				Opponent->ApplyDamageToUnit(RandomUnitIndex, 1);
			}
			else
			{
				Opponent->ApplyDamage(1);
			}
		}
	}
	else if (Def.EffectId == FName(TEXT("ConditionalDamage3or2"))) // C024 逆転の号令
	{
		if (!Opponent)
		{
			return;
		}
		const int32 Damage = ((CurrentHP <= Opponent->CurrentHP) ? 3 : 2) + FirstSpellDamageBonus;
		if (Opponent->BoardUnitCardIds.IsValidIndex(TargetUnitIndex))
		{
			Opponent->ApplyDamageToUnit(TargetUnitIndex, Damage);
		}
		else if (Opponent->BoardUnitCardIds.Num() > 0)
		{
			Opponent->ApplyDamageToUnit(0, Damage);
		}
		else
		{
			Opponent->ApplyDamage(Damage);
		}
	}
}

void ACGPlayerState::ResolveUnitOnPlayEffect(const FCGCardDef& Def, ACGPlayerState* Opponent)
{
	if (Def.EffectId == FName(TEXT("OnPlayDiscard1"))) // C008 錆びた巨兵
	{
		DiscardRandomFromHand();
	}
	else if (Def.EffectId == FName(TEXT("OnPlayReturnGraveyardCheapCard"))) // C011 再誕の司祭
	{
		TryReturnCheapestFromDiscardToHand(1);
	}
	else if (Def.EffectId == FName(TEXT("GraveyardToDeckBottomDraw1"))) // C006 墓場あさり
	{
		TryMoveRandomDiscardCardToDeckBottom();
		DrawCard();
	}
	else if (Def.EffectId == FName(TEXT("ScoutTop1"))) // C001 先駆けの斥候
	{
		ScryTop();
	}
}

bool ACGPlayerState::DiscardRandomFromHand()
{
	if (HandCardIds.Num() == 0)
	{
		return false;
	}
	const int32 Index = FMath::RandRange(0, HandCardIds.Num() - 1);
	DiscardCardIds.Add(HandCardIds[Index]);
	HandCardIds.RemoveAt(Index);
	return true;
}

bool ACGPlayerState::TryReturnCheapestFromDiscardToHand(int32 MaxCost)
{
	if (HandCardIds.Num() >= 10)
	{
		return false;
	}
	for (int32 i = 0; i < DiscardCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(DiscardCardIds[i], Def) && Def.Cost <= MaxCost)
		{
			HandCardIds.Add(DiscardCardIds[i]);
			DiscardCardIds.RemoveAt(i);
			return true;
		}
	}
	return false;
}

bool ACGPlayerState::TryMoveRandomDiscardCardToDeckBottom()
{
	if (DiscardCardIds.Num() == 0)
	{
		return false;
	}
	const int32 Index = FMath::RandRange(0, DiscardCardIds.Num() - 1);
	DeckCardIds.Add(DiscardCardIds[Index]); // DrawCardは先頭(index 0)から引くため、末尾に追加=デッキの一番下。
	DiscardCardIds.RemoveAt(Index);
	return true;
}

bool ACGPlayerState::TryReturnRandomSpellFromDiscardToHand()
{
	if (HandCardIds.Num() >= 10)
	{
		return false;
	}
	for (int32 i = 0; i < DiscardCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(DiscardCardIds[i], Def) && Def.CardType == ECGCardType::Spell)
		{
			HandCardIds.Add(DiscardCardIds[i]);
			DiscardCardIds.RemoveAt(i);
			return true;
		}
	}
	return false;
}

bool ACGPlayerState::TryMoveRandomDiscardUnitToDeckTop()
{
	TArray<int32> UnitIndices;
	for (int32 i = 0; i < DiscardCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(DiscardCardIds[i], Def) && Def.CardType == ECGCardType::Unit)
		{
			UnitIndices.Add(i);
		}
	}
	if (UnitIndices.Num() == 0)
	{
		return false;
	}
	const int32 Chosen = UnitIndices[FMath::RandRange(0, UnitIndices.Num() - 1)];
	DeckCardIds.Insert(DiscardCardIds[Chosen], 0);
	DiscardCardIds.RemoveAt(Chosen);
	return true;
}

void ACGPlayerState::ScryTop()
{
	if (DeckCardIds.Num() == 0)
	{
		return;
	}
	FCGCardDef TopDef;
	if (!UCGCardDatabase::FindCard(DeckCardIds[0], TopDef))
	{
		return;
	}
	// プレイヤー操作の代わりの簡易ヒューリスティック: 今すぐ(+1マナ後まで)払えなさそうな
	// 高コストカードなら山札の一番下へ送り、そうでなければ一番上に残す。
	if (TopDef.Cost > CurrentMana + 1)
	{
		const FName Top = DeckCardIds[0];
		DeckCardIds.RemoveAt(0);
		DeckCardIds.Add(Top);
	}
}

void ACGPlayerState::AddBoardUnitDirect(FName CardId, int32 Atk, int32 Hp, bool bCanAttackImmediately, bool bHasGuard)
{
	BoardUnitCardIds.Add(CardId);
	BoardUnitAtk.Add(Atk);
	BoardUnitHp.Add(Hp);
	BoardUnitCanAttack.Add(bCanAttackImmediately);
	BoardUnitHasGuard.Add(bHasGuard);
}

bool ACGPlayerState::BuyCard(FName CardId)
{
	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return false;
	}

	// 市場監督官(C012)が場にいる間、購入コストを1軽減(最小1)。
	const int32 EffectiveCost = HasBoardUnitWithEffect(FName(TEXT("BuyCostReductionThisTurn")))
		? FMath::Max(1, Def.Cost - 1)
		: Def.Cost;

	if (CurrentMana < EffectiveCost || HandCardIds.Num() >= 10)
	{
		return false;
	}

	CurrentMana -= EffectiveCost;
	HandCardIds.Add(CardId);
	bBoughtThisTurn = true;
	return true;
}

void ACGPlayerState::ApplyDamage(int32 Amount)
{
	CurrentHP = FMath::Max(0, CurrentHP - Amount);
	if (CurrentHP <= 0)
	{
		bIsDefeated = true;
	}
}

void ACGPlayerState::Heal(int32 Amount)
{
	CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);
}

void ACGPlayerState::ApplyDamageToUnit(int32 UnitIndex, int32 Amount)
{
	if (!BoardUnitHp.IsValidIndex(UnitIndex))
	{
		return;
	}
	BoardUnitHp[UnitIndex] -= Amount;
}

int32 ACGPlayerState::RemoveDeadUnitsAndGetDeathDrawCount()
{
	int32 DrawCount = 0;
	for (int32 i = BoardUnitHp.Num() - 1; i >= 0; --i)
	{
		if (BoardUnitHp[i] > 0)
		{
			continue;
		}

		FCGCardDef Def;
		const bool bFound = UCGCardDatabase::FindCard(BoardUnitCardIds[i], Def);
		if (bFound && Def.EffectId == FName(TEXT("OnDeathDraw")))
		{
			DrawCount += Def.EffectValue;
		}

		DiscardCardIds.Add(BoardUnitCardIds[i]);
		BoardUnitCardIds.RemoveAt(i);
		BoardUnitAtk.RemoveAt(i);
		BoardUnitHp.RemoveAt(i);
		BoardUnitCanAttack.RemoveAt(i);
		BoardUnitHasGuard.RemoveAt(i);

		if (bFound && Def.EffectId == FName(TEXT("OnDeathReturnRandomGraveyardUnit"))) // C014 霊廟の守り手
		{
			TryMoveRandomDiscardUnitToDeckTop();
		}
	}
	return DrawCount;
}

bool ACGPlayerState::HasGuardUnit() const
{
	return BoardUnitHasGuard.Contains(true);
}

bool ACGPlayerState::HasBoardUnitWithEffect(FName EffectId) const
{
	for (const FName& Id : BoardUnitCardIds)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(Id, Def) && Def.EffectId == EffectId)
		{
			return true;
		}
	}
	return false;
}

void ACGPlayerState::RefreshManaForNewTurn()
{
	// docs/initial-cards-v0.1.md「毎ターン自動増加マナ」。上限10は暫定ルール。
	MaxMana = FMath::Min(MaxMana + 1, 10);
	CurrentMana = MaxMana;
	CardsPlayedThisTurn = 0;
	SpellsPlayedThisTurn = 0;
	bBoughtThisTurn = false;
	bAllySpellPingUsedThisTurn = false;
}

void ACGPlayerState::ResolveEndTurnEffects()
{
	if (bBoughtThisTurn && HasBoardUnitWithEffect(FName(TEXT("OnBuyEndTurnDiscardDraw"))) && HandCardIds.Num() > 0) // C007
	{
		DiscardRandomFromHand();
		DrawCard();
	}
}
