#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "CGTypes.h"

// カード一覧の並び替え基準。デッキ構築画面・カード図鑑の両方で共通利用する
// (docs/architecture.md「カードUIの設計」。「カードが増えたときの工夫」フィードバックのため)。
enum class ECGCardSortMode : uint8
{
	Cost,
	Name,
	Tribe,
};

// 検索/種別フィルタ/並び替えの現在の条件。UIの状態そのものを表す。
struct FCGCardFilterSortState
{
	FString SearchQuery;
	TOptional<ECGCardType> TypeFilter;
	// 色フィルタ(デッキ構築画面「デッキ構築で色でフィルターをかけれるように
	// してほしい」というフィードバックへの対応)。ECGColor::Noneを指定すると
	// 無色カードだけに絞り込める(TypeFilterと同じくAND条件で併用可能)。
	TOptional<ECGColor> ColorFilter;
	ECGCardSortMode SortMode = ECGCardSortMode::Cost;
};

namespace CGCardFilterSort
{
	// AllCardsにState(検索文字列/種別フィルタ/並び替え)を適用した一覧を返す。
	// デッキ構築画面(UCGDeckBuilderHUD)とカード図鑑(UCGLobbyHUD)の両方から使う
	// 共通ロジックで、両画面で挙動が食い違わないようにするためにここへ集約している。
	TArray<FCGCardDef> BuildFilteredSortedList(const TArray<FCGCardDef>& AllCards, const FCGCardFilterSortState& State);
}
