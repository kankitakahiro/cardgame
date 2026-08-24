#include "CGPlayerState.h"
#include "CGCardDatabase.h"

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

bool ACGPlayerState::PlayCardFromHand(FName CardId, ACGPlayerState* Opponent, int32 TargetUnitIndex)
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

	if (Def.CardType == ECGCardType::Unit)
	{
		BoardUnitCardIds.Add(CardId);
		BoardUnitAtk.Add(Def.Atk);
		BoardUnitHp.Add(Def.Hp);
		BoardUnitCanAttack.Add(Def.HasTag(TEXT("Haste")));
		BoardUnitHasGuard.Add(Def.HasTag(TEXT("Guard")));
	}
	else
	{
		ResolveSpellEffect(Def, Opponent, TargetUnitIndex);
		DiscardCardIds.Add(CardId);
	}

	return true;
}

void ACGPlayerState::ResolveSpellEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, int32 TargetUnitIndex)
{
	// 実装済みの基本効果のみ(docs/game-rules-minimum.md「まず実装する効果種」)。
	// TODO_ 接頭辞のEffectIdはデータのみ保持し、ここでは何もしない。
	if (Def.EffectId == FName(TEXT("OnPlayDamageTarget")))
	{
		if (!Opponent)
		{
			return;
		}
		if (Opponent->BoardUnitCardIds.IsValidIndex(TargetUnitIndex))
		{
			Opponent->ApplyDamageToUnit(TargetUnitIndex, Def.EffectValue);
		}
		else if (Opponent->BoardUnitCardIds.Num() > 0)
		{
			// 対象未指定時は先頭ユニットへ自動着弾(単一対象、docs/game-rules-minimum.md)。
			Opponent->ApplyDamageToUnit(0, Def.EffectValue);
		}
		else
		{
			Opponent->ApplyDamage(Def.EffectValue);
		}
	}
	else if (Def.EffectId == FName(TEXT("OnPlayHealSelf")))
	{
		Heal(Def.EffectValue);
	}
}

bool ACGPlayerState::BuyCard(FName CardId)
{
	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return false;
	}
	if (CurrentMana < Def.Cost || HandCardIds.Num() >= 10)
	{
		return false;
	}

	CurrentMana -= Def.Cost;
	HandCardIds.Add(CardId);
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
		if (UCGCardDatabase::FindCard(BoardUnitCardIds[i], Def) && Def.EffectId == FName(TEXT("OnDeathDraw")))
		{
			DrawCount += Def.EffectValue;
		}

		DiscardCardIds.Add(BoardUnitCardIds[i]);
		BoardUnitCardIds.RemoveAt(i);
		BoardUnitAtk.RemoveAt(i);
		BoardUnitHp.RemoveAt(i);
		BoardUnitCanAttack.RemoveAt(i);
		BoardUnitHasGuard.RemoveAt(i);
	}
	return DrawCount;
}

bool ACGPlayerState::HasGuardUnit() const
{
	return BoardUnitHasGuard.Contains(true);
}

void ACGPlayerState::RefreshManaForNewTurn()
{
	// docs/initial-cards-v0.1.md「毎ターン自動増加マナ」。上限10は暫定ルール。
	MaxMana = FMath::Min(MaxMana + 1, 10);
	CurrentMana = MaxMana;
}
