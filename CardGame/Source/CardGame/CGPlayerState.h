#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CGTypes.h"
#include "CGPlayerState.generated.h"

class ACGGameState;

// 1プレイヤー(片側)の対戦データ(docs/architecture.md「対戦ロジックのクラス責務」)。
// ローカル対戦プロトタイプのため、実際のネットワーク接続(PlayerController)とは独立に
// ACGGameMode が2体を直接SpawnActorして「対戦相手」として扱う。
UCLASS()
class ACGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// HandCardIds/DeckCardIdsの実際の枚数をHandCount/DeckCountへ反映する
	// (docs/online-play-design.md「`ACGPlayerState`のレプリケーション」)。
	// `ACGGameMode::NotifyStateChanged()`から、状態が変わるたびに呼ばれる。
	void SyncPublicCardCounts();

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 SideIndex = 0;

	// 開始HP。第7回で試合を長引かせる狙いで20→30に引き上げたが、その後の
	// パッシブ再設計(コイン+1への統一)を経てユーザーの判断で20に戻した
	// (docs/next-ruleset-simulation-v1.md「第7回」「第10回」参照)。実際の
	// 初期化はACGGameMode::InitializeMatch()/InitializeOnlineMatch()/
	// PostLogin()/RunSelfPlaySimulation()で上書きされるため、そちらも
	// 合わせて変更している。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 CurrentHP = 20;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 MaxHP = 20;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 CurrentMana = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 MaxMana = 0;

	// 後手ハンデ用(Hearthstoneの「コイン」相当): 次にRefreshManaForNewTurn()が
	// 呼ばれたとき、その1ターンだけCurrentManaに上乗せされ、直後に0へ戻る
	// (MaxManaは変えないため、以降のマナランプは先攻と完全に対等になる)。
	// 永続的にMaxManaへ+1する版は3000戦シミュレーションで後攻82.7%勝と過剰に
	// 効きすぎたため、この一時マナ版に変更した(docs/next-ruleset-simulation-v1.md
	// 「第12回」参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 PendingBonusMana = 0;

	// 後手(先攻/後攻はランダム、InitializeMatch/InitializeOnlineMatch/
	// RunSelfPlaySimulationで決定)かどうか。PendingBonusMana等と同じ箇所で
	// 設定する。フィニッシャー到達条件のハンデ(CheckAndSpawnFinisher、
	// docs/next-ruleset-simulation-v1.md「第21回」)に使う。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bWentSecond = false;

	// 購入専用の資源(次期ルール、docs/game-rules-minimum.md「色ガイド」)。
	// 各色のパッシブ条件を満たすと+1される。通常のマナ(CurrentMana)と違い
	// ターンをまたいでも減らない(使うまで貯まる)。BuyCard()でマーケットの
	// カードを買うときにだけ使え、手札のカードをプレイする側には使えない。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 PurchaseMana = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bIsDefeated = false;

	// 非公開情報のため本人にしか複製しない(COND_OwnerOnly、docs/online-play-
	// design.md「`ACGPlayerState`のレプリケーション」)。対戦相手には代わりに
	// HandCountだけを見せる。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	TArray<FName> HandCardIds;

	// 同上、非公開(COND_OwnerOnly)。対戦相手にはDeckCountだけを見せる。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	TArray<FName> DeckCardIds;

	// 既存デザイン上、墓地は公開情報として扱われている(墓地から1枚選ぶ効果が
	// 対象を選べる=中身が見える前提)ため、全員に複製する。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	TArray<FName> DiscardCardIds;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	TArray<FCGBoardUnit> BoardUnits;

	// HandCardIds.Num()の公開版。対戦相手には手札の中身ではなく枚数だけを
	// 見せるために使う(SyncPublicCardCounts()で同期する)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 HandCount = 0;

	// DeckCardIds.Num()の公開版。上記と同じ理由。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 DeckCount = 0;

	// 廃墟街道の突撃兵(C009)判定用、および赤パッシブ判定用(3回目で発動)。
	// このターン何枚目のカードをプレイしたか。StartTurnで0に戻る。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 CardsPlayedThisTurn = 0;

	// バランス検証用(-SimulateMatches)専用: この対戦中にPlayCardFromHandで
	// プレイした全カードの履歴(重複あり)。通常対戦では複製されずログにも出ない、
	// ACGGameMode::RunSelfPlaySimulationがカード単位の勝率を集計するためだけに
	// 使う一時データ(docs/architecture.md参照不要、シミュレーション専用)。
	UPROPERTY()
	TArray<FName> CardsPlayedThisMatch;

	// 第六界を記す学者(C016)判定用: このターン何枚目のSpellをプレイしたか。StartTurnで0に戻る。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 SpellsPlayedThisTurn = 0;

	// 青パッシブ判定用(2回目のドローで発動)。このターン何回ドローしたか
	// (DrawCard()のみカウント、マーケット補充用のDrawCardForMarket()は含めない)。
	// StartTurnで0に戻る。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 DrawsThisTurn = 0;

	// 荒野の行商人(C007)判定用: このターン購入したか。StartTurnでfalseに戻る。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bBoughtThisTurn = false;

	// 結晶の谺を読む射手(C010)判定用: このターンにすでに1回発動したか。StartTurnでfalseに戻る。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bAllySpellPingUsedThisTurn = false;

	// 黄金の商機(O10)判定用: このターン、購入するたびに場のUnit全てを+1/+0するか。
	// StartTurnでfalseに戻る。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bBuffBoardOnPurchaseThisTurn = false;

	// 市場開放の号令(O13)判定用: このターン、購入コストを全て1軽減するか。StartTurnでfalseに戻る。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bAllPurchasesDiscountedThisTurn = false;

	// このデッキで発動中の色(次期ルール、docs/next-ruleset-design.md「色システム」)。
	// デッキ25枚中17枚以上を占める色がここに入る。デッキが確定するInitializeStartingDeck()
	// 時点で1回だけ計算し、以後は試合中変化しない(毎回デッキを数え直さない)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	TArray<ECGColor> ActiveColors;

	// フィニッシャー(各色固有の条件を達成すると自動で場に駆けつける専用Unit。
	// docs/architecture.md参照不要、新規追加の常設ルール)の条件判定用カウンタ。
	// 試合を通じて増え続け、ターンでは戻らない(ActiveColors同様、試合単位の値)。
	// 緑(場のUnit6体)だけはBoardUnits.Num()を都度見れば良いため専用カウンタ不要。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 AlliesDiedThisMatch = 0; // 赤: 味方Unitが10体死亡

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CardsPurchasedThisMatch = 0; // 橙: マーケットから9枚購入

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 CardsDrawnThisMatch = 0; // 青: カードを20枚ドロー

	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	int32 UnitsTransformedThisMatch = 0; // 紫: ユニットが5体変貌

	// 既に駆けつけたフィニッシャーの色。1試合につき同じ色は1体までしか出さない
	// (条件を満たし続けても重複召喚しないようにする)。
	UPROPERTY(BlueprintReadOnly, Category = "CardGame")
	TArray<ECGColor> FinishersSpawned;

	// 5色のパッシブ判定用フラグ(このターンに使用済みか。全色1ターン1回まで)。
	// StartTurnで全てfalseに戻る。以前はパッシブごとに効果がバラバラ
	// (ドロー/攻撃力/割引等)だったが、「それぞれのパッシブの説明が分かる
	// ようにしてほしい」「マナが増えるのはおかしい」というフィードバックを
	// 受け、5色とも同じ「コイン+1」(GrantPurchaseManaFromPassive()参照)に
	// 統一し、発動条件だけが色ごとに異なる形に整理した
	// (docs/game-rules-minimum.md「色ガイド」)。
	// 赤: このターン3枚目のカードをプレイしたとき(CardsPlayedThisTurn参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bRedPassiveUsedThisTurn = false;

	// 橙: このターン最初の購入をしたとき(bBoughtThisTurn参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bOrangePassiveUsedThisTurn = false;

	// 緑: 自分のターン開始時、場にUnitが3体以上いたとき。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bGreenPassiveUsedThisTurn = false;

	// 青: このターン2回目のドローをしたとき(DrawsThisTurn参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bBluePassiveUsedThisTurn = false;

	// 紫: 変貌が成功したとき。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	bool bPurplePassiveUsedThisTurn = false;

	// P11取り次ぎの女官用: 次に自分がプレイするUnit1体のコストに適用される割引。
	// 使うと0に戻る(docs/architecture.md「次期ルール移行時の実装メモ」参照)。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 NextUnitPlayDiscount = 0;

	// 紫パッシブ「王座を窺う者」等の判定用: このターンに変貌が成功した回数。StartTurnで0に戻る。
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "CardGame")
	int32 TransformsSucceededThisTurn = 0;

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void InitializeStartingDeck(const TArray<FName>& StarterCardIds);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ShuffleDeck();

	// 山札が0枚の状態で引こうとした場合、捨て札をシャッフルして山札に戻し
	// ライフを減らす(山札切れペナルティ、docs/next-ruleset-design.md)。
	// 捨て札も0枚なら(断罪等で大半を失った極端なケース)falseを返しbIsDefeatedを立てる。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool DrawCard();

	// マーケットの補充用に山札トップを1枚取り出す(手札には加えない)。DrawCard()と
	// 同じ山札切れペナルティ処理を共有する(PopTopOfDeckWithReshuffle経由)。
	// 山札・捨て札とも尽きていればNAME_Noneを返す(その枠は空のままになる)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	FName DrawCardForMarket();

	// CGState はマーケットを参照する効果(例: 廃墟に転がる戦利品)のためだけに使う。不要なら nullptr で可。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool PlayCardFromHand(FName CardId, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState = nullptr);

	// 手札から出したUnitを実際に場へ追加し、登場時効果まで解決する
	// (PlayCardFromHand()から切り出した共通処理)。生け贄(Sacrifice)持ちの
	// Unitは、生け贄選択(AllyUnitTarget選択待ち)が解決してから
	// ACGGameMode::ResolvePendingChoiceAllyTargetがこれを呼ぶため、
	// PlayCardFromHand以外からも呼べるようpublicにしている
	// (docs/keywords.md「生け贄(Sacrifice)」参照)。
	void SpawnUnitFromHandOnBoard(FName CardId, const FCGCardDef& Def, bool bIsSecondOrLaterPlayThisTurn, ACGPlayerState* Opponent, ACGGameState* CGState);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool BuyCard(FName CardId);

	// 今すぐ購入した場合に実際に適用される割引の合計(荒野の物々交換人/橙パッシブ/
	// 先物/O13の重ね掛けをBuyCard()と同じ式で計算する、状態を変えない参照用)。
	// UI側が「今買うといくら軽減されるか」を表示するために使う
	// (「割引が分かりにくい」というフィードバックへの対応)。
	UFUNCTION(BlueprintPure, Category = "CardGame")
	int32 ComputeCurrentPurchaseDiscount() const;

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ApplyDamage(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void Heal(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ApplyDamageToUnit(int32 UnitIndex, int32 Amount);

	// 場のHP0以下ユニットを取り除き、墓地へ送る。OnDeathDraw を持つユニットが死亡した分の
	// ドロー枚数を返す(実際のドローはGameMode側で行う=このPlayerStateを跨いだ効果もあるため)。
	// Opponentは死亡時に敵リーダーへダメージを与える効果(R06捨て身の道場破り、
	// R12黒鉄の刀鍛冶)のために渡す(呼び出し側は常にこのユニットの持ち主から見た
	// 敵側を渡すこと。例: 攻撃ならExecuteAttackの相手側)。CGStateは死亡による
	// 変貌条件の再判定(ApplyTransformIfConditionMet)のために渡す。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	int32 RemoveDeadUnitsAndGetDeathDrawCount(ACGPlayerState* Opponent, ACGGameState* CGState);

	UFUNCTION(BlueprintPure, Category = "CardGame")
	bool HasGuardUnit() const;

	// 場のUnit1体の実効攻撃力(BoardUnit.Atk + 緑パッシブ等の継続的なボーナス)。
	// 緑パッシブ(場にUnit3体以上で全Unit+1)は他の永続バフと違い場の増減に応じて
	// 変動し続けるため、BoardUnit.Atkへ直接焼き込まず参照のたびに計算する
	// (docs/game-rules-minimum.md「緑」)。ダメージ計算・AIの判断・UI表示は
	// 全てこの関数経由でAtkを読むこと。
	UFUNCTION(BlueprintPure, Category = "CardGame")
	int32 GetEffectiveAtk(int32 UnitIndex) const;

	// 場に指定EffectIdを持つUnitが1体でもいるか(常在効果の判定に使う)。
	UFUNCTION(BlueprintPure, Category = "CardGame")
	bool HasBoardUnitWithEffect(FName EffectId) const;

	// B06「深淵の封印」用: Enemy(このプレイヤーの相手)がSuppressEnemyUnitAbilities
	// を持つUnitを場に出している間、このプレイヤーのUnitの能力(登場時/死亡時/
	// 常在アウラ等の発動)はすべて発動しない。各効果の発動箇所はこの関数で
	// 事前にチェックすること(docs/next-ruleset-cards-v1.md「青」参照)。
	UFUNCTION(BlueprintPure, Category = "CardGame")
	bool AreUnitAbilitiesSuppressedByEnemy(const ACGPlayerState* Enemy) const;

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void RefreshManaForNewTurn();

	// EndTurn時に呼ぶ。選択不要なターン終了時常在効果があればここで解決する
	// (荒野の行商人(C007)は選択式のためACGGameMode::RequestEndTurnで個別処理する。
	// docs/architecture.md「選択待ち(PendingChoice)の仕組み」参照)。Opponentは
	// 敵Unitを対象にする効果(B10弱点の考察官等)のために渡す。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ResolveEndTurnEffects(ACGPlayerState* Opponent);

	// 以下はカード効果ハンドラ(CGPlayerState.cpp 無名namespace内のHandle_*関数群、
	// docs/architecture.md「カード効果ディスパッチ」参照)から呼ばれるための公開メンバー。
	// 外部からの直接呼び出しは想定していない。

	// 手札から指定したCardIdのカードを1枚捨てる。選択式カード効果
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」)でプレイヤー/AIが選んだ結果を反映する。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool DiscardSpecificFromHand(FName CardId);

	// 墓地から指定したCardIdのカードを1枚、デッキの一番下/手札へ移す
	// (docs/architecture.md「選択待ち(PendingChoice)の仕組み」でプレイヤー/AIが
	// 選んだ結果を反映する)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool MoveSpecificDiscardCardToDeckBottom(FName CardId);

	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool MoveSpecificDiscardCardToHand(FName CardId);

	// 墓地からUnitをランダムに1枚探してデッキの一番上へ戻す
	// (廃墟の霊廟守り(C014)はカード自身が「ランダムで」と明記しているため選択式にしない)。
	bool TryMoveRandomDiscardUnitToDeckTop();

	// デッキの一番上をデッキの一番下へ移す(廃墟を渡る斥候(C001)の「上下選択」で
	// 「下に送る」を選んだ結果を反映する。docs/architecture.md「選択待ち(PendingChoice)の仕組み」
	// 「④山札の上を見て上下を選ぶ」)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool MoveDeckTopToBottom();

	// マーケットや購入を介さず、直接カードデータIDを指定して場にユニットを1体追加する
	// (彷徨う者たちの合流の「1/1トークンを出す」等、実在カードではないユニットの生成に使う)。
	void AddBoardUnitDirect(FName CardId, int32 Atk, int32 Hp, bool bCanAttackImmediately, bool bHasGuard);

	// マーケットからUnit1枚を選び、コストを支払わずそのまま場に出す選択を開始する
	// (O15黒鉄の買い占め屋、FIN_ORANGE港を興す者で使用)。RemainingRepeatsを渡すと、
	// この選択が解決された直後にACGGameMode::ResolvePendingChoiceWithCard側で
	// 同じ選択が続けて開始される(港を興す者のように複数枚を1枚ずつ選ばせる場合に
	// 使う。docs/next-ruleset-cards-v1.md「橙」。「プレイヤーがマーケットのカードを
	// 選択して無料でプレイできるカードを選びたい」というフィードバックへの対応で、
	// 以前はO15と違いプレイヤーに選ばせず自動で3枚デプロイしていた)。候補が
	// 無ければ何もしない(選択待ちには入らない)。
	void BeginMarketDeployChoice(ACGGameState* CGState, int32 MaxCost, int32 RemainingRepeats);

	// 追放(青フィニッシャー専用): 対象ユニットを墓地に送らず場から完全に取り除く
	// (死亡時効果も発動しない、真の除去)。取り除いたユニットの(変貌前の)
	// コストをOutSealedCostへ返す。以前は「封印」キーワード全体(B01/B03/B06/
	// B07/B09/B11/B13/B15等)がこの関数を使っていたが、「ユニットを死亡させる
	// キーワード能力にしてほしい」というフィードバックを受け、キーワード側は
	// CondemnUnit()(通常の死亡処理を経由する)に切り替えた。この関数は
	// FIN_BLUE(書庫の大賢者)の「敵の場のUnitをすべて追放する」効果専用として
	// 残している(docs/keywords.md「断罪(Condemn)」参照)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool SealUnit(int32 UnitIndex, int32& OutSealedCost);

	// 断罪(Condemn、青、docs/keywords.md): 対象ユニットのHpを0にする(通常の
	// 死亡処理を経由させるため、その場では墓地送り・死亡時効果を行わない)。
	// 呼び出し側が必ずACGGameMode::ResolveDeathsForBothSides()等の死亡処理を
	// 続けて呼ぶこと。取り除いたユニットの(変貌前の)コストをOutCostへ返す。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	bool CondemnUnit(int32 UnitIndex, int32& OutCost);

	// B04頁繰りの魔道士用: 断罪が成立するたびに呼ぶ。自分の場にOnSealDamageFace1を
	// 持つUnitがいれば、Opponentへ1ダメージを与える(1回の断罪につき最大1回)。
	// 断罪が成立する全ての箇所(ACGGameMode::ResolvePendingChoiceSealTarget、
	// ApplyOnTurnStartAuraEffects内のB14)から呼ぶ。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void NotifySealSucceeded(ACGPlayerState* Opponent);

	// P13玉座の噂用: 変貌条件を無視して、変貌可能な(TransformTargetCardIdを
	// 持つ)場の味方Unit全てを即座に変貌させる。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ForceTransformAll(ACGPlayerState* Opponent, ACGGameState* CGState);

	// 指定した場のUnit1体が、現在の状態(まだ変貌前のカードかどうか)から
	// 変貌可能かどうか。選択待ちの対象候補を絞り込む(HUDのグレーアウト、
	// AIの対象選択、選択解決時のバリデーション)のに使う。
	UFUNCTION(BlueprintPure, Category = "CardGame")
	bool CanUnitTransform(int32 UnitIndex) const;

	// P16用: 変貌条件を無視して、指定した場のUnit1体だけを即座に変貌させる
	// (ForceTransformAll()の単体対象指定版。対象が変貌不可なら何もしない)。
	UFUNCTION(BlueprintCallable, Category = "CardGame")
	void ForceTransformUnit(int32 UnitIndex, ACGPlayerState* Opponent, ACGGameState* CGState);

	// フィニッシャー: 自分がアクティブにしている色(ActiveColors)ごとに固有の
	// 条件(AlliesDiedThisMatch等)を満たしているか判定し、満たしていて未召喚なら
	// UCGCardDatabase::GetFinisherCardIdのUnitを手札・コストを介さず直接場に
	// 出す(ACGGameMode::NotifyStateChangedから毎回呼ぶ。「各色で特定の条件を
	// 達成したときにフィニッシャーが駆けつける」というフィードバックへの対応)。
	void CheckAndSpawnFinisher(ACGPlayerState* Opponent, ACGGameState* CGState);

	// 5色共通のパッシブ報酬: Colorが発動中(ActiveColors)で、かつ
	// bUsedThisTurnFlagがまだfalseなら、PurchaseManaを+1して使用済みにする
	// (1ターン1回。docs/game-rules-minimum.md「色ガイド」)。各色の発動条件
	// 判定(3回目のプレイ、最初の購入等)は呼び出し側で行い、条件を満たした
	// 瞬間にこの関数を呼ぶ。
	void GrantPurchaseManaFromPassive(ECGColor Color, bool& bUsedThisTurnFlag);

	// デッキ確定時(InitializeStartingDeck)に1回だけ、ActiveColorsを計算する
	// (次期ルール、docs/next-ruleset-design.md「色システム」しきい値17/25)。
	void ComputeActiveColors();

	// 変貌(紫、docs/game-rules-minimum.md)。自分のターン開始時に呼ぶ:
	// TurnsInPlay系の進行度を+1し、RandomChancePerTurn系は抽選する。その後、
	// 条件を満たした全ユニットへCheckAllTransforms()相当の判定を行う。Opponent/
	// CGStateは変貌先カード自身が持つ登場時効果(P15T等、docs/next-ruleset-cards-v1.md
	// 「変貌先カード」。通常のUnit登場時効果ハンドラをそのまま再利用するため
	// CGStateも必要)のために渡す。
	void OnTurnStartTransformTick(ACGPlayerState* Opponent, ACGGameState* CGState);

	// 場の全ユニットについて、変貌条件が満たされていれば変貌させる
	// (HandSizeAtMost等、進行度を伴わない条件の再判定用。手札枚数が変わる操作の後に呼ぶ)。
	void CheckAllTransforms(ACGPlayerState* Opponent, ACGGameState* CGState);

	// 自分のターン開始時に、場のUnitが持つ「ターン開始時」常在効果を判定する
	// (次期ルール、フェーズ4b。B05の1ドロー、O08の購入割引、B14の敵最高コスト
	// 断罪など)。Opponentは敵Unitを対象にする効果(B14)のために渡す。CGStateは
	// B14の断罪(死亡処理)がACGGameState::TurnCount等の変貌条件を参照する
	// RemoveDeadUnitsAndGetDeathDrawCount()に必要。
	void ApplyOnTurnStartAuraEffects(ACGPlayerState* Opponent, ACGGameState* CGState);

	// 分身(緑、docs/next-ruleset-cards-v1.md「緑」)。場の全ユニットについて、
	// 分身条件が満たされていれば分身させる(味方Unit数条件のような進行度を
	// 伴わない条件の再判定用。ACGGameMode::NotifyStateChangedから毎回呼ぶことで、
	// 場のUnit数が変わるあらゆる操作の後に再判定される)。SurvivedAttack/
	// AttackedAndSurvived/LeaderHealedはCGGameMode側で該当イベント発生時に
	// CloneProgressを+1してから、この関数(経由でNotifyStateChanged)が実際の
	// 分身を成立させる。
	void CheckAllClones(ACGPlayerState* Opponent, ACGGameState* CGState);

	// 自分のリーダーが回復した直後に呼ぶ(Heal()の全呼び出し元から)。分身条件
	// LeaderHealedを持つ場の全ユニットのCloneProgressを+1する(実際の分身は
	// 後続のCheckAllClonesが行う)。
	void NotifyOwnLeaderHealed();

private:
	void ResolveSpellEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, int32 TargetUnitIndex, ACGGameState* CGState);
	// CGStateを受け取るのは、選択式カード効果(docs/architecture.md「選択待ち(PendingChoice)の仕組み」)で
	// CGState->PendingChoiceへ選択待ちを書き込むハンドラがあるため。
	void ResolveUnitOnPlayEffect(const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState);

	// DrawCard()/DrawCardForMarket()共通の「山札トップを1枚取り出す。山札が
	// 尽きていれば捨て札からリシャッフルしてライフを減らす」処理(docs/next-ruleset-
	// design.md「山札切れペナルティ」)。取り出せなければNAME_Noneを返す。
	FName PopTopOfDeckWithReshuffle();

	// Unit1体について変貌条件(TransformConditionId、docs/next-ruleset-cards-v1.md
	// 「変貌先カード」)を判定し、満たしていれば変貌させる(CardId/Atk/Hpを変貌先の
	// ものに置き換え、OriginalCardIdへ変貌前を退避)。判定自体はGetTransformConditionPredicates()
	// のテーブルに委譲しているため、新しい条件を増やす場合はテーブルに1行足すだけでよい。
	// RandomChancePerTurnはここでは判定しない(OnTurnStartTransformTickでターン開始時に
	// 1回だけ抽選する)。
	void ApplyTransformIfConditionMet(FCGBoardUnit& Unit, ACGPlayerState* Opponent, ACGGameState* CGState);

	// 実際に変貌を適用する(CardId/Atk/Hpの置き換え、進行度リセット、紫パッシブの
	// 付与、変貌先カード自身の登場時効果)。ApplyTransformIfConditionMet()と
	// OnTurnStartTransformTick()のRandomChancePerTurn抽選成功時、ForceTransformAll()の
	// 3箇所から呼ばれる共通処理。変貌先カード自身の登場時効果は通常のUnit登場時
	// 効果ハンドラ(ResolveUnitOnPlayEffect、GetUnitOnPlayEffectHandlers())を
	// そのまま再利用するため、新しい変貌先カードに登場時効果を持たせる場合も
	// 専用の分岐を書く必要はなく、他のUnitと同じ手順(CGTypes.hにEffectId定数を
	// 追加しHandle_XXXをテーブル登録する)だけで済む。
	void PerformTransform(FCGBoardUnit& Unit, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState);

	// Unit1体について分身条件(CloneConditionId、docs/next-ruleset-cards-v1.md「緑」)を
	// 判定し、満たしていて未発動なら分身させる。判定はGetCloneConditionPredicates()の
	// テーブルに委譲しているため、新しい条件を増やす場合はテーブルに1行足すだけでよい。
	void ApplyCloneIfConditionMet(FCGBoardUnit& Unit, ACGPlayerState* Opponent, ACGGameState* CGState);

	// 実際に分身を適用する(このユニットをbHasCloned済みにし、素の基本ステータス・
	// キーワード無しのコピーをトークンとして場に追加、コピー自身の登場時効果を
	// 通常のUnit登場時効果ハンドラで解決する。docs/next-ruleset-cards-v1.md「緑」)。
	void PerformClone(FCGBoardUnit& Unit, const FCGCardDef& Def, ACGPlayerState* Opponent, ACGGameState* CGState);
};
