#include "CGPlayerState.h"
#include "CGCardDatabase.h"
#include "CGGameState.h"
#include "CardGame.h"
#include "Net/UnrealNetwork.h"

void ACGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 非公開情報(本人にしか複製しない、docs/online-play-design.md
	// 「`ACGPlayerState`のレプリケーション」)。
	DOREPLIFETIME_CONDITION(ACGPlayerState, HandCardIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ACGPlayerState, DeckCardIds, COND_OwnerOnly);

	// 以下は非公開情報を含まない公開情報のため、条件無し(全員へ複製)。
	DOREPLIFETIME(ACGPlayerState, SideIndex);
	DOREPLIFETIME(ACGPlayerState, CurrentHP);
	DOREPLIFETIME(ACGPlayerState, MaxHP);
	DOREPLIFETIME(ACGPlayerState, CurrentMana);
	DOREPLIFETIME(ACGPlayerState, MaxMana);
	DOREPLIFETIME(ACGPlayerState, PendingBonusMana);
	DOREPLIFETIME(ACGPlayerState, bWentSecond);
	DOREPLIFETIME(ACGPlayerState, PurchaseMana);
	DOREPLIFETIME(ACGPlayerState, bIsDefeated);
	DOREPLIFETIME(ACGPlayerState, DiscardCardIds);
	DOREPLIFETIME(ACGPlayerState, BoardUnits);
	DOREPLIFETIME(ACGPlayerState, HandCount);
	DOREPLIFETIME(ACGPlayerState, DeckCount);
	DOREPLIFETIME(ACGPlayerState, CardsPlayedThisTurn);
	DOREPLIFETIME(ACGPlayerState, SpellsPlayedThisTurn);
	DOREPLIFETIME(ACGPlayerState, DrawsThisTurn);
	DOREPLIFETIME(ACGPlayerState, bBoughtThisTurn);
	DOREPLIFETIME(ACGPlayerState, bAllySpellPingUsedThisTurn);
	DOREPLIFETIME(ACGPlayerState, bBuffBoardOnPurchaseThisTurn);
	DOREPLIFETIME(ACGPlayerState, bAllPurchasesDiscountedThisTurn);
	DOREPLIFETIME(ACGPlayerState, ActiveColors);
	DOREPLIFETIME(ACGPlayerState, bRedPassiveUsedThisTurn);
	DOREPLIFETIME(ACGPlayerState, bOrangePassiveUsedThisTurn);
	DOREPLIFETIME(ACGPlayerState, bGreenPassiveUsedThisTurn);
	DOREPLIFETIME(ACGPlayerState, bBluePassiveUsedThisTurn);
	DOREPLIFETIME(ACGPlayerState, bPurplePassiveUsedThisTurn);
	DOREPLIFETIME(ACGPlayerState, NextUnitPlayDiscount);
	DOREPLIFETIME(ACGPlayerState, TransformsSucceededThisTurn);
}

void ACGPlayerState::SyncPublicCardCounts()
{
	HandCount = HandCardIds.Num();
	DeckCount = DeckCardIds.Num();
}

FCGPlayerStateSnapshot ACGPlayerState::CaptureSnapshot() const
{
	FCGPlayerStateSnapshot Snapshot;
	Snapshot.CurrentHP = CurrentHP;
	Snapshot.CurrentMana = CurrentMana;
	Snapshot.PurchaseMana = PurchaseMana;
	Snapshot.bIsDefeated = bIsDefeated;
	Snapshot.HandCardIds = HandCardIds;
	Snapshot.DeckCardIds = DeckCardIds;
	Snapshot.DiscardCardIds = DiscardCardIds;
	Snapshot.BoardUnits = BoardUnits;
	Snapshot.CardsPlayedThisTurn = CardsPlayedThisTurn;
	Snapshot.CardsPlayedThisMatch = CardsPlayedThisMatch;
	Snapshot.SpellsPlayedThisTurn = SpellsPlayedThisTurn;
	Snapshot.DrawsThisTurn = DrawsThisTurn;
	Snapshot.bBoughtThisTurn = bBoughtThisTurn;
	Snapshot.bAllySpellPingUsedThisTurn = bAllySpellPingUsedThisTurn;
	Snapshot.bBuffBoardOnPurchaseThisTurn = bBuffBoardOnPurchaseThisTurn;
	Snapshot.bAllPurchasesDiscountedThisTurn = bAllPurchasesDiscountedThisTurn;
	Snapshot.AlliesDiedThisMatch = AlliesDiedThisMatch;
	Snapshot.CardsPurchasedThisMatch = CardsPurchasedThisMatch;
	Snapshot.CardsDrawnThisMatch = CardsDrawnThisMatch;
	Snapshot.UnitsTransformedThisMatch = UnitsTransformedThisMatch;
	Snapshot.FinishersSpawned = FinishersSpawned;
	Snapshot.bRedPassiveUsedThisTurn = bRedPassiveUsedThisTurn;
	Snapshot.bOrangePassiveUsedThisTurn = bOrangePassiveUsedThisTurn;
	Snapshot.bGreenPassiveUsedThisTurn = bGreenPassiveUsedThisTurn;
	Snapshot.bBluePassiveUsedThisTurn = bBluePassiveUsedThisTurn;
	Snapshot.bPurplePassiveUsedThisTurn = bPurplePassiveUsedThisTurn;
	Snapshot.NextUnitPlayDiscount = NextUnitPlayDiscount;
	Snapshot.TransformsSucceededThisTurn = TransformsSucceededThisTurn;
	return Snapshot;
}

void ACGPlayerState::RestoreFromSnapshot(const FCGPlayerStateSnapshot& Snapshot)
{
	CurrentHP = Snapshot.CurrentHP;
	CurrentMana = Snapshot.CurrentMana;
	PurchaseMana = Snapshot.PurchaseMana;
	bIsDefeated = Snapshot.bIsDefeated;
	HandCardIds = Snapshot.HandCardIds;
	DeckCardIds = Snapshot.DeckCardIds;
	DiscardCardIds = Snapshot.DiscardCardIds;
	BoardUnits = Snapshot.BoardUnits;
	CardsPlayedThisTurn = Snapshot.CardsPlayedThisTurn;
	CardsPlayedThisMatch = Snapshot.CardsPlayedThisMatch;
	SpellsPlayedThisTurn = Snapshot.SpellsPlayedThisTurn;
	DrawsThisTurn = Snapshot.DrawsThisTurn;
	bBoughtThisTurn = Snapshot.bBoughtThisTurn;
	bAllySpellPingUsedThisTurn = Snapshot.bAllySpellPingUsedThisTurn;
	bBuffBoardOnPurchaseThisTurn = Snapshot.bBuffBoardOnPurchaseThisTurn;
	bAllPurchasesDiscountedThisTurn = Snapshot.bAllPurchasesDiscountedThisTurn;
	AlliesDiedThisMatch = Snapshot.AlliesDiedThisMatch;
	CardsPurchasedThisMatch = Snapshot.CardsPurchasedThisMatch;
	CardsDrawnThisMatch = Snapshot.CardsDrawnThisMatch;
	UnitsTransformedThisMatch = Snapshot.UnitsTransformedThisMatch;
	FinishersSpawned = Snapshot.FinishersSpawned;
	bRedPassiveUsedThisTurn = Snapshot.bRedPassiveUsedThisTurn;
	bOrangePassiveUsedThisTurn = Snapshot.bOrangePassiveUsedThisTurn;
	bGreenPassiveUsedThisTurn = Snapshot.bGreenPassiveUsedThisTurn;
	bBluePassiveUsedThisTurn = Snapshot.bBluePassiveUsedThisTurn;
	bPurplePassiveUsedThisTurn = Snapshot.bPurplePassiveUsedThisTurn;
	NextUnitPlayDiscount = Snapshot.NextUnitPlayDiscount;
	TransformsSucceededThisTurn = Snapshot.TransformsSucceededThisTurn;
	SyncPublicCardCounts();
}

// カード効果ディスパッチ(docs/architecture.md「カード効果ディスパッチ」)。
// 以前はResolveSpellEffect/ResolveUnitOnPlayEffectがEffectId文字列で分岐する
// if/elseの塊で、カードが増えるほど際限なく伸びる作りだった。
// EffectId -> ハンドラ関数 のテーブルに置き換え、新しいカード効果を足すときは
// 「Handle_XXX関数を1つ書いて、対応するGet*Handlers()のテーブルに1行足すだけ」で
// 済むようにしている。ハンドラは全て静的な関数ポインタ(状態を持たない)なので、
// Side0/Side1どちらのACGPlayerStateインスタンスに対しても同じテーブルを使い回せる。
namespace
{
	// 山札切れ時、捨て札をシャッフルして山札に戻す代わりに受けるライフ減少量
	// (docs/next-ruleset-design.md「山札切れペナルティ」。即死にはしない)。
	constexpr int32 DeckOutLifeLoss = 3;

	using FSpellEffectHandler = void(*)(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus);
	// CGStateを受け取るのは、選択式カード効果(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)で
	// CGState->PendingChoiceへ選択待ちを書き込むハンドラがあるため。
	using FUnitOnPlayEffectHandler = void(*)(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent,
		ACGGameState* CGState);
	// Opponentは死亡時に敵リーダーへダメージを与える効果(R06等)のために必要
	// (docs/next-ruleset-cards-v1.md赤)。
	using FUnitOnDeathEffectHandler = void(*)(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def, int32& OutDrawCount);
	// Opponentは敵Unitを対象にするターン終了時常在効果(B10等)のために必要。
	using FEndTurnAuraEffectHandler = void(*)(ACGPlayerState& Self, ACGPlayerState* Opponent);

	// 墓地に条件(コスト上限/Spell限定)を満たすカードが1枚でもあるか。
	// GraveyardCard選択待ちを開始する前の判定に使う(候補が無ければ選択自体が
	// 発生しないので、選択待ちを開始しない。docs/architecture.md「選択待ち(PendingChoice)の仕組み」
	// 「②墓地から1枚選ぶ」)。
	bool HasGraveyardCandidate(const ACGPlayerState& Self, int32 MaxCost, bool bRequireSpell)
	{
		for (const FName& CardId : Self.DiscardCardIds)
		{
			FCGCardDef Def;
			if (!UCGCardDatabase::FindCard(CardId, Def))
			{
				continue;
			}
			if (MaxCost >= 0 && Def.Cost > MaxCost)
			{
				continue;
			}
			if (bRequireSpell && Def.CardType != ECGCardType::Spell)
			{
				continue;
			}
			return true;
		}
		return false;
	}

	// --- Spell効果ハンドラ ---

	void Handle_OnPlayDamageTarget(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C017 結晶の欠片
	{
		// どの敵ユニット/顔面へダメージを与えるかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		// 実際のダメージ処理はACGGameMode::ResolvePendingChoiceWithTarget側で行う。
		if (!Opponent || !CGState)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::EnemyOrFaceTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayDamageTarget);
		Choice.PendingDamageAmount = Def.EffectValue + FirstSpellDamageBonus;
		Choice.PromptText = TEXT("ダメージを与える対象を選んでください");
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayDamageUnitTargetSpell(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // R05 黒鉄の抜き打ち(敵Unit限定、顔面は選べない)
	{
		// 「対象がない場合でもゲームの進行が不能にならないようにしてほしい」という
		// フィードバックへの対応。敵Unit限定(顔面を選べない)効果は、敵の場が
		// 空だと選択待ちに入ったまま誰も解決できずゲームが進行不能になるため、
		// 他の敵Unit限定効果(B02のOnPlayDebuffTarget等)と同様に候補が無ければ
		// 選択待ちに入らず何もしない。
		if (!Opponent || !CGState || Opponent->BoardUnits.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::EnemyOrFaceTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayDamageUnitTarget);
		Choice.PendingDamageAmount = Def.EffectValue + FirstSpellDamageBonus;
		Choice.bRequireUnitTarget = true;
		Choice.PromptText = TEXT("ダメージを与える敵Unitを選んでください");
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayDamageTargetByAllyUnitCount(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // G11(新)群れの猛攻
	{
		// ダメージ量は自分の場のUnit数(プレイ時点)。対象は敵Unitのみ(顔面不可)。
		if (!Opponent || !CGState || Opponent->BoardUnits.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::EnemyUnitTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayDamageTargetByAllyUnitCount);
		Choice.PendingDamageAmount = Self.BoardUnits.Num() + FirstSpellDamageBonus;
		Choice.MaxCost = -1;
		Choice.PromptText = FString::Printf(TEXT("ダメージ(%d)を与える敵ユニットを選んでください"), Choice.PendingDamageAmount);
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayHealSelf(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus)
	{
		Self.Heal(Def.EffectValue);
		Self.NotifyOwnLeaderHealed(); // 分身(緑)のLeaderHealed条件用(G12不屈の大樹)。
	}

	void Handle_Discard1Draw2(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C019 荒野の取捨選択
	{
		// どのカードを捨てるかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		// 捨てた後の2ドローはACGGameMode::ResolvePendingChoiceWithCard側で行う。
		if (!CGState || Self.HandCardIds.Num() == 0)
		{
			// 捨てられる手札が無くても2ドローだけは行う(効果自体は継続)。
			Self.DrawCard();
			Self.DrawCard();
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::HandCard;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::Discard1Draw2);
		Choice.PromptText = TEXT("捨てるカードを選んでください(捨てた後に2枚引きます)");
		CGState->PendingChoice = Choice;
	}

	void Handle_SummonApprenticeTokens(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C020彷徨う者たちの合流/G02列に加わった旅人/G11大群の号令
	{
		// 0/1に弱体化(以前は1/1)。「緑のトークンを出すスペルが強すぎる」という
		// フィードバックへの対応(docs/next-ruleset-cards-v1.md「緑」)。
		for (int32 i = 0; i < Def.EffectValue; ++i)
		{
			Self.AddBoardUnitDirect(FName(TEXT("TK_APPRENTICE")), 0, 1, false, false);
		}
	}

	void Handle_SummonToughApprenticeTokens(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // G05 群れの誕生
	{
		// 1/1に弱体化(以前は1/2)。「緑のトークンを出すスペルが強すぎる」という
		// フィードバックへの対応(docs/next-ruleset-cards-v1.md「緑」)。
		for (int32 i = 0; i < Def.EffectValue; ++i)
		{
			Self.AddBoardUnitDirect(FName(TEXT("TK_APPRENTICE_TOUGH")), 1, 1, false, false);
		}
	}

	void Handle_SummonEmberTokens(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // R08 赤熱の一閃
	{
		// TK_EMBER(赤熱の残り火)は死亡時に敵リーダーへ1ダメージを与える
		// (CardDef.EffectId=OnDeathDamageFace1、死亡処理時にFindCard経由で
		// 参照される。docs/next-ruleset-cards-v1.md「赤」参照)。
		for (int32 i = 0; i < Def.EffectValue; ++i)
		{
			Self.AddBoardUnitDirect(FName(TEXT("TK_EMBER")), 0, 1, false, false);
		}
	}

	void Handle_GrantPurchaseMana(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // O01 風の相場(Spell版。EffectValue=増加量)
	{
		Self.PurchaseMana += Def.EffectValue;
	}

	void Handle_ReturnGraveyardSpellSelfDamage1(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C021 結晶に灯る記憶
	{
		// どの墓地Spellを手札へ戻すかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		// 自傷はGameMode::ResolvePendingChoiceWithCard側で、選択解決(=戻すのに成功した)後に行う。
		if (!CGState || !HasGraveyardCandidate(Self, /*MaxCost=*/-1, /*bRequireSpell=*/true))
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::GraveyardCard;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::ReturnGraveyardSpellSelfDamage1);
		Choice.bRequireSpell = true;
		Choice.PromptText = TEXT("墓地から手札に戻すSpellを選んでください(1点自傷します)");
		CGState->PendingChoice = Choice;
	}

	// マーケットから1枚、コストを支払わず手札へ加える選択を開始する共通処理
	// (C022廃墟に転がる戦利品/O07買い付けで共有)。MaxCostは-1で無条件。
	void BeginFreeMarketFetchChoice(ACGPlayerState& Self, ACGGameState* CGState, int32 MaxCost)
	{
		if (!CGState || Self.HandCardIds.Num() >= 10)
		{
			return;
		}
		bool bHasCandidate = false;
		for (const FName& MarketCardId : CGState->GetMarketCardIds())
		{
			FCGCardDef MarketDef;
			if (UCGCardDatabase::FindCard(MarketCardId, MarketDef) && (MaxCost < 0 || MarketDef.Cost <= MaxCost))
			{
				bHasCandidate = true;
				break;
			}
		}
		if (!bHasCandidate)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::MarketCard;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::BuyFromMarketCostUnder3ToHand);
		Choice.MaxCost = MaxCost;
		Choice.PromptText = (MaxCost >= 0)
			? FString::Printf(TEXT("マーケットから手札に加えるカードを選んでください(コスト%d以下)"), MaxCost)
			: TEXT("マーケットから手札に加えるカードを選んでください");
		CGState->PendingChoice = Choice;
	}


	void Handle_BuyFromMarketCostUnder3ToHand(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C022 廃墟に転がる戦利品/O07 買い付け(EffectValue=コスト上限)
	{
		// どのマーケットカードを手札へ加えるかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		BeginFreeMarketFetchChoice(Self, CGState, Def.EffectValue);
	}

	void Handle_RerollMarketSlotSpell(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // 橙: O04 市場の噂話
	{
		// どの枠を選ぶかはプレイヤー(またはAI)が選ぶ(コスト上限なし、6枠のどれでも可)。
		if (!CGState || CGState->MarketSlots.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::MarketSlotTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::RerollMarketSlot);
		Choice.PromptText = TEXT("補充し直すマーケットの枠を選んでください");
		CGState->PendingChoice = Choice;
	}

	void Handle_RandomEnemyDamage1x4(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C023 結晶が弾く雨
	{
		if (!Opponent)
		{
			return;
		}
		for (int32 Hit = 0; Hit < 4; ++Hit)
		{
			if (Opponent->BoardUnits.Num() > 0)
			{
				const int32 RandomUnitIndex = FMath::RandRange(0, Opponent->BoardUnits.Num() - 1);
				Opponent->ApplyDamageToUnit(RandomUnitIndex, 1);
			}
			else
			{
				Opponent->ApplyDamage(1);
			}
		}
	}

	void Handle_ConditionalDamage3or2(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // C024 土壇場の号令
	{
		// どの敵ユニット/顔面へダメージを与えるかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		if (!Opponent || !CGState)
		{
			return;
		}
		const int32 Damage = ((Self.CurrentHP <= Opponent->CurrentHP) ? 3 : 2) + FirstSpellDamageBonus;
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::EnemyOrFaceTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::ConditionalDamage3or2);
		Choice.PendingDamageAmount = Damage;
		Choice.PromptText = TEXT("ダメージを与える対象を選んでください");
		CGState->PendingChoice = Choice;
	}

	// 断罪(青、次期ルール)の対象候補が1体でもいるか(条件に合わなければ選択自体を
	// 始めない。既存のHasGraveyardCandidateと同じ考え方)。
	bool HasSealCandidate(const ACGPlayerState& Opponent, int32 MaxCost)
	{
		for (const FCGBoardUnit& Unit : Opponent.BoardUnits)
		{
			// 変貌済みでも判定は変貌前のカードのコストで行う(SealUnit()と揃える)。
			const FName EffectiveCardId = Unit.OriginalCardId.IsNone() ? Unit.CardId : Unit.OriginalCardId;
			FCGCardDef Def;
			if (UCGCardDatabase::FindCard(EffectiveCardId, Def) && (MaxCost < 0 || Def.Cost <= MaxCost))
			{
				return true;
			}
		}
		return false;
	}

	// B11/B13用: コストではなく現在の攻撃力(パワー)で候補を判定する
	// (docs/next-ruleset-cards-v1.md「青」)。変貌の有無に関わらず、場に出ている
	// 実際のAtk(バフ/デバフ後の値)をそのまま見る。マイナスになっていても
	// クランプせずに比較するため、正しく「上限以下」と判定できる。
	bool HasSealCandidateByPower(const ACGPlayerState& Opponent, int32 MaxPower)
	{
		for (const FCGBoardUnit& Unit : Opponent.BoardUnits)
		{
			if (MaxPower < 0 || Unit.Atk <= MaxPower)
			{
				return true;
			}
		}
		return false;
	}

	// 断罪(青)の選択待ちを開始する共通処理。MaxCostは-1で無条件
	// (docs/game-rules-minimum.md「青」、ECGChoiceType::EnemyUnitTarget)。EffectIdMarkerは
	// ACGGameMode::ResolvePendingChoiceSealTargetが断罪成立後の追加処理(B09の購入用
	// マナ付与、B15の2体目選択)を分岐するために使う(既定はSealSpellで追加処理無し)。
	// bFilterByCurrentAtkがtrueのとき(B11/B13)、MaxCostは「現在の攻撃力の上限」として
	// 扱われる(FCGPendingChoice::bFilterByCurrentAtk参照)。
	void BeginSealChoice(ACGPlayerState& Self, ACGGameState* CGState, int32 MaxCost,
		FName EffectIdMarker = FName(CGEffectId::SealSpell), bool bFilterByCurrentAtk = false)
	{
		if (!CGState)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::EnemyUnitTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = EffectIdMarker;
		Choice.MaxCost = MaxCost;
		Choice.bFilterByCurrentAtk = bFilterByCurrentAtk;
		const TCHAR* FilterLabel = bFilterByCurrentAtk ? TEXT("攻撃力") : TEXT("コスト");
		Choice.PromptText = (MaxCost >= 0)
			? FString::Printf(TEXT("断罪する敵ユニットを選んでください(%s%d以下)"), FilterLabel, MaxCost)
			: TEXT("断罪する敵ユニットを選んでください");
		CGState->PendingChoice = Choice;
	}

	void Handle_SealSpell(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // 青: 断罪(Spell版。EffectValue=コスト上限、-1で無条件)
	{
		if (!Opponent || !HasSealCandidate(*Opponent, Def.EffectValue))
		{
			return;
		}
		BeginSealChoice(Self, CGState, Def.EffectValue);
	}

	void Handle_SealSpellByPower(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // B01 封書の術(Spell版。コストではなく現在の攻撃力(パワー)で判定。EffectValue=攻撃力上限)
	{
		if (!Opponent || !HasSealCandidateByPower(*Opponent, Def.EffectValue))
		{
			return;
		}
		BeginSealChoice(Self, CGState, Def.EffectValue, FName(CGEffectId::SealSpell), /*bFilterByCurrentAtk=*/true);
	}

	void Handle_SealSpellGrantPurchaseMana(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // B09 叡智の追放(EffectValue=コスト上限、-1で無条件)
	{
		// コインの付与はACGGameMode::ResolvePendingChoiceSealTargetが、
		// この選択(SealSpellGrantPurchaseMana)の断罪成立を確認してから行う
		// (対象がいない=選択自体が始まらない場合は付与しない)。
		if (!Opponent || !HasSealCandidate(*Opponent, Def.EffectValue))
		{
			return;
		}
		BeginSealChoice(Self, CGState, Def.EffectValue, FName(CGEffectId::SealSpellGrantPurchaseMana));
	}

	void Handle_SealSpellTwo(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // B15 双つの追放
	{
		// 2体目の選択はACGGameMode::ResolvePendingChoiceSealTargetが、1体目の
		// 断罪成立後に対象が残っていれば自動で開始する。
		if (!Opponent || !HasSealCandidate(*Opponent, -1))
		{
			return;
		}
		BeginSealChoice(Self, CGState, -1, FName(CGEffectId::SealSpellTwo));
	}

	void Handle_BuffAllyTarget(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // 紫: 対象指定の味方強化(Spell版。EffectValue=Atk/Hp増分)
	{
		// どの味方ユニットを強化するかはプレイヤー(またはAI)が選ぶ。
		if (!CGState || Self.BoardUnits.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::AllyUnitTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::BuffAllyTarget);
		Choice.PendingBuffAmount = Def.EffectValue;
		Choice.PromptText = FString::Printf(TEXT("強化する味方ユニットを選んでください(+%d/+%d)"), Def.EffectValue, Def.EffectValue);
		CGState->PendingChoice = Choice;
	}

	void Handle_BuffAllyTargetAndDraw(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // P10 秘宝の授与(強化後に1ドロー)
	{
		if (!CGState || Self.BoardUnits.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::AllyUnitTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::BuffAllyTargetAndDraw);
		Choice.PendingBuffAmount = Def.EffectValue;
		Choice.PromptText = FString::Printf(TEXT("強化する味方ユニットを選んでください(+%d/+%d、その後1ドロー)"), Def.EffectValue, Def.EffectValue);
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayDamageFaceSpell(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // 赤: 選択不要の直接顔面ダメージ(Spell版。R08赤熱の一閃)
	{
		if (Opponent)
		{
			Opponent->ApplyDamage(Def.EffectValue + FirstSpellDamageBonus);
		}
	}

	void Handle_RandomEnemyDamage2x3(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // R11 廃墟の剣戟
	{
		if (!Opponent)
		{
			return;
		}
		for (int32 Hit = 0; Hit < 3; ++Hit)
		{
			if (Opponent->BoardUnits.Num() > 0)
			{
				const int32 RandomUnitIndex = FMath::RandRange(0, Opponent->BoardUnits.Num() - 1);
				Opponent->ApplyDamageToUnit(RandomUnitIndex, Def.EffectValue);
			}
			else
			{
				Opponent->ApplyDamage(Def.EffectValue);
			}
		}
	}

	void Handle_DamageFaceByUnitCount(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // R14 決闘状
	{
		if (Opponent)
		{
			Opponent->ApplyDamage(Self.BoardUnits.Num() * Def.EffectValue + FirstSpellDamageBonus);
		}
	}

	void Handle_MassDebuffEnemies(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // B12 禁書一斉開帳(EffectValue=Atk/Hp減少量)
	{
		if (!Opponent)
		{
			return;
		}
		for (FCGBoardUnit& Unit : Opponent->BoardUnits)
		{
			Unit.Atk = FMath::Max(0, Unit.Atk - Def.EffectValue);
			Unit.Hp -= Def.EffectValue;
		}
	}

	void Handle_GrantBoardBuffOnPurchaseThisTurn(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // O10 黄金の商機
	{
		Self.bBuffBoardOnPurchaseThisTurn = true;
	}

	void Handle_GrantAllPurchasesDiscountThisTurn(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // O13 市場開放の号令
	{
		Self.bAllPurchasesDiscountedThisTurn = true;
	}

	void Handle_BuffAllAlliesFlat(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // G14 世界樹の加護(EffectValue分だけAtk/Hp共に増加)
	{
		for (FCGBoardUnit& Unit : Self.BoardUnits)
		{
			Unit.Atk += Def.EffectValue;
			Unit.Hp += Def.EffectValue;
		}
	}

	void Handle_BuffAllAlliesAtkOnly(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // G08 祈りの輪(EffectValue分だけAtkのみ増加)
	{
		for (FCGBoardUnit& Unit : Self.BoardUnits)
		{
			Unit.Atk += Def.EffectValue;
		}
	}

	void Handle_ForceTransformAllAllies(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def,
		int32 TargetUnitIndex, ACGGameState* CGState, int32 FirstSpellDamageBonus) // P13 玉座の噂
	{
		Self.ForceTransformAll(Opponent, CGState);
	}

	const TMap<FName, FSpellEffectHandler>& GetSpellEffectHandlers()
	{
		static const TMap<FName, FSpellEffectHandler> Handlers = {
			{ FName(CGEffectId::OnPlayDamageTarget), &Handle_OnPlayDamageTarget },
			{ FName(CGEffectId::OnPlayDamageUnitTarget), &Handle_OnPlayDamageUnitTargetSpell },
			{ FName(CGEffectId::OnPlayDamageTargetByAllyUnitCount), &Handle_OnPlayDamageTargetByAllyUnitCount },
			{ FName(CGEffectId::OnPlayHealSelf), &Handle_OnPlayHealSelf },
			{ FName(CGEffectId::Discard1Draw2), &Handle_Discard1Draw2 },
			{ FName(CGEffectId::SummonApprenticeTokens), &Handle_SummonApprenticeTokens },
			{ FName(CGEffectId::SummonToughApprenticeTokens), &Handle_SummonToughApprenticeTokens },
			{ FName(CGEffectId::SummonEmberTokens), &Handle_SummonEmberTokens },
			{ FName(CGEffectId::ReturnGraveyardSpellSelfDamage1), &Handle_ReturnGraveyardSpellSelfDamage1 },
			{ FName(CGEffectId::BuyFromMarketCostUnder3ToHand), &Handle_BuyFromMarketCostUnder3ToHand },
			{ FName(CGEffectId::RerollMarketSlot), &Handle_RerollMarketSlotSpell },
			{ FName(CGEffectId::RandomEnemyDamage1x4), &Handle_RandomEnemyDamage1x4 },
			{ FName(CGEffectId::ConditionalDamage3or2), &Handle_ConditionalDamage3or2 },
			{ FName(CGEffectId::SealSpell), &Handle_SealSpell },
			{ FName(CGEffectId::SealSpellByPower), &Handle_SealSpellByPower },
			{ FName(CGEffectId::SealSpellGrantPurchaseMana), &Handle_SealSpellGrantPurchaseMana },
			{ FName(CGEffectId::SealSpellTwo), &Handle_SealSpellTwo },
			{ FName(CGEffectId::GrantPurchaseMana), &Handle_GrantPurchaseMana },
			{ FName(CGEffectId::BuffAllyTarget), &Handle_BuffAllyTarget },
			{ FName(CGEffectId::BuffAllyTargetAndDraw), &Handle_BuffAllyTargetAndDraw },
			{ FName(CGEffectId::OnPlayDamageFace), &Handle_OnPlayDamageFaceSpell },
			{ FName(CGEffectId::RandomEnemyDamage2x3), &Handle_RandomEnemyDamage2x3 },
			{ FName(CGEffectId::DamageFaceByUnitCount), &Handle_DamageFaceByUnitCount },
			{ FName(CGEffectId::MassDebuffEnemies), &Handle_MassDebuffEnemies },
			{ FName(CGEffectId::GrantBoardBuffOnPurchaseThisTurn), &Handle_GrantBoardBuffOnPurchaseThisTurn },
			{ FName(CGEffectId::GrantAllPurchasesDiscountThisTurn), &Handle_GrantAllPurchasesDiscountThisTurn },
			{ FName(CGEffectId::BuffAllAlliesFlat), &Handle_BuffAllAlliesFlat },
			{ FName(CGEffectId::BuffAllAlliesAtkOnly), &Handle_BuffAllAlliesAtkOnly },
			{ FName(CGEffectId::ForceTransformAllAllies), &Handle_ForceTransformAllAllies },
		};
		return Handlers;
	}

	// --- Unit登場時効果ハンドラ ---

	void Handle_OnPlayDiscard1(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // C008 錆びた黒鉄の巨兵
	{
		// どのカードを捨てるかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		if (!CGState || Self.HandCardIds.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::HandCard;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayDiscard1);
		Choice.PromptText = TEXT("捨てるカードを選んでください");
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayReturnGraveyardCheapCard(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // C011 廃墟の蘇生司祭
	{
		// どの墓地カードを手札へ戻すかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		if (!CGState || !HasGraveyardCandidate(Self, /*MaxCost=*/1, /*bRequireSpell=*/false))
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::GraveyardCard;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayReturnGraveyardCheapCard);
		Choice.MaxCost = 1;
		Choice.PromptText = TEXT("墓地から手札に戻すカードを選んでください(コスト1以下)");
		CGState->PendingChoice = Choice;
	}

	void Handle_GraveyardToDeckBottomDraw1(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // C006 廃墟あさりの拾い屋
	{
		// どの墓地カードを山札下へ送るかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		// ドロー自体は墓地に候補が無くても行う(既存仕様どおり)。
		if (CGState && HasGraveyardCandidate(Self, /*MaxCost=*/-1, /*bRequireSpell=*/false))
		{
			FCGPendingChoice Choice;
			Choice.ChoiceType = ECGChoiceType::GraveyardCard;
			Choice.SideIndex = Self.SideIndex;
			Choice.EffectId = FName(CGEffectId::GraveyardToDeckBottomDraw1);
			Choice.PromptText = TEXT("山札の下へ送るカードを墓地から選んでください");
			CGState->PendingChoice = Choice;
		}
		Self.DrawCard();
	}

	void Handle_ScoutTop1(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // C001 廃墟を渡る斥候
	{
		// 山札の一番上を見せて、上に残すか下に送るかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		if (!CGState || Self.DeckCardIds.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::KeepOrBury;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::ScoutTop1);
		Choice.RevealedCardId = Self.DeckCardIds[0];
		Choice.PromptText = TEXT("山札の一番上を見ました。上に残しますか、下に送りますか?");
		CGState->PendingChoice = Choice;
	}

	void Handle_SealOnPlay(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 青: 断罪(Unit登場時版。EffectValue=コスト上限、-1で無条件)
	{
		if (!Opponent || !HasSealCandidate(*Opponent, Def.EffectValue))
		{
			return;
		}
		BeginSealChoice(Self, CGState, Def.EffectValue);
	}

	void Handle_SealOnPlayByPower(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 青: 断罪(Unit登場時版、現在の攻撃力で判定。EffectValue=攻撃力上限。B11/B13)
	{
		if (!Opponent || !HasSealCandidateByPower(*Opponent, Def.EffectValue))
		{
			return;
		}
		BeginSealChoice(Self, CGState, Def.EffectValue, FName(CGEffectId::SealSpell), /*bFilterByCurrentAtk=*/true);
	}

	void Handle_OnPlayGrantPurchaseMana(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // O09 船団長グラナ(EffectValue=増加量)
	{
		Self.PurchaseMana += Def.EffectValue;
	}

	void Handle_OnPlayBuffAllyTarget(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 紫: 対象指定の味方強化(Unit登場時版。EffectValue=Atk/Hp増分。P05)
	{
		// 登場した自分自身も対象に選べる(場に出た直後、既にBoardUnitsに追加済みのため)。
		if (!CGState || Self.BoardUnits.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::AllyUnitTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayBuffAllyTarget);
		Choice.PendingBuffAmount = Def.EffectValue;
		Choice.PromptText = FString::Printf(TEXT("強化する味方ユニットを選んでください(+%d/+%d)"), Def.EffectValue, Def.EffectValue);
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayForceTransformAllyTarget(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // P16(仮称): 登場時、味方Unit1体を選んで即座に変貌させる
	{
		// 登場した自分自身(P16)には変貌先が無いため対象になり得ない。他に変貌可能な
		// 味方(TransformTargetCardIdを持ち、まだ変貌していないUnit)がいなければ
		// 選択自体を開始しない(HasSealCandidate等、他の対象指定効果と同じ方針)。
		bool bHasCandidate = false;
		for (int32 i = 0; i < Self.BoardUnits.Num(); ++i)
		{
			if (Self.CanUnitTransform(i))
			{
				bHasCandidate = true;
				break;
			}
		}
		if (!CGState || !bHasCandidate)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::AllyUnitTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayForceTransformAllyTarget);
		Choice.PromptText = TEXT("変貌させる味方ユニットを選んでください");
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayBuffAllyTargetAndUnitDiscount(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // P14 宮廷の采配官
	{
		// 「手札の変貌前Unit1体のコストを1軽減」は、同名重複カードを区別する仕組みが
		// 無いため、P11取り次ぎの女官と同じ簡略化方針で「次にプレイするUnitを割引する」
		// (NextUnitPlayDiscount)に置き換えている。
		Self.NextUnitPlayDiscount += 1;
		if (!CGState || Self.BoardUnits.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::AllyUnitTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayBuffAllyTarget);
		Choice.PendingBuffAmount = Def.EffectValue;
		Choice.PromptText = FString::Printf(TEXT("強化する味方ユニットを選んでください(+%d/+%d)"), Def.EffectValue, Def.EffectValue);
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayDamageFaceUnit(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 赤: 選択不要の直接顔面ダメージ(Unit登場時版。R03/R15)
	{
		if (Opponent)
		{
			Opponent->ApplyDamage(Def.EffectValue);
		}
	}

	void Handle_OnPlayDamageUnitTargetUnit(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // R10 灼熱の一番槍(敵Unit限定、顔面は選べない)
	{
		// Handle_OnPlayDamageUnitTargetSpell(R05)と同じ理由(進行不能対策)。
		if (!Opponent || !CGState || Opponent->BoardUnits.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::EnemyOrFaceTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayDamageUnitTarget);
		Choice.PendingDamageAmount = Def.EffectValue;
		Choice.bRequireUnitTarget = true;
		Choice.PromptText = TEXT("ダメージを与える敵Unitを選んでください");
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayHealSelfUnit(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 緑: 登場時に味方リーダーを回復(Unit登場時版。G06/G15)
	{
		Self.Heal(Def.EffectValue);
		Self.NotifyOwnLeaderHealed(); // 分身(緑)のLeaderHealed条件用(G12不屈の大樹)。
	}

	void Handle_OnPlayDebuffTarget(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 青: 対象指定の敵単体デバフ(B02。EffectValue=Atk/Hp共通の増分、負の値)
	{
		// どの敵ユニットをデバフするかはプレイヤー(またはAI)が選ぶ
		// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)。
		// BuffAllyTargetの敵版で、対象がOpponentの場になる点だけが異なる。
		if (!CGState || !Opponent || Opponent->BoardUnits.Num() == 0)
		{
			return;
		}
		FCGPendingChoice Choice;
		Choice.ChoiceType = ECGChoiceType::EnemyUnitTarget;
		Choice.SideIndex = Self.SideIndex;
		Choice.EffectId = FName(CGEffectId::OnPlayDebuffTarget);
		Choice.PendingBuffAmount = Def.EffectValue;
		Choice.PromptText = FString::Printf(TEXT("デバフする敵ユニットを選んでください(%d/%d)"), Def.EffectValue, Def.EffectValue);
		CGState->PendingChoice = Choice;
	}

	void Handle_OnPlayBuffSelfIfAlliesPresent(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // G10 群像の雄叫び
	{
		// 登場時点で自分自身は既にBoardUnitsへ追加済みのため、他の味方が3体以上
		// (=BoardUnits.Num()が自分を含め4以上)いるかで判定する。
		if (Self.BoardUnits.Num() < 4)
		{
			return;
		}
		FCGBoardUnit& Self_Unit = Self.BoardUnits.Last();
		Self_Unit.Atk += Def.EffectValue;
		Self_Unit.Hp += Def.EffectValue;
	}

	void Handle_OnPlaySelfBuffAtkIfAlliesPresent(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // G04 森の猪
	{
		// Handle_OnPlayBuffSelfIfAlliesPresent(G10)のAtkのみ版。
		if (Self.BoardUnits.Num() < 4)
		{
			return;
		}
		Self.BoardUnits.Last().Atk += Def.EffectValue;
	}

	void Handle_OnPlayRerollMarketRandom(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // O11 先物師の元締め
	{
		// 次の購入の割引はDef.HasTag("Discount")からACGPlayerState::PlayCardFromHandが
		// 別途付与するため、ここではマーケットの補充のみ行う(「好きな枠」を選べる
		// O04より弱い、ランダムな1枠限定の副次効果という位置づけ)。
		if (CGState && CGState->MarketSlots.Num() > 0)
		{
			CGState->RerollMarketSlot(FMath::RandRange(0, CGState->MarketSlots.Num() - 1));
		}
	}

	void Handle_OnPlayDeployFromMarketFree(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // O15 黒鉄の買い占め屋(EffectValue=-1でコスト上限なし)
	{
		Self.BeginMarketDeployChoice(CGState, Def.EffectValue, 0);
	}

	void Handle_OnPlayGrantNextUnitPlayDiscount(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // P11 取り次ぎの女官
	{
		Self.NextUnitPlayDiscount += Def.EffectValue;
	}

	// --- フィニッシャー専用ハンドラ ---

	void Handle_OnPlayFreeMarketCards(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 橙フィニッシャー(EffectValue=枚数)
	{
		// O15黒鉄の買い占め屋と同じ「マーケットのUnitを1枚選び、コストを支払わずそのまま
		// 場に出す」選択(BeginMarketDeployChoice)を、EffectValue回連続で行う
		// (「黄色のフィニッシャーはプレイヤーがマーケットのカードを選択して無料で
		// プレイできるカードを選びたい」というフィードバックへの対応。以前は
		// プレイヤーに選ばせず先頭のUnit枠から自動でEffectValue枚デプロイしていた)。
		// 1回目の選択後、続きは`ACGGameMode::ResolvePendingChoiceWithCard`が
		// `Choice.RemainingRepeats`を見て自動で次の選択を開始する。
		Self.BeginMarketDeployChoice(CGState, -1, Def.EffectValue - 1);
	}

	void Handle_OnPlayExileAllEnemyUnits(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 青フィニッシャー
	{
		if (!Opponent)
		{
			return;
		}
		// SealUnit()は墓地へ送らず死亡時効果も発動させない「追放」そのもの
		// (docs/game-rules-minimum.md「青」)。インデックスがずれるため常に先頭を追放する。
		int32 UnusedSealedCost = 0;
		while (Opponent->BoardUnits.Num() > 0)
		{
			Opponent->SealUnit(0, UnusedSealedCost);
		}
	}

	void Handle_OnPlayBuffAllAlliesFlat(ACGPlayerState& Self, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState) // 緑フィニッシャー(EffectValue分だけAtk/Hp共に増加)
	{
		for (FCGBoardUnit& Unit : Self.BoardUnits)
		{
			Unit.Atk += Def.EffectValue;
			Unit.Hp += Def.EffectValue;
		}
	}

	const TMap<FName, FUnitOnPlayEffectHandler>& GetUnitOnPlayEffectHandlers()
	{
		static const TMap<FName, FUnitOnPlayEffectHandler> Handlers = {
			{ FName(CGEffectId::OnPlayDiscard1), &Handle_OnPlayDiscard1 },
			{ FName(CGEffectId::OnPlayReturnGraveyardCheapCard), &Handle_OnPlayReturnGraveyardCheapCard },
			{ FName(CGEffectId::GraveyardToDeckBottomDraw1), &Handle_GraveyardToDeckBottomDraw1 },
			{ FName(CGEffectId::ScoutTop1), &Handle_ScoutTop1 },
			{ FName(CGEffectId::SealOnPlay), &Handle_SealOnPlay },
			{ FName(CGEffectId::SealOnPlayByPower), &Handle_SealOnPlayByPower },
			{ FName(CGEffectId::GrantPurchaseMana), &Handle_OnPlayGrantPurchaseMana },
			{ FName(CGEffectId::OnPlayBuffAllyTarget), &Handle_OnPlayBuffAllyTarget },
			{ FName(CGEffectId::OnPlayBuffAllyTargetAndUnitDiscount), &Handle_OnPlayBuffAllyTargetAndUnitDiscount },
			{ FName(CGEffectId::OnPlayForceTransformAllyTarget), &Handle_OnPlayForceTransformAllyTarget },
			{ FName(CGEffectId::OnPlayDamageFace), &Handle_OnPlayDamageFaceUnit },
			{ FName(CGEffectId::OnPlayDamageUnitTarget), &Handle_OnPlayDamageUnitTargetUnit },
			{ FName(CGEffectId::OnPlayHealSelf), &Handle_OnPlayHealSelfUnit },
			{ FName(CGEffectId::OnPlayDebuffTarget), &Handle_OnPlayDebuffTarget },
			{ FName(CGEffectId::OnPlayBuffSelfIfAlliesPresent), &Handle_OnPlayBuffSelfIfAlliesPresent },
			{ FName(CGEffectId::OnPlaySelfBuffAtkIfAlliesPresent), &Handle_OnPlaySelfBuffAtkIfAlliesPresent },
			{ FName(CGEffectId::OnPlayRerollMarketRandom), &Handle_OnPlayRerollMarketRandom },
			{ FName(CGEffectId::OnPlayDeployFromMarketFree), &Handle_OnPlayDeployFromMarketFree },
			{ FName(CGEffectId::OnPlayGrantNextUnitPlayDiscount), &Handle_OnPlayGrantNextUnitPlayDiscount },
			{ FName(CGEffectId::OnPlayFreeMarketCards), &Handle_OnPlayFreeMarketCards },
			{ FName(CGEffectId::OnPlayExileAllEnemyUnits), &Handle_OnPlayExileAllEnemyUnits },
			{ FName(CGEffectId::OnPlayBuffAllAlliesFlat), &Handle_OnPlayBuffAllAlliesFlat },
		};
		return Handlers;
	}

	// --- Unit死亡時効果ハンドラ ---

	// 場のUnitの中で最もコストが高いものの場インデックスを返す(無ければ-1)。
	// 変貌済みでも判定は変貌前のカードのコストで行う(SealUnit()等と揃える)。
	// B10(ターン終了時デバフ)/B14(ターン開始時断罪)が対象を自動選定するのに使う。
	// MaxCost(-1で無条件)は、B14終焉の裁定者のナーフ用: 無条件だと毎ターン
	// 相手の最高コストUnitを無料で断罪し続けられて強すぎたため、コスト上限を
	// 設けられるようにしている(docs/game-rules-minimum.md「青」ナーフ経緯参照)。
	int32 FindHighestCostBoardUnitIndex(const ACGPlayerState& Side, int32 MaxCost = -1)
	{
		int32 BestIndex = -1;
		int32 BestCost = -1;
		for (int32 i = 0; i < Side.BoardUnits.Num(); ++i)
		{
			const FCGBoardUnit& Unit = Side.BoardUnits[i];
			const FName EffectiveCardId = Unit.OriginalCardId.IsNone() ? Unit.CardId : Unit.OriginalCardId;
			FCGCardDef Def;
			if (UCGCardDatabase::FindCard(EffectiveCardId, Def) && Def.Cost > BestCost
				&& (MaxCost < 0 || Def.Cost <= MaxCost))
			{
				BestCost = Def.Cost;
				BestIndex = i;
			}
		}
		return BestIndex;
	}

	void Handle_OnDeathDraw(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def, int32& OutDrawCount)
	{
		OutDrawCount += Def.EffectValue;
	}

	void Handle_OnDeathGrantPurchaseMana(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def, int32& OutDrawCount) // R03/R04/R07/R13相当
	{
		Self.PurchaseMana += Def.EffectValue;
	}

	void Handle_OnDeathReturnRandomGraveyardUnit(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def, int32& OutDrawCount) // C014 廃墟の霊廟守り
	{
		Self.TryMoveRandomDiscardUnitToDeckTop();
	}

	void Handle_OnDeathDamageFace1(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def, int32& OutDrawCount) // R06 捨て身の道場破り
	{
		if (Opponent)
		{
			Opponent->ApplyDamage(Def.EffectValue);
		}
	}

	void Handle_OnDeathDamageRandomEnemyUnit1(ACGPlayerState& Self, ACGPlayerState* Opponent, const FCGCardDef& Def, int32& OutDrawCount) // R01 黒鉄拾いの悪童
	{
		if (Opponent && Opponent->BoardUnits.Num() > 0)
		{
			const int32 RandomUnitIndex = FMath::RandRange(0, Opponent->BoardUnits.Num() - 1);
			Opponent->ApplyDamageToUnit(RandomUnitIndex, Def.EffectValue);
		}
	}

	const TMap<FName, FUnitOnDeathEffectHandler>& GetUnitOnDeathEffectHandlers()
	{
		static const TMap<FName, FUnitOnDeathEffectHandler> Handlers = {
			{ FName(CGEffectId::OnDeathDraw), &Handle_OnDeathDraw },
			{ FName(CGEffectId::OnDeathReturnRandomGraveyardUnit), &Handle_OnDeathReturnRandomGraveyardUnit },
			{ FName(CGEffectId::OnDeathDamageFace1), &Handle_OnDeathDamageFace1 },
			{ FName(CGEffectId::OnDeathDamageRandomEnemyUnit1), &Handle_OnDeathDamageRandomEnemyUnit1 },
			{ FName(CGEffectId::OnDeathGrantPurchaseMana), &Handle_OnDeathGrantPurchaseMana },
		};
		return Handlers;
	}

	// --- ターン終了時の常在効果ハンドラ(場にそのEffectIdを持つUnitがいれば発動) ---
	// 荒野の行商人(OnBuyEndTurnDiscardDraw、C007)は「どのカードを捨てるか」を
	// プレイヤーが選ぶ必要があり、かつターン終了そのものを選択完了まで保留する
	// 必要があるため、この一般的なハンドラテーブルではなく
	// ACGGameMode::RequestEndTurn()側で個別に処理している
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」参照)。

	void Handle_OnBuyEndTurnGrantPurchaseMana(ACGPlayerState& Self, ACGPlayerState* Opponent) // O12 錨を抱く老船長
	{
		if (Self.bBoughtThisTurn)
		{
			Self.PurchaseMana += 1;
		}
	}

	// Self.BoardUnitsの中から指定EffectIdを持つ最初のUnitのFCGCardDefを返す
	// (見つからなければfalse)。EffectIdの存在確認だけならHasBoardUnitWithEffect
	// で足りるが、EffectValueなど効果のパラメータ自体が必要な常在効果ハンドラ
	// (B10等)では、実際に発動した(かもしれない複数のうち先頭の)UnitのDefが要る。
	bool FindBoardUnitDefWithEffect(const ACGPlayerState& Self, FName EffectId, FCGCardDef& OutDef)
	{
		for (const FCGBoardUnit& Unit : Self.BoardUnits)
		{
			FCGCardDef Def;
			if (UCGCardDatabase::FindCard(Unit.CardId, Def) && Def.EffectId == EffectId)
			{
				OutDef = Def;
				return true;
			}
		}
		return false;
	}

	void Handle_EndTurnDebuffHighestCostEnemy(ACGPlayerState& Self, ACGPlayerState* Opponent) // B10 弱点の考察官
	{
		FCGCardDef Def;
		if (!Opponent || !FindBoardUnitDefWithEffect(Self, FName(CGEffectId::EndTurnDebuffHighestCostEnemy), Def))
		{
			return;
		}
		const int32 TargetIndex = FindHighestCostBoardUnitIndex(*Opponent);
		if (!Opponent->BoardUnits.IsValidIndex(TargetIndex))
		{
			return;
		}
		Opponent->BoardUnits[TargetIndex].Atk = FMath::Max(0, Opponent->BoardUnits[TargetIndex].Atk + Def.EffectValue);
		Opponent->BoardUnits[TargetIndex].Hp += Def.EffectValue;
	}

	const TMap<FName, FEndTurnAuraEffectHandler>& GetEndTurnAuraEffectHandlers()
	{
		static const TMap<FName, FEndTurnAuraEffectHandler> Handlers = {
			{ FName(CGEffectId::OnBuyEndTurnGrantPurchaseMana), &Handle_OnBuyEndTurnGrantPurchaseMana },
			{ FName(CGEffectId::EndTurnDebuffHighestCostEnemy), &Handle_EndTurnDebuffHighestCostEnemy },
		};
		return Handlers;
	}
}

void ACGPlayerState::InitializeStartingDeck(const TArray<FName>& StarterCardIds)
{
	DeckCardIds = StarterCardIds;
	HandCardIds.Reset();
	DiscardCardIds.Reset();
	BoardUnits.Reset();
	ComputeActiveColors();
	ShuffleDeck();
}

void ACGPlayerState::ComputeActiveColors()
{
	// しきい値17/25(次期ルール、docs/next-ruleset-design.md)。デッキの構成は
	// この時点(試合開始時)で確定するため、以後は数え直さない。
	constexpr int32 ColorThreshold = 17;

	TMap<ECGColor, int32> ColorCounts;
	for (const FName& CardId : DeckCardIds)
	{
		FCGCardDef Def;
		if (!UCGCardDatabase::FindCard(CardId, Def) || Def.Color == ECGColor::None)
		{
			continue;
		}
		ColorCounts.FindOrAdd(Def.Color)++;
	}

	ActiveColors.Reset();
	for (const TPair<ECGColor, int32>& Pair : ColorCounts)
	{
		if (Pair.Value >= ColorThreshold)
		{
			ActiveColors.Add(Pair.Key);
		}
	}
}

void ACGPlayerState::ShuffleDeck()
{
	const int32 LastIndex = DeckCardIds.Num() - 1;
	for (int32 i = 0; i <= LastIndex; ++i)
	{
		const int32 SwapIndex = FMath::RandRange(i, LastIndex);
		DeckCardIds.Swap(i, SwapIndex);
	}
}

FName ACGPlayerState::PopTopOfDeckWithReshuffle()
{
	if (DeckCardIds.Num() == 0)
	{
		if (DiscardCardIds.Num() == 0)
		{
			// 捨て札も尽きている(断罪等で持ち札の大半を失った極端なケース)。
			// リシャッフルする材料が無いため、これ以上は取り出せない。
			return NAME_None;
		}

		// 山札切れペナルティ(docs/next-ruleset-design.md): 捨て札をシャッフルして
		// 山札に戻し、ライフを減らす(即死にはしない。0以下になればApplyDamage内で
		// bIsDefeatedが立つ)。
		DeckCardIds = DiscardCardIds;
		DiscardCardIds.Reset();
		ShuffleDeck();
		ApplyDamage(DeckOutLifeLoss);
	}

	if (DeckCardIds.Num() == 0)
	{
		return NAME_None;
	}

	const FName Top = DeckCardIds[0];
	DeckCardIds.RemoveAt(0);
	return Top;
}

bool ACGPlayerState::DrawCard()
{
	const FName Top = PopTopOfDeckWithReshuffle();
	if (Top.IsNone())
	{
		// 山札・捨て札とも尽きていて、これ以上リシャッフルできない極端なケース。
		// 従来どおり山札切れ負けとして扱う。
		bIsDefeated = true;
		return false;
	}

	// フィニッシャー(青): カードを20枚ドローする。手札上限で捨て札に送られる分も
	// 「ドローした」こと自体は変わらないため、振り分け前にカウントする。
	++CardsDrawnThisMatch;

	// 手札上限10を超える分は、ルール未規定のため引いた瞬間に捨て札へ送る運用としている。
	if (HandCardIds.Num() < 10)
	{
		HandCardIds.Add(Top);
	}
	else
	{
		DiscardCardIds.Add(Top);
	}

	// 青パッシブ: このターン2回目のドローをしたとき、コイン+1
	// (docs/game-rules-minimum.md「色ガイド」)。通常のターン開始時ドローが
	// 1回目になるため、B05等の追加ドロー元があるターンに発動しやすい。
	++DrawsThisTurn;
	if (DrawsThisTurn == 2)
	{
		GrantPurchaseManaFromPassive(ECGColor::Blue, bBluePassiveUsedThisTurn);
	}

	return true;
}

FName ACGPlayerState::DrawCardForMarket()
{
	return PopTopOfDeckWithReshuffle();
}

bool ACGPlayerState::PlayCardFromHand(FName CardId, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState)
{
	if (!HandCardIds.Contains(CardId))
	{
		return false;
	}

	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return false;
	}

	// 生け贄(Sacrifice、R15黒鉄を継ぐ大剣士): 道連れにする自分のUnitが1体も
	// いなければそもそもプレイできない(docs/keywords.md「生け贄(Sacrifice)」参照)。
	if (Def.HasTag(TEXT("Sacrifice")) && BoardUnits.Num() == 0)
	{
		return false;
	}

	// P11取り次ぎの女官: 次にプレイするUnit1体のコスト軽減(Spellには適用しない)。
	// docs/architecture.md「次期ルール移行時の実装メモ」参照。
	const bool bApplyUnitPlayDiscount = (Def.CardType == ECGCardType::Unit) && NextUnitPlayDiscount > 0;
	const int32 EffectiveCost = bApplyUnitPlayDiscount ? FMath::Max(1, Def.Cost - NextUnitPlayDiscount) : Def.Cost;

	if (CurrentMana < EffectiveCost)
	{
		return false;
	}

	HandCardIds.RemoveSingle(CardId);
	CurrentMana -= EffectiveCost;
	if (bApplyUnitPlayDiscount)
	{
		NextUnitPlayDiscount = 0;
	}
	CardsPlayedThisMatch.Add(CardId);

	// 廃墟街道の突撃兵(C009)判定用: このカードが「今ターン2枚目以降」かどうかを、
	// カウンタをインクリメントする前に確定させる。
	const bool bIsSecondOrLaterPlayThisTurn = (CardsPlayedThisTurn >= 1);
	CardsPlayedThisTurn++;

	// 赤パッシブ: このターン3枚目のカードをプレイしたとき、コイン+1
	// (docs/game-rules-minimum.md「色ガイド」)。Unit/Spellどちらの
	// プレイも数える。
	if (CardsPlayedThisTurn == 3)
	{
		GrantPurchaseManaFromPassive(ECGColor::Red, bRedPassiveUsedThisTurn);
	}

	if (Def.CardType == ECGCardType::Unit)
	{
		// 生け贄(Sacrifice、R07/R13/R15): 道連れにする味方Unitをプレイヤー
		// (またはAI)に選ばせる(docs/keywords.md「生け贄(Sacrifice)」)。
		// この関数の先頭でBoardUnits.Num()==0なら既に弾いているため、ここでは
		// 必ず候補が1体以上いる。選択が解決するまでこのUnit自身は場に出さず、
		// Choice.RevealedCardIdに控えておいたCardIdをACGGameMode::
		// ResolvePendingChoiceAllyTarget側でSpawnUnitFromHandOnBoard()に渡して
		// 実際に場へ出す(SecondPlayBuff等、bIsSecondOrLaterPlayThisTurnに
		// 依存する効果は現状どの生け贄持ちカードも持たないため、選択解決後は
		// falseとして扱ってよい)。
		if (Def.HasTag(TEXT("Sacrifice")))
		{
			FCGPendingChoice Choice;
			Choice.ChoiceType = ECGChoiceType::AllyUnitTarget;
			Choice.SideIndex = SideIndex;
			Choice.EffectId = FName(CGEffectId::SacrificeAllyOnPlay);
			Choice.RevealedCardId = CardId;
			Choice.PromptText = TEXT("生け贄にする味方Unitを選んでください");
			CGState->PendingChoice = Choice;
			return true;
		}

		SpawnUnitFromHandOnBoard(CardId, Def, bIsSecondOrLaterPlayThisTurn, Opponent, CGState);
	}
	else
	{
		SpellsPlayedThisTurn++;
		ResolveSpellEffect(Def, Opponent, TargetUnitIndex, CGState);
		DiscardCardIds.Add(CardId);

		// 結晶の谺を読む射手(C010): 味方Spell使用時、1ターンに1回だけ敵リーダーへ1ダメージ。
		if (Opponent && !bAllySpellPingUsedThisTurn && HasBoardUnitWithEffect(FName(CGEffectId::OnAllySpellPing1)))
		{
			Opponent->ApplyDamage(1);
			bAllySpellPingUsedThisTurn = true;
		}
	}

	return true;
}

void ACGPlayerState::SpawnUnitFromHandOnBoard(FName CardId, const FCGCardDef& Def, bool bIsSecondOrLaterPlayThisTurn, ACGPlayerState* Opponent, ACGGameState* CGState)
{
	int32 EnterAtk = Def.Atk;
	if (Def.EffectId == FName(CGEffectId::SecondPlayBuff) && bIsSecondOrLaterPlayThisTurn) // C009 廃墟街道の突撃兵/R09 連携の立会人
	{
		EnterAtk += Def.EffectValue;
	}

	FCGBoardUnit NewUnit;
	NewUnit.CardId = CardId;
	NewUnit.Atk = EnterAtk;
	NewUnit.Hp = Def.Hp;
	NewUnit.bCanAttack = Def.HasTag(TEXT("Haste"));
	NewUnit.bHasGuard = Def.HasTag(TEXT("Guard"));
	BoardUnits.Add(NewUnit);

	// 「分身のキーワードの条件をプレイしたときに達成していることにしてほしい」
	// というフィードバックへの対応。分身条件(特にAllyUnitCountAtLeast/
	// LeaderHpAtMostのような「状態」条件)は、このユニット自身がプレイされた
	// 時点で既に満たされていることがあるため、この後のNotifyStateChanged経由の
	// CheckAllClones(盤面全体の再判定)を待たず、ここで明示的に即座判定する。
	ApplyCloneIfConditionMet(BoardUnits.Last(), Opponent, CGState);

	// 紫フィニッシャーの常在アウラ: このユニットが場にいる限り、新たに登場した
	// 味方Unitは変貌条件を無視して即座に変貌する。ForceTransformAll()は既に
	// 変貌済み/変貌先を持たないUnitには何もしないため、毎回呼んでも安全
	// (「このユニットがいる限り味方のすべてのユニットは登場時に即座に変貌する」
	// というフィードバックへの対応)。
	if (HasBoardUnitWithEffect(FName(CGEffectId::FinisherTransformAura)))
	{
		ForceTransformAll(Opponent, CGState);
	}

	// 先物(橙、Discountタグ): 登場時、コインを1増やす。専用のEffectIdを
	// 持たせず、タグから直接付与することで複数のカード(O02/O03/O06/O11/O14)が
	// 同じ効果を共有できるようにしている(以前は「次の購入コストを1軽減」
	// だったが、「先物のキーワード効果をコイン付与にしてほしい」という
	// フィードバックで変更した。docs/keywords.md「先物」参照)。
	if (Def.HasTag(TEXT("Discount")))
	{
		PurchaseMana += 1;
	}

	ResolveUnitOnPlayEffect(Def, Opponent, CGState);

	if (Def.EffectId == FName(CGEffectId::AllyBuffAtkThisTurn)) // C013 国無き旗手
	{
		// 簡易実装: 本来は「ターン中のみ」の一時バフだが、一時バフ管理の仕組みを
		// 新設するコストを避けるため、登場時点にいる味方(このユニット自身を除く)へ
		// 永続的に+1/+0を付与する形に簡略化している(docs/game-rules-minimum.md「未実装・今後の検討事項」参照)。
		for (int32 i = 0; i < BoardUnits.Num() - 1; ++i)
		{
			BoardUnits[i].Atk += 1;
		}
	}
}

void ACGPlayerState::ResolveSpellEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState)
{
	// 第六界を記す学者(C016)判定用: このSpellが「今ターン最初のSpell」かどうか
	// (呼び出し元でSpellsPlayedThisTurnをインクリメント済みのため、1なら初回)。
	const int32 FirstSpellDamageBonus =
		(SpellsPlayedThisTurn == 1 && HasBoardUnitWithEffect(FName(CGEffectId::FirstSpellBonusDamage))) ? 1 : 0;

	// 実装済みの効果のみテーブルに登録されている。未登録のEffectId(TODO_接頭辞等)は
	// 何もしない(docs/architecture.md「カード効果ディスパッチ」参照)。
	if (const FSpellEffectHandler* Handler = GetSpellEffectHandlers().Find(Def.EffectId))
	{
		(*Handler)(*this, Opponent, Def, TargetUnitIndex, CGState, FirstSpellDamageBonus);
	}
}

void ACGPlayerState::ResolveUnitOnPlayEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState)
{
	// B06「深淵の封印」: 相手の場にこのユニットがいる限り、自分のUnitの能力は
	// 発動しない(登場時効果。分身のコピー・変貌先カードの登場時効果もこの
	// 関数を経由するため、まとめて抑制される。docs/next-ruleset-cards-v1.md
	// 「青」参照)。
	if (AreUnitAbilitiesSuppressedByEnemy(Opponent))
	{
		return;
	}
	if (const FUnitOnPlayEffectHandler* Handler = GetUnitOnPlayEffectHandlers().Find(Def.EffectId))
	{
		(*Handler)(*this, Def, Opponent, CGState);
	}
}

bool ACGPlayerState::DiscardSpecificFromHand(FName CardId)
{
	if (!HandCardIds.Contains(CardId))
	{
		return false;
	}
	HandCardIds.RemoveSingle(CardId);
	DiscardCardIds.Add(CardId);
	return true;
}

bool ACGPlayerState::MoveSpecificDiscardCardToDeckBottom(FName CardId)
{
	const int32 Index = DiscardCardIds.IndexOfByKey(CardId);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	DeckCardIds.Add(CardId); // DrawCardは先頭(index 0)から引くため、末尾に追加=デッキの一番下。
	DiscardCardIds.RemoveAt(Index);
	return true;
}

bool ACGPlayerState::MoveSpecificDiscardCardToHand(FName CardId)
{
	if (HandCardIds.Num() >= 10)
	{
		return false;
	}
	const int32 Index = DiscardCardIds.IndexOfByKey(CardId);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	HandCardIds.Add(CardId);
	DiscardCardIds.RemoveAt(Index);
	return true;
}

bool ACGPlayerState::TryMoveRandomDiscardUnitToDeckTop()
{
	TArray<int32> UnitIndices;
	for (int32 i = 0; i < DiscardCardIds.Num(); ++i)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(DiscardCardIds[i], Def) && Def.CardType == ECGCardType::Unit)
		{
			UnitIndices.Add(i);
		}
	}
	if (UnitIndices.Num() == 0)
	{
		return false;
	}
	const int32 Chosen = UnitIndices[FMath::RandRange(0, UnitIndices.Num() - 1)];
	DeckCardIds.Insert(DiscardCardIds[Chosen], 0);
	DiscardCardIds.RemoveAt(Chosen);
	return true;
}

bool ACGPlayerState::MoveDeckTopToBottom()
{
	if (DeckCardIds.Num() == 0)
	{
		return false;
	}
	const FName Top = DeckCardIds[0];
	DeckCardIds.RemoveAt(0);
	DeckCardIds.Add(Top);
	return true;
}

void ACGPlayerState::AddBoardUnitDirect(FName CardId, int32 Atk, int32 Hp, bool bCanAttackImmediately, bool bHasGuard)
{
	FCGBoardUnit NewUnit;
	NewUnit.CardId = CardId;
	NewUnit.Atk = Atk;
	NewUnit.Hp = Hp;
	NewUnit.bCanAttack = bCanAttackImmediately;
	NewUnit.bHasGuard = bHasGuard;
	BoardUnits.Add(NewUnit);
}

void ACGPlayerState::BeginMarketDeployChoice(ACGGameState* CGState, int32 MaxCost, int32 RemainingRepeats)
{
	if (!CGState)
	{
		return;
	}
	bool bHasCandidate = false;
	for (const FName& MarketCardId : CGState->GetMarketCardIds())
	{
		FCGCardDef MarketDef;
		if (UCGCardDatabase::FindCard(MarketCardId, MarketDef) && MarketDef.CardType == ECGCardType::Unit
			&& (MaxCost < 0 || MarketDef.Cost <= MaxCost))
		{
			bHasCandidate = true;
			break;
		}
	}
	if (!bHasCandidate)
	{
		return;
	}
	FCGPendingChoice Choice;
	Choice.ChoiceType = ECGChoiceType::MarketCard;
	Choice.SideIndex = SideIndex;
	Choice.EffectId = FName(CGEffectId::OnPlayDeployFromMarketFree);
	Choice.MaxCost = MaxCost;
	Choice.RemainingRepeats = RemainingRepeats;
	Choice.PromptText = TEXT("場に出すマーケットのUnitを選んでください");
	CGState->PendingChoice = Choice;
}

bool ACGPlayerState::BuyCard(FName CardId)
{
	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(CardId, Def))
	{
		return false;
	}

	// 割引の下限は0(割引を積み重ねれば無料購入もあり得る。docs/game-rules-
	// minimum.md「橙」参照)。
	const int32 EffectiveCost = FMath::Max(0, Def.Cost - ComputeCurrentPurchaseDiscount());
	if (CurrentMana + PurchaseMana < EffectiveCost)
	{
		return false;
	}

	// 橙パッシブ: このターン最初の購入をしたとき、コイン+1
	// (docs/game-rules-minimum.md「色ガイド」)。bBoughtThisTurnを更新する
	// 前に判定することで「最初の購入」を確定する。この購入自体の支払いには
	// 使えない(効果が発生するのは購入が成立した後のため)。
	if (!bBoughtThisTurn)
	{
		GrantPurchaseManaFromPassive(ECGColor::Orange, bOrangePassiveUsedThisTurn);
	}

	// 手札に入れるか山札の一番下に送るかは、購入後にプレイヤー/AIが選ぶ
	// (ACGGameMode::RequestBuyCard、ECGChoiceType::BuyDestination)。手札上限に
	// 達していても、山札へ送る狙いで購入できるようにするため、ここでは手札上限を
	// チェックしない。
	// 購入専用のPurchaseManaを優先して使い、残りを通常のCurrentManaから払う
	// (docs/game-rules-minimum.md「色ガイド」)。
	const int32 FromPurchaseMana = FMath::Min(PurchaseMana, EffectiveCost);
	PurchaseMana -= FromPurchaseMana;
	CurrentMana -= (EffectiveCost - FromPurchaseMana);
	bBoughtThisTurn = true;
	++CardsPurchasedThisMatch; // フィニッシャー(橙): マーケットから10枚購入

	// 市場の設営係(O05)/増強(Reinforceキーワード、O14/O17): 自分が購入するたび、
	// この効果・キーワードを持つ自分の場のユニットを強化する(常在効果)。
	// 「このターン中のみ」という原設計は、国無き旗手(C013)と同じ簡略化方針
	// (docs/architecture.md)に合わせて永続強化にしている。増強はO14専用の
	// 固定EffectIdだったものを、複数カードに配布しやすい汎用キーワードへ
	// 一般化したもの(docs/keywords.md「増強(Reinforce)」参照。「橙に新しい
	// キーワードを追加して改善したい」というフィードバックへの対応)。
	for (FCGBoardUnit& Unit : BoardUnits)
	{
		FCGCardDef UnitDef;
		if (!UCGCardDatabase::FindCard(Unit.CardId, UnitDef))
		{
			continue;
		}
		if (UnitDef.EffectId == FName(CGEffectId::OnBuyBuffSelfAtk))
		{
			Unit.Atk += UnitDef.EffectValue;
		}
		else if (UnitDef.HasTag(TEXT("Reinforce")))
		{
			Unit.Hp += 1;
		}
	}

	// 黄金の商機(O10、次期ルール): このターン中、購入するたびに場のUnit全てが+1/+0する。
	if (bBuffBoardOnPurchaseThisTurn)
	{
		for (FCGBoardUnit& Unit : BoardUnits)
		{
			Unit.Atk += 1;
		}
	}

	return true;
}

int32 ACGPlayerState::ComputeCurrentPurchaseDiscount() const
{
	int32 Discount = 0;
	// 荒野の物々交換人(C012)が場にいる間。
	if (HasBoardUnitWithEffect(FName(CGEffectId::BuyCostReductionThisTurn)))
	{
		Discount += 1;
	}
	// 市場開放の号令(O13、次期ルール): このターン中、購入コストが全て1軽減される。
	if (bAllPurchasesDiscountedThisTurn)
	{
		Discount += 1;
	}
	return Discount;
}

void ACGPlayerState::ApplyDamage(int32 Amount)
{
	CurrentHP = FMath::Max(0, CurrentHP - Amount);
	if (CurrentHP <= 0)
	{
		bIsDefeated = true;
	}
}

void ACGPlayerState::Heal(int32 Amount)
{
	CurrentHP = FMath::Min(MaxHP, CurrentHP + Amount);
}

void ACGPlayerState::ApplyDamageToUnit(int32 UnitIndex, int32 Amount)
{
	if (!BoardUnits.IsValidIndex(UnitIndex))
	{
		return;
	}
	BoardUnits[UnitIndex].Hp -= Amount;
}

int32 ACGPlayerState::RemoveDeadUnitsAndGetDeathDrawCount(ACGPlayerState* Opponent, ACGGameState* CGState)
{
	int32 DrawCount = 0;
	for (int32 i = BoardUnits.Num() - 1; i >= 0; --i)
	{
		if (BoardUnits[i].Hp > 0)
		{
			continue;
		}

		// 変貌済みなら、墓地には変貌前のカードとして戻す(場を離れたら変貌が解ける。
		// docs/next-ruleset-design.md「変貌の詳細ルール」)。
		const FName EffectiveCardId = BoardUnits[i].OriginalCardId.IsNone()
			? BoardUnits[i].CardId
			: BoardUnits[i].OriginalCardId;

		FCGCardDef Def;
		const bool bFound = UCGCardDatabase::FindCard(EffectiveCardId, Def);

		DiscardCardIds.Add(EffectiveCardId);
		BoardUnits.RemoveAt(i);
		++AlliesDiedThisMatch; // フィニッシャー(赤): 味方Unitが10体死亡

		// 常在アウラ: 場のUnitが1体死亡するたび(自分・相手どちらの死亡でも)、
		// アウラを持つ側が敵リーダーへ1ダメージ。死んだユニット自身がアウラの
		// 持ち主だった場合は既に場から除かれているため発動しない(R12と同じ扱い)。
		// B06「深淵の封印」: アウラの持ち主から見た相手がB06を出していれば発動しない。
		if (Opponent)
		{
			if (!AreUnitAbilitiesSuppressedByEnemy(Opponent) && HasBoardUnitWithEffect(FName(CGEffectId::OnAnyUnitDeathDamageFace1)))
			{
				Opponent->ApplyDamage(1);
			}
			if (!Opponent->AreUnitAbilitiesSuppressedByEnemy(this) && Opponent->HasBoardUnitWithEffect(FName(CGEffectId::OnAnyUnitDeathDamageFace1)))
			{
				ApplyDamage(1);
			}
		}

		if (bFound && !AreUnitAbilitiesSuppressedByEnemy(Opponent))
		{
			if (const FUnitOnDeathEffectHandler* Handler = GetUnitOnDeathEffectHandlers().Find(Def.EffectId))
			{
				(*Handler)(*this, Opponent, Def, DrawCount);
			}
		}

		// 変貌(紫)のAlliesDiedSinceSummon条件用/黒鉄の刀鍛冶(R12)の「自分の他の
		// Unit死亡時」アウラ用: 残っている味方Unit全てを走査する。前者は死亡数に
		// 応じた変貌進行度、後者は生き残っているR12自身が敵リーダーへダメージを
		// 与えるトリガー(このユニット自身の死亡時ではなく「他のUnitが死んだとき」
		// 発動するため、OnDeathハンドラではなくこちらの生存者ループで扱う)。
		for (FCGBoardUnit& Survivor : BoardUnits)
		{
			FCGCardDef SurvivorDef;
			if (!UCGCardDatabase::FindCard(Survivor.CardId, SurvivorDef))
			{
				continue;
			}
			if (SurvivorDef.TransformConditionId == FName(CGTransformConditionId::AlliesDiedSinceSummon))
			{
				++Survivor.TransformProgress;
				ApplyTransformIfConditionMet(Survivor, Opponent, CGState);
			}
			// B06「深淵の封印」: 相手がB06を出していればR12のアウラも発動しない。
			if (SurvivorDef.EffectId == FName(CGEffectId::OnAllyDeathDamageFace1) && Opponent
				&& !AreUnitAbilitiesSuppressedByEnemy(Opponent)) // R12 黒鉄の刀鍛冶
			{
				Opponent->ApplyDamage(SurvivorDef.EffectValue);
			}
		}
	}
	return DrawCount;
}

bool ACGPlayerState::SealUnit(int32 UnitIndex, int32& OutSealedCost)
{
	if (!BoardUnits.IsValidIndex(UnitIndex))
	{
		OutSealedCost = 0;
		return false;
	}

	// 変貌済みでも、コストは変貌前のカードのもので算出する(断罪されたカードは
	// 二度と使えなくなるため、どちらの見た目で計算しても実質的な差は無いが、
	// 「元のカードを断罪した」という扱いに揃える。docs/next-ruleset-design.md
	// 「変貌の詳細ルール」)。
	const FName EffectiveCardId = BoardUnits[UnitIndex].OriginalCardId.IsNone()
		? BoardUnits[UnitIndex].CardId
		: BoardUnits[UnitIndex].OriginalCardId;

	FCGCardDef Def;
	UCGCardDatabase::FindCard(EffectiveCardId, Def);
	OutSealedCost = Def.Cost;

	// 追放: 墓地に送らず、死亡時効果も発動させない(FIN_BLUE専用、docs/keywords.md
	// 「断罪(Condemn)」参照)。
	BoardUnits.RemoveAt(UnitIndex);
	return true;
}

bool ACGPlayerState::CondemnUnit(int32 UnitIndex, int32& OutCost)
{
	if (!BoardUnits.IsValidIndex(UnitIndex))
	{
		OutCost = 0;
		return false;
	}

	// 変貌済みでも、コストは変貌前のカードのもので算出する(SealUnit()と揃える)。
	const FName EffectiveCardId = BoardUnits[UnitIndex].OriginalCardId.IsNone()
		? BoardUnits[UnitIndex].CardId
		: BoardUnits[UnitIndex].OriginalCardId;

	FCGCardDef Def;
	UCGCardDatabase::FindCard(EffectiveCardId, Def);
	OutCost = Def.Cost;

	// 断罪: Hpを0にするだけで、実際の死亡処理(墓地送り・死亡時効果・死亡数
	// カウント)は呼び出し側のResolveDeathsForBothSides()に任せる(通常の
	// 戦闘・カード効果によるダメージ死と同じ扱いにするため。「ユニットを
	// 死亡させるキーワード能力にしてほしい」というフィードバックへの対応)。
	BoardUnits[UnitIndex].Hp = 0;
	return true;
}

void ACGPlayerState::GrantPurchaseManaFromPassive(ECGColor Color, bool& bUsedThisTurnFlag)
{
	if (ActiveColors.Contains(Color) && !bUsedThisTurnFlag)
	{
		++PurchaseMana;
		bUsedThisTurnFlag = true;
	}
}

namespace
{
	// 変貌条件(CGTransformConditionId)ごとの判定関数テーブル。TurnsInPlay/
	// SurvivedAttacks/AlliesDiedSinceSummon/BuffedCountはいずれも「進行度が
	// 閾値以上か」という同じ判定だが、条件を増やしやすいようテーブルの1エントリ
	// として扱う(新しい条件を増やす場合はCGTypes.hに定数を追加しここへ1行足す
	// だけでよい)。RandomChancePerTurnはOnTurnStartTransformTick()がターン開始時に
	// 1回だけ抽選する特殊な条件のため、このテーブルには含めない。
	using FTransformConditionPredicate = bool(*)(const ACGPlayerState&, const FCGBoardUnit&, int32);

	bool Predicate_ProgressAtLeast(const ACGPlayerState&, const FCGBoardUnit& Unit, int32 Value)
	{
		return Unit.TransformProgress >= Value;
	}

	bool Predicate_HandSizeAtMost(const ACGPlayerState& Self, const FCGBoardUnit&, int32 Value)
	{
		return Self.HandCardIds.Num() <= Value;
	}

	bool Predicate_TransformsThisTurnAtLeast(const ACGPlayerState& Self, const FCGBoardUnit&, int32 Value)
	{
		return Self.TransformsSucceededThisTurn >= Value;
	}

	const TMap<FName, FTransformConditionPredicate>& GetTransformConditionPredicates()
	{
		static const TMap<FName, FTransformConditionPredicate> Predicates = {
			{ FName(CGTransformConditionId::TurnsInPlay), &Predicate_ProgressAtLeast },
			{ FName(CGTransformConditionId::SurvivedAttacks), &Predicate_ProgressAtLeast },
			{ FName(CGTransformConditionId::AlliesDiedSinceSummon), &Predicate_ProgressAtLeast },
			{ FName(CGTransformConditionId::BuffedCount), &Predicate_ProgressAtLeast },
			{ FName(CGTransformConditionId::HandSizeAtMost), &Predicate_HandSizeAtMost },
			{ FName(CGTransformConditionId::TransformsThisTurnCount), &Predicate_TransformsThisTurnAtLeast },
		};
		return Predicates;
	}

	// 分身条件(CGCloneConditionId)ごとの判定関数テーブル。GetTransformConditionPredicates()と
	// 同じ考え方(docs/next-ruleset-cards-v1.md「緑」)。
	using FCloneConditionPredicate = bool(*)(const ACGPlayerState&, const FCGBoardUnit&, int32);

	bool Predicate_CloneProgressAtLeast(const ACGPlayerState&, const FCGBoardUnit& Unit, int32 Value)
	{
		return Unit.CloneProgress >= Value;
	}

	bool Predicate_AllyUnitCountAtLeast(const ACGPlayerState& Self, const FCGBoardUnit&, int32 Value)
	{
		// 「ユニット数に自分は含まない」というフィードバックを受け、判定対象の
		// このユニット自身(常に場に存在し、Self.BoardUnitsに含まれている)を
		// 1体分差し引いた「自分以外の味方Unit数」で判定する。
		return (Self.BoardUnits.Num() - 1) >= Value;
	}

	bool Predicate_LeaderHpAtMost(const ACGPlayerState& Self, const FCGBoardUnit&, int32 Value)
	{
		return Self.CurrentHP <= Value;
	}

	const TMap<FName, FCloneConditionPredicate>& GetCloneConditionPredicates()
	{
		static const TMap<FName, FCloneConditionPredicate> Predicates = {
			{ FName(CGCloneConditionId::AllyUnitCountAtLeast), &Predicate_AllyUnitCountAtLeast },
			{ FName(CGCloneConditionId::SurvivedAttack), &Predicate_CloneProgressAtLeast },
			{ FName(CGCloneConditionId::AttackedAndSurvived), &Predicate_CloneProgressAtLeast },
			{ FName(CGCloneConditionId::LeaderHealed), &Predicate_CloneProgressAtLeast },
			{ FName(CGCloneConditionId::LeaderHpAtMost), &Predicate_LeaderHpAtMost },
		};
		return Predicates;
	}
}

void ACGPlayerState::PerformTransform(FCGBoardUnit& Unit, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState)
{
	FCGCardDef TargetDef;
	UCGCardDatabase::FindCard(Def.TransformTargetCardId, TargetDef);

	// 変貌前カードの基本ステータスとの差分(バフ/デバフによる増減、被ダメージに
	// よる減少)を求め、変貌先の基本ステータスへそのまま加算して引き継ぐ
	// (「変貌時にバフやダメージがリセットされていたので、変貌後にも引き継がれる
	// ようにしてほしい」というフィードバックへの対応。以前は変貌先の基本値へ
	// 単純に上書きしており、変貌前に受けていたバフ・ダメージが失われていた)。
	// HpはMaxHpを持たず「現在の残り体力」をそのまま表すフィールドのため
	// (FCGBoardUnit::Hp、Handle_OnPlayBuffAllAlliesFlat等のバフも同じ値を
	// 直接増減させている)、基本値との差がそのまま「ダメージなら負、バフなら正」
	// の増減量になる。
	FCGCardDef PreTransformDef;
	UCGCardDatabase::FindCard(Unit.CardId, PreTransformDef);
	const int32 AtkDelta = Unit.Atk - PreTransformDef.Atk;
	const int32 HpDelta = Unit.Hp - PreTransformDef.Hp;

	Unit.OriginalCardId = Unit.CardId;
	Unit.CardId = Def.TransformTargetCardId;
	Unit.Atk = TargetDef.Atk + AtkDelta;
	Unit.Hp = TargetDef.Hp + HpDelta;
	Unit.bHasGuard = TargetDef.HasTag(TEXT("Guard")); // P12T「王座に迫る者」等、変貌先だけが庇護を持つケース。
	Unit.TransformProgress = 0;
	++TransformsSucceededThisTurn;
	++UnitsTransformedThisMatch; // フィニッシャー(紫): ユニットが5体変貌

	// 変貌先カード自身が持つ登場時効果(P06T/P15T等、docs/next-ruleset-cards-v1.md
	// 「変貌先カード」)は、通常のUnit登場時効果と同じディスパッチテーブル
	// (GetUnitOnPlayEffectHandlers())をそのまま再利用する。新しい変貌先カードに
	// 登場時効果を持たせる場合も、他のUnitと同じ「EffectId定数を追加しHandle_XXXを
	// テーブル登録する」手順だけで済み、専用の分岐は不要。
	ResolveUnitOnPlayEffect(TargetDef, Opponent, CGState);

	// 紫パッシブ: 変貌が成功したとき、コイン+1(1ターン1回。
	// docs/game-rules-minimum.md「色ガイド」)。複数体が同時に変貌しても
	// 1ターンに1回までしか発動しない(他の4色と条件を揃えるため)。
	GrantPurchaseManaFromPassive(ECGColor::Purple, bPurplePassiveUsedThisTurn);
}

void ACGPlayerState::ApplyTransformIfConditionMet(FCGBoardUnit& Unit, ACGPlayerState* Opponent, ACGGameState* CGState)
{
	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(Unit.CardId, Def) || Def.TransformTargetCardId.IsNone())
	{
		return;
	}

	const FTransformConditionPredicate* Predicate = GetTransformConditionPredicates().Find(Def.TransformConditionId);
	if (Predicate && (*Predicate)(*this, Unit, Def.TransformConditionValue))
	{
		PerformTransform(Unit, Def, Opponent, CGState);
	}
}

void ACGPlayerState::PerformClone(FCGBoardUnit& Unit, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState)
{
	// このユニットにつき生涯で1回だけ(docs/next-ruleset-cards-v1.md「緑」)。
	Unit.bHasCloned = true;

	// コピーは元カードの素の基本ステータスのみを持ち、バフ・デバフもキーワード
	// (疾駆/庇護/分身自身を含む)も一切持たない。bHasCloned=trueで生成することで、
	// 同じCardId(=同じCloneConditionId)を持つコピー自身がさらに分身することを防ぐ。
	FCGBoardUnit Copy;
	Copy.CardId = Unit.CardId;
	Copy.Atk = Def.Atk;
	Copy.Hp = Def.Hp;
	Copy.bCanAttack = false;
	Copy.bHasGuard = false;
	Copy.bHasCloned = true;
	BoardUnits.Add(Copy);

	// 登場時効果はコピーが場に出たときにも発動する(docs/next-ruleset-cards-v1.md「緑」。
	// 通常のUnit登場時効果ハンドラ(ResolveUnitOnPlayEffect)をそのまま再利用する)。
	ResolveUnitOnPlayEffect(Def, Opponent, CGState);
}

void ACGPlayerState::ApplyCloneIfConditionMet(FCGBoardUnit& Unit, ACGPlayerState* Opponent, ACGGameState* CGState)
{
	if (Unit.bHasCloned)
	{
		return;
	}

	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(Unit.CardId, Def) || Def.CloneConditionId.IsNone())
	{
		return;
	}

	const FCloneConditionPredicate* Predicate = GetCloneConditionPredicates().Find(Def.CloneConditionId);
	if (Predicate && (*Predicate)(*this, Unit, Def.CloneConditionValue))
	{
		PerformClone(Unit, Def, Opponent, CGState);
	}
}

void ACGPlayerState::CheckAllClones(ACGPlayerState* Opponent, ACGGameState* CGState)
{
	// PerformClone()がBoardUnits.Add()で配列を伸ばすことがあるため(分身の結果)、
	// 範囲for文ではなく都度Num()を再評価するインデックスループにする
	// (新しく追加されたコピーはbHasCloned=trueなので、走査対象に入っても何もしない)。
	for (int32 i = 0; i < BoardUnits.Num(); ++i)
	{
		ApplyCloneIfConditionMet(BoardUnits[i], Opponent, CGState);
	}
}

void ACGPlayerState::NotifyOwnLeaderHealed()
{
	for (FCGBoardUnit& Unit : BoardUnits)
	{
		if (Unit.bHasCloned)
		{
			continue;
		}
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(Unit.CardId, Def) && Def.CloneConditionId == FName(CGCloneConditionId::LeaderHealed))
		{
			++Unit.CloneProgress;
		}
	}
}

void ACGPlayerState::OnTurnStartTransformTick(ACGPlayerState* Opponent, ACGGameState* CGState)
{
	TransformsSucceededThisTurn = 0;

	for (FCGBoardUnit& Unit : BoardUnits)
	{
		FCGCardDef Def;
		if (!UCGCardDatabase::FindCard(Unit.CardId, Def) || Def.TransformTargetCardId.IsNone())
		{
			continue;
		}

		if (Def.TransformConditionId == FName(CGTransformConditionId::TurnsInPlay))
		{
			++Unit.TransformProgress;
		}
		else if (Def.TransformConditionId == FName(CGTransformConditionId::RandomChancePerTurn))
		{
			if (FMath::RandRange(1, 100) <= Def.TransformConditionValue)
			{
				PerformTransform(Unit, Def, Opponent, CGState);
			}
		}
	}

	// TurnsInPlay系で進行度を+1したユニットについて、条件達成を判定する
	// (RandomChancePerTurnで既に変貌したユニットはTransformTargetCardIdが
	// 空になっているため、ApplyTransformIfConditionMetは何もしない)。
	for (FCGBoardUnit& Unit : BoardUnits)
	{
		ApplyTransformIfConditionMet(Unit, Opponent, CGState);
	}
}

void ACGPlayerState::CheckAllTransforms(ACGPlayerState* Opponent, ACGGameState* CGState)
{
	for (FCGBoardUnit& Unit : BoardUnits)
	{
		ApplyTransformIfConditionMet(Unit, Opponent, CGState);
	}
}

void ACGPlayerState::ForceTransformAll(ACGPlayerState* Opponent, ACGGameState* CGState)
{
	for (int32 i = 0; i < BoardUnits.Num(); ++i)
	{
		ForceTransformUnit(i, Opponent, CGState);
	}
}

bool ACGPlayerState::CanUnitTransform(int32 UnitIndex) const
{
	if (!BoardUnits.IsValidIndex(UnitIndex))
	{
		return false;
	}
	FCGCardDef Def;
	return UCGCardDatabase::FindCard(BoardUnits[UnitIndex].CardId, Def) && !Def.TransformTargetCardId.IsNone();
}

void ACGPlayerState::ForceTransformUnit(int32 UnitIndex, ACGPlayerState* Opponent, ACGGameState* CGState)
{
	if (!BoardUnits.IsValidIndex(UnitIndex))
	{
		return;
	}
	FCGCardDef Def;
	if (!UCGCardDatabase::FindCard(BoardUnits[UnitIndex].CardId, Def) || Def.TransformTargetCardId.IsNone())
	{
		return;
	}
	PerformTransform(BoardUnits[UnitIndex], Def, Opponent, CGState);
}

void ACGPlayerState::CheckAndSpawnFinisher(ACGPlayerState* Opponent, ACGGameState* CGState)
{
	if (!CGState)
	{
		return;
	}

	struct FFinisherRule
	{
		ECGColor Color;
		bool bConditionMet;
	};
	// 緑(場に8体)だけはBoardUnits.Num()をここで直接見る。他の4色は試合を通じた
	// 累計カウンタ(ACGGameMode::NotifyStateChangedの度にこの関数が呼ばれるため、
	// ドロー/購入/死亡/変貌のたびに再判定される)。
	// 緑6体/青ドロー20枚は3000戦シミュレーションで緑の発動が突出して多く
	// (最強色をさらに強化)、青の発動が最少(発動しても勝率改善に届かず)と
	// 判明したため緑8体/青ドロー15枚に調整したが、15枚は平均試合長(19ターン
	// 前後)に対して緩すぎて青が今度は過剰補正で最強色になったため、
	// 18枚に再調整した(docs/next-ruleset-simulation-v1.md「第16回」参照)。
	//
	// 後手は各条件の必要数を一律-2する(先攻優位の緩和策。ドロー+1/コイン+1/
	// 一時マナ+1と同じ「後手にハンデを与える」方針の延長。第21回の-1では
	// 先攻61.1%までしか下がらなかったため-2に強化した。docs/next-ruleset-
	// simulation-v1.md「第22回」参照)。
	const int32 SecondPlayerBonus = bWentSecond ? 2 : 0;
	const FFinisherRule Rules[] = {
		{ ECGColor::Red,    AlliesDiedThisMatch >= 10 - SecondPlayerBonus },
		// 「カードの効果でマーケットから購入された場合でも橙のフィニッシャーの
		// カウントが増えるようにしてほしい」という対応で購入経路を増やしたのに
		// 合わせ、「橙のフィニッシャーの条件を1減らしてほしい」というフィード
		// バックを受け、しきい値を10→9にした。
		{ ECGColor::Orange, CardsPurchasedThisMatch >= 9 - SecondPlayerBonus },
		{ ECGColor::Blue,   CardsDrawnThisMatch >= 18 - SecondPlayerBonus },
		{ ECGColor::Green,  BoardUnits.Num() >= 8 - SecondPlayerBonus },
		{ ECGColor::Purple, UnitsTransformedThisMatch >= 5 - SecondPlayerBonus },
	};

	for (const FFinisherRule& Rule : Rules)
	{
		// フィニッシャーは、その色を実際にアクティブにしている(デッキの17/25以上を
		// 占める)プレイヤーにしか駆けつけない。無関係な色の条件を偶然満たしても
		// (例えば紫デッキが手札事故で大量に処分して赤条件を満たす、等)発動しない。
		if (!Rule.bConditionMet || !ActiveColors.Contains(Rule.Color) || FinishersSpawned.Contains(Rule.Color))
		{
			continue;
		}

		const FName FinisherCardId = UCGCardDatabase::GetFinisherCardId(Rule.Color);
		FCGCardDef Def;
		if (FinisherCardId.IsNone() || !UCGCardDatabase::FindCard(FinisherCardId, Def))
		{
			continue;
		}

		FinishersSpawned.Add(Rule.Color);
		AddBoardUnitDirect(FinisherCardId, Def.Atk, Def.Hp, Def.HasTag(TEXT("Haste")), Def.HasTag(TEXT("Guard")));
		ResolveUnitOnPlayEffect(Def, Opponent, CGState);

		// 行動ログ(次期ルール、docs/game-rules-minimum.md「行動ログ」参照)。
		CGState->AppendActionLog(SideIndex, FString::Printf(TEXT("フィニッシャー「%s」が登場！"), *Def.CardName));

		UE_LOG(LogCardGame, Log, TEXT("CheckAndSpawnFinisher: Side=%d Color=%s Card=%s"),
			SideIndex, *UEnum::GetValueAsString(Rule.Color), *FinisherCardId.ToString());
	}
}

void ACGPlayerState::NotifySealSucceeded(ACGPlayerState* Opponent)
{
	// B06「深淵の封印」: 相手がB06を出していればB04/B08のアウラも発動しない。
	if (AreUnitAbilitiesSuppressedByEnemy(Opponent))
	{
		return;
	}
	if (Opponent && HasBoardUnitWithEffect(FName(CGEffectId::OnSealDamageFace1))) // B04 頁繰りの魔道士
	{
		Opponent->ApplyDamage(1);
	}
	if (HasBoardUnitWithEffect(FName(CGEffectId::OnSealDraw1))) // B08 万年七年生
	{
		DrawCard();
	}
}

void ACGPlayerState::ApplyOnTurnStartAuraEffects(ACGPlayerState* Opponent, ACGGameState* CGState)
{
	// B06「深淵の封印」: 相手がB06を出していれば、ターン開始時アウラ(B05/O08/B14等)は
	// いずれも発動しない。ただし色パッシブ(下記の緑コイン+1)はUnitの能力では
	// ないため、このループだけを抑制し関数全体は抜けない。
	if (!AreUnitAbilitiesSuppressedByEnemy(Opponent))
	{
		for (const FCGBoardUnit& Unit : BoardUnits)
		{
			FCGCardDef Def;
			if (!UCGCardDatabase::FindCard(Unit.CardId, Def))
			{
				continue;
			}
			if (Def.EffectId == FName(CGEffectId::OnTurnStartDraw)) // B05 荒野の記録係
			{
				DrawCard();
			}
			else if (Def.EffectId == FName(CGEffectId::OnTurnStartGrantPurchaseMana)) // O08 風の航路士
			{
				PurchaseMana += Def.EffectValue;
			}
			else if (Def.EffectId == FName(CGEffectId::OnTurnStartSealHighestCostEnemy) && Opponent) // B14 終焉の裁定者
			{
				// ナーフ: 無条件だと毎ターン相手の最高コストUnitを無料で断罪し続け
				// られて強すぎたため、コスト上限(Def.EffectValue)を設けている
				// (docs/game-rules-minimum.md「青」、docs/next-ruleset-simulation-v1.md)。
				// 断罪(旧「封印」)自体は青パッシブの発動条件(2回目のドロー)とは
				// 無関係になったため、ここではパッシブを呼ばない。B04のOnSealDamageFace1
				// アウラは断罪元(=この側)が持っていれば発動するため、成功時に通知する。
				// 断罪は通常の死亡処理を経由するため、ここで死亡処理も確定させる
				// (docs/keywords.md「断罪(Condemn)」参照)。
				const int32 TargetIndex = FindHighestCostBoardUnitIndex(*Opponent, Def.EffectValue);
				int32 CondemnedCost = 0;
				if (Opponent->CondemnUnit(TargetIndex, CondemnedCost))
				{
					NotifySealSucceeded(Opponent);

					// 断罪された側(Opponent)の死亡時ドローを先に反映してから、
					// 巻き添え(例: 相手のR01のような「死亡時、敵のランダムなUnit1体に
					// ダメージ」)で自分の場にも死亡が生まれていないか確認する
					// (ACGGameMode::ResolveDeathsForBothSidesと同じ考え方だが、
					// ACGPlayerStateからは呼べないためここで同等の処理を行う)。
					const int32 OpponentDeathDraws = Opponent->RemoveDeadUnitsAndGetDeathDrawCount(this, CGState);
					for (int32 i = 0; i < OpponentDeathDraws; ++i)
					{
						Opponent->DrawCard();
					}
					const int32 SelfDeathDraws = RemoveDeadUnitsAndGetDeathDrawCount(Opponent, CGState);
					for (int32 i = 0; i < SelfDeathDraws; ++i)
					{
						DrawCard();
					}
				}
			}
		}
	}

	// 緑パッシブ: 自分のターン開始時、場にUnitが3体以上いればコイン+1
	// (1ターン1回。docs/game-rules-minimum.md「色ガイド」)。
	if (BoardUnits.Num() >= 3)
	{
		GrantPurchaseManaFromPassive(ECGColor::Green, bGreenPassiveUsedThisTurn);
	}
}

bool ACGPlayerState::HasGuardUnit() const
{
	return BoardUnits.ContainsByPredicate([](const FCGBoardUnit& Unit) { return Unit.bHasGuard; });
}

int32 ACGPlayerState::GetEffectiveAtk(int32 UnitIndex) const
{
	if (!BoardUnits.IsValidIndex(UnitIndex))
	{
		return 0;
	}
	// 緑パッシブはコイン+1に変更され、攻撃力への継続ボーナスは無くなった
	// (docs/game-rules-minimum.md「色ガイド」)。この関数自体は combat/AI/UI
	// の呼び出し箇所を変えずに済むよう、素のAtkを返すだけの形で残している。
	return BoardUnits[UnitIndex].Atk;
}

bool ACGPlayerState::HasBoardUnitWithEffect(FName EffectId) const
{
	for (const FCGBoardUnit& Unit : BoardUnits)
	{
		FCGCardDef Def;
		if (UCGCardDatabase::FindCard(Unit.CardId, Def) && Def.EffectId == EffectId)
		{
			return true;
		}
	}
	return false;
}

bool ACGPlayerState::AreUnitAbilitiesSuppressedByEnemy(const ACGPlayerState* Enemy) const
{
	return Enemy && Enemy->HasBoardUnitWithEffect(FName(CGEffectId::SuppressEnemyUnitAbilities));
}

void ACGPlayerState::RefreshManaForNewTurn()
{
	// docs/initial-cards-v0.1.md「毎ターン自動増加マナ」。上限10は暫定ルール。
	MaxMana = FMath::Min(MaxMana + 1, 10);
	CurrentMana = MaxMana + PendingBonusMana;
	PendingBonusMana = 0;
	// PurchaseManaはここでリセットしない。通常のマナと違い、使うまでターンを
	// またいで貯まり続ける資源にしている(docs/game-rules-minimum.md「色ガイド」)。
	CardsPlayedThisTurn = 0;
	SpellsPlayedThisTurn = 0;
	DrawsThisTurn = 0;
	bBoughtThisTurn = false;
	bAllySpellPingUsedThisTurn = false;
	bRedPassiveUsedThisTurn = false;
	bOrangePassiveUsedThisTurn = false;
	bGreenPassiveUsedThisTurn = false;
	bBluePassiveUsedThisTurn = false;
	bPurplePassiveUsedThisTurn = false;
	bBuffBoardOnPurchaseThisTurn = false;
	bAllPurchasesDiscountedThisTurn = false;
}

void ACGPlayerState::ResolveEndTurnEffects(ACGPlayerState* Opponent)
{
	for (const TPair<FName, FEndTurnAuraEffectHandler>& Pair : GetEndTurnAuraEffectHandlers())
	{
		if (HasBoardUnitWithEffect(Pair.Key))
		{
			Pair.Value(*this, Opponent);
		}
	}
}
