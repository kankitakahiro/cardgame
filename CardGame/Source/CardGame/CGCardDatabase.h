#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CGTypes.h"
#include "CGCardDatabase.generated.h"

// カードマスタデータを保持する静的データベース。次期ルールの5色75種
// (docs/next-ruleset-cards-v1.md)+旧24種(docs/initial-cards-v0.1.md、
// 「無色」グループとして6つ目の選択肢を成す)。
// Content/CardGame/Data/DA_CG_C001〜C024 (PrimaryDataAsset) は参考用のインスペクト用データであり、
// ゲームロジックが実際に参照する正データはこちら(C++側)。
UCLASS()
class UCGCardDatabase : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// FindCard/GetAllCardIdsが参照する全カード(次期ルールの75種+旧24種の
	// 無色カード、計99種)。GetBuildableCards()と同じ内容を返す
	// (デッキ構築画面・カード図鑑もこの全量を表示対象にしている)。
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static const TArray<FCGCardDef>& GetAllCards();

	// デッキ構築画面・カード図鑑で選べるカード一覧(次期ルールの5色75種+
	// 旧24種の無色、計99種)。無色カードはどの色のデッキにも入れられ、色の
	// パッシブしきい値(17/25)にはカウントされない。
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static const TArray<FCGCardDef>& GetBuildableCards();

	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static bool FindCard(FName CardId, FCGCardDef& OutCard);

	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static TArray<FName> GetAllCardIds();

	// 初期デッキ(次期ルールの25枚、複数色混在の標準構成。docs/next-ruleset-design.md)。
	// デッキ構築画面を経ずに直接バトル画面へ入った場合(未構築時の人間側)の
	// フォールバック用。
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static TArray<FName> GetStarterDeckCardIds();

	// 色ごとの基本デッキ(25枚、当該色のパッシブしきい値(17/25)を満たす純色構成。
	// docs/next-ruleset-cards-v1.md「サンプルデッキ」参照)。対戦AI(Side1)がこの中からランダムに
	// 1色を選んで使う(ACGGameMode::InitializeMatch())。Colorに`None`を渡すと空配列を返す。
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static TArray<FName> GetBasicColorDeckCardIds(ECGColor Color);

	// フィニッシャー(各色固有の条件を達成すると自動で場に駆けつける専用Unit、
	// ACGPlayerState::CheckAndSpawnFinisher参照)のCardIdを返す。マーケット/
	// デッキ構築には出さないため、GetAllCards()/GetBuildableCards()には含まれず
	// FindCard()経由でのみ参照できる(変貌先カード・トークンと同じ扱い)。
	// Colorに対応するフィニッシャーが無ければNAME_Noneを返す。
	UFUNCTION(BlueprintPure, Category = "CardGame|Cards")
	static FName GetFinisherCardId(ECGColor Color);
};
