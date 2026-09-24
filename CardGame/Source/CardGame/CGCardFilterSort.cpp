#include "CGCardFilterSort.h"

TArray<FCGCardDef> CGCardFilterSort::BuildFilteredSortedList(const TArray<FCGCardDef>& AllCards, const FCGCardFilterSortState& State)
{
	TArray<FCGCardDef> Result;
	for (const FCGCardDef& Def : AllCards)
	{
		if (State.TypeFilter.IsSet() && Def.CardType != State.TypeFilter.GetValue())
		{
			continue;
		}
		if (State.ColorFilter.IsSet() && Def.Color != State.ColorFilter.GetValue())
		{
			continue;
		}
		if (!State.SearchQuery.IsEmpty() && !Def.CardName.Contains(State.SearchQuery, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Result.Add(Def);
	}

	switch (State.SortMode)
	{
	case ECGCardSortMode::Cost:
		Result.Sort([](const FCGCardDef& A, const FCGCardDef& B)
		{
			return A.Cost != B.Cost ? A.Cost < B.Cost : A.CardName < B.CardName;
		});
		break;
	case ECGCardSortMode::Name:
		Result.Sort([](const FCGCardDef& A, const FCGCardDef& B)
		{
			return A.CardName < B.CardName;
		});
		break;
	case ECGCardSortMode::Tribe:
		Result.Sort([](const FCGCardDef& A, const FCGCardDef& B)
		{
			// 種族未設定のカードは最後にまとめる(現状は一部のカードにしか
			// 設定されていない予約フィールドのため。docs/architecture.md参照)。
			const bool bAEmpty = A.Tribe.IsEmpty();
			const bool bBEmpty = B.Tribe.IsEmpty();
			if (bAEmpty != bBEmpty)
			{
				return !bAEmpty;
			}
			if (A.Tribe != B.Tribe)
			{
				return A.Tribe < B.Tribe;
			}
			return A.Cost < B.Cost;
		});
		break;
	}
	return Result;
}
