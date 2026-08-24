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
