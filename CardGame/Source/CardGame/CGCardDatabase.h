#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CGTypes.h"
#include "CGCardDatabase.generated.h"

// 24枚のカードマスタデータ(docs/initial-cards-v0.1.md)を保持する静的データベース。
// Content/CardGame/Data/DA_CG_C001〜C024 (PrimaryDataAsset) は参考用のインスペクト用データであり、
// ゲームロジックが実際に参照する正データはこちら(C++側)。
UCLASS()
class UCGCardDatabase : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static const TArray<FCGCardDef>& GetAllCards();

	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static bool FindCard(FName CardId, FCGCardDef& OutCard);

	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static TArray<FName> GetAllCardIds();

	// 初期デッキ(12枚, docs/game-rules-minimum.md)。両プレイヤー共通の暫定ルールとして
	// コストの低い12枚(C001〜C012)を採用している。docs/automation-notes.md にも根拠を記載。
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static TArray<FName> GetStarterDeckCardIds();
};
