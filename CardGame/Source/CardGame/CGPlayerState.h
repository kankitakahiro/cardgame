#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CGTypes.h"
#include "CGPlayerState.generated.h"

// 1プレイヤー(片側)の対戦データ。docs/blueprint-architecture.md の BP_CG_PlayerState に相当。
// ローカル対戦プロトタイプのため、実際のネットワーク接続(PlayerController)とは独立に
// ACGGameMode が2体を直接SpawnActorして「対戦相手」として扱う。
UCLASS()
class ACGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 SideIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CurrentHP = 20;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 MaxHP = 20;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CurrentMana = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 MaxMana = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bIsDefeated = false;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<FName> HandCardIds;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<FName> DeckCardIds;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<FName> DiscardCardIds;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<FName> BoardUnitCardIds;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<int32> BoardUnitAtk;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<int32> BoardUnitHp;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<bool> BoardUnitCanAttack;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<bool> BoardUnitHasGuard;

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void InitializeStartingDeck(const TArray<FName>& StarterCardIds);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ShuffleDeck();

	// デッキが0枚の状態で引こうとした場合は false を返し bIsDefeated を立てる(山札切れ負け)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool DrawCard();

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool PlayCardFromHand(FName CardId, ACGPlayerState* Opponent, int32 TargetUnitIndex);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool BuyCard(FName CardId);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ApplyDamage(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void Heal(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ApplyDamageToUnit(int32 UnitIndex, int32 Amount);

	// 場のHP0以下ユニットを取り除き、墓地へ送る。OnDeathDraw を持つユニットが死亡した分の
	// ドロー枚数を返す(実際のドローはGameMode側で行う=このPlayerStateを跨いだ効果もあるため)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	int32 RemoveDeadUnitsAndGetDeathDrawCount();

	UFUNCTION(BlueprintPure, Category = "CardGame")
	bool HasGuardUnit() const;

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RefreshManaForNewTurn();

private:
	void ResolveSpellEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, int32 TargetUnitIndex);
};
