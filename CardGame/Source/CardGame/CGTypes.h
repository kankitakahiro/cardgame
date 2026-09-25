#pragma once

#include "CoreMinimal.h"
#include "CGTypes.generated.h"

// カード種別。docs/game-rules-minimum.md の「まず実装するカード種」に対応。
// Permanent(永続)は次期ルール(docs/next-ruleset-design.md)で追加した種別で、
// 場に残るがユニット枠を使わず攻撃も被攻撃もしない継続効果カード。
UENUM(BlueprintType)
enum class ECGCardType : uint8
{
	Unit,
	Spell,
	Permanent,
};

// 保存されたデッキ1つ分(名前+カードID一覧)。複数デッキを保存して一覧から
// 選べるようにするため、UCGGameInstance::SavedDecks/UCGDeckSaveGameで使う
// (docs/architecture.md「デッキの永続化」参照)。
USTRUCT(BlueprintType)
struct FCGSavedDeck
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString DeckName;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<FName> CardIds;
};

// カードの色(次期ルール、docs/next-ruleset-design.md「色システム」)。Noneは
// 現行の無色カード(旧24種)や、色を持たない特殊カードのための値。
UENUM(BlueprintType)
enum class ECGColor : uint8
{
	None,
	Red,     // 赤: 速攻/バーン
	Orange,  // 橙: 購入加速
	Green,   // 緑: 物量/守護/回復
	Blue,    // 青: 除去/追放
	Purple,  // 紫: 強化/変容
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

	// 次期ルール(docs/next-ruleset-design.md)の色。旧24種の無色カードはNoneのまま。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	ECGColor Color = ECGColor::None;

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

	// カンマ区切りのキーワード群。"Haste"(速攻) / "Guard"(守護)に加え、次期ルールで
	// "Seal"(封印) / "Transform"(変貌) / "Discount"(先物)を追加
	// (docs/next-ruleset-design.md、docs/game-rules-minimum.md)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString Tags;

	bool HasTag(const FString& Tag) const
	{
		TArray<FString> Parts;
		Tags.ParseIntoArray(Parts, TEXT(","), true);
		return Parts.Contains(Tag);
	}

	// 変貌(Transformタグ)を持つカードのみ意味を持つ。TransformTargetCardIdが
	// NAME_Noneなら変貌能力を持たない。TransformConditionId/Valueの組み合わせは
	// CGTransformConditionId(下記)を参照。変貌先カード自身はこれらを空のままにする
	// (変貌先はさらに変貌しない。docs/next-ruleset-cards-v1.md「変貌先カード」)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName TransformTargetCardId;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName TransformConditionId;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 TransformConditionValue = 0;

	// 分身(Cloneタグ)を持つカードのみ意味を持つ。CloneConditionIdがNAME_Noneなら
	// 分身能力を持たない。条件の意味はCGCloneConditionId(下記)を参照。分身の
	// コピー先は常に自分自身(CardId)のため、変貌と違いターゲットカードIDは不要。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName CloneConditionId;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CloneConditionValue = 0;
};

// FCGCardDef::TransformConditionId に入る値の一覧(docs/next-ruleset-cards-v1.md
// 「変貌先カード」参照)。CGEffectIdと同じ考え方で、判定ロジックとカードデータの
// 両方がこの定数を参照する。
namespace CGTransformConditionId
{
	// 自分のターンをTransformConditionValue回、場にいたまま経過すると変貌
	// (P01無垢の使い魔=3、P08静かなる怪物=3)。
	inline constexpr const TCHAR* TurnsInPlay = TEXT("TurnsInPlay");

	// 攻撃を受けて生き残った回数がTransformConditionValueに達すると変貌
	// (P03若き見習い=2)。
	inline constexpr const TCHAR* SurvivedAttacks = TEXT("SurvivedAttacks");

	// 自分の手札がTransformConditionValue枚以下になった瞬間に変貌
	// (P06封じられし魔物=4)。
	inline constexpr const TCHAR* HandSizeAtMost = TEXT("HandSizeAtMost");

	// このユニットが場に出てから、自分の味方Unitの死亡数がTransformConditionValueに
	// 達すると変貌(P09眠れる怒り=3)。
	inline constexpr const TCHAR* AlliesDiedSinceSummon = TEXT("AlliesDiedSinceSummon");

	// 同じターン中に味方の変貌がTransformConditionValue回成功していれば、
	// 即座に変貌(P12胎動する秘術=2)。
	inline constexpr const TCHAR* TransformsThisTurnCount = TEXT("TransformsThisTurnCount");

	// 自分のターン開始時、TransformConditionValue%の確率で変貌(P15不完全な神=50)。
	inline constexpr const TCHAR* RandomChancePerTurn = TEXT("RandomChancePerTurn");

	// 対象指定の味方強化(AllyUnitTarget)の対象に選ばれた回数がTransformConditionValueに
	// 達すると変貌(P03若き見習い=2、docs/next-ruleset-cards-v1.md「カードの効果を
	// 書き直しました」対応)。ACGGameMode::ResolvePendingChoiceAllyTargetで進行度を+1する。
	inline constexpr const TCHAR* BuffedCount = TEXT("BuffedCount");
}

// FCGCardDef::CloneConditionId に入る値の一覧(docs/next-ruleset-cards-v1.md「緑」)。
// CGTransformConditionIdと同じ考え方で、判定ロジックとカードデータの両方がこの
// 定数を参照する。
namespace CGCloneConditionId
{
	// 場の味方Unit数(自分自身を含む)がCloneConditionValue以上になった瞬間に分身
	// (G01若木の番人=2、G06聖樹の守護者=2、G09巨石の壁役=3、G15万象の守り神=4)。
	inline constexpr const TCHAR* AllyUnitCountAtLeast = TEXT("AllyUnitCountAtLeast");

	// このユニットが(防御側として)攻撃を受けて生き残ると分身(G03熊の盾持ち)。
	inline constexpr const TCHAR* SurvivedAttack = TEXT("SurvivedAttack");

	// このユニットが(攻撃側として)攻撃して生き残ると分身(G13森の巨人)。
	inline constexpr const TCHAR* AttackedAndSurvived = TEXT("AttackedAndSurvived");

	// 味方リーダーが回復すると分身(G12不屈の大樹)。
	inline constexpr const TCHAR* LeaderHealed = TEXT("LeaderHealed");

	// 自分のリーダーの体力がCloneConditionValue以下になった瞬間に分身
	// (G07猛る大鹿=10)。
	inline constexpr const TCHAR* LeaderHpAtMost = TEXT("LeaderHpAtMost");
}

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

	// 変貌済みなら変貌前のCardIdを保持し、CardIdには変貌先を入れる(表示・判定は
	// 常にCardIdを見ればよい)。空(NAME_None)なら変貌していない。場を離れる際は
	// これを見てCardIdを変貌前へ戻す(docs/next-ruleset-design.md「変貌の詳細ルール」)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName OriginalCardId;

	// 変貌条件の進行度を数える汎用カウンタ。解釈はCardDef.TransformConditionIdに
	// よって変わる(経過ターン数/生存回数/味方死亡数など。docs/next-ruleset-cards-v1.md
	// 「変貌先カード」参照)。変貌条件を持たないユニットでは未使用。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 TransformProgress = 0;

	// 分身条件の進行度を数える汎用カウンタ。TransformProgressと同じ考え方で、
	// 解釈はCardDef.CloneConditionIdによって変わる(docs/next-ruleset-cards-v1.md「緑」)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CloneProgress = 0;

	// 分身が発動済みか(このユニット1体につき生涯で1回だけ)。分身で生まれた
	// コピー自身もtrueで生成し、コピーがさらに分身することを防ぐ
	// (docs/next-ruleset-cards-v1.md「緑」)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bHasCloned = false;
};

// 共通マーケットの1枠(次期ルール、docs/next-ruleset-design.md「マーケット」)。
// 各枠は出どころ(元々並べた側)を保持し、購入されたら常にその出どころの山札
// から補充する(買った本人の山札からではない。自滅スパイラル対策として決定)。
// 両者の山札に同じCardIdのカードが同時に並ぶことがあり得るため、購入操作は
// CardIdではなく枠のインデックスで行う(ACGGameMode::RequestBuyCard参照)。
USTRUCT(BlueprintType)
struct FCGMarketSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName CardId;

	// 0または1。出どころの山札が尽きた場合、このカードが空(NAME_None)のまま
	// 補充されないことがある(ACGGameState::RefillMarketSlot参照)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 OriginSideIndex = -1;
};

// 攻撃演出(突進アニメーション・命中フラッシュ・ダメージ数値ポップアップ)のために、
// 直近の攻撃で何が起きたかをHUD側へ伝える。ACGGameState::AttackSequenceNumberが
// 変化したときだけHUDが演出を再生することで、同じ結果をRefreshUI()のたびに
// 繰り返し再生しないようにする(docs/architecture.md「カードUIの設計」)。
USTRUCT(BlueprintType)
struct FCGLastAttackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 AttackerSideIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 AttackerUnitIndex = -1;

	// 攻撃を受けた側の場インデックス。-1なら顔面(Unit以外)への攻撃。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 TargetUnitIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 DamageToTarget = 0;

	// 反撃ダメージ(対象がUnitのときだけ発生。顔面攻撃なら常に0)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CounterDamageToAttacker = 0;
};

// 行動ログ1件分(次期ルール)。「CPUと対戦したときに何をされたのか分からない」
// というフィードバックへの対応。AIのターンは`UCGAIOpponent::RunTurn()`が
// 購入→プレイ→攻撃→EndTurnまで一括で同期的に処理してしまうため、プレイヤーが
// 後から見返せるようテキストで記録しておく(docs/game-rules-minimum.md
// 「行動ログ」参照)。SideIndexは行動した側(0/1)。HUD側は自分のMySideIndexと
// 比較して「あなた」「相手」を判定して表示する(ActionLog自体には「あなたが」
// 等の視点依存の文言を含めない)。
USTRUCT(BlueprintType)
struct FCGActionLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 SideIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString Text;
};

// カードプレイ演出用。ACGGameMode::RequestPlayCard()のたびに内容が更新され、
// CardPlaySequenceNumberがインクリメントされる。HUD側はAttackSequenceNumberと
// 同じ仕組みでこの番号の変化を検知し、プレイされたカードの演出(Unitなら盤面の
// カードを点滅、Spellなら手札位置にテキストを表示)を1回だけ再生する。
USTRUCT(BlueprintType)
struct FCGLastCardPlayResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 SideIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName CardId;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bIsUnit = false;

	// Unitの場合、着地した盤面インデックス(常に末尾に追加されるため配列の最後尾)。
	// Spellの場合は使わない(-1)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 BoardIndex = -1;
};

// カード効果や攻撃で「プレイヤー(またはAI)が対象を選ぶ必要がある」ときの、
// 選択待ち状態の種類。専用モーダルは作らず、既に画面にある手札/盤面/マーケット/
// 一時墓地ビューアーのカードを直接クリックしてもらう方式にしているため、
// UCGGameHUD側はこの値を見てどの行のクリックを「選択」として扱うかを切り替える
// (docs/architecture.md「カード効果ディスパッチ」の選択式カード効果版)。
UENUM(BlueprintType)
enum class ECGChoiceType : uint8
{
	None,              // 選択待ちなし
	HandCard,          // 自分の手札から1枚選ぶ(捨てる用)
	GraveyardCard,     // 自分の墓地から1枚選ぶ
	EnemyOrFaceTarget, // 敵ユニットまたは敵リーダー(顔面)を1つ選ぶ(ダメージ/攻撃共通)
	KeepOrBury,        // 山札の一番上を見て、上に残すか下に送るかを選ぶ
	MarketCard,        // マーケットから1枚選ぶ
	BuyDestination,    // 購入したカードを手札に入れるか、山札の一番下に送るかを選ぶ

	// 封印(青)、または対象指定の敵単体デバフ(B02/B04)の対象となる敵ユニットを
	// 1体選ぶ。EffectId(SealSpell/SealOnPlay vs OnPlayDebuffTarget)でどちらの
	// 解決をするかを分岐する(ACGGameMode::ResolvePendingChoiceSealTarget)。
	// EnemyOrFaceTargetと異なり顔面は選べない(必ずユニットが対象。
	// docs/game-rules-minimum.md「青」)。
	EnemyUnitTarget,

	// 強化対象の味方ユニットを1体選ぶ(次期ルール、紫の対象指定バフ等)。
	// EnemyUnitTargetと違い自分の場から選ぶ(docs/next-ruleset-cards-v1.md参照)。
	AllyUnitTarget,

	// 補充し直すマーケットの枠を1つ選ぶ(橙、O04市場の噂話)。選ばれた枠の中身は
	// 出どころの山札の一番下へ戻り、同じ出どころから新たに補充される
	// (ACGGameState::RerollMarketSlot)。コストの上限は無く、6枠のどれでも選べる。
	MarketSlotTarget,
};

// 選択待ち状態そのもの。ChoiceType==Noneが「選択待ちなし」。どの効果を再開する
// ためのものかをEffectId(既存のCGEffectId定数、攻撃なら専用のCGEffectId::AttackTarget)
// で持ち、ACGGameMode::ResolvePendingChoice()がEffectIdで分岐して対応する処理を呼ぶ
// (既存のEffectIdベースのカード効果ディスパッチと同じ考え方を選択再開にも使う)。
USTRUCT(BlueprintType)
struct FCGPendingChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	ECGChoiceType ChoiceType = ECGChoiceType::None;

	// 選択する側。AIはこの状態に入らずその場で即決するため、実際にはほぼ常に
	// 人間側のインデックスになる(docs/architecture.md「選択待ち(PendingChoice)の仕組み」参照)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 SideIndex = -1;

	// 画面上部に出す説明文(例: 「捨てるカードを選んでください」)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FString PromptText;

	// この選択を経て再開する効果。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName EffectId;

	// 候補の絞り込み条件。使わない選択種別では既定値のまま無視される。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 MaxCost = -1; // -1 = 無条件

	// EnemyUnitTarget(封印)用: MaxCostを「コスト上限」ではなく「現在の攻撃力
	// (パワー)上限」として解釈する(B11/B13、docs/next-ruleset-cards-v1.md「青」の
	// 「パワー」表記への対応)。falseのとき(B01/B03/B06/B07/B09/B14)はこれまで通り
	// カード定義のコストで判定する。現在の攻撃力はデバフ等で変動した後の実際の値
	// (ACGPlayerState::BoardUnits[].Atk)をそのまま使うため、マイナスになっていても
	// 追加のクランプ無しで正しく大小比較できる。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bFilterByCurrentAtk = false;

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	bool bRequireSpell = false; // trueならSpellのみが候補(墓地選択で使用)

	// KeepOrBury用: 見せている山札の一番上のカード。BuyDestinationでも、行き先待ちの
	// 購入したカードを保持するために同じフィールドを流用している。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	FName RevealedCardId;

	// EnemyOrFaceTarget用。攻撃によるものならAttackerUnitIndexに攻撃中のユニットの
	// 場インデックスを設定する(-1ならカード効果によるダメージ対象選択)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 AttackerUnitIndex = -1;

	// EnemyOrFaceTarget用。カード効果由来のダメージ量(攻撃のときは無視され、
	// 攻撃側ユニットの現在Atkが使われる)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 PendingDamageAmount = 0;

	// AllyUnitTarget/EnemyUnitTarget共通。選んだユニットのAtk/Hpに加える増分
	// (次期ルール)。AllyUnitTargetでは紫の対象指定バフ系(docs/next-ruleset-
	// cards-v1.md)で正の値、EnemyUnitTargetでは青の対象指定デバフ系
	// (OnPlayDebuffTarget、B02)で負の値として使う。簡略化のためAtk/Hp
	// とも同じ増分にしている(元の設計で非対称なもの(例: +1/+2、-2/+0)も
	// 対称に丸めた。docs/architecture.md「次期ルール移行時の実装メモ」参照)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 PendingBuffAmount = 0;

	// MarketCard用: この選択を解決した後、続けて同じ種類の選択をあと何回
	// 繰り返すか(黄金の帝王(FIN_ORANGE)のように、複数枚を1枚ずつ選ばせる効果で
	// 使う。docs/next-ruleset-cards-v1.md「橙」)。0なら繰り返さない。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 RemainingRepeats = 0;

	bool IsActive() const { return ChoiceType != ECGChoiceType::None; }
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
	// EffectValue体の「見習い兵」トークンを場に出す(EffectValueを読むよう
	// 汎用化済み。C020見習い召集=2、G02癒しの若葉=2)。
	inline constexpr const TCHAR* SummonApprenticeTokens = TEXT("SummonApprenticeTokens");

	// 自分の場のUnit数だけダメージを、選んだ敵Unit1体に与える(G11(新)群れの猛攻。
	// トークン生成スペルが強すぎるというフィードバックへの対応で、大群の号令の
	// 差し替えとして追加。EnemyUnitTarget、顔面は選べない)。
	inline constexpr const TCHAR* OnPlayDamageTargetByAllyUnitCount = TEXT("OnPlayDamageTargetByAllyUnitCount");

	inline constexpr const TCHAR* ReturnGraveyardSpellSelfDamage1 = TEXT("ReturnGraveyardSpellSelfDamage1"); // C021 墓地再点火
	inline constexpr const TCHAR* BuyFromMarketCostUnder3ToHand = TEXT("BuyFromMarketCostUnder3ToHand");     // C022 市場調達
	inline constexpr const TCHAR* RandomEnemyDamage1x4 = TEXT("RandomEnemyDamage1x4");              // C023 連弾の雨
	inline constexpr const TCHAR* ConditionalDamage3or2 = TEXT("ConditionalDamage3or2");            // C024 逆転の号令

	// カード効果ではなく、通常攻撃のターゲット選択を再開するための内部マーカー
	// (FCGPendingChoice::EffectId用。docs/architecture.md「選択待ち(PendingChoice)の仕組み」参照)。
	inline constexpr const TCHAR* AttackTarget = TEXT("__AttackTarget");

	// 封印(青、次期ルール、docs/game-rules-minimum.md)。EffectValueをコスト上限
	// フィルタとして使う(-1なら無条件)。Spell版とUnit登場時版で分けている。
	inline constexpr const TCHAR* SealSpell = TEXT("SealSpell");                   // B01/B03/B06 相当
	inline constexpr const TCHAR* SealOnPlay = TEXT("SealOnPlay");                 // B07 相当(コスト上限で判定)

	// B11/B13: 対象の絞り込みをコストではなく「現在の攻撃力(パワー)」で行う版
	// (docs/next-ruleset-cards-v1.md「青」の「パワー」表記への対応)。EffectValueは
	// 攻撃力の上限フィルタとして使う。バフ/デバフ後の実際の値で判定するため、
	// マイナスになっていても正しく比較できる(FCGPendingChoice::bFilterByCurrentAtk参照)。
	inline constexpr const TCHAR* SealOnPlayByPower = TEXT("SealOnPlayByPower");   // B11/B13 相当

	// 先物(橙)キーワードそのものが持つ「登場時、次の購入のコストを1軽減」効果は、
	// 専用のEffectIdではなくFCGCardDef::HasTag("Discount")から直接
	// ACGPlayerState::PlayCardFromHandで付与する(O03/O06/O11/O14。
	// docs/next-ruleset-cards-v1.md「カードの効果を書き直しました」対応。
	// 複数の橙カードが同じ「次の購入+1軽減」を専用EffectId無しで共有できるようにした)。

	// コイン(5色共通パッシブと同じ資源)を直接増加させる系(次期ルール)。
	// EffectValue=増加量。Spell版(O01)とUnit登場時版(O09)で同じ定数を共有する
	// (OnPlayDamageFaceと同じ、Spell/Unitテーブル使い分けパターン)。
	inline constexpr const TCHAR* GrantPurchaseMana = TEXT("GrantPurchaseMana"); // O01/O09相当

	// マーケット操作系(橙、次期ルール、フェーズ4c)。
	// O04: 選んだ1枠を即座に補充し直す(Spell、対象コスト無制限)。
	inline constexpr const TCHAR* RerollMarketSlot = TEXT("RerollMarketSlot");
	// O11: ランダムな1枠を即座に補充し直す(Unit登場時。次の購入の割引はDiscount
	// タグから別途付与されるため、ここでは補充のみを行う)。
	inline constexpr const TCHAR* OnPlayRerollMarketRandom = TEXT("OnPlayRerollMarketRandom");
	// O15: マーケットから1枚、コストを支払わず場に直接出す(Unit登場時。
	// 手札へ加えるBuyFromMarketCostUnder3ToHandとは異なり、選んだカードをそのまま
	// Unitとして場に追加する。EffectValue=-1でコスト上限なし)。
	inline constexpr const TCHAR* OnPlayDeployFromMarketFree = TEXT("OnPlayDeployFromMarketFree");

	// P11: 登場時、次にプレイするUnit1体のコストをEffectValue軽減する
	// (次期ルール、フェーズ4c)。手札の同名重複カードのどの1枚かを区別する
	// 仕組みが無いため、「手札の特定カードに割引を付与する」という原設計を
	// 「次に自分がプレイするUnitを割引する」に簡略化している
	// (NextPurchaseDiscountと同じ考え方)。
	inline constexpr const TCHAR* OnPlayGrantNextUnitPlayDiscount = TEXT("OnPlayGrantNextUnitPlayDiscount");

	// 対象指定の味方強化(次期ルール、フェーズ4b)。EffectValueをAtk/Hp共通の
	// 増分として使う(非対称なものは対称に丸めている)。
	inline constexpr const TCHAR* BuffAllyTarget = TEXT("BuffAllyTarget");             // P02/P04/P07相当(Spell)
	inline constexpr const TCHAR* OnPlayBuffAllyTarget = TEXT("OnPlayBuffAllyTarget"); // P05相当(Unit登場時)
	// P10: 味方強化に加えて、選択解決後に1ドローする(ACGGameMode::ResolvePendingChoiceAllyTarget
	// でEffectIdを見て分岐する)。
	inline constexpr const TCHAR* BuffAllyTargetAndDraw = TEXT("BuffAllyTargetAndDraw");
	// P14: 登場時の味方強化に加えて、次にプレイするUnit1体のコストを1軽減する
	// (P11予見の魔導師と同じ簡略化方針、NextUnitPlayDiscountを流用)。
	inline constexpr const TCHAR* OnPlayBuffAllyTargetAndUnitDiscount = TEXT("OnPlayBuffAllyTargetAndUnitDiscount");
	// P16: 登場時、変貌条件を無視して味方Unit1体を選んで強制的に変貌させる
	// (ForceTransformAllAlliesの単体対象指定版)。
	inline constexpr const TCHAR* OnPlayForceTransformAllyTarget = TEXT("OnPlayForceTransformAllyTarget");

	// 場の味方Unit全体への効果(次期ルール、フェーズ4b)。
	inline constexpr const TCHAR* BuffAllAlliesFlat = TEXT("BuffAllAlliesFlat"); // G14相当(EffectValue分だけAtk/Hp共に増加、回復無し)
	inline constexpr const TCHAR* BuffAllAlliesAtkOnly = TEXT("BuffAllAlliesAtkOnly"); // G08相当(EffectValue分だけAtkのみ増加)
	// P13: 変貌条件を無視して、変貌可能な味方Unit全てを強制的に変貌させる。
	inline constexpr const TCHAR* ForceTransformAllAllies = TEXT("ForceTransformAllAllies");

	// 赤の直接ダメージ・トリガー系(次期ルール、フェーズ4b)。
	inline constexpr const TCHAR* OnPlayDamageFace = TEXT("OnPlayDamageFace");           // R03/R08/R15相当(選択不要、必ず顔面)
	inline constexpr const TCHAR* OnDeathDamageFace1 = TEXT("OnDeathDamageFace1");       // R06相当(自分自身の死亡時)
	// 自分自身の死亡時、敵のランダムなUnit1体にEffectValueダメージ(対象がいなければ
	// 何も起きない。R01相当。バニラのステータス効率が高すぎたため、ステータスを
	// 下げる代わりに持たせた。docs/next-ruleset-cards-v1.md「赤」参照)。
	inline constexpr const TCHAR* OnDeathDamageRandomEnemyUnit1 = TEXT("OnDeathDamageRandomEnemyUnit1");
	// 自分の他のUnitが死亡するたび(このユニット自身は場に残っている前提)敵リーダーへ
	// EffectValueダメージ(R12相当)。OnDeathDamageFace1と違い「死んだユニットの
	// ハンドラ」ではなく「生き残ったユニットのアウラ」として判定する
	// (ACGPlayerState::RemoveDeadUnitsAndGetDeathDrawCount内、AlliesDiedSinceSummon
	// と同じ生存者走査ループで扱う)。
	inline constexpr const TCHAR* OnAllyDeathDamageFace1 = TEXT("OnAllyDeathDamageFace1");
	inline constexpr const TCHAR* RandomEnemyDamage2x3 = TEXT("RandomEnemyDamage2x3");   // R11相当
	inline constexpr const TCHAR* DamageFaceByUnitCount = TEXT("DamageFaceByUnitCount"); // R14相当

	// 青のマス除去・単体デバフ(次期ルール、フェーズ4b)。
	inline constexpr const TCHAR* MassDebuffEnemies = TEXT("MassDebuffEnemies"); // B12相当

	// 対象指定の敵単体デバフ(Unit登場時。次期ルール、フェーズ4b)。EffectValueを
	// Atk/Hp共通の増分として使う(負の値。B02=-1。BuffAllyTargetの敵版で、
	// ECGChoiceType::EnemyUnitTargetを使う点だけが異なる)。
	inline constexpr const TCHAR* OnPlayDebuffTarget = TEXT("OnPlayDebuffTarget"); // B02相当

	// B04: 自分が封印を成功させるたびに敵リーダーへ1ダメージ(常在アウラ、
	// EffectIdテーブルには登録せずHasBoardUnitWithEffectで直接判定する)。
	// ACGPlayerState::NotifySealSucceededから、封印が成立した全ての箇所
	// (ACGGameMode::ResolvePendingChoiceSealTarget、B14のターン開始時封印)で呼ぶ。
	inline constexpr const TCHAR* OnSealDamageFace1 = TEXT("OnSealDamageFace1");

	// 常在アウラ: このユニットが場にいる間、自分が封印を成立させるたびに1ドロー
	// (B08 霧の壁。ドキュメントに設計だけあり未実装だった効果。docs/next-ruleset-
	// cards-v1.md「青」参照)。OnSealDamageFace1と同じくEffectIdテーブルには
	// 登録せず、ACGPlayerState::NotifySealSucceededから直接判定する。
	inline constexpr const TCHAR* OnSealDraw1 = TEXT("OnSealDraw1");

	// B09: 敵Unit1体を封印し、コインを1増加する(Spell版封印の亜種。
	// EffectValue=コスト上限、-1で無条件)。
	inline constexpr const TCHAR* SealSpellGrantPurchaseMana = TEXT("SealSpellGrantPurchaseMana");

	// B15: 封印(Spell)を2回連続で行う。1体目の封印成功後、
	// ACGGameMode::ResolvePendingChoiceSealTargetが自動で2体目の選択を開始する。
	inline constexpr const TCHAR* SealSpellTwo = TEXT("SealSpellTwo");

	// 自分のターン終了時、敵Unitの中で最もコストが高いものへEffectValue(負の値)分
	// Atk/Hpデバフを行う常在効果(B10相当)。
	inline constexpr const TCHAR* EndTurnDebuffHighestCostEnemy = TEXT("EndTurnDebuffHighestCostEnemy");

	// 自分のターン開始時、敵Unitの中で最もコストが高いものを封印する常在効果
	// (B14相当。青パッシブの対象にもなる)。
	inline constexpr const TCHAR* OnTurnStartSealHighestCostEnemy = TEXT("OnTurnStartSealHighestCostEnemy");

	// 登場時、場に他の味方Unitが3体以上いれば自分自身をEffectValue分バフする
	// (Atk/Hp共通。G10相当)。
	inline constexpr const TCHAR* OnPlayBuffSelfIfAlliesPresent = TEXT("OnPlayBuffSelfIfAlliesPresent");

	// 登場時、場に他の味方Unitが3体以上いれば自分自身のAtkだけEffectValue分
	// 上げる(Hpは変化しない。G04森の猪。バニラのステータス効率が高すぎたため、
	// 素のステータスを下げる代わりに条件付きバフを持たせた)。
	inline constexpr const TCHAR* OnPlaySelfBuffAtkIfAlliesPresent = TEXT("OnPlaySelfBuffAtkIfAlliesPresent");

	// 緑: このユニットが攻撃(防御側として)を受けるたびに味方リーダーを1回復する
	// 常在アウラ(G12相当。EffectIdテーブルには登録せず、ACGGameMode::ExecuteAttack内で
	// 防御側ユニットのDefを直接見て判定する)。
	inline constexpr const TCHAR* OnDefendHealSelf1 = TEXT("OnDefendHealSelf1");

	// 緑: 見習い兵よりHPが高い1/2トークンをEffectValue体出す(G05相当。
	// SummonApprenticeTokensの1/1版と別トークンを使う)。
	inline constexpr const TCHAR* SummonToughApprenticeTokens = TEXT("SummonToughApprenticeTokens");

	// 橙の購入トリガー系(次期ルール、フェーズ4b)。EffectValueは効果量として使う。
	inline constexpr const TCHAR* OnBuyBuffSelfAtk = TEXT("OnBuyBuffSelfAtk");                       // O05相当(常在)
	inline constexpr const TCHAR* OnBuyBuffSelfHp = TEXT("OnBuyBuffSelfHp");                         // O14相当(常在)
	inline constexpr const TCHAR* OnBuyEndTurnGrantPurchaseMana = TEXT("OnBuyEndTurnGrantPurchaseMana"); // O12相当(常在、ターン終了時判定)
	// O10: このターン中、購入するたびに場のUnit全てが+1/+0される(Spell)。
	inline constexpr const TCHAR* GrantBoardBuffOnPurchaseThisTurn = TEXT("GrantBoardBuffOnPurchaseThisTurn");
	inline constexpr const TCHAR* GrantAllPurchasesDiscountThisTurn = TEXT("GrantAllPurchasesDiscountThisTurn"); // O13相当(Spell)

	// 自分のターン開始時に判定する常在効果(次期ルール、フェーズ4b)。
	// ACGPlayerState::ApplyOnTurnStartAuraEffects()が場のUnitを走査して判定する。
	inline constexpr const TCHAR* OnTurnStartDraw = TEXT("OnTurnStartDraw");                       // B05相当
	inline constexpr const TCHAR* OnTurnStartGrantPurchaseMana = TEXT("OnTurnStartGrantPurchaseMana"); // O08相当(コインを直接増加)

	// フィニッシャー(各色固有の条件を達成すると手札・コストを介さず自動で場に
	// 駆けつける専用Unit。UCGCardDatabase::GetFinisherCardId/BuildFinisherCards、
	// ACGPlayerState::CheckAndSpawnFinisher参照。「各色で条件を達成するとフィニッシャーが
	// 駆けつける」というフィードバックへの対応)。赤フィニッシャーは疾駆持ちの
	// ステータスのみで専用効果を持たないためEffectId=Noneのまま。
	inline constexpr const TCHAR* OnPlayFreeMarketCards = TEXT("OnPlayFreeMarketCards");       // 橙フィニッシャー(EffectValue=枚数、コスト無視でプレイヤーが1枚ずつ選んで場に出す)
	inline constexpr const TCHAR* OnPlayExileAllEnemyUnits = TEXT("OnPlayExileAllEnemyUnits"); // 青フィニッシャー(登場時、敵の場を全て追放)
	inline constexpr const TCHAR* OnPlayBuffAllAlliesFlat = TEXT("OnPlayBuffAllAlliesFlat");   // 緑フィニッシャー(登場時、EffectValue分だけ味方全体Atk/Hp増加)
	// 紫フィニッシャー: 登場時効果ではなく常在アウラ(このユニットが場にいる間、
	// 新たに登場した味方Unitは変貌条件を無視して即座に変貌する)。他の常在アウラ
	// (OnSealDamageFace1等)と同じくGetUnitOnPlayEffectHandlers()には登録せず、
	// ACGPlayerState::HasBoardUnitWithEffectで直接判定する。
	inline constexpr const TCHAR* FinisherTransformAura = TEXT("FinisherTransformAura");
}
