#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CGPlayerController.generated.h"

// オンライン対戦での「1接続」を表すPlayerController(docs/online-play-design.md
// 「クラス設計」)。カードプレイ等のアクション要求をServer RPCとしてサーバーへ
// 送る窓口。サーバー側の実処理は既存の`ACGGameMode`のメソッドをそのまま呼ぶだけで、
// ゲームロジック自体はここに書かない(重複実装しない)。
//
// オフライン(vs AI、スタンドアロン)でもこのクラスを経由する(呼び出し経路を
// 統一するため)。スタンドアロン/リッスンサーバーでは、自分が所有する
// PlayerControllerへのServer RPC呼び出しは同一プロセス内で即座に実行される
// ため、オフラインの挙動は変わらない。
UCLASS()
class ACGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	// 接続直後、自分のデッキ(UCGGameInstance::PlayerDeckCardIds)をサーバーへ
	// 送る(オンライン対戦のみ。オフラインは`ACGGameMode::InitializeMatch()`が
	// 直接GameInstanceを読むため呼ばれない)。25枚に満たない場合はサーバー側
	// (`ACGGameMode::SubmitDeckForSide`)でスターターデッキにフォールバックする。
	UFUNCTION(Server, Reliable)
	void ServerSubmitDeck(const TArray<FName>& DeckCardIds);

	// 以下、docs/online-play-design.md「アクションRPC一覧」の10個に対応する
	// Server RPC。実装は対応する`ACGGameMode`のメソッドを呼ぶだけ。SideIndexは
	// 常にこのPlayerControllerが持つ`ACGPlayerState::SideIndex`から求め、
	// クライアントからは受け取らない(なりすまし対策、docs/online-play-design.md
	// 「セキュリティ上の注意点」)。

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPlayCard(FName CardId, int32 TargetUnitIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestBuyCard(int32 MarketSlotIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestAttack(int32 AttackerUnitIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestEndTurn();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResolveChoiceWithCard(FName ChosenCardId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResolveChoiceWithTarget(int32 ChosenUnitIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResolveChoiceKeepOrBury(bool bKeepOnTop);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResolveChoiceBuyDestination(bool bToHand);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResolveChoiceSealTarget(int32 ChosenUnitIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResolveChoiceAllyTarget(int32 ChosenUnitIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerResolveChoiceMarketSlotTarget(int32 ChosenSlotIndex);

protected:
	// `ACGGameMode::SetupHUD()`相当をこちらへ統一する(サーバー側からはリモートの
	// クライアントの画面へウィジェットを作れないため。docs/online-play-design.md
	// 「フェーズ1」参照)。オフライン(スタンドアロン)では`ACGGameMode::BeginPlay()`
	// 側が引き続きHUDを作るため、ここでは何もしない。
	void SetupHUDIfOnline();

	FTimerHandle HUDSetupTimerHandle;
};
