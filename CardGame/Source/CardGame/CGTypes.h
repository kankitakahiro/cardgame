#pragma once

#include "CoreMinimal.h"
#include "CGTypes.generated.h"

// カード種別。docs/game-rules-minimum.md の「まず実装するカード種」に対応。
UENUM(BlueprintType)
enum class ECGCardType : uint8
{
	Unit,
	Spell
};

// ターン内フェーズ。docs/game-rules-minimum.md の「ターン構造」に対応。
UENUM(BlueprintType)
enum class ECGPhase : uint8
{
	Draw,
	Main,
	End
};

// docs/initial-cards-v0.1.md の1行に対応するカード定義。
// DataTable+UserDefinedStructはPython自動化では作成できなかった(docs/automation-notes.md参照)ため、
// C++側のネイティブ構造体として定義し、UCGCardDatabase が静的データとして保持する。
USTRUCT(BlueprintType)
struct FCGCardDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName CardId;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString CardName;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	ECGCardType CardType = ECGCardType::Unit;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 Atk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 Hp = 0;

	// docs/initial-cards-v0.1.md の「効果概要」列そのまま。UI表示用。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString Description;

	// 部族/系統(例: 人間、アンデッド等)。今後のシナジー要素での活用を見込んだ予約
	// フィールドで、現状は一部のカードにサンプル値を入れているのみ(docs/architecture.md参照)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString Tribe;

	// カード下部に載せる短いフレーバーテキスト(世界観演出用)。現状は一部のカードのみ。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString FlavorText;

	// 実装済みの基本効果("OnPlayDamageTarget" / "OnPlayHealSelf" / "OnDeathDraw")のみロジックが動く。
	// それ以外は "TODO_" 接頭辞のデータのみで、効果は未実装(docs/initial-cards-v0.1.md参照)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName EffectId;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 EffectValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	float Ratio = 0.f;

	// カンマ区切りのキーワード群。現状 "Haste"(速攻) / "Guard"(守護) のみロジックが参照する。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString Tags;

	bool HasTag(const FString& Tag) const
	{
		TArray<FString> Parts;
		Tags.ParseIntoArray(Parts, TEXT(","), true);
		return Parts.Contains(Tag);
	}
};

// 場に出ているユニット1体分の状態。以前はCGPlayerState側で
// BoardUnitCardIds/Atk/Hp/CanAttack/HasGuardという5本のパラレル配列で
// 管理していたが、新しい状態(毒/バフ/沈黙等)を足すたびに配列が増えて
// 同期が崩れやすかったため、1つの構造体にまとめている
// (docs/architecture.md「場のユニットの状態管理」参照)。
USTRUCT(BlueprintType)
struct FCGBoardUnit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName CardId;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 Atk = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 Hp = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bCanAttack = false;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bHasGuard = false;
};

// FCGCardDef::EffectId に入る値の一覧。以前は FName(TEXT("...")) のリテラルが
// CGCardDatabase.cpp(カード定義)とCGPlayerState.cpp(効果ハンドラ登録)の両方に
// 重複して散らばっており、タイポがあっても気付きにくかった。ここに集約することで
// 両者が同じ定数を参照するようにする(docs/architecture.md「カード効果ディスパッチ」参照)。
namespace CGEffectId
{
	inline constexpr const TCHAR* None = TEXT("None");

	// Unit登場時効果
	inline constexpr const TCHAR* ScoutTop1 = TEXT("ScoutTop1");                                   // C001 先駆けの斥候
	inline constexpr const TCHAR* GraveyardToDeckBottomDraw1 = TEXT("GraveyardToDeckBottomDraw1");  // C006 墓場あさり
	inline constexpr const TCHAR* OnPlayDiscard1 = TEXT("OnPlayDiscard1");                          // C008 錆びた巨兵
	inline constexpr const TCHAR* SecondPlayBuff = TEXT("SecondPlayBuff");                          // C009 街道の突撃兵
	inline constexpr const TCHAR* OnPlayReturnGraveyardCheapCard = TEXT("OnPlayReturnGraveyardCheapCard"); // C011 再誕の司祭
	inline constexpr const TCHAR* AllyBuffAtkThisTurn = TEXT("AllyBuffAtkThisTurn");                // C013 戦場の旗手

	// Unit常在効果(場にいる間ずっと有効。HasBoardUnitWithEffectで判定)
	inline constexpr const TCHAR* OnDeathDraw = TEXT("OnDeathDraw");                                // C004 小さな研究者
	inline constexpr const TCHAR* OnBuyEndTurnDiscardDraw = TEXT("OnBuyEndTurnDiscardDraw");        // C007 市場の仲買人
	inline constexpr const TCHAR* OnAllySpellPing1 = TEXT("OnAllySpellPing1");                      // C010 追撃の射手
	inline constexpr const TCHAR* BuyCostReductionThisTurn = TEXT("BuyCostReductionThisTurn");      // C012 市場監督官
	inline constexpr const TCHAR* OnDeathReturnRandomGraveyardUnit = TEXT("OnDeathReturnRandomGraveyardUnit"); // C014 霊廟の守り手
	inline constexpr const TCHAR* FirstSpellBonusDamage = TEXT("FirstSpellBonusDamage");            // C016 連鎖術の教授

	// Spell効果
	inline constexpr const TCHAR* OnPlayDamageTarget = TEXT("OnPlayDamageTarget");                  // C017 火花の一撃
	inline constexpr const TCHAR* OnPlayHealSelf = TEXT("OnPlayHealSelf");                          // C018 応急手当
	inline constexpr const TCHAR* Discard1Draw2 = TEXT("Discard1Draw2");                            // C019 手札の選別
	inline constexpr const TCHAR* Summon2x1_1Unit = TEXT("Summon2x1_1Unit");                        // C020 見習い召集
	inline constexpr const TCHAR* ReturnGraveyardSpellSelfDamage1 = TEXT("ReturnGraveyardSpellSelfDamage1"); // C021 墓地再点火
	inline constexpr const TCHAR* BuyFromMarketCostUnder3ToHand = TEXT("BuyFromMarketCostUnder3ToHand");     // C022 市場調達
	inline constexpr const TCHAR* RandomEnemyDamage1x4 = TEXT("RandomEnemyDamage1x4");              // C023 連弾の雨
	inline constexpr const TCHAR* ConditionalDamage3or2 = TEXT("ConditionalDamage3or2");            // C024 逆転の号令
}
