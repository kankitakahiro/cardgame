#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CGTypes.h"
#include "CGPlayerState.generated.h"

class ACGGameState;

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
	TArray<FCGBoardUnit> BoardUnits;

	// 街道の突撃兵(C009)判定用: このターン何枚目のカードをプレイしたか。StartTurnで0に戻る。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CardsPlayedThisTurn = 0;

	// 連鎖術の教授(C016)判定用: このターン何枚目のSpellをプレイしたか。StartTurnで0に戻る。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 SpellsPlayedThisTurn = 0;

	// 市場の仲買人(C007)判定用: このターン購入したか。StartTurnでfalseに戻る。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bBoughtThisTurn = false;

	// 追撃の射手(C010)判定用: このターンにすでに1回発動したか。StartTurnでfalseに戻る。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bAllySpellPingUsedThisTurn = false;

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void InitializeStartingDeck(const TArray<FName>& StarterCardIds);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ShuffleDeck();

	// デッキが0枚の状態で引こうとした場合は false を返し bIsDefeated を立てる(山札切れ負け)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool DrawCard();

	// CGState はマーケットを参照する効果(例: 市場調達)のためだけに使う。不要なら nullptr で可。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool PlayCardFromHand(FName CardId, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState = nullptr);

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

	// 場に指定EffectIdを持つUnitが1体でもいるか(常在効果の判定に使う)。
	UFUNCTION(BlueprintPure, Category = "CardGame")
	bool HasBoardUnitWithEffect(FName EffectId) const;

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RefreshManaForNewTurn();

	// EndTurn時に呼ぶ。市場の仲買人(C007): このターン購入していれば、
	// 手札を1枚捨てて1ドローする(本来は任意選択だが、選択UI省略のため自動発動)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ResolveEndTurnEffects();

	// 以下はカード効果ハンドラ(CGPlayerState.cpp 無名namespace内のHandle_*関数群、
	// docs/refactor-plan-architecture.md Step 2参照)から呼ばれるための公開メンバー。
	// 外部からの直接呼び出しは想定していない。

	// 手札からランダムに1枚選んで捨てる(捨てる枚数を指定する効果がまだ無いため、
	// 対象を選ぶUIも用意していない簡易実装)。
	bool DiscardRandomFromHand();

	// 墓地からコストMaxCost以下のカードを1枚探して手札へ戻す。
	bool TryReturnCheapestFromDiscardToHand(int32 MaxCost);

	// 墓地からランダムに1枚選んでデッキの一番下(=配列の末尾、DrawCardは先頭から引く)へ戻す。
	bool TryMoveRandomDiscardCardToDeckBottom();

	// 墓地からSpellを1枚探して手札へ戻す。
	bool TryReturnRandomSpellFromDiscardToHand();

	// 墓地からUnitをランダムに1枚探してデッキの一番上へ戻す。
	bool TryMoveRandomDiscardUnitToDeckTop();

	// デッキ一番上を確認し、当面プレイできそうにない(コストが高すぎる)なら一番下へ送る
	// (先駆けの斥候の「上下選択」を、プレイヤー操作なしの簡易ヒューリスティックで代替)。
	void ScryTop();

	// マーケットや購入を介さず、直接カードデータIDを指定して場にユニットを1体追加する
	// (見習い召集の「1/1トークンを出す」等、実在カードではないユニットの生成に使う)。
	void AddBoardUnitDirect(FName CardId, int32 Atk, int32 Hp, bool bCanAttackImmediately, bool bHasGuard);

private:
	void ResolveSpellEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState);
	void ResolveUnitOnPlayEffect(const FCGCardDef& Def, ACGPlayerState* Opponent);
};
