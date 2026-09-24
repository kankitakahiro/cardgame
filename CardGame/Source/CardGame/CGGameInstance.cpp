#include "CGGameInstance.h"
#include "CGDeckSaveGame.h"
#include "CGCardDatabase.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FString CGDeckSaveSlotName = TEXT("CGDeckSaveSlot");
	constexpr int32 CGDeckSaveUserIndex = 0;

	// 保存済みデッキが1件も無い(初回起動、または保存データが壊れていて全て
	// 除外された)場合に使う、唯一保証されるフォールバックデッキ。
	FCGSavedDeck MakeStarterDeck()
	{
		FCGSavedDeck Starter;
		Starter.DeckName = TEXT("スターターデッキ");
		Starter.CardIds = UCGCardDatabase::GetStarterDeckCardIds();
		return Starter;
	}
}

void UCGGameInstance::Init()
{
	Super::Init();

	if (UCGDeckSaveGame* Loaded = Cast<UCGDeckSaveGame>(UGameplayStatics::LoadGameFromSlot(CGDeckSaveSlotName, CGDeckSaveUserIndex)))
	{
		SavedDecks = Loaded->SavedDecks;
		ActiveDeckName = Loaded->ActiveDeckName;
	}

	// 壊れている(25枚固定ルールに反する)保存データは除外する
	// (次期ルールへの移行前に保存された12枚デッキ等、旧仕様の残骸を含む)。
	SavedDecks.RemoveAll([](const FCGSavedDeck& Deck) { return Deck.CardIds.Num() != 25; });

	// 保存済みデッキが1つも無ければ、スターターデッキを唯一のデッキとして用意する。
	if (SavedDecks.Num() == 0)
	{
		SavedDecks.Add(MakeStarterDeck());
	}

	// ActiveDeckNameに一致するものがあればそれを、無ければ先頭をアクティブにする。
	const int32 ActiveIndex = SavedDecks.IndexOfByPredicate(
		[this](const FCGSavedDeck& Deck) { return Deck.DeckName == ActiveDeckName; });
	const int32 UseIndex = SavedDecks.IsValidIndex(ActiveIndex) ? ActiveIndex : 0;
	PlayerDeckCardIds = SavedDecks[UseIndex].CardIds;
	ActiveDeckName = SavedDecks[UseIndex].DeckName;

	PersistSavedDecksToDisk();
}

void UCGGameInstance::SaveDeckAs(const FString& DeckName, const TArray<FName>& CardIds)
{
	const FString Trimmed = DeckName.TrimStartAndEnd();
	const FString FinalName = Trimmed.IsEmpty() ? TEXT("名称未設定デッキ") : Trimmed;

	const int32 ExistingIndex = SavedDecks.IndexOfByPredicate(
		[&FinalName](const FCGSavedDeck& Deck) { return Deck.DeckName == FinalName; });
	if (SavedDecks.IsValidIndex(ExistingIndex))
	{
		SavedDecks[ExistingIndex].CardIds = CardIds;
	}
	else
	{
		FCGSavedDeck NewDeck;
		NewDeck.DeckName = FinalName;
		NewDeck.CardIds = CardIds;
		SavedDecks.Add(NewDeck);
	}

	PlayerDeckCardIds = CardIds;
	ActiveDeckName = FinalName;
	PersistSavedDecksToDisk();
}

void UCGGameInstance::SelectSavedDeck(int32 Index)
{
	if (!SavedDecks.IsValidIndex(Index))
	{
		return;
	}
	PlayerDeckCardIds = SavedDecks[Index].CardIds;
	ActiveDeckName = SavedDecks[Index].DeckName;
	PersistSavedDecksToDisk();
}

void UCGGameInstance::DeleteSavedDeck(int32 Index)
{
	if (!SavedDecks.IsValidIndex(Index))
	{
		return;
	}

	const bool bWasActive = (SavedDecks[Index].DeckName == ActiveDeckName);
	SavedDecks.RemoveAt(Index);

	if (SavedDecks.Num() == 0)
	{
		SavedDecks.Add(MakeStarterDeck());
	}

	if (bWasActive)
	{
		PlayerDeckCardIds = SavedDecks[0].CardIds;
		ActiveDeckName = SavedDecks[0].DeckName;
	}

	PersistSavedDecksToDisk();
}

void UCGGameInstance::PersistSavedDecksToDisk()
{
	UCGDeckSaveGame* SaveGameObject = Cast<UCGDeckSaveGame>(UGameplayStatics::CreateSaveGameObject(UCGDeckSaveGame::StaticClass()));
	if (!SaveGameObject)
	{
		return;
	}
	SaveGameObject->SavedDecks = SavedDecks;
	SaveGameObject->ActiveDeckName = ActiveDeckName;
	UGameplayStatics::SaveGameToSlot(SaveGameObject, CGDeckSaveSlotName, CGDeckSaveUserIndex);
}
