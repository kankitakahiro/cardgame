#include "CGGameMode.h"
#include "CGGameState.h"
#include "CGPlayerState.h"
#include "CGCardDatabase.h"
#include "CGGameHUD.h"
#include "CGAIOpponent.h"
#include "CGGameInstance.h"
#include "CGPlayerController.h"
#include "CardGame.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	// Side 0が人間、Side 1が常にAI。一人二役をなくすため、対戦相手は自動で行動する。
	constexpr int32 AISideIndex = 1;
}

ACGGameMode::ACGGameMode()
{
	// GameStateClass / PlayerStateClass は、Blueprint再生成スクリプト側で
	// BP_CG_GameState / BP_CG_PlayerState を指すよう上書き設定される。
	GameStateClass = ACGGameState::StaticClass();
	PlayerStateClass = ACGPlayerState::StaticClass();
	// オンライン対戦のアクション要求(Server RPC)の窓口(docs/online-play-design.md
	// 「クラス設計」)。オフラインでも同じクラスを使う(Server RPCはスタンドアロンでは
	// 同一プロセス内で即実行されるだけなので、挙動は変わらない)。
	PlayerControllerClass = ACGPlayerController::StaticClass();

	// 3Dの操作対象を持たないUI主体のカードゲームのため、デフォルトPawnは不要。
	DefaultPawnClass = nullptr;
}

void ACGGameMode::BeginPlay()
{
	Super::BeginPlay();

	AIOpponent = NewObject<UCGAIOpponent>(this);

	// バランス検証用のheadlessシミュレーション(コマンドライン`-SimulateMatches=N`)。
	// 通常の対戦フローとHUDは行わず、結果をLogCardGameへ出力して終わる。
	int32 NumSimulatedMatches = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("SimulateMatches="), NumSimulatedMatches) && NumSimulatedMatches > 0)
	{
		RunSelfPlaySimulation(NumSimulatedMatches);
		return;
	}

	// バランス検証用のheadlessデッキ最適化(コマンドライン`-OptimizeDecks=N`)。
	// Nは色ごとの試行回数(IterationsPerColor)。`-OptimizeDeckMatches=N`と
	// `-OptimizeDeckRounds=N`で評価1回あたりの対戦数・色を巡回する周回数を
	// 上書きできる(RunDeckOptimization()のコメント参照)。
	int32 OptimizeDeckIterations = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("OptimizeDecks="), OptimizeDeckIterations) && OptimizeDeckIterations > 0)
	{
		int32 MatchesPerEvaluation = 200;
		FParse::Value(FCommandLine::Get(), TEXT("OptimizeDeckMatches="), MatchesPerEvaluation);
		int32 Rounds = 3;
		FParse::Value(FCommandLine::Get(), TEXT("OptimizeDeckRounds="), Rounds);
		RunDeckOptimization(OptimizeDeckIterations, MatchesPerEvaluation, Rounds);
		return;
	}

	// オンライン対戦(リッスンサーバー/クライアント)の場合は、PostLogin()で
	// 両陣営分の接続とデッキ提出が揃ってからInitializeOnlineMatch()で試合を
	// 開始する(docs/online-play-design.md「全体アーキテクチャ」)。HUDも
	// ACGPlayerController側で各クライアントごとに作る。オフライン
	// (スタンドアロン、vs AI)は従来どおりここで即座に初期化する。
	if (GetNetMode() != NM_Standalone)
	{
		return;
	}

	InitializeMatch();

	// BeginPlay時点だとローカルプレイヤーのビューポートがまだ準備できておらず
	// AddToViewport()が画面に反映されないことがあるため、1フレーム遅延させる。
	GetWorldTimerManager().SetTimerForNextTick(this, &ACGGameMode::SetupHUD);
}

void ACGGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// オフライン(スタンドアロン)ではBeginPlayが直接両陣営をSpawnActorするため、
	// この標準の参加フローは使わない(docs/online-play-design.md「SideIndexの
	// 割り当て」)。
	if (GetNetMode() == NM_Standalone)
	{
		return;
	}

	bOnlineMatch = true;

	ACGGameState* CGState = GetCGGameState();
	ACGPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ACGPlayerState>() : nullptr;
	if (!CGState || !PS)
	{
		UE_LOG(LogCardGame, Error, TEXT("PostLogin: missing CGState or PlayerState for %s"),
			NewPlayer ? *NewPlayer->GetName() : TEXT("null"));
		return;
	}

	// 最初の接続(ホスト自身)がSide0、2人目(ゲスト)がSide1になる
	// (docs/online-play-design.md「SideIndexの割り当て」)。
	const int32 SideIndex = NumOnlineConnections;
	++NumOnlineConnections;

	CGState->Sides.SetNum(2);
	if (!CGState->Sides.IsValidIndex(SideIndex))
	{
		UE_LOG(LogCardGame, Error, TEXT("PostLogin: too many connections (SideIndex=%d)"), SideIndex);
		return;
	}

	PS->SideIndex = SideIndex;
	PS->MaxHP = 20;
	PS->CurrentHP = 20;
	PS->MaxMana = 0;
	PS->CurrentMana = 0;
	PS->bIsDefeated = false;
	CGState->Sides[SideIndex] = PS;

	UE_LOG(LogCardGame, Log, TEXT("PostLogin: assigned SideIndex=%d to %s"), SideIndex, *NewPlayer->GetName());
}

void ACGGameMode::Logout(AController* Exiting)
{
	// オフラインではSpawnActorで両陣営を自前生成しているため、この経路は
	// 関係ない(docs/online-play-plan.md「フェーズ4」参照)。
	if (bOnlineMatch)
	{
		ACGGameState* CGState = GetCGGameState();
		ACGPlayerState* PS = Exiting ? Exiting->GetPlayerState<ACGPlayerState>() : nullptr;
		if (CGState && PS && CGState->Sides.IsValidIndex(PS->SideIndex) && CGState->WinnerPlayerIndex == -1)
		{
			// 切断していない側の勝ちとして扱う(それ以上続行しようが無いため)。
			// 通常の決着と区別できるようbOpponentDisconnectedを立てる。
			const int32 RemainingSideIndex = PS->SideIndex == 0 ? 1 : 0;
			CGState->WinnerPlayerIndex = RemainingSideIndex;
			CGState->bOpponentDisconnected = true;
			NotifyStateChanged();
			UE_LOG(LogCardGame, Log, TEXT("Logout: Side=%d disconnected, Side=%d wins by forfeit"),
				PS->SideIndex, RemainingSideIndex);
		}
	}

	Super::Logout(Exiting);
}

void ACGGameMode::SubmitDeckForSide(int32 SideIndex, const TArray<FName>& DeckCardIds)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->Sides.IsValidIndex(SideIndex) || !CGState->Sides[SideIndex])
	{
		UE_LOG(LogCardGame, Error, TEXT("SubmitDeckForSide: invalid SideIndex=%d"), SideIndex);
		return;
	}
	if (PendingOnlineDecks.Contains(SideIndex))
	{
		// 二重送信は無視(BeginPlayが複数回呼ばれた場合等の対策)。
		return;
	}

	// 25枚に満たない/不正なデッキはスターターデッキへフォールバックする
	// (オフラインのInitializeMatch()と同じ方針、docs/architecture.md
	// 「デッキの永続化」)。
	const bool bValidDeck = DeckCardIds.Num() == 25;
	PendingOnlineDecks.Add(SideIndex, bValidDeck ? DeckCardIds : UCGCardDatabase::GetStarterDeckCardIds());

	UE_LOG(LogCardGame, Log, TEXT("SubmitDeckForSide: SideIndex=%d DeckValid=%d Submitted=%d/2"),
		SideIndex, bValidDeck ? 1 : 0, PendingOnlineDecks.Num());

	if (PendingOnlineDecks.Num() < 2)
	{
		return;
	}

	InitializeOnlineMatch();
}

void ACGGameMode::InitializeOnlineMatch()
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->Sides.IsValidIndex(0) || !CGState->Sides.IsValidIndex(1)
		|| !CGState->Sides[0] || !CGState->Sides[1])
	{
		UE_LOG(LogCardGame, Error, TEXT("InitializeOnlineMatch: Sides not ready"));
		return;
	}

	// 先攻後攻はランダム(docs/next-ruleset-design.md)。後手は+1ドロー+コイン
	// +1のハンデを受ける(InitializeMatch()と同じ方針。3000戦シミュレーションで
	// +1ドローのみでは先攻73.9%勝と致命的に偏ることが判明したため、コインも
	// 追加した。docs/next-ruleset-simulation-v1.md「第12回」参照)。
	const int32 FirstPlayerIndex = FMath::RandBool() ? 0 : 1;
	for (int32 i = 0; i < 2; ++i)
	{
		ACGPlayerState* Side = CGState->Sides[i];
		const TArray<FName>* Deck = PendingOnlineDecks.Find(i);
		Side->InitializeStartingDeck(Deck ? *Deck : UCGCardDatabase::GetStarterDeckCardIds());

		for (int32 d = 0; d < 5; ++d)
		{
			Side->DrawCard();
		}
		if (i != FirstPlayerIndex)
		{
			Side->DrawCard();
			Side->PurchaseMana += 1;
			Side->PendingBonusMana = 1; // 後手の最初の1ターンだけマナ+1(Hearthstoneの「コイン」相当)。
			Side->bWentSecond = true;
		}
	}

	// マーケット初期化(両者の山札トップから3枚ずつ、計6枚。InitializeMatch()と同じ)。
	CGState->InitializeMarket();

	CGState->TurnCount = 0;
	CGState->WinnerPlayerIndex = -1;
	CGState->bOpponentDisconnected = false;
	CGState->CurrentTurnPlayerIndex = FirstPlayerIndex;

	UE_LOG(LogCardGame, Log, TEXT("InitializeOnlineMatch: Sides=2 Side0 HP=%d Hand=%d Deck=%d / Side1 HP=%d Hand=%d Deck=%d FirstPlayer=%d"),
		CGState->Sides[0]->CurrentHP, CGState->Sides[0]->HandCardIds.Num(), CGState->Sides[0]->DeckCardIds.Num(),
		CGState->Sides[1]->CurrentHP, CGState->Sides[1]->HandCardIds.Num(), CGState->Sides[1]->DeckCardIds.Num(),
		CGState->CurrentTurnPlayerIndex);

	StartTurn();
}

void ACGGameMode::SetupHUD()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		UE_LOG(LogCardGame, Error, TEXT("SetupHUD: No PlayerController(0) found"));
		return;
	}

	UCGGameHUD* HUD = CreateWidget<UCGGameHUD>(PC, UCGGameHUD::StaticClass());
	if (!HUD)
	{
		UE_LOG(LogCardGame, Error, TEXT("SetupHUD: CreateWidget<UCGGameHUD> failed"));
		return;
	}

	HUD->AddToViewport();
	PC->bShowMouseCursor = true;
	PC->SetInputMode(FInputModeUIOnly());
	UE_LOG(LogCardGame, Log, TEXT("SetupHUD: HUD created and added to viewport for PC=%s IsInViewport=%d"),
		*PC->GetName(), HUD->IsInViewport() ? 1 : 0);
}

ACGGameState* ACGGameMode::GetCGGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ACGGameState>() : nullptr;
}

void ACGGameMode::InitializeMatch()
{
	UWorld* World = GetWorld();
	ACGGameState* CGState = GetCGGameState();
	if (!World || !CGState)
	{
		return;
	}

	CGState->Sides.Reset();
	const TArray<FName> Starter = UCGCardDatabase::GetStarterDeckCardIds();

	// Side0(人間)はデッキ構築画面で選んだデッキ(GameInstanceに保持)を使う。
	// デッキ構築を経ずにこのレベルへ直接PIEした場合など、GameInstanceが無い/
	// デッキが不正(25枚固定ルールに反する)な場合は現行のスターターデッキへ
	// フォールバックする(docs/architecture.md「デッキの永続化」)。
	UCGGameInstance* CGGameInstance = GetGameInstance<UCGGameInstance>();

	// AI(Side1)は色ごとの基本デッキ(各色15種類中5種類を3枚+残り10種類を1枚の
	// 純色構成、しきい値17/25を満たす)を使う。ロビーの「対戦相手デッキ」画面で
	// 選んだ色があればそれを使い(`UCGGameInstance::SelectedAIOpponentColor`、
	// 「CPUと対戦するときに相手のデッキを選べるようにしてほしい」という
	// フィードバックへの対応)、未選択(`ECGColor::None`、既定値)なら以前と同じく
	// 5色からランダムに1色選ぶ。人間側がデッキ未構築のときのフォールバックは
	// 「色の強さを検証する」目的ではないため、引き続き複数色混在のStarterのまま。
	static constexpr ECGColor BasicDeckColors[] = {
		ECGColor::Red, ECGColor::Orange, ECGColor::Green, ECGColor::Blue, ECGColor::Purple
	};
	const ECGColor SelectedAIColor = CGGameInstance ? CGGameInstance->SelectedAIOpponentColor : ECGColor::None;
	const ECGColor AIDeckColor = (SelectedAIColor != ECGColor::None)
		? SelectedAIColor
		: BasicDeckColors[FMath::RandRange(0, UE_ARRAY_COUNT(BasicDeckColors) - 1)];
	const TArray<FName> AIDeck = UCGCardDatabase::GetBasicColorDeckCardIds(AIDeckColor);

	const TSubclassOf<APlayerState> SideClass = PlayerStateClass ? *PlayerStateClass : ACGPlayerState::StaticClass();

	// 先攻後攻はランダム(docs/next-ruleset-design.md)。後手は+1ドロー+コイン+1の
	// ハンデを受ける(Hearthstoneの「コイン」相当だが、マーケットの早い者勝ち購入が
	// 毎ラウンド複利的に効くため、+1ドローだけでは先攻73.9%勝と致命的に偏ることが
	// 3000戦シミュレーションで判明し、コインも追加した。
	// docs/next-ruleset-simulation-v1.md「第12回」参照)。
	const int32 FirstPlayerIndex = FMath::RandBool() ? 0 : 1;

	for (int32 i = 0; i < 2; ++i)
	{
		ACGPlayerState* Side = World->SpawnActor<ACGPlayerState>(SideClass);
		if (!Side)
		{
			continue;
		}
		Side->SideIndex = i;
		Side->MaxHP = 20;
		Side->CurrentHP = 20;
		Side->MaxMana = 0;
		Side->CurrentMana = 0;
		Side->bIsDefeated = false;

		const bool bUsePlayerDeck = (i == 0) && CGGameInstance && CGGameInstance->PlayerDeckCardIds.Num() == 25;
		const TArray<FName>& DeckForThisSide = bUsePlayerDeck ? CGGameInstance->PlayerDeckCardIds : (i == 1 ? AIDeck : Starter);
		Side->InitializeStartingDeck(DeckForThisSide);

		for (int32 d = 0; d < 5; ++d)
		{
			Side->DrawCard();
		}
		if (i != FirstPlayerIndex)
		{
			Side->DrawCard();
			Side->PurchaseMana += 1;
			Side->PendingBonusMana = 1; // 後手の最初の1ターンだけマナ+1(Hearthstoneの「コイン」相当)。
			Side->bWentSecond = true;
		}
		CGState->Sides.Add(Side);
	}

	// マーケット初期化(両者の山札トップから3枚ずつ、計6枚。docs/next-ruleset-
	// design.md「マーケット」)。両者の初期手札ドローが終わった後、山札の
	// 残りトップから取るため、上記のドロー処理より後に呼ぶ必要がある。
	CGState->InitializeMarket();

	CGState->TurnCount = 0;
	CGState->WinnerPlayerIndex = -1;
	CGState->CurrentTurnPlayerIndex = FirstPlayerIndex;

	UE_LOG(LogCardGame, Log, TEXT("InitializeMatch: Sides=%d Side0 HP=%d Hand=%d Deck=%d / Side1(AI, Deck=%s) HP=%d Hand=%d Deck=%d / Market=%d FirstPlayer=%d"),
		CGState->Sides.Num(),
		CGState->Sides.IsValidIndex(0) ? CGState->Sides[0]->CurrentHP : -1,
		CGState->Sides.IsValidIndex(0) ? CGState->Sides[0]->HandCardIds.Num() : -1,
		CGState->Sides.IsValidIndex(0) ? CGState->Sides[0]->DeckCardIds.Num() : -1,
		*UEnum::GetValueAsString(AIDeckColor),
		CGState->Sides.IsValidIndex(1) ? CGState->Sides[1]->CurrentHP : -1,
		CGState->Sides.IsValidIndex(1) ? CGState->Sides[1]->HandCardIds.Num() : -1,
		CGState->Sides.IsValidIndex(1) ? CGState->Sides[1]->DeckCardIds.Num() : -1,
		CGState->MarketSlots.Num(),
		CGState->CurrentTurnPlayerIndex);

	StartTurn();
}

int32 ACGGameMode::PlaySimulatedMatch(const TArray<FName>& DeckSide0, const TArray<FName>& DeckSide1)
{
	UWorld* World = GetWorld();
	ACGGameState* CGState = GetCGGameState();
	if (!World || !CGState)
	{
		return 0;
	}
	const TSubclassOf<APlayerState> SideClass = PlayerStateClass ? *PlayerStateClass : ACGPlayerState::StaticClass();
	const TArray<FName>* Decks[2] = { &DeckSide0, &DeckSide1 };

	// 前回対戦分のPlayerStateを破棄してから作り直す(InitializeMatch()と
	// 同じ手順だが、こちらはSides.Reset()の前に明示的にDestroyする点が異なる。
	// 通常のInitializeMatch()は起動時に1回しか呼ばれないため、そこでは
	// 前回分の破棄を気にする必要が無かった)。
	for (ACGPlayerState* OldSide : CGState->Sides)
	{
		if (OldSide)
		{
			OldSide->Destroy();
		}
	}
	CGState->Sides.Reset();
	CGState->PendingChoice = FCGPendingChoice();

	const int32 FirstPlayerIndex = FMath::RandBool() ? 0 : 1;
	for (int32 i = 0; i < 2; ++i)
	{
		ACGPlayerState* Side = World->SpawnActor<ACGPlayerState>(SideClass);
		if (!Side)
		{
			continue;
		}
		Side->SideIndex = i;
		Side->MaxHP = 20;
		Side->CurrentHP = 20;
		Side->MaxMana = 0;
		Side->CurrentMana = 0;
		Side->bIsDefeated = false;
		Side->InitializeStartingDeck(*Decks[i]);
		for (int32 d = 0; d < 5; ++d)
		{
			Side->DrawCard();
		}
		if (i != FirstPlayerIndex)
		{
			Side->DrawCard();
			Side->PurchaseMana += 1;
			Side->PendingBonusMana = 1; // 後手の最初の1ターンだけマナ+1(Hearthstoneの「コイン」相当)。
			Side->bWentSecond = true;
		}
		CGState->Sides.Add(Side);
	}
	CGState->InitializeMarket();
	CGState->TurnCount = 0;
	CGState->WinnerPlayerIndex = -1;
	CGState->CurrentTurnPlayerIndex = FirstPlayerIndex;

	// StartTurn()がbSimulateBothSidesAsAI経由で両陣営とも自動進行し、
	// RequestEndTurn->CompleteEndTurn->StartTurnの再帰チェーンで、決着が
	// つくか(StartTurn内の)ターン数上限に達するまで、この1回の呼び出しで
	// 対戦全体が進む。呼び出し側はCGState->WinnerPlayerIndex/TurnCount/
	// Sides[]から結果を読む。
	StartTurn();
	return FirstPlayerIndex;
}

void ACGGameMode::RunSelfPlaySimulation(int32 NumMatches)
{
	UWorld* World = GetWorld();
	ACGGameState* CGState = GetCGGameState();
	if (!World || !CGState)
	{
		return;
	}

	static constexpr ECGColor AllColors[] = {
		ECGColor::Red, ECGColor::Orange, ECGColor::Green, ECGColor::Blue, ECGColor::Purple
	};

	TMap<ECGColor, int32> WinsByColor;
	TMap<ECGColor, int32> LossesByColor;
	TMap<ECGColor, int32> DrawsByColor;
	// MatchupWins[A][B] = Aの基本デッキがBの基本デッキに勝った回数(引き分け除く)。
	TMap<ECGColor, TMap<ECGColor, int32>> MatchupWins;
	int32 DrawCount = 0;
	int64 TotalTurns = 0;

	// カード単位の強さ分析用(「対戦シミュレーションから強い/弱いカードを
	// pickupしてほしい」というフィードバックへの対応)。そのカードをプレイした
	// 試合数と、そのうち自分が勝った試合数を集計し、「プレイした試合の勝率」を
	// カードごとに算出する。同じ色の中で比較することで、その色の中で相対的に
	// 強い/弱いカードが分かる(色ごとの地力の差は既にSimSummary Colorで別途
	// 分かっているため、ここでは色内比較に主眼を置く)。
	TMap<FName, int32> CardPlayedGames;
	TMap<FName, int32> CardWonGames;

	// 先攻/後攻の有利・不利を検証するための集計(色とは独立に、先攻側/後攻側
	// それぞれの勝敗数を数える)。
	int32 FirstPlayerWins = 0;
	int32 FirstPlayerLosses = 0;
	int32 FirstPlayerDraws = 0;

	bSimulateBothSidesAsAI = true;
	bDiagDisableBuyPhase = FParse::Param(FCommandLine::Get(), TEXT("SimDisableBuy"));
	if (bDiagDisableBuyPhase)
	{
		UE_LOG(LogCardGame, Log, TEXT("RunSelfPlaySimulation: -SimDisableBuy specified, market purchases disabled for this run (diagnostic only)"));
	}

	for (int32 MatchIndex = 0; MatchIndex < NumMatches; ++MatchIndex)
	{
		const ECGColor SideColors[2] = {
			AllColors[FMath::RandRange(0, UE_ARRAY_COUNT(AllColors) - 1)],
			AllColors[FMath::RandRange(0, UE_ARRAY_COUNT(AllColors) - 1)],
		};

		// 対戦1回分の準備(PlayerState破棄・再生成・デッキ配布・マーケット公開)と
		// StartTurn()呼び出しはPlaySimulatedMatch()に切り出している
		// (RunDeckOptimization()と共有するため)。
		const int32 FirstPlayerIndex = PlaySimulatedMatch(
			UCGCardDatabase::GetBasicColorDeckCardIds(SideColors[0]),
			UCGCardDatabase::GetBasicColorDeckCardIds(SideColors[1]));

		// カード単位の集計。同じカードを1試合中に複数回プレイしても1試合として
		// 二重に数えないよう、試合ごとにユニーク化してから集計する。
		for (int32 i = 0; i < 2; ++i)
		{
			if (!CGState->Sides.IsValidIndex(i) || !CGState->Sides[i])
			{
				continue;
			}
			TSet<FName> UniquePlayed(CGState->Sides[i]->CardsPlayedThisMatch);
			const bool bWonThisMatch = (CGState->WinnerPlayerIndex == i);
			for (const FName& PlayedCardId : UniquePlayed)
			{
				CardPlayedGames.FindOrAdd(PlayedCardId)++;
				if (bWonThisMatch)
				{
					CardWonGames.FindOrAdd(PlayedCardId)++;
				}
			}
		}

		if (CGState->WinnerPlayerIndex == -1)
		{
			++DrawCount;
			DrawsByColor.FindOrAdd(SideColors[0])++;
			DrawsByColor.FindOrAdd(SideColors[1])++;
			++FirstPlayerDraws;
		}
		else
		{
			const int32 LoserIndex = (CGState->WinnerPlayerIndex == 0) ? 1 : 0;
			const ECGColor WinnerColor = SideColors[CGState->WinnerPlayerIndex];
			const ECGColor LoserColor = SideColors[LoserIndex];
			WinsByColor.FindOrAdd(WinnerColor)++;
			LossesByColor.FindOrAdd(LoserColor)++;
			MatchupWins.FindOrAdd(WinnerColor).FindOrAdd(LoserColor)++;

			if (CGState->WinnerPlayerIndex == FirstPlayerIndex)
			{
				++FirstPlayerWins;
			}
			else
			{
				++FirstPlayerLosses;
			}
		}
		TotalTurns += CGState->TurnCount;

		UE_LOG(LogCardGame, Log, TEXT("SimMatch %d/%d: %s vs %s -> %s (Turns=%d)"),
			MatchIndex + 1, NumMatches,
			*UEnum::GetValueAsString(SideColors[0]), *UEnum::GetValueAsString(SideColors[1]),
			CGState->WinnerPlayerIndex == -1 ? TEXT("Draw") : *FString::Printf(TEXT("%s wins"), *UEnum::GetValueAsString(SideColors[CGState->WinnerPlayerIndex])),
			CGState->TurnCount);
	}

	bSimulateBothSidesAsAI = false;

	UE_LOG(LogCardGame, Log, TEXT("=== SimSummary: %d matches, %d draws, avg %.1f turns/match ==="),
		NumMatches, DrawCount, NumMatches > 0 ? static_cast<float>(TotalTurns) / NumMatches : 0.f);

	// 先攻/後攻の有利・不利(色に依らない集計。docs/next-ruleset-design.md
	// 「先手/後手調整: 後手にドロー1枚追加」の効果を実測で検証する)。
	{
		const int32 FirstPlayerGames = FirstPlayerWins + FirstPlayerLosses + FirstPlayerDraws;
		const float FirstPlayerWinRate = FirstPlayerGames > 0 ? 100.f * FirstPlayerWins / FirstPlayerGames : 0.f;
		UE_LOG(LogCardGame, Log, TEXT("SimSummary FirstPlayer Games=%d Wins=%d Losses=%d Draws=%d WinRate=%.1f%%"),
			FirstPlayerGames, FirstPlayerWins, FirstPlayerLosses, FirstPlayerDraws, FirstPlayerWinRate);
	}

	for (const ECGColor Color : AllColors)
	{
		const int32 Wins = WinsByColor.FindRef(Color);
		const int32 Losses = LossesByColor.FindRef(Color);
		const int32 Draws = DrawsByColor.FindRef(Color);
		const int32 Games = Wins + Losses + Draws;
		const float WinRate = Games > 0 ? 100.f * Wins / Games : 0.f;
		UE_LOG(LogCardGame, Log, TEXT("SimSummary Color=%s Games=%d Wins=%d Losses=%d Draws=%d WinRate=%.1f%%"),
			*UEnum::GetValueAsString(Color), Games, Wins, Losses, Draws, WinRate);

		const TMap<ECGColor, int32>* Beaten = MatchupWins.Find(Color);
		for (const ECGColor Opponent : AllColors)
		{
			if (Opponent == Color)
			{
				continue;
			}
			const int32 WinsVsOpponent = Beaten ? Beaten->FindRef(Opponent) : 0;
			const int32 LossesVsOpponent = MatchupWins.Contains(Opponent) ? MatchupWins[Opponent].FindRef(Color) : 0;
			if (WinsVsOpponent + LossesVsOpponent > 0)
			{
				UE_LOG(LogCardGame, Log, TEXT("SimSummary   %s vs %s: %d-%d"),
					*UEnum::GetValueAsString(Color), *UEnum::GetValueAsString(Opponent), WinsVsOpponent, LossesVsOpponent);
			}
		}
	}

	// カード単位の強さ分析(「対戦シミュレーションから強い/弱いカードをpickup
	// してほしい」というフィードバックへの対応)。色ごとの地力差は上のSimSummary
	// Colorで別途分かっているため、ここでは「そのカードをプレイした試合のうち
	// 勝てた割合」を同じ色の中で比較する形にする(色をまたいだ比較は地力差と
	// 混ざってしまい意味をなさないため)。試合数が少なすぎるカードはノイズが
	// 大きいため、目安の下限を設けて除外する。
	constexpr int32 MinGamesForCardStats = 20;
	struct FCardStat
	{
		FName CardId;
		FString CardName;
		int32 Games;
		int32 Wins;
		float WinRate;
	};
	for (const ECGColor Color : AllColors)
	{
		TArray<FCardStat> Stats;
		for (const FCGCardDef& Def : UCGCardDatabase::GetAllCards())
		{
			if (Def.Color != Color)
			{
				continue;
			}
			const int32 Games = CardPlayedGames.FindRef(Def.CardId);
			if (Games < MinGamesForCardStats)
			{
				continue;
			}
			const int32 Wins = CardWonGames.FindRef(Def.CardId);
			Stats.Add(FCardStat{ Def.CardId, Def.CardName, Games, Wins, 100.f * Wins / Games });
		}
		Stats.Sort([](const FCardStat& A, const FCardStat& B) { return A.WinRate > B.WinRate; });
		UE_LOG(LogCardGame, Log, TEXT("SimSummary CardStats Color=%s (プレイした試合の勝率、%d試合以上のみ、強い順)"),
			*UEnum::GetValueAsString(Color), MinGamesForCardStats);
		for (const FCardStat& Stat : Stats)
		{
			UE_LOG(LogCardGame, Log, TEXT("SimSummary   %s(%s): PlayedGames=%d WinRate=%.1f%%"),
				*Stat.CardId.ToString(), *Stat.CardName, Stat.Games, Stat.WinRate);
		}
	}

	// シミュレーション専用起動(-SimulateMatches)ではGUI操作の必要が無いため、
	// 集計ログの出力が終わり次第エンジンを自動終了させる。これにより呼び出し側
	// スクリプトはプロセスの終了を待つだけで次のログ集計処理に進める。
	UE_LOG(LogCardGame, Log, TEXT("RunSelfPlaySimulation: simulation complete, requesting engine exit"));
	FPlatformMisc::RequestExit(false, TEXT("RunSelfPlaySimulation"));
}

namespace
{
	// 色の基本デッキ(25枚、同名カード最大3枚)から、1枚だけ別のカードに入れ替えた
	// 新しいデッキを作る(山登り法の近傍生成)。Poolはその色のカードId一覧
	// (`UCGCardDatabase::GetAllCards()`をColorで絞り込んだもの)。
	TArray<FName> MutateDeckOneCard(const TArray<FName>& Deck, const TArray<FName>& Pool)
	{
		TArray<FName> NewDeck = Deck;
		if (Pool.Num() < 2 || NewDeck.Num() == 0)
		{
			return NewDeck;
		}

		const int32 RemoveIndex = FMath::RandRange(0, NewDeck.Num() - 1);
		const FName RemovedId = NewDeck[RemoveIndex];

		TMap<FName, int32> Counts;
		for (const FName& Id : NewDeck)
		{
			Counts.FindOrAdd(Id)++;
		}

		// ランダムな候補を同名3枚制限に収まるまで試す(最大30回、それでも
		// 見つからなければ入れ替えを諦めて元のデッキのまま返す)。
		for (int32 Try = 0; Try < 30; ++Try)
		{
			const FName Candidate = Pool[FMath::RandRange(0, Pool.Num() - 1)];
			if (Candidate == RemovedId)
			{
				continue;
			}
			if (Counts.FindRef(Candidate) < 3)
			{
				NewDeck[RemoveIndex] = Candidate;
				break;
			}
		}
		return NewDeck;
	}

	// デッキの構成(カードId→採用枚数)を、枚数の多い順・カードIdの昇順で
	// ログに出しやすい形に整形する。
	TArray<TPair<FName, int32>> SortedDeckComposition(const TArray<FName>& Deck)
	{
		TMap<FName, int32> Counts;
		for (const FName& Id : Deck)
		{
			Counts.FindOrAdd(Id)++;
		}
		TArray<TPair<FName, int32>> Sorted;
		for (const TPair<FName, int32>& Pair : Counts)
		{
			Sorted.Add(Pair);
		}
		Sorted.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B)
		{
			if (A.Value != B.Value)
			{
				return A.Value > B.Value;
			}
			return A.Key.LexicalLess(B.Key);
		});
		return Sorted;
	}
}

void ACGGameMode::RunDeckOptimization(int32 IterationsPerColor, int32 MatchesPerEvaluation, int32 Rounds)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState)
	{
		return;
	}

	static constexpr ECGColor AllColors[] = {
		ECGColor::Red, ECGColor::Orange, ECGColor::Green, ECGColor::Blue, ECGColor::Purple
	};

	// 色ごとのカードId一覧(山登り法の入れ替え候補プール)。
	TMap<ECGColor, TArray<FName>> ColorPools;
	for (const FCGCardDef& Card : UCGCardDatabase::GetAllCards())
	{
		if (Card.Color != ECGColor::None)
		{
			ColorPools.FindOrAdd(Card.Color).Add(Card.CardId);
		}
	}

	// 探索の起点は現状の基本デッキ(`GetBasicColorDeckCardIds`)。
	TMap<ECGColor, TArray<FName>> BestDecks;
	TMap<ECGColor, TArray<FName>> BaselineDecks;
	for (const ECGColor Color : AllColors)
	{
		const TArray<FName> Deck = UCGCardDatabase::GetBasicColorDeckCardIds(Color);
		BestDecks.Add(Color, Deck);
		BaselineDecks.Add(Color, Deck);
	}

	bSimulateBothSidesAsAI = true;

	// CandidateDeck(色Color)を、他4色の現時点のベストデッキと総当たりで
	// MatchesPerEvaluation回対戦させ、勝率(%)を返す(引き分けは分母に含むが
	// 勝ちには数えない)。対戦相手は他色を均等に巡回させる。
	auto EvaluateWinRate = [&](ECGColor Color, const TArray<FName>& CandidateDeck, int32 NumMatches) -> float
	{
		TArray<ECGColor> Opponents;
		for (const ECGColor Other : AllColors)
		{
			if (Other != Color)
			{
				Opponents.Add(Other);
			}
		}
		if (Opponents.Num() == 0 || NumMatches <= 0)
		{
			return 0.f;
		}

		int32 Wins = 0;
		for (int32 MatchIndex = 0; MatchIndex < NumMatches; ++MatchIndex)
		{
			const ECGColor OpponentColor = Opponents[MatchIndex % Opponents.Num()];
			PlaySimulatedMatch(CandidateDeck, BestDecks[OpponentColor]);
			if (CGState->WinnerPlayerIndex == 0)
			{
				++Wins;
			}
		}
		return 100.f * Wins / NumMatches;
	};

	UE_LOG(LogCardGame, Log, TEXT("=== DeckOptimization: %d round(s), %d iteration(s)/color/round, %d match(es)/evaluation ==="),
		Rounds, IterationsPerColor, MatchesPerEvaluation);

	for (int32 Round = 0; Round < Rounds; ++Round)
	{
		for (const ECGColor Color : AllColors)
		{
			const TArray<FName>* Pool = ColorPools.Find(Color);
			if (!Pool || Pool->Num() < 2)
			{
				continue;
			}

			float CurrentBestWinRate = EvaluateWinRate(Color, BestDecks[Color], MatchesPerEvaluation);
			UE_LOG(LogCardGame, Log, TEXT("DeckOptimization Round=%d Color=%s start WinRate=%.1f%%"),
				Round + 1, *UEnum::GetValueAsString(Color), CurrentBestWinRate);

			for (int32 Iter = 0; Iter < IterationsPerColor; ++Iter)
			{
				const TArray<FName> Candidate = MutateDeckOneCard(BestDecks[Color], *Pool);
				const float CandidateWinRate = EvaluateWinRate(Color, Candidate, MatchesPerEvaluation);
				if (CandidateWinRate > CurrentBestWinRate)
				{
					UE_LOG(LogCardGame, Log, TEXT("DeckOptimization Round=%d Color=%s Iter=%d/%d WinRate=%.1f%% -> accepted (was %.1f%%)"),
						Round + 1, *UEnum::GetValueAsString(Color), Iter + 1, IterationsPerColor, CandidateWinRate, CurrentBestWinRate);
					BestDecks[Color] = Candidate;
					CurrentBestWinRate = CandidateWinRate;
				}
				else
				{
					UE_LOG(LogCardGame, Log, TEXT("DeckOptimization Round=%d Color=%s Iter=%d/%d WinRate=%.1f%% -> rejected (best %.1f%%)"),
						Round + 1, *UEnum::GetValueAsString(Color), Iter + 1, IterationsPerColor, CandidateWinRate, CurrentBestWinRate);
				}
			}
		}
	}

	// 最終評価(ベースライン/最終デッキの勝率再計測)もPlaySimulatedMatch経由の
	// 自己対戦のため、bSimulateBothSidesAsAIをfalseに戻すのはこの後で行う
	// (先に戻すと両陣営ともAI自動進行しなくなり、対戦が決着せず勝率が
	// 正しく計測できない)。
	UE_LOG(LogCardGame, Log, TEXT("=== DeckOptimization: final decks ==="));
	for (const ECGColor Color : AllColors)
	{
		const float BaselineWinRate = EvaluateWinRate(Color, BaselineDecks[Color], MatchesPerEvaluation);
		const float FinalWinRate = EvaluateWinRate(Color, BestDecks[Color], MatchesPerEvaluation);
		UE_LOG(LogCardGame, Log, TEXT("DeckOptimization Result Color=%s BaselineWinRate=%.1f%% FinalWinRate=%.1f%% Delta=%+.1f%%"),
			*UEnum::GetValueAsString(Color), BaselineWinRate, FinalWinRate, FinalWinRate - BaselineWinRate);

		for (const TPair<FName, int32>& Entry : SortedDeckComposition(BestDecks[Color]))
		{
			FCGCardDef Def;
			const FString CardName = UCGCardDatabase::FindCard(Entry.Key, Def) ? Def.CardName : TEXT("?");
			UE_LOG(LogCardGame, Log, TEXT("DeckOptimization   %s(%s) x%d"), *Entry.Key.ToString(), *CardName, Entry.Value);
		}
	}

	bSimulateBothSidesAsAI = false;

	// シミュレーション専用起動(-OptimizeDecks)ではGUI操作の必要が無いため、
	// 結果の出力が終わり次第エンジンを自動終了させる(-SimulateMatchesと同じ方針)。
	UE_LOG(LogCardGame, Log, TEXT("RunDeckOptimization: optimization complete, requesting engine exit"));
	FPlatformMisc::RequestExit(false, TEXT("RunDeckOptimization"));
}

void ACGGameMode::StartTurn()
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->Sides.Num() < 2)
	{
		return;
	}

	// シミュレーション用の安全装置: 何らかの理由で決着がつかないまま長引き
	// すぎた対戦を打ち切る(RunSelfPlaySimulation側で引き分け扱いにする)。
	// StartTurn->RunTurn->RequestEndTurn->CompleteEndTurn->StartTurnと
	// 再帰するため、ここで止めないと終わらない対戦がスタックを伸ばし続ける。
	if (bSimulateBothSidesAsAI && CGState->TurnCount >= 100)
	{
		return;
	}

	CGState->TurnCount++;
	CGState->CurrentPhase = ECGPhase::Draw;

	ACGPlayerState* Active = CGState->Sides[CGState->CurrentTurnPlayerIndex];
	if (Active)
	{
		Active->RefreshManaForNewTurn();
		Active->DrawCard();
		for (FCGBoardUnit& Unit : Active->BoardUnits)
		{
			Unit.bCanAttack = true;
		}

		ACGPlayerState* TurnOpponent = GetOpponent(CGState->CurrentTurnPlayerIndex);

		// 変貌(紫、docs/game-rules-minimum.md): 経過ターン数の進行と、確率発動の
		// 抽選をここで行う。手札枚数条件はドロー後の枚数で判定させたいため、
		// DrawCard()の後に呼ぶ。
		Active->OnTurnStartTransformTick(TurnOpponent, CGState);

		// ターン開始時の常在効果(次期ルール、フェーズ4b。B05の1ドロー、O08の
		// コイン増加など)。
		Active->ApplyOnTurnStartAuraEffects(TurnOpponent, CGState);
	}

	CGState->CurrentPhase = ECGPhase::Main;

	UE_LOG(LogCardGame, Log, TEXT("StartTurn: TurnCount=%d ActiveSide=%d Mana=%d/%d HandSize=%d"),
		CGState->TurnCount, CGState->CurrentTurnPlayerIndex,
		Active ? Active->CurrentMana : -1, Active ? Active->MaxMana : -1,
		Active ? Active->HandCardIds.Num() : -1);

	CheckWinLose();
	NotifyStateChanged();

	// オンライン対戦ではSide1も人間のため、AIには操作させない
	// (docs/online-play-design.md参照)。
	if (CGState->WinnerPlayerIndex == -1 && AIOpponent && !bOnlineMatch
		&& (CGState->CurrentTurnPlayerIndex == AISideIndex || bSimulateBothSidesAsAI))
	{
		AIOpponent->RunTurn(this, CGState->CurrentTurnPlayerIndex);
	}
}

ACGPlayerState* ACGGameMode::GetOpponent(int32 SideIndex) const
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->Sides.Num() < 2)
	{
		return nullptr;
	}
	return CGState->Sides[SideIndex == 0 ? 1 : 0];
}

void ACGGameMode::ResolveDeathsForBothSides(ACGPlayerState* SideA, ACGPlayerState* SideB, ACGGameState* CGState)
{
	if (!SideA || !SideB)
	{
		return;
	}

	// 1回のRemoveDeadUnitsAndGetDeathDrawCount()呼び出しは、呼び出した側自身の場に
	// 残るHp<=0のUnitを全て取り除く(自陣内で死が死を呼ぶ効果は今のところ無い)。
	// ただし死亡時効果が反対側の場のUnitを巻き添えにすることがある(R01)ため、
	// 反対側にも新たな死亡が生まれていないか、両陣営を交互に見て収束するまで
	// 繰り返す。現実装の巻き添え効果は1体・1回だけなので通常1〜2周で収まるが、
	// 想定外の連鎖で無限ループにならないよう上限を設ける。
	for (int32 SafetyCounter = 0; SafetyCounter < 8; ++SafetyCounter)
	{
		const int32 DeathDrawsA = SideA->RemoveDeadUnitsAndGetDeathDrawCount(SideB, CGState);
		for (int32 i = 0; i < DeathDrawsA; ++i)
		{
			SideA->DrawCard();
		}
		const int32 DeathDrawsB = SideB->RemoveDeadUnitsAndGetDeathDrawCount(SideA, CGState);
		for (int32 i = 0; i < DeathDrawsB; ++i)
		{
			SideB->DrawCard();
		}

		const bool bAnyDeadRemaining =
			SideA->BoardUnits.ContainsByPredicate([](const FCGBoardUnit& Unit) { return Unit.Hp <= 0; })
			|| SideB->BoardUnits.ContainsByPredicate([](const FCGBoardUnit& Unit) { return Unit.Hp <= 0; });
		if (!bAnyDeadRemaining)
		{
			break;
		}
	}
}

bool ACGGameMode::RequestPlayCard(int32 SideIndex, FName CardId, int32 TargetUnitIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->Sides.IsValidIndex(SideIndex))
	{
		return false;
	}
	if (CGState->CurrentTurnPlayerIndex != SideIndex || CGState->CurrentPhase != ECGPhase::Main
		|| CGState->WinnerPlayerIndex != -1 || CGState->PendingChoice.IsActive())
	{
		return false;
	}

	ACGPlayerState* Side = CGState->Sides[SideIndex];
	ACGPlayerState* Opponent = GetOpponent(SideIndex);

	// 「対象を選ぶ操作を取りやめて戻れるようにしてほしい。カードを完全にプレイした
	// 後は戻れない」というフィードバックへの対応。PlayCardFromHand以降の処理が
	// 両陣営の状態を変え得るため、その直前の状態を保存しておく。実際に人間の
	// 対象選択待ちが残った場合だけ、下で「キャンセル可能」として有効化する
	// (FCGPlayerStateSnapshotのコメント参照)。
	const FCGPlayerStateSnapshot PreCancelSelfSnapshot = Side ? Side->CaptureSnapshot() : FCGPlayerStateSnapshot();
	const FCGPlayerStateSnapshot PreCancelOpponentSnapshot = Opponent ? Opponent->CaptureSnapshot() : FCGPlayerStateSnapshot();

	const bool bResult = Side && Side->PlayCardFromHand(CardId, Opponent, TargetUnitIndex, CGState);

	if (bResult)
	{
		// 行動ログ(次期ルール):「CPUが何をしたか分からない」というフィードバック
		// への対応。対象選択が絡む効果は実際の解決が後で非同期に起きるため
		// 効果の細部までは追えないが、カード名+効果テキスト(Description)だけでも
		// 「何を使われたか」を後から見返せるようにする。
		FCGCardDef PlayedDef;
		if (UCGCardDatabase::FindCard(CardId, PlayedDef))
		{
			CGState->AppendActionLog(SideIndex, PlayedDef.Description.IsEmpty()
				? FString::Printf(TEXT("「%s」を使用"), *PlayedDef.CardName)
				: FString::Printf(TEXT("「%s」を使用(%s)"), *PlayedDef.CardName, *PlayedDef.Description));

			// カードプレイ演出用(「それぞれのカードがプレイされた演出もない」という
			// フィードバックへの対応)。Unitはこの時点でSide->BoardUnitsの末尾に
			// 追加済みのため、その場インデックスを記録しておく。ただし生け贄
			// (Sacrifice)持ちUnitは、この時点ではまだ場に出ておらず(生け贄選択が
			// 解決してから出る)、代わりにACGGameMode::ResolvePendingChoiceAllyTarget
			// 側で記録する(そうしないと既存の無関係なUnitを誤って演出対象に
			// してしまう)。
			const bool bSacrificePending = CGState->PendingChoice.IsActive()
				&& CGState->PendingChoice.EffectId == FName(CGEffectId::SacrificeAllyOnPlay);
			if (!bSacrificePending)
			{
				CGState->LastCardPlayResult.SideIndex = SideIndex;
				CGState->LastCardPlayResult.CardId = CardId;
				CGState->LastCardPlayResult.bIsUnit = (PlayedDef.CardType == ECGCardType::Unit);
				CGState->LastCardPlayResult.BoardIndex = (CGState->LastCardPlayResult.bIsUnit && Side)
					? Side->BoardUnits.Num() - 1
					: -1;
				++CGState->CardPlaySequenceNumber;
			}
		}

		if (Opponent)
		{
			ResolveDeathsForBothSides(Side, Opponent, CGState);
		}
		// 変貌(紫)のHandSizeAtMost条件用: カードをプレイすると手札が減るため、
		// ここで再判定する(docs/next-ruleset-cards-v1.md「変貌先カード」P06)。
		if (Side)
		{
			Side->CheckAllTransforms(Opponent, CGState);
		}
		// カード効果が選択待ちを開始していた場合、AI側ならその場で即決する
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		AutoResolveChoiceIfAI();
		CheckWinLose();

		// AIが即決せず、このプレイの結果として人間の対象選択待ち(Spell/Unit登場時の
		// 対象、生け贄の対象等)がまだ残っているなら、上で保存したスナップショットを
		// 使ってキャンセル可能にする。既に勝敗が決まっている場合はキャンセルの
		// 対象にしない(勝敗確定後に状態を戻すと矛盾するため)。
		if (CGState->PendingChoice.IsActive() && CGState->PendingChoice.SideIndex == SideIndex
			&& CGState->WinnerPlayerIndex == -1)
		{
			CGState->PendingChoice.bCancellable = true;
			CancelSnapshotSelf = PreCancelSelfSnapshot;
			CancelSnapshotOpponent = PreCancelOpponentSnapshot;
		}
		NotifyStateChanged();
	}
	UE_LOG(LogCardGame, Log, TEXT("RequestPlayCard: Side=%d Card=%s Target=%d -> %s"),
		SideIndex, *CardId.ToString(), TargetUnitIndex, bResult ? TEXT("OK") : TEXT("REJECTED"));
	return bResult;
}

bool ACGGameMode::RequestBuyCard(int32 SideIndex, int32 MarketSlotIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->Sides.IsValidIndex(SideIndex))
	{
		return false;
	}
	if (CGState->CurrentTurnPlayerIndex != SideIndex || CGState->CurrentPhase != ECGPhase::Main
		|| CGState->WinnerPlayerIndex != -1 || CGState->PendingChoice.IsActive())
	{
		return false;
	}
	if (!CGState->MarketSlots.IsValidIndex(MarketSlotIndex))
	{
		return false;
	}
	const FName CardId = CGState->MarketSlots[MarketSlotIndex].CardId;
	if (CardId.IsNone())
	{
		// 出どころの山札・捨て札とも尽きて空になっている枠(docs/next-ruleset-design.md)。
		return false;
	}

	ACGPlayerState* Side = CGState->Sides[SideIndex];
	if (Side && Side->BuyCard(CardId))
	{
		// 補充は買った本人の山札からではなく、この枠の出どころの山札から行う
		// (自滅スパイラル対策として決定。docs/next-ruleset-design.md「マーケット」)。
		CGState->RefillMarketSlot(MarketSlotIndex);

		// O16強奪の商人: 自分が購入するたびに、敵のランダムなUnit1体へダメージを
		// 与える常在効果。BuyCard()自体はOpponentを持たないため、ここで判定する。
		// ダメージで死亡が起こり得るため(R01のような巻き添え連鎖も含め)、
		// 両陣営分の死亡処理を必ず行う。
		ACGPlayerState* Opponent = GetOpponent(SideIndex);
		// B06「深淵の封印」: 相手がB06を出していればO16のアウラも発動しない。
		if (Opponent && Opponent->BoardUnits.Num() > 0 && !Side->AreUnitAbilitiesSuppressedByEnemy(Opponent))
		{
			for (const FCGBoardUnit& Unit : Side->BoardUnits)
			{
				FCGCardDef UnitDef;
				if (UCGCardDatabase::FindCard(Unit.CardId, UnitDef)
					&& UnitDef.EffectId == FName(CGEffectId::OnBuyDamageRandomEnemyUnit)
					&& Opponent->BoardUnits.Num() > 0)
				{
					const int32 RandomUnitIndex = FMath::RandRange(0, Opponent->BoardUnits.Num() - 1);
					Opponent->ApplyDamageToUnit(RandomUnitIndex, UnitDef.EffectValue);
				}
			}
			ResolveDeathsForBothSides(Side, Opponent, CGState);
			CheckWinLose();
		}

		// 行動ログ(次期ルール、docs/game-rules-minimum.md「行動ログ」参照)。
		FCGCardDef BoughtDef;
		if (UCGCardDatabase::FindCard(CardId, BoughtDef))
		{
			CGState->AppendActionLog(SideIndex, FString::Printf(TEXT("マーケットから「%s」を購入"), *BoughtDef.CardName));
		}

		// 購入したカードを手札に入れるか山札の一番下に送るかを選ぶ
		// (「購入時に行き先を選べるようにしてほしい」というフィードバックのため。
		// 既存の選択式カード効果と同じPendingChoiceの仕組みに乗せている)。
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::BuyDestination;
		Choice.SideIndex = SideIndex;
		Choice.RevealedCardId = CardId;
		Choice.PromptText = TEXT("購入したカードを手札に入れますか？山札の一番下に送りますか？");
		BeginChoice(Choice);
		AutoResolveChoiceIfAI();

		UE_LOG(LogCardGame, Log, TEXT("RequestBuyCard: Side=%d Slot=%d Card=%s -> OK"), SideIndex, MarketSlotIndex, *CardId.ToString());
		return true;
	}
	UE_LOG(LogCardGame, Log, TEXT("RequestBuyCard: Side=%d Slot=%d Card=%s -> REJECTED"), SideIndex, MarketSlotIndex, *CardId.ToString());
	return false;
}

bool ACGGameMode::RequestAttack(int32 AttackerSideIndex, int32 AttackerUnitIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->Sides.IsValidIndex(AttackerSideIndex))
	{
		return false;
	}
	if (CGState->CurrentTurnPlayerIndex != AttackerSideIndex || CGState->CurrentPhase != ECGPhase::Main
		|| CGState->WinnerPlayerIndex != -1 || CGState->PendingChoice.IsActive())
	{
		return false;
	}

	// 先攻1ターン目(TurnCount==1、必ず先攻側の番)は攻撃を一切禁止する(疾駆
	// Unitも含む)。3000戦シミュレーションで、マーケットを完全に無効化しても
	// 先攻62.8%勝という戦闘の手番差(先攻が毎ラウンド半ターン早く攻撃できる
	// 複利効果)が主因と判明したため、「無料の先制攻撃」を直接なくす対策として
	// 導入した(docs/next-ruleset-simulation-v1.md「第12回」参照)。
	if (CGState->TurnCount == 1)
	{
		return false;
	}

	ACGPlayerState* Attacker = CGState->Sides[AttackerSideIndex];
	ACGPlayerState* Defender = GetOpponent(AttackerSideIndex);
	if (!Attacker || !Defender)
	{
		return false;
	}
	if (!Attacker->BoardUnits.IsValidIndex(AttackerUnitIndex) || !Attacker->BoardUnits[AttackerUnitIndex].bCanAttack)
	{
		return false;
	}

	// 守護(Guard)がいる場合、選べる対象は結局そのユニットだけだが(既存仕様どおり、
	// ResolvePendingChoiceWithTarget側のバリデーションで引き続き強制する)、
	// 以前はここで選択UIを経由せず即座に攻撃を確定させていたため、プレイヤーから
	// 見ると「攻撃したら選ぶ間もなく結果だけが起きる」ように見えていた
	// (「庇護があるときでも対象を選ぶ権利がほしい」というフィードバックへの対応)。
	// 守護の有無に関わらず必ずBeginChoiceを経由させ、対象選択のUI(グレーアウト表示は
	// UCGGameHUD::PopulateBoardRow/PopulateEnemyBoardRow側)を必ず表示する。

	// 守護がいなければ、対象(敵ユニットまたは顔面)をプレイヤー/AIが選ぶ
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	FCGPendingChoice Choice;
	Choice.ChoiceType = ECGChoiceType::EnemyOrFaceTarget;
	Choice.SideIndex = AttackerSideIndex;
	Choice.EffectId = FName(CGEffectId::AttackTarget);
	Choice.AttackerUnitIndex = AttackerUnitIndex;
	Choice.PromptText = TEXT("攻撃対象を選んでください(敵ユニットまたは顔面)");
	// 攻撃対象選択はbCanAttack等をまだ何も変えていないため、キャンセル可能にする
	// (「攻撃をして対象を選ぼうとしているときに取りやめて戻れるようにしてほしい。
	// 攻撃を完了した場合は戻れない」というフィードバックへの対応。ExecuteAttackで
	// bCanAttack=falseになるのは対象確定後なので、選択待ちの間は何も戻す必要が無い)。
	Choice.bCancellable = true;
	BeginChoice(Choice);
	AutoResolveChoiceIfAI();
	return true;
}

bool ACGGameMode::RequestCancelChoice(int32 SideIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->PendingChoice.SideIndex != SideIndex || !CGState->PendingChoice.IsActive()
		|| !CGState->PendingChoice.bCancellable)
	{
		return false;
	}

	const FCGPendingChoice Choice = CGState->PendingChoice;
	ACGPlayerState* Side = CGState->Sides.IsValidIndex(SideIndex) ? CGState->Sides[SideIndex] : nullptr;
	if (!Side)
	{
		return false;
	}

	if (Choice.EffectId == FName(CGEffectId::AttackTarget))
	{
		// 攻撃対象選択のキャンセル: bCanAttackはExecuteAttackが対象確定後に初めて
		// falseにするため、この時点では何も変わっていない。選択待ちを消すだけでよい。
		CGState->AppendActionLog(SideIndex, TEXT("攻撃を取りやめた"));
	}
	else
	{
		// カードプレイのキャンセル: RequestPlayCardがプレイ直前に保存した
		// スナップショットへ両陣営とも丸ごと復元する。
		Side->RestoreFromSnapshot(CancelSnapshotSelf);
		if (ACGPlayerState* Opponent = GetOpponent(SideIndex))
		{
			Opponent->RestoreFromSnapshot(CancelSnapshotOpponent);
		}
		CGState->AppendActionLog(SideIndex, TEXT("カードの使用を取りやめた"));
	}

	CGState->PendingChoice = FCGPendingChoice();
	CheckWinLose();
	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("RequestCancelChoice: Side=%d Effect=%s -> OK"),
		SideIndex, *Choice.EffectId.ToString());
	return true;
}

bool ACGGameMode::ExecuteAttack(int32 AttackerSideIndex, int32 AttackerUnitIndex, int32 TargetUnitIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState)
	{
		return false;
	}
	ACGPlayerState* Attacker = CGState->Sides.IsValidIndex(AttackerSideIndex) ? CGState->Sides[AttackerSideIndex] : nullptr;
	ACGPlayerState* Defender = GetOpponent(AttackerSideIndex);
	if (!Attacker || !Defender || !Attacker->BoardUnits.IsValidIndex(AttackerUnitIndex))
	{
		return false;
	}

	// 緑パッシブ等、盤面状況で変動する継続的なボーナスを含む実効値で計算する
	// (ACGPlayerState::GetEffectiveAtk参照)。
	const int32 Damage = Attacker->GetEffectiveAtk(AttackerUnitIndex);
	Attacker->BoardUnits[AttackerUnitIndex].bCanAttack = false;

	// 攻撃演出用に、このタイミング(死亡処理より前=場のインデックスがまだ有効な状態)の
	// 結果を記録しておく(docs/architecture.md「カードUIの設計」)。
	CGState->LastAttackResult.AttackerSideIndex = AttackerSideIndex;
	CGState->LastAttackResult.AttackerUnitIndex = AttackerUnitIndex;
	CGState->LastAttackResult.TargetUnitIndex = TargetUnitIndex;
	CGState->LastAttackResult.DamageToTarget = Damage;
	CGState->LastAttackResult.CounterDamageToAttacker = 0;
	++CGState->AttackSequenceNumber;

	// 行動ログ(次期ルール、docs/game-rules-minimum.md「行動ログ」参照)。
	{
		FCGCardDef AttackerDef;
		UCGCardDatabase::FindCard(Attacker->BoardUnits[AttackerUnitIndex].CardId, AttackerDef);
		FString TargetLabel = TEXT("敵リーダー");
		if (Defender->BoardUnits.IsValidIndex(TargetUnitIndex))
		{
			FCGCardDef TargetLabelDef;
			UCGCardDatabase::FindCard(Defender->BoardUnits[TargetUnitIndex].CardId, TargetLabelDef);
			TargetLabel = FString::Printf(TEXT("「%s」"), *TargetLabelDef.CardName);
		}
		CGState->AppendActionLog(AttackerSideIndex, FString::Printf(TEXT("「%s」で%sを攻撃(%dダメージ)"),
			*AttackerDef.CardName, *TargetLabel, Damage));
	}

	if (Defender->BoardUnits.IsValidIndex(TargetUnitIndex))
	{
		const int32 CounterDamage = Defender->GetEffectiveAtk(TargetUnitIndex);
		CGState->LastAttackResult.CounterDamageToAttacker = CounterDamage;

		FCGCardDef TargetDef;
		UCGCardDatabase::FindCard(Defender->BoardUnits[TargetUnitIndex].CardId, TargetDef);

		Defender->ApplyDamageToUnit(TargetUnitIndex, Damage);
		Attacker->ApplyDamageToUnit(AttackerUnitIndex, CounterDamage);

		// G12不屈の大樹: このユニットが攻撃(防御側として)を受けるたびに味方
		// リーダーを回復する(docs/next-ruleset-cards-v1.md「緑」)。「緑に回復
		// できる要素を少し増やしてほしい」というフィードバックを受け1→2に調整
		// (EffectId名の末尾の「1」は旧仕様の名残でそのまま残している)。
		if (TargetDef.EffectId == FName(CGEffectId::OnDefendHealSelf1))
		{
			Defender->Heal(2);
		}

		// 変貌(紫)のSurvivedAttacks条件用: 攻撃を受けて生き残ったユニットの進行度を
		// +1する(docs/next-ruleset-cards-v1.md「変貌先カード」P03香り売りの侍従)。
		// 死亡処理(RemoveDeadUnitsAndGetDeathDrawCount)より前、HPがまだ確定した
		// 直後のこのタイミングで判定する必要がある。
		if (Defender->BoardUnits[TargetUnitIndex].Hp > 0
			&& TargetDef.TransformConditionId == FName(CGTransformConditionId::SurvivedAttacks))
		{
			++Defender->BoardUnits[TargetUnitIndex].TransformProgress;
		}

		// 分身(緑)のSurvivedAttack条件用: 被弾して生き残った進行度を+1する
		// (docs/next-ruleset-cards-v1.md「緑」G03熊の盾持ち)。実際の分身は
		// この後のNotifyStateChanged()内のCheckAllClonesが成立させる。
		if (Defender->BoardUnits[TargetUnitIndex].Hp > 0
			&& TargetDef.CloneConditionId == FName(CGCloneConditionId::SurvivedAttack))
		{
			++Defender->BoardUnits[TargetUnitIndex].CloneProgress;
		}

		// 分身(緑)のAttackedAndSurvived条件用: 攻撃して(反撃を受けて)生き残った
		// 進行度を+1する(docs/next-ruleset-cards-v1.md「緑」G13森の巨人)。顔面への
		// 攻撃は反撃が発生せず常に「生き残る」ため対象外にしている(このブロックは
		// 対象がUnitのときだけ実行される)。
		if (Attacker->BoardUnits[AttackerUnitIndex].Hp > 0)
		{
			FCGCardDef AttackerCloneDef;
			if (UCGCardDatabase::FindCard(Attacker->BoardUnits[AttackerUnitIndex].CardId, AttackerCloneDef)
				&& AttackerCloneDef.CloneConditionId == FName(CGCloneConditionId::AttackedAndSurvived))
			{
				++Attacker->BoardUnits[AttackerUnitIndex].CloneProgress;
			}
		}
	}
	else
	{
		Defender->ApplyDamage(Damage);
	}

	// 上記でSurvivedAttacksの進行度を更新した後、両陣営の変貌条件をまとめて再判定する
	// (死亡処理より前に行うことで、生き残ったユニットの変貌を正しく反映する)。
	Attacker->CheckAllTransforms(Defender, CGState);
	Defender->CheckAllTransforms(Attacker, CGState);

	ResolveDeathsForBothSides(Attacker, Defender, CGState);

	CheckWinLose();
	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("ExecuteAttack: Attacker=%d Unit=%d Target=%d Damage=%d -> OK"),
		AttackerSideIndex, AttackerUnitIndex, TargetUnitIndex, Damage);
	return true;
}

void ACGGameMode::RequestEndTurn(int32 SideIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->CurrentTurnPlayerIndex != SideIndex || CGState->WinnerPlayerIndex != -1
		|| CGState->PendingChoice.IsActive())
	{
		return;
	}

	CGState->CurrentPhase = ECGPhase::End;
	ACGPlayerState* Side = CGState->Sides.IsValidIndex(SideIndex) ? CGState->Sides[SideIndex] : nullptr;
	if (!Side)
	{
		return;
	}
	ACGPlayerState* EndTurnOpponent = GetOpponent(SideIndex);
	Side->ResolveEndTurnEffects(EndTurnOpponent);

	// B10(弱点の考察官)等、敵Unitを対象にするターン終了時常在効果でユニットが
	// 死亡することがあるため、通常の攻撃・カード効果と同じ死亡処理を行う。
	if (EndTurnOpponent)
	{
		ResolveDeathsForBothSides(Side, EndTurnOpponent, CGState);
		CheckWinLose();
	}

	// 荒野の行商人(C007): 条件を満たしていれば、捨てるカードを選んでからターン終了を
	// 完了する(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	// 本来は発動する/しないも選べるはずだが、まずは「どのカードを捨てるか」を
	// 選べるようにする範囲に留めている。
	if (Side->HasBoardUnitWithEffect(FName(CGEffectId::OnBuyEndTurnDiscardDraw))
		&& Side->bBoughtThisTurn && Side->HandCardIds.Num() > 0)
	{
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::HandCard;
		Choice.SideIndex = SideIndex;
		Choice.EffectId = FName(CGEffectId::OnBuyEndTurnDiscardDraw);
		Choice.PromptText = TEXT("荒野の行商人: 捨てるカードを選んでください(捨てると1枚引きます)");
		BeginChoice(Choice);
		AutoResolveChoiceIfAI();
		return; // ターン終了はResolvePendingChoiceWithCard側で完了させる。
	}

	CompleteEndTurn(SideIndex);
}

void ACGGameMode::CompleteEndTurn(int32 SideIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState)
	{
		return;
	}
	CGState->CurrentTurnPlayerIndex = (SideIndex == 0) ? 1 : 0;
	StartTurn();
}

void ACGGameMode::BeginChoice(const FCGPendingChoice& Choice)
{
	if (ACGGameState* CGState = GetCGGameState())
	{
		CGState->PendingChoice = Choice;
		UE_LOG(LogCardGame, Log, TEXT("BeginChoice: Side=%d Type=%d Effect=%s"),
			Choice.SideIndex, static_cast<int32>(Choice.ChoiceType), *Choice.EffectId.ToString());
	}
}

void ACGGameMode::AutoResolveChoiceIfAI()
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || !CGState->PendingChoice.IsActive() || bOnlineMatch
		|| (CGState->PendingChoice.SideIndex != AISideIndex && !bSimulateBothSidesAsAI))
	{
		return;
	}

	const FCGPendingChoice Choice = CGState->PendingChoice;
	ACGPlayerState* Side = CGState->Sides.IsValidIndex(Choice.SideIndex) ? CGState->Sides[Choice.SideIndex] : nullptr;
	if (!Side)
	{
		CGState->PendingChoice = FCGPendingChoice();
		return;
	}

	switch (Choice.ChoiceType)
	{
	case ECGChoiceType::HandCard:
	{
		const FName Chosen = UCGAIOpponent::ChooseHandCardToDiscard(*Side);
		ResolvePendingChoiceWithCard(Choice.SideIndex, Chosen);
		break;
	}
	case ECGChoiceType::GraveyardCard:
	{
		const FName Chosen = UCGAIOpponent::ChooseGraveyardCard(*Side, Choice);
		ResolvePendingChoiceWithCard(Choice.SideIndex, Chosen);
		break;
	}
	case ECGChoiceType::EnemyOrFaceTarget:
	{
		ACGPlayerState* Opponent = GetOpponent(Choice.SideIndex);
		int32 ChosenTarget = -1;
		if (Opponent)
		{
			// 守護(Guard)がいる場合は必ずそちらへ強制される(ResolvePendingChoiceWithTarget
			// 側のバリデーションと同じ規則)。以前はRequestAttack側の即時確定でこのケースが
			// AIの選択ロジックに到達すること自体が無かったため、ChooseAttackTarget/
			// ChooseDamageTargetは守護を一切考慮していない。守護がいる間はそちらを
			// 優先し、いなければ従来通りの選択ロジックに任せる。
			int32 GuardIndex = -1;
			if (Opponent->HasGuardUnit())
			{
				for (int32 i = 0; i < Opponent->BoardUnits.Num(); ++i)
				{
					if (Opponent->BoardUnits[i].bHasGuard)
					{
						GuardIndex = i;
						break;
					}
				}
			}
			ChosenTarget = (GuardIndex != -1) ? GuardIndex
				: (Choice.EffectId == FName(CGEffectId::AttackTarget))
					? UCGAIOpponent::ChooseAttackTarget(*Side, *Opponent, Choice.AttackerUnitIndex)
					: UCGAIOpponent::ChooseDamageTarget(*Side, *Opponent, Choice.PendingDamageAmount, Choice.bRequireUnitTarget);
		}
		ResolvePendingChoiceWithTarget(Choice.SideIndex, ChosenTarget);
		break;
	}
	case ECGChoiceType::KeepOrBury:
	{
		const bool bKeepOnTop = UCGAIOpponent::ChooseKeepOnTop(*Side, Choice.RevealedCardId);
		ResolvePendingChoiceKeepOrBury(Choice.SideIndex, bKeepOnTop);
		break;
	}
	case ECGChoiceType::MarketCard:
	{
		const FName Chosen = UCGAIOpponent::ChooseMarketCard(CGState->GetMarketCardIds(), Choice);
		ResolvePendingChoiceWithCard(Choice.SideIndex, Chosen);
		break;
	}
	case ECGChoiceType::BuyDestination:
	{
		const bool bToHand = UCGAIOpponent::ChooseBuyDestination(*Side, Choice.RevealedCardId);
		ResolvePendingChoiceBuyDestination(Choice.SideIndex, bToHand);
		break;
	}
	case ECGChoiceType::EnemyUnitTarget:
	{
		ACGPlayerState* Opponent = GetOpponent(Choice.SideIndex);
		const int32 ChosenTarget = Opponent ? UCGAIOpponent::ChooseEnemyUnitTarget(*Opponent, Choice) : -1;
		ResolvePendingChoiceSealTarget(Choice.SideIndex, ChosenTarget);
		break;
	}
	case ECGChoiceType::AllyUnitTarget:
	{
		// P16(仮称、強制変貌)は強化と選ぶ基準が違う(Atkの高さではなく
		// 変貌可能かどうか)ため専用の選択関数を使う。生け贄(Sacrifice)も
		// 同様に「一番弱いUnitを選ぶ」という逆方向の基準になるため専用関数を使う。
		const int32 ChosenTarget = (Choice.EffectId == FName(CGEffectId::OnPlayForceTransformAllyTarget))
			? UCGAIOpponent::ChooseAllyTransformTarget(*Side)
			: (Choice.EffectId == FName(CGEffectId::SacrificeAllyOnPlay))
				? UCGAIOpponent::ChooseAllySacrificeTarget(*Side)
				: UCGAIOpponent::ChooseAllyBuffTarget(*Side);
		ResolvePendingChoiceAllyTarget(Choice.SideIndex, ChosenTarget);
		break;
	}
	case ECGChoiceType::MarketSlotTarget:
	{
		const int32 ChosenSlot = UCGAIOpponent::ChooseMarketSlotToReroll(*CGState);
		ResolvePendingChoiceMarketSlotTarget(Choice.SideIndex, ChosenSlot);
		break;
	}
	default:
		CGState->PendingChoice = FCGPendingChoice();
		break;
	}
}

bool ACGGameMode::ResolvePendingChoiceWithCard(int32 SideIndex, FName ChosenCardId)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->PendingChoice.SideIndex != SideIndex)
	{
		return false;
	}
	if (CGState->PendingChoice.ChoiceType != ECGChoiceType::HandCard
		&& CGState->PendingChoice.ChoiceType != ECGChoiceType::GraveyardCard
		&& CGState->PendingChoice.ChoiceType != ECGChoiceType::MarketCard)
	{
		return false;
	}

	const FCGPendingChoice Choice = CGState->PendingChoice;
	ACGPlayerState* Side = CGState->Sides.IsValidIndex(SideIndex) ? CGState->Sides[SideIndex] : nullptr;
	if (!Side)
	{
		return false;
	}

	bool bResolved = false;
	if (Choice.ChoiceType == ECGChoiceType::HandCard)
	{
		if (Side->DiscardSpecificFromHand(ChosenCardId))
		{
			bResolved = true;
			if (Choice.EffectId == FName(CGEffectId::Discard1Draw2)) // C019 荒野の取捨選択
			{
				Side->DrawCard();
				Side->DrawCard();
			}
			else if (Choice.EffectId == FName(CGEffectId::OnBuyEndTurnDiscardDraw)) // C007 荒野の行商人
			{
				Side->DrawCard();
			}
			// OnPlayDiscard1(C008)は捨てるだけで完了。
		}
	}
	else if (Choice.ChoiceType == ECGChoiceType::GraveyardCard)
	{
		// 不正なクリック(条件に合わないカードID等)を弾くため、候補条件を再確認する。
		FCGCardDef ChosenDef;
		const bool bValidCandidate = Side->DiscardCardIds.Contains(ChosenCardId)
			&& UCGCardDatabase::FindCard(ChosenCardId, ChosenDef)
			&& (Choice.MaxCost < 0 || ChosenDef.Cost <= Choice.MaxCost)
			&& (!Choice.bRequireSpell || ChosenDef.CardType == ECGCardType::Spell);

		if (bValidCandidate)
		{
			if (Choice.EffectId == FName(CGEffectId::GraveyardToDeckBottomDraw1)) // C006 廃墟あさりの拾い屋
			{
				bResolved = Side->MoveSpecificDiscardCardToDeckBottom(ChosenCardId);
			}
			else if (Choice.EffectId == FName(CGEffectId::ReturnGraveyardSpellSelfDamage1)) // C021 結晶に灯る記憶
			{
				bResolved = Side->MoveSpecificDiscardCardToHand(ChosenCardId);
				if (bResolved)
				{
					Side->ApplyDamage(1);
				}
			}
			else if (Choice.EffectId == FName(CGEffectId::OnPlayReturnGraveyardCheapCard)) // C011 廃墟の蘇生司祭
			{
				bResolved = Side->MoveSpecificDiscardCardToHand(ChosenCardId);
			}
		}
	}
	else if (Choice.ChoiceType == ECGChoiceType::MarketCard)
	{
		// CardIdだけでは枠を一意に特定できない(両者の山札に同じカードが同時に
		// 並び得るため。docs/next-ruleset-design.md「マーケット」)ので、該当する
		// 最初の枠を探して、その枠のインデックスで補充する。
		const int32 SlotIndex = CGState->MarketSlots.IndexOfByPredicate(
			[&ChosenCardId](const FCGMarketSlot& Slot) { return Slot.CardId == ChosenCardId; });

		// O15黒鉄の買い占め屋: 手札へ加えず、コストを支払わずそのまま場に出す
		// (docs/next-ruleset-cards-v1.md「橙」)。手札上限は関係しないため、
		// この場合だけ手札枚数チェックを行わず、代わりにUnitであることを確認する。
		const bool bDeployToBoard = Choice.EffectId == FName(CGEffectId::OnPlayDeployFromMarketFree);

		FCGCardDef ChosenDef;
		const bool bValidCandidate = SlotIndex != INDEX_NONE
			&& UCGCardDatabase::FindCard(ChosenCardId, ChosenDef)
			&& (Choice.MaxCost < 0 || ChosenDef.Cost <= Choice.MaxCost)
			&& (bDeployToBoard ? ChosenDef.CardType == ECGCardType::Unit : Side->HandCardIds.Num() < 10);
		if (bValidCandidate)
		{
			if (bDeployToBoard)
			{
				Side->AddBoardUnitDirect(ChosenCardId, ChosenDef.Atk, ChosenDef.Hp,
					ChosenDef.HasTag(TEXT("Haste")), ChosenDef.HasTag(TEXT("Guard")));
			}
			else
			{
				Side->HandCardIds.Add(ChosenCardId);
			}
			CGState->RefillMarketSlot(SlotIndex);
			// 「カードの効果でマーケットから購入された場合でも橙のフィニッシャーの
			// カウントが増えるようにしてほしい」というフィードバックへの対応。
			// 通常のBuyCard()(ACGPlayerState::BuyCard内)以外の経路でマーケットから
			// カードを取得する場合(O07買い付け/O15・FIN_ORANGEの無料デプロイ等)も
			// ここでCardsPurchasedThisMatchを加算する(docs/keywords.md、
			// フィニッシャーの発動条件参照)。
			++Side->CardsPurchasedThisMatch;
			bResolved = true;
		}
	}

	if (!bResolved)
	{
		return false;
	}

	// 選択待ちを解除してから、必要ならターン終了を完了させる。
	CGState->PendingChoice = FCGPendingChoice();
	if (Choice.EffectId == FName(CGEffectId::OnBuyEndTurnDiscardDraw))
	{
		CompleteEndTurn(SideIndex);
	}
	else if (Choice.ChoiceType == ECGChoiceType::MarketCard
		&& Choice.EffectId == FName(CGEffectId::OnPlayDeployFromMarketFree)
		&& Choice.RemainingRepeats > 0)
	{
		// 港を興す者(FIN_ORANGE)のように複数枚を1枚ずつ選ばせる効果の続き
		// (docs/next-ruleset-cards-v1.md「橙」)。候補が無ければ
		// BeginMarketDeployChoice内で何もせず終わる(選択待ちに入らない)。
		// AI側の連続選択もここから継続する必要があるため、新しい選択待ちに
		// 対しても改めてAutoResolveChoiceIfAI()を呼ぶ。
		Side->BeginMarketDeployChoice(CGState, Choice.MaxCost, Choice.RemainingRepeats - 1);
		AutoResolveChoiceIfAI();
	}

	CheckWinLose();
	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceWithCard: Side=%d Card=%s Effect=%s -> OK"),
		SideIndex, *ChosenCardId.ToString(), *Choice.EffectId.ToString());
	return true;
}

bool ACGGameMode::ResolvePendingChoiceWithTarget(int32 SideIndex, int32 ChosenUnitIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->PendingChoice.SideIndex != SideIndex
		|| CGState->PendingChoice.ChoiceType != ECGChoiceType::EnemyOrFaceTarget)
	{
		return false;
	}

	const FCGPendingChoice Choice = CGState->PendingChoice;
	ACGPlayerState* Opponent = GetOpponent(SideIndex);
	if (!Opponent)
	{
		return false;
	}

	// 守護(Guard)がいる場合は必ずそちらを対象にしなければならない
	// (攻撃・カード効果によるダメージ共通のルール)。
	if (Opponent->HasGuardUnit())
	{
		if (!Opponent->BoardUnits.IsValidIndex(ChosenUnitIndex) || !Opponent->BoardUnits[ChosenUnitIndex].bHasGuard)
		{
			return false;
		}
	}
	else if (ChosenUnitIndex != -1 && !Opponent->BoardUnits.IsValidIndex(ChosenUnitIndex))
	{
		return false;
	}

	// Unit限定の効果(例: R05黒鉄の抜き打ち)は顔面(-1)を選べない
	// (「赤のUnit限定/リーダー限定カードがどちらも選べてしまうバグがある」
	// というフィードバックへの対応。従来はEnemyOrFaceTarget選択が一律で
	// 顔面も選べてしまっていた)。
	if (Choice.bRequireUnitTarget && ChosenUnitIndex == -1)
	{
		return false;
	}

	CGState->PendingChoice = FCGPendingChoice();

	if (Choice.EffectId == FName(CGEffectId::AttackTarget))
	{
		return ExecuteAttack(SideIndex, Choice.AttackerUnitIndex, ChosenUnitIndex);
	}

	// カード効果由来のダメージ(C017 結晶の欠片・C024 土壇場の号令)。
	if (Opponent->BoardUnits.IsValidIndex(ChosenUnitIndex))
	{
		Opponent->ApplyDamageToUnit(ChosenUnitIndex, Choice.PendingDamageAmount);
	}
	else
	{
		Opponent->ApplyDamage(Choice.PendingDamageAmount);
	}

	ACGPlayerState* Side = CGState->Sides.IsValidIndex(SideIndex) ? CGState->Sides[SideIndex] : nullptr;
	ResolveDeathsForBothSides(Side, Opponent, CGState);

	CheckWinLose();
	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceWithTarget: Side=%d Target=%d Effect=%s -> OK"),
		SideIndex, ChosenUnitIndex, *Choice.EffectId.ToString());
	return true;
}

bool ACGGameMode::ResolvePendingChoiceKeepOrBury(int32 SideIndex, bool bKeepOnTop)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->PendingChoice.SideIndex != SideIndex
		|| CGState->PendingChoice.ChoiceType != ECGChoiceType::KeepOrBury)
	{
		return false;
	}
	ACGPlayerState* Side = CGState->Sides.IsValidIndex(SideIndex) ? CGState->Sides[SideIndex] : nullptr;
	if (!Side)
	{
		return false;
	}

	CGState->PendingChoice = FCGPendingChoice();

	if (!bKeepOnTop)
	{
		Side->MoveDeckTopToBottom();
	}
	// 上に残す場合は何もしない(既に一番上にある)。

	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceKeepOrBury: Side=%d KeepOnTop=%d -> OK"),
		SideIndex, bKeepOnTop ? 1 : 0);
	return true;
}

bool ACGGameMode::ResolvePendingChoiceBuyDestination(int32 SideIndex, bool bToHand)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->PendingChoice.SideIndex != SideIndex
		|| CGState->PendingChoice.ChoiceType != ECGChoiceType::BuyDestination)
	{
		return false;
	}
	ACGPlayerState* Side = CGState->Sides.IsValidIndex(SideIndex) ? CGState->Sides[SideIndex] : nullptr;
	if (!Side)
	{
		return false;
	}

	const FName CardId = CGState->PendingChoice.RevealedCardId;
	CGState->PendingChoice = FCGPendingChoice();

	// 手札へ選んでいても手札が上限に達していれば(UI側では上限時にボタンを隠すが、
	// 念のための保険として)山札の一番下へ回す。
	if (bToHand && Side->HandCardIds.Num() < 10)
	{
		Side->HandCardIds.Add(CardId);
	}
	else
	{
		Side->DeckCardIds.Add(CardId); // 一番下(DrawCardは先頭=[0]から引くため)。
	}

	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceBuyDestination: Side=%d Card=%s ToHand=%d -> OK"),
		SideIndex, *CardId.ToString(), bToHand ? 1 : 0);
	return true;
}

bool ACGGameMode::ResolvePendingChoiceSealTarget(int32 SideIndex, int32 ChosenUnitIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->PendingChoice.SideIndex != SideIndex
		|| CGState->PendingChoice.ChoiceType != ECGChoiceType::EnemyUnitTarget)
	{
		return false;
	}

	ACGPlayerState* Side = CGState->Sides.IsValidIndex(SideIndex) ? CGState->Sides[SideIndex] : nullptr;
	ACGPlayerState* Opponent = GetOpponent(SideIndex);
	if (!Side || !Opponent || !Opponent->BoardUnits.IsValidIndex(ChosenUnitIndex))
	{
		return false;
	}

	// コスト(または現在の攻撃力)の条件(Choice.MaxCost、-1なら無条件)を満たすか
	// 再確認する(不正なクリック対策。既存のGraveyardCard/MarketCard選択と同じ
	// 考え方)。B11/B13(Choice.bFilterByCurrentAtk)は現在の実際のAtk(バフ/デバフ
	// 後の値、マイナスもあり得る)で判定するため、カード定義を引き直さず
	// BoardUnitの値をそのまま比較する。
	if (CGState->PendingChoice.MaxCost >= 0)
	{
		const FCGBoardUnit& TargetUnit = Opponent->BoardUnits[ChosenUnitIndex];
		if (CGState->PendingChoice.bFilterByCurrentAtk)
		{
			if (TargetUnit.Atk > CGState->PendingChoice.MaxCost)
			{
				return false;
			}
		}
		else
		{
			const FName TargetCardId = TargetUnit.OriginalCardId.IsNone() ? TargetUnit.CardId : TargetUnit.OriginalCardId;
			FCGCardDef TargetDef;
			if (!UCGCardDatabase::FindCard(TargetCardId, TargetDef) || TargetDef.Cost > CGState->PendingChoice.MaxCost)
			{
				return false;
			}
		}
	}

	const FCGPendingChoice Choice = CGState->PendingChoice;
	CGState->PendingChoice = FCGPendingChoice();

	// EnemyUnitTargetは断罪(青)だけでなく、対象指定の敵単体デバフ(B02、
	// docs/next-ruleset-cards-v1.md)も同じ選択の仕組みを使う。EffectIdで分岐する
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
	if (Choice.EffectId == FName(CGEffectId::OnPlayDebuffTarget))
	{
		const int32 Delta = Choice.PendingBuffAmount;
		Opponent->BoardUnits[ChosenUnitIndex].Atk = FMath::Max(0, Opponent->BoardUnits[ChosenUnitIndex].Atk + Delta);
		Opponent->BoardUnits[ChosenUnitIndex].Hp += Delta;

		ResolveDeathsForBothSides(Side, Opponent, CGState);

		CheckWinLose();
		NotifyStateChanged();
		UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceSealTarget(debuff): Side=%d Target=%d Delta=%d -> OK"),
			SideIndex, ChosenUnitIndex, Delta);
		return true;
	}

	// G11(新)群れの猛攻: 自分の場のUnit数だけダメージ(デバフではなく実ダメージ。
	// Atkは変えずHpだけ減らす。docs/next-ruleset-cards-v1.md「緑」)。
	if (Choice.EffectId == FName(CGEffectId::OnPlayDamageTargetByAllyUnitCount))
	{
		Opponent->ApplyDamageToUnit(ChosenUnitIndex, Choice.PendingDamageAmount);

		ResolveDeathsForBothSides(Side, Opponent, CGState);

		CheckWinLose();
		NotifyStateChanged();
		UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceSealTarget(damage): Side=%d Target=%d Damage=%d -> OK"),
			SideIndex, ChosenUnitIndex, Choice.PendingDamageAmount);
		return true;
	}

	int32 CondemnedCost = 0;
	if (!Opponent->CondemnUnit(ChosenUnitIndex, CondemnedCost))
	{
		return false;
	}

	// 断罪(Condemn、旧「封印」)は通常の死亡処理(墓地送り・死亡時効果・死亡数
	// カウント)を経由させる。B15の2体目選択で場のUnit数を数え直す前に、
	// 必ずここで死亡処理を確定させる(「ユニットを死亡させるキーワード能力に
	// してほしい」というフィードバックへの対応。docs/keywords.md「断罪
	// (Condemn)」参照)。
	ResolveDeathsForBothSides(Side, Opponent, CGState);

	// 青パッシブは「2回目のドロー時」に発動条件が変わり、断罪とは無関係になった
	// (docs/game-rules-minimum.md「色ガイド」)。

	// B04頁繰りの魔道士: 自分が断罪を成功させるたびに敵リーダーへ1ダメージ。
	Side->NotifySealSucceeded(Opponent);

	// B09叡智の追放: 断罪に成功したらコインを1増加。
	if (Choice.EffectId == FName(CGEffectId::SealSpellGrantPurchaseMana))
	{
		Side->PurchaseMana += 1;
	}

	// B15双つの追放: 1体目の断罪が成功したら、対象が残っていれば2体目の選択を
	// 自動で開始する(2体目はここで打ち止め、SealSpellマーカーで再帰させない)。
	if (Choice.EffectId == FName(CGEffectId::SealSpellTwo) && Opponent->BoardUnits.Num() > 0)
	{
		FCGPendingChoice SecondChoice;
		SecondChoice.ChoiceType = ECGChoiceType::EnemyUnitTarget;
		SecondChoice.SideIndex = SideIndex;
		SecondChoice.EffectId = FName(CGEffectId::SealSpell);
		SecondChoice.MaxCost = -1;
		SecondChoice.PromptText = TEXT("断罪するもう1体の敵ユニットを選んでください");
		BeginChoice(SecondChoice);
		AutoResolveChoiceIfAI();
	}

	CheckWinLose();
	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceSealTarget: Side=%d Target=%d CondemnedCost=%d -> OK"),
		SideIndex, ChosenUnitIndex, CondemnedCost);
	return true;
}

bool ACGGameMode::ResolvePendingChoiceAllyTarget(int32 SideIndex, int32 ChosenUnitIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->PendingChoice.SideIndex != SideIndex
		|| CGState->PendingChoice.ChoiceType != ECGChoiceType::AllyUnitTarget)
	{
		return false;
	}

	ACGPlayerState* Side = CGState->Sides.IsValidIndex(SideIndex) ? CGState->Sides[SideIndex] : nullptr;
	if (!Side || !Side->BoardUnits.IsValidIndex(ChosenUnitIndex))
	{
		return false;
	}

	const FCGPendingChoice Choice = CGState->PendingChoice;

	// P16(仮称): 強化ではなく、選んだ味方Unit1体を強制的に変貌させる。他の
	// AllyUnitTarget効果(強化)とは処理内容が別物のため、ここで分岐して独立に
	// 完結させる(対象は変貌可能なUnitに限る。選択開始時点でHandle_
	// OnPlayForceTransformAllyTarget側が候補の有無を確認済みだが、選択肢は
	// 場の全Unitに開かれているため、ここでも変貌不可な対象を弾く)。
	if (Choice.EffectId == FName(CGEffectId::OnPlayForceTransformAllyTarget))
	{
		if (!Side->CanUnitTransform(ChosenUnitIndex))
		{
			return false;
		}
		CGState->PendingChoice = FCGPendingChoice();
		Side->ForceTransformUnit(ChosenUnitIndex, GetOpponent(SideIndex), CGState);
		NotifyStateChanged();
		UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceAllyTarget: Side=%d Target=%d ForceTransform -> OK"),
			SideIndex, ChosenUnitIndex);
		return true;
	}

	// 生け贄(Sacrifice、R07/R13/R15): 選んだ味方Unitを生け贄にしてから、
	// プレイ中だったUnit(Choice.RevealedCardIdに控えてある)を実際に場へ出す
	// (docs/keywords.md「生け贄(Sacrifice)」参照)。
	if (Choice.EffectId == FName(CGEffectId::SacrificeAllyOnPlay))
	{
		FCGCardDef SacrificeSourceDef;
		if (!UCGCardDatabase::FindCard(Choice.RevealedCardId, SacrificeSourceDef))
		{
			return false;
		}

		CGState->PendingChoice = FCGPendingChoice();

		// 生け贄にするUnitのHpを0にするだけでよく、実際の死亡処理(墓地送り・
		// 死亡時効果・死亡数カウント)は下のResolveDeathsForBothSidesが行う。
		Side->BoardUnits[ChosenUnitIndex].Hp = 0;

		ACGPlayerState* SacrificeOpponent = GetOpponent(SideIndex);
		Side->SpawnUnitFromHandOnBoard(Choice.RevealedCardId, SacrificeSourceDef, /*bIsSecondOrLaterPlayThisTurn=*/false, SacrificeOpponent, CGState);

		// カードプレイ演出用(RequestPlayCard側では、生け贄選択待ちに入った時点では
		// まだ場に出ていなかったため記録を見送っていた。ここで改めて記録する)。
		CGState->LastCardPlayResult.SideIndex = SideIndex;
		CGState->LastCardPlayResult.CardId = Choice.RevealedCardId;
		CGState->LastCardPlayResult.bIsUnit = true;
		CGState->LastCardPlayResult.BoardIndex = Side->BoardUnits.Num() - 1;
		++CGState->CardPlaySequenceNumber;

		if (SacrificeOpponent)
		{
			ResolveDeathsForBothSides(Side, SacrificeOpponent, CGState);
		}

		CheckWinLose();
		NotifyStateChanged();
		UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceAllyTarget: Side=%d Target=%d Sacrifice Card=%s -> OK"),
			SideIndex, ChosenUnitIndex, *Choice.RevealedCardId.ToString());
		return true;
	}

	const int32 BuffAmount = Choice.PendingBuffAmount;
	CGState->PendingChoice = FCGPendingChoice();

	Side->BoardUnits[ChosenUnitIndex].Atk += BuffAmount;
	Side->BoardUnits[ChosenUnitIndex].Hp += BuffAmount;

	// P03香り売りの侍従: 対象指定の味方強化を受けるたびに進行度+1(2回で変貌)。
	FCGBoardUnit& BuffedUnit = Side->BoardUnits[ChosenUnitIndex];
	FCGCardDef BuffedDef;
	if (UCGCardDatabase::FindCard(BuffedUnit.CardId, BuffedDef)
		&& BuffedDef.TransformConditionId == FName(CGTransformConditionId::BuffedCount))
	{
		++BuffedUnit.TransformProgress;
	}
	Side->CheckAllTransforms(GetOpponent(SideIndex), CGState);

	// P10秘宝の授与: 強化に加えて1ドローする。
	if (Choice.EffectId == FName(CGEffectId::BuffAllyTargetAndDraw))
	{
		Side->DrawCard();
	}

	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceAllyTarget: Side=%d Target=%d Buff=+%d/+%d -> OK"),
		SideIndex, ChosenUnitIndex, BuffAmount, BuffAmount);
	return true;
}

bool ACGGameMode::ResolvePendingChoiceMarketSlotTarget(int32 SideIndex, int32 ChosenSlotIndex)
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->PendingChoice.SideIndex != SideIndex
		|| CGState->PendingChoice.ChoiceType != ECGChoiceType::MarketSlotTarget
		|| !CGState->MarketSlots.IsValidIndex(ChosenSlotIndex))
	{
		return false;
	}

	CGState->PendingChoice = FCGPendingChoice();
	CGState->RerollMarketSlot(ChosenSlotIndex);

	NotifyStateChanged();
	UE_LOG(LogCardGame, Log, TEXT("ResolvePendingChoiceMarketSlotTarget: Side=%d Slot=%d -> OK"),
		SideIndex, ChosenSlotIndex);
	return true;
}

void ACGGameMode::NotifyStateChanged()
{
	if (ACGGameState* CGState = GetCGGameState())
	{
		for (ACGPlayerState* Side : CGState->Sides)
		{
			if (Side)
			{
				Side->SyncPublicCardCounts();
				// フィニッシャー: 状態が変わるたびに(ドロー/購入/死亡/変貌/場のUnit数
				// いずれの変化でも)ここで一括して判定する(「各色で特定の条件を
				// 達成したときにフィニッシャーが駆けつける」というフィードバックへの対応)。
				Side->CheckAndSpawnFinisher(GetOpponent(Side->SideIndex), CGState);
				// 分身(緑): 味方Unit数条件(AllyUnitCountAtLeast)はここで一括判定する。
				// SurvivedAttack/AttackedAndSurvived/LeaderHealedはCGGameMode::ExecuteAttack/
				// ACGPlayerState::NotifyOwnLeaderHealedが該当イベントでCloneProgressを
				// 先に+1しており、この呼び出しで実際の分身を成立させる
				// (docs/next-ruleset-cards-v1.md「緑」)。
				Side->CheckAllClones(GetOpponent(Side->SideIndex), CGState);
				// オンライン対戦: このActor(PlayerState)の複製を次回の定期
				// スケジュールまで待たず、今すぐ送るようサーバーに指示する。
				// デフォルトのNetUpdateFrequencyだけに頼ると、変更してから
				// 実際に相手へ届くまでに最大で1/NetUpdateFrequency秒分の
				// 遅れが乗ってしまい、「相手の操作の反映が遅い」と感じられる
				// (docs/online-play-plan.md「フェーズ4」参照)。
				Side->ForceNetUpdate();
			}
		}
		++CGState->StateVersion;
		// GameState自身(MarketSlots/TurnCount/PendingChoice等)についても同様。
		CGState->ForceNetUpdate();
	}

	// CheckAndSpawnFinisher(上記ループ内)は港を興す者(FIN_ORANGE)のように
	// 登場時に新しい選択待ち(PendingChoice)を開始することがある。この関数の
	// 呼び出し元は「自分が起こした選択」をそれぞれ個別にAutoResolveChoiceIfAI()
	// 済みだが、フィニッシャー登場に伴う選択はここで初めて発生するため、
	// 誰も解決しないまま残ってしまう(選択待ちがAI側だと、AIがその後
	// 一切行動できずに試合が止まってしまう不具合があった)。ここで改めて
	// 呼んでおくことで、AI側の選択ならその場で解決する(人間側の選択なら
	// 何もしない。AutoResolveChoiceIfAI()自身の判定に委ねる)。
	AutoResolveChoiceIfAI();
	OnCardGameStateChanged();
}

void ACGGameMode::CheckWinLose()
{
	ACGGameState* CGState = GetCGGameState();
	if (!CGState || CGState->Sides.Num() < 2 || CGState->WinnerPlayerIndex != -1)
	{
		return;
	}

	const bool bSide0Lost = CGState->Sides[0] && (CGState->Sides[0]->bIsDefeated || CGState->Sides[0]->CurrentHP <= 0);
	const bool bSide1Lost = CGState->Sides[1] && (CGState->Sides[1]->bIsDefeated || CGState->Sides[1]->CurrentHP <= 0);

	if (bSide0Lost && bSide1Lost)
	{
		CGState->WinnerPlayerIndex = -1;
		UE_LOG(LogCardGame, Log, TEXT("Match ended: Draw"));
		OnMatchEnded(-1);
	}
	else if (bSide0Lost)
	{
		CGState->WinnerPlayerIndex = 1;
		UE_LOG(LogCardGame, Log, TEXT("Match ended: Winner=Side1"));
		OnMatchEnded(1);
	}
	else if (bSide1Lost)
	{
		CGState->WinnerPlayerIndex = 0;
		UE_LOG(LogCardGame, Log, TEXT("Match ended: Winner=Side0"));
		OnMatchEnded(0);
	}
}
