#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CGTypes.h"
#include "CGAIOpponent.generated.h"

class ACGGameMode;
class ACGGameState;
class ACGPlayerState;

// 対戦相手側(AI制御)の意思決定ロジック。以前は進行管理を担う ACGGameMode に
// RunAITurn() として同居していたが、AIをもっと賢くする/難易度を分けるといった
// 将来の拡張時にGameMode本体を肥大化させないよう、専用クラスへ切り出している
// (docs/architecture.md「対戦ロジックのクラス責務」)。
// GameModeの公開APIのみを呼んで進行させるため、進行ルール自体はGameMode側に残る。
UCLASS(BlueprintType)
class UCGAIOpponent : public UObject
{
	GENERATED_BODY()

public:
	// 手番がSideIndexに回ってきた直後に呼ぶ。購入→プレイ→攻撃→EndTurnまでを
	// 1回の呼び出し内で同期的に完結させる(HUDのRefreshUI()が呼ばれる頃には
	// 手番は必ず人間側に戻っている)。
	UFUNCTION(BlueprintCallable, Category = "CardGame|AI")
	void RunTurn(ACGGameMode* GameMode, int32 SideIndex);

	// 選択式カード効果(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)で、選ぶ側がAIのときに
	// 呼ぶ判断関数群。プレイヤー向けのUIを経由せず、その場で結果を返して即座に
	// 選択を解決するために使う(ACGGameMode::AutoResolveChoiceIfAI参照)。
	// 手札から捨てるカードを選ぶ: 一番コストが高い(=今後も使いにくい)カードを選ぶ。
	static FName ChooseHandCardToDiscard(const ACGPlayerState& Self);

	// 墓地から選ぶ(山札下へ送る/手札に戻す等): Choiceの絞り込み条件を満たす中で
	// 一番コストが高い(=価値が高い)カードを選ぶ。
	static FName ChooseGraveyardCard(const ACGPlayerState& Self, const FCGPendingChoice& Choice);

	// 攻撃対象を選ぶ(docs/architecture.md「選択待ち(PendingChoice)の仕組み」):
	// 打点で倒せて、かつ返り討ちに遭わない相手を優先する。無ければ顔面(-1)を返す。
	static int32 ChooseAttackTarget(const ACGPlayerState& Attacker, const ACGPlayerState& Defender, int32 AttackerUnitIndex);

	// カード効果由来のダメージ対象を選ぶ: 打点で倒せる敵ユニットのうち一番HPが低い
	// ものを優先する。無ければ顔面(-1)を返す。
	static int32 ChooseDamageTarget(const ACGPlayerState& Opponent, int32 Damage);

	// 山札の一番上を見て、上に残すか下に送るかを選ぶ(docs/card-effect-player-
	// choice-plan.md「④山札の上を見て上下を選ぶ」)。今すぐ(+1マナ後まで)
	// 払えなさそうな高コストカードなら下へ送る、という従来の簡易ヒューリスティックを踏襲する。
	static bool ChooseKeepOnTop(const ACGPlayerState& Self, FName RevealedCardId);

	// マーケットから選ぶ(docs/architecture.md「選択待ち(PendingChoice)の仕組み」):
	// Choiceの絞り込み条件を満たす中で一番コストが高い(=価値が高い)
	// カードを選ぶ。
	static FName ChooseMarketCard(const TArray<FName>& MarketCardIds, const FCGPendingChoice& Choice);

	// 購入したカードを手札に入れるか、山札の一番下に送るかを選ぶ。手札に十分
	// 空きがあるうちはすぐ使える手札を優先し、手札が埋まってきたら(8枚以上)
	// すぐには払えなさそうな高コストカードだけ山札下へ送って後で引く
	// (ChooseKeepOnTopと同じヒューリスティックを流用)。手札上限(10枚)なら送るしかない。
	static bool ChooseBuyDestination(const ACGPlayerState& Self, FName CardId);

	// EnemyUnitTarget選択待ち(封印(青)、または対象指定の敵単体デバフB02/B04、
	// 次期ルール)の対象を選ぶ: Choiceの絞り込み条件(コスト上限、封印のみ)を
	// 満たす中で一番コストが高い(=一番脅威度が高い)ユニットを選ぶ
	// (ChooseGraveyardCard/ChooseMarketCardと同じヒューリスティック)。
	static int32 ChooseEnemyUnitTarget(const ACGPlayerState& Opponent, const FCGPendingChoice& Choice);

	// 対象指定の味方強化(次期ルール)対象を選ぶ: 現在Atkが一番高いユニットを
	// 選ぶ(攻めを伸ばす方針。場にユニットが無ければ-1)。
	static int32 ChooseAllyBuffTarget(const ACGPlayerState& Self);

	// P16(仮称、強制変貌)の対象を選ぶ: 変貌可能な(ACGPlayerState::CanUnitTransform
	// がtrueを返す)Unitの中から、コストが一番高いものを選ぶ(変貌先の方が
	// 基本ステータスが高い傾向にあるため、恩恵の大きい方を優先する方針。
	// 候補が無ければ-1)。
	static int32 ChooseAllyTransformTarget(const ACGPlayerState& Self);

	// 補充し直すマーケットの枠を選ぶ(橙O04市場の噂話、次期ルール):
	// 一番コストが低い(=価値が低い)枠を選び直す(空枠は最優先で選ぶ)。
	static int32 ChooseMarketSlotToReroll(const ACGGameState& CGState);
};
