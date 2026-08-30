#include "CGGameInstance.h"
#include "CGDeckSaveGame.h"
#include "CGCardDatabase.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FString CGDeckSaveSlotName = TEXT("CGDeckSaveSlot");
	constexpr int32 CGDeckSaveUserIndex = 0;
}

void UCGGameInstance::Init()
{
	Super::Init();

	if (UCGDeckSaveGame* Loaded = Cast<UCGDeckSaveGame>(UGameplayStatics::LoadGameFromSlot(CGDeckSaveSlotName, CGDeckSaveUserIndex)))
	{
		PlayerDeckCardIds = Loaded->SavedDeckCardIds;
	}

	// 保存データが無い(初回起動)、または12枚ぴったりでない(壊れている/仕様変更で
	// 無効化された)場合は、現行のスターター12枚をデフォルトにする。
	if (PlayerDeckCardIds.Num() != 12)
	{
		PlayerDeckCardIds = UCGCardDatabase::GetStarterDeckCardIds();
	}
}

void UCGGameInstance::SaveDeckToDisk()
{
	UCGDeckSaveGame* SaveGameObject = Cast<UCGDeckSaveGame>(UGameplayStatics::CreateSaveGameObject(UCGDeckSaveGame::StaticClass()));
	if (!SaveGameObject)
	{
		return;
	}
	SaveGameObject->SavedDeckCardIds = PlayerDeckCardIds;
	UGameplayStatics::SaveGameToSlot(SaveGameObject, CGDeckSaveSlotName, CGDeckSaveUserIndex);
}
