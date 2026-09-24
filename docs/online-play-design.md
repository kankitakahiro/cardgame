# オンライン対戦 設計書

実装は完了済み(実装フェーズの進捗管理をしていた`online-play-plan.md`は完了に
伴い削除し、今後も価値のある技術的判断は[architecture.md](architecture.md)
「オンライン対戦移行時の実装メモ」へ移した)。壁打ちの経緯を残すため、
このドキュメント自体は削除せず参照用に残す。

## 目的・スコープ

- 知り合い同士がIPアドレス(LAN)、またはホストのSteamID(インターネット越し、
  ポート開放不要)を直接指定してホスト/参加し、1対1のオンライン対戦を
  行えるようにする
- サーバー権威(server-authoritative)を最初から採用し、「手札を不正に見る」
  「自分の手番でないのに操作する」といった改ざんに強い作りにする
- **スコープ外**(将来必要になれば別途検討): Steam経由のフレンド招待/
  マッチメイキング(ロビー・セッション機能)、観戦機能、再接続、3人以上の対戦、
  モバイル/クロスプレイ。SteamSockets(下記)はあくまで通信経路(ポート開放
  回避)としてのみ使い、参加は引き続き「相手のIDを手入力する」手動接続のまま
  にしている(まずは最小構成で導入し、段階的に検証する考え方に倣った)

## 全体アーキテクチャ

UE標準のClient-Server構成+Actorレプリケーションを使う。マッチメイキングは
行わず、UEの直接接続機能(`FURL`+`GEngine->Browse()`、下記参照)で足りる。
通信経路(NetDriver)は`SteamSockets`プラグイン経由になっており、LANでのIP
直接接続と、インターネット越しのSteamID指定接続の両方を、コード上は同じ
接続処理のまま使い分けられる(下記「SteamSocketsによるインターネット越し
接続」参照)。

- **ホスト**: 自分のPCでリッスンサーバーを起動する(`UGameplayStatics::OpenLevel(
  this, MapName, /*bAbsolute=*/true, TEXT("listen"))`)。ホストの`ACGGameMode`が
  サーバー権威を持ち、ホスト自身のクライアントもこのサーバーに接続した1つの
  クライアントとして扱われる(リッスンサーバーの標準的な形)
- **ゲスト**: ロビー画面でホストのIPアドレス、またはSteamIDを入力して接続する。
  当初は`PlayerController->ConsoleCommand(FString::Printf(TEXT("open %s"), *IP))`
  を想定していたが、実機検証で**UEの`open`コンソールコマンドはドットを含まない
  純粋な数値文字列(SteamID64)をホスト名ではなくマップ名として誤解釈する**
  ((`LogLongPackageNames: Can't Find URL`)ことが判明した。IPアドレス
  (`127.0.0.1`等)はドットを含むため正しく解釈されるが、SteamIDは常に失敗する。
  そのため`ConsoleCommand("open ...")`は使わず、`FURL`を直接組み立てて
  (`URL.Host`/`URL.Port`を設定)`GEngine->Browse(*WorldContext, URL, Error)`を
  呼ぶ方式に変更した(`UCGLobbyHUD::HandleJoinGameClicked()`)。`WorldContext`は
  `GEngine->GetWorldContextFromWorld(World)`(nullを返すことがあった)ではなく
  `GetWorld()->GetGameInstance()->GetWorldContext()`で取得する。IPアドレスと
  SteamIDのどちらでも同じコードパスで動作する
- 既存のオフライン(vs AI)モードはこのアーキテクチャに変更を加えない。
  `ACGGameMode`内でオンライン/オフラインの初期化パスを分岐させる
  (`GetNetMode() != NM_Standalone`で判定できる。リッスンサーバー/クライアントは
  `NM_ListenServer`/`NM_Client`になる)

### SideIndexの割り当て

現状`SideIndex`(0=人間、1=AI)は`InitializeMatch()`が直接決め打ちしているが、
オンライン対戦では「どの接続がSide0/Side1か」を接続順で決める。

- ホスト(最初から存在する接続)→ SideIndex 0
- ゲスト(`PostLogin`で2人目として参加してきた接続)→ SideIndex 1
- 2人目の`PostLogin`が呼ばれた時点で試合を初期化する(`InitializeOnlineMatch()`、
  下記参照)

## クラス設計

### 新設: `ACGPlayerController`

現状カスタムPlayerControllerが存在しない(`ACGGameMode::PlayerControllerClass`は
デフォルトのまま)。オンライン対戦のアクション要求(Server RPC)の窓口として
新設する。

- `ACGGameMode::PlayerControllerClass = ACGPlayerController::StaticClass()`に変更
- 保持する情報: 特に無し(`GetPlayerState<ACGPlayerState>()->SideIndex`で
  自分のSideIndexを取得できるため、追加のメンバ変数は不要)
- 下記「アクションRPC一覧」の10個のServer RPC関数を持つ

### `ACGGameMode`の変更

- `PostLogin(APlayerController* NewPlayer)`をオーバーライドし、接続してきた
  PlayerControllerに対応する`ACGPlayerState`へSideIndexを割り当てる
  (UEの標準フローでは`PlayerState`はGameMode内部で自動生成されるため、
  現状の「`InitializeMatch()`が`SpawnActor<ACGPlayerState>()`を自前で呼ぶ」処理は
  オンラインモードでは使わない)
- `InitializeOnlineMatch()`を新設: 2人目の`PostLogin`が来た時点で、現行の
  `InitializeMatch()`のうち「デッキ初期化・初期手札ドロー・マーケット初期化・
  先攻決定・`StartTurn()`」の部分を流用して試合を開始する。人間側のデッキは
  各クライアントの`UCGGameInstance::PlayerDeckCardIds`をサーバーへ伝える必要が
  あるため、接続時に1回だけデッキ内容をServer RPCで送ってもらう
  (`ServerSubmitDeck(const TArray<FName>& DeckCardIds)`)
- 既存の`InitializeMatch()`(AI対戦用)・`RunSelfPlaySimulation()`は変更しない
- 上記10個のRPCの実処理(サーバー側での実行内容)は、既存の`RequestPlayCard`等を
  **そのまま呼ぶだけ**にする(ゲームロジック自体の変更は不要。呼び出し経路を
  追加するだけ)

### `ACGGameState`のレプリケーション

`AGameStateBase`は元々全クライアントに複製される想定のクラスなので、
以下のフィールドに`UPROPERTY(Replicated)`を付け、`GetLifetimeReplicatedProps`を
実装するだけでよい(非公開情報を含まないため、条件付き複製は不要)。

| フィールド | 複製 | 備考 |
|---|---|---|
| `MarketSlots` | 全員 | 元々公開情報(マーケットは常時公開) |
| `TurnCount` / `CurrentTurnPlayerIndex` | 全員 | |
| `WinnerPlayerIndex` | 全員 | |
| `PendingChoice` | 全員 | 「誰が」「何のために」選択待ちかは両者に見せてよい。選択の候補自体は各クライアントが自分の手札/盤面から動的に導出するため、`PendingChoice`自体に非公開データは乗らない |

### `ACGPlayerState`のレプリケーション(最大の設計ポイント)

デフォルトのAPlayerStateは中身を含めて全クライアントに複製される。これだと
相手の手札の中身が見えてしまうため、フィールドごとに複製条件を分ける。

| フィールド | 複製条件 | 理由 |
|---|---|---|
| `CurrentHP`/`MaxHP`/`CurrentMana`/`MaxMana` | 全員 | ライフ・マナは公開情報 |
| `BoardUnits` | 全員 | 盤面は公開情報 |
| `ActiveColors` | 全員 | 現状HUDが本人に表示している情報であり、対戦相手のパッシブ発動状況も見えるべき情報(隠す設計にはなっていない) |
| `HandCardIds` | **本人のみ**(`COND_OwnerOnly`) | 手札の中身は非公開情報の中核 |
| `DeckCardIds` | **本人のみ**(`COND_OwnerOnly`) | 山札の並び(ドロー順)も非公開 |
| `DiscardCardIds` | 全員 | 既存デザイン上、墓地は公開情報として扱われている(「墓地から1枚選ぶ」効果が対象を選べる=中身が見える前提) |
| 新設 `HandCount`(int32) | 全員 | 相手の手札**枚数**だけを見せる。`HandCardIds`の要素数が変わるたびにサーバー側で更新する |
| 新設 `DeckCount`(int32) | 全員 | 同上、山札枚数 |
| 各種ターン内フラグ(`bBoughtThisTurn`等) | 全員 | ゲームロジック上の内部状態で、非公開にする設計要求は無い |

`COND_OwnerOnly`が機能するには、その`ACGPlayerState`のOwnerが対応する
`PlayerController`になっている必要がある。UEの標準フロー
(`AController::InitPlayerState()`が`PlayerState->SetOwner(this)`を呼ぶ)に
乗っていれば自動的に満たされるため、「SideIndexの割り当て」を`PostLogin`側の
標準フローに寄せることが前提になる。

## アクションRPC一覧

既存の`ACGGameMode`の10個の公開メソッドに対応するServer RPCを
`ACGPlayerController`に追加する。**サーバー側の実処理は既存メソッドを
呼ぶだけ**で、ロジックの重複実装はしない。

| 既存の`ACGGameMode`メソッド | 追加するServer RPC | 呼び出し元(HUD) |
|---|---|---|
| `RequestPlayCard(SideIndex, CardId, TargetUnitIndex)` | `ServerRequestPlayCard(CardId, TargetUnitIndex)` | 手札クリック |
| `RequestBuyCard(SideIndex, MarketSlotIndex)` | `ServerRequestBuyCard(MarketSlotIndex)` | マーケットクリック |
| `RequestAttack(AttackerSideIndex, AttackerUnitIndex)` | `ServerRequestAttack(AttackerUnitIndex)` | 自分の盤面クリック |
| `RequestEndTurn(SideIndex)` | `ServerRequestEndTurn()` | ターン終了ボタン |
| `ResolvePendingChoiceWithCard(SideIndex, ChosenCardId)` | `ServerResolveChoiceWithCard(ChosenCardId)` | 手札/墓地/マーケットの選択 |
| `ResolvePendingChoiceWithTarget(SideIndex, ChosenUnitIndex)` | `ServerResolveChoiceWithTarget(ChosenUnitIndex)` | 攻撃対象選択(-1=顔面) |
| `ResolvePendingChoiceKeepOrBury(SideIndex, bKeepOnTop)` | `ServerResolveChoiceKeepOrBury(bKeepOnTop)` | 山札トップ保持/送り選択 |
| `ResolvePendingChoiceBuyDestination(SideIndex, bToHand)` | `ServerResolveChoiceBuyDestination(bToHand)` | 購入先選択 |
| `ResolvePendingChoiceSealTarget(SideIndex, ChosenUnitIndex)` | `ServerResolveChoiceSealTarget(ChosenUnitIndex)` | 封印/デバフ対象選択 |
| `ResolvePendingChoiceAllyTarget(SideIndex, ChosenUnitIndex)` | `ServerResolveChoiceAllyTarget(ChosenUnitIndex)` | 味方強化対象選択 |

各RPCは`UFUNCTION(Server, Reliable, WithValidation)`で宣言し、実装は次の
定型パターンにする(セキュリティ上の注意点も参照):

```cpp
void ACGPlayerController::ServerRequestPlayCard_Implementation(FName CardId, int32 TargetUnitIndex)
{
    ACGPlayerState* MyState = GetPlayerState<ACGPlayerState>();
    ACGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACGGameMode>() : nullptr;
    if (!MyState || !GameMode)
    {
        return;
    }
    GameMode->RequestPlayCard(MyState->SideIndex, CardId, TargetUnitIndex);
}
```

`Reliable`にするのはターン制で頻度が低く、取りこぼしが即座にゲーム進行不能に
つながるため(信頼性より頻度を優先する`Unreliable`にする理由が無い)。

## セキュリティ上の注意点

- **SideIndexは常にサーバー側で`GetPlayerState<ACGPlayerState>()->SideIndex`
  から求め、クライアントからパラメータとして受け取らない**。これにより
  「自分が相手のSideIndexを騙って操作する」cheatを構造的に防ぐ
- 手番チェック(「今は自分の番か」「今は自分への選択待ちか」)は既存の
  `ACGGameMode`側の各メソッドが内部で検証している前提を維持する(現状の実装を
  確認し、抜けがあれば`WithValidation`のバリデータ関数、または各メソッド内で
  弾く)
- カードの存在チェック(「本当に自分の手札にあるカードか」)も既存メソッド内の
  `HandCardIds.Contains(CardId)`等のチェックに委ねる。RPC層で重複実装しない

## `PendingChoice`の非同期化について

現状の`ACGGameMode::BeginChoice()`は「`CGState->PendingChoice`を設定して即座に
returnする」という作りのため、**元々非同期な設計になっている**(人間側の
選択待ちは、後で来るクリック=関数呼び出しを待つ形。オンライン化にあたって
構造変更は不要)。`AutoResolveChoiceIfAI()`によるAIの即時解決も、AIは常に
サーバー(ホスト)側で動くため影響を受けない。変更が必要なのは「クリックが
`ACGGameMode`への直接呼び出しではなく、上記のServer RPC経由になる」という
呼び出し経路の部分のみ。

## HUD側の変更点

`CGGameHUD.cpp`の以下の呼び出し箇所(`GameMode->RequestXxx(CGState->
CurrentTurnPlayerIndex, ...)`または`GameMode->ResolvePendingChoiceXxx(...)`の形)を、
`GetOwningPlayer<ACGPlayerController>()->ServerRequestXxx(...)`
形式に置き換える。

- `HandleHandSlotClicked`(手札クリック): `RequestPlayCard`/`ResolvePendingChoiceWithCard`
- `HandleGraveyardSlotClicked`(墓地クリック): `ResolvePendingChoiceWithCard`
- `HandleMarketSlotClicked`(マーケットクリック): `ResolvePendingChoiceWithCard`/
  `ResolvePendingChoiceMarketSlotTarget`/`RequestBuyCard`
- 自分の盤面クリック: `ResolvePendingChoiceAllyTarget`/`RequestAttack`
- 敵の盤面クリック: `ResolvePendingChoiceSealTarget`/`ResolvePendingChoiceWithTarget`
- 顔面クリック: `ResolvePendingChoiceWithTarget`(-1)
- 山札保持/送りボタン: `ResolvePendingChoiceKeepOrBury`
- 購入先(手札/山札下)ボタン: `ResolvePendingChoiceBuyDestination`
- ターン終了ボタン: `RequestEndTurn`

**この置き換えは、`CGState->CurrentTurnPlayerIndex`を「自分のSideIndex」の
代用として使っていた既存コードの潜在的な不備(相手の手番でも理屈上呼び出せて
しまう)も合わせて解消する**(サーバー側でSideIndexを検証するため)。

オフラインモード(vs AI)でも同じRPC経由の呼び出しに統一する。リッスン
サーバー/スタンドアロンでは自分が所有する`PlayerController`へのServer RPC呼び
出しは同一プロセス内で即座に実行されるため、**オフラインモードの挙動は変わらない**
(往復遅延が発生するのはリモート接続の場合のみ)。これにより「オンライン/
オフラインで別のコードパスを持つ」ことを避け、1つの経路だけを保守すればよく
なる。

## ロビー画面の変更点

`CGLobbyHUD.cpp`に以下を追加する。

- 「ホストする」ボタン: `UGameplayStatics::OpenLevel(this, TEXT("/Game/CardGame/
  Maps/L_Card_GamePrototype"), true, TEXT("listen"))`
- 「IPを指定して参加する」: テキスト入力欄+ボタン。`FURL`+`GEngine->Browse()`
  で接続する(上記「全体アーキテクチャ」参照。`ConsoleCommand("open ...")`は
  SteamIDの誤解釈が判明したため不採用)
- 参加前にデッキを選択させる(既存のデッキ選択画面を流用)。選択したデッキは
  接続完了後に`ServerSubmitDeck()`でサーバーへ送る
- 「あなたのSteamID」表示行(`YourSteamIdText`): ホストする側が友達に伝える
  用。`IOnlineIdentity::GetUniquePlayerId(0)`から取得(下記参照)

## SteamSocketsによるインターネット越し接続

「別ネットワークの友達を簡単に参加させたい」という要望を受けて追加した。
ポート開放(ルーターのポートフォワーディング)を友達に頼まずに済むよう、
通信経路をSteamの中継網(Steam Datagram Relay、NAT越え込み)経由にする。
**マッチメイキング/セッション機能は使わず、あくまで通信経路の差し替えのみ**
(スコープを最小限に保つ方針、目的・スコープ参照)。

### 仕組み

- プラグイン`OnlineSubsystem`/`OnlineSubsystemSteam`/`SteamSockets`を有効化
  (`CardGame.uproject`)。`SteamSockets`はValveのGameNetworkingSockets/SDRを
  使うP2P NetDriverで、`SocketSubsystemSteamIP`(NAT越え非対応の旧世代)とは
  別物
- `DefaultEngine.ini`で`GameNetDriver`を`/Script/SteamSockets.
  SteamSocketsNetDriver`に差し替え、`DriverClassNameFallback`に通常の
  `IpNetDriver`を指定。**Steamが使えない環境(未起動等)では自動的にこれまで
  通りのIP直接接続にフォールバックする**ため、LAN対戦は無条件で動き続ける
- テスト用AppID **480**(Valve提供のSpacewar)を使用(`SteamDevAppId=480`)。
  Steamworksパートナー登録($100)は不要だが、参加する全員のPCでSteam
  クライアントが起動している必要がある(任意のSteamアカウントでよい)。
  `bRelaunchInSteam=false`にして、exeを直接ダブルクリックしても毎回Steam
  経由の再起動を挟まないようにしている
- 接続時のアドレス文字列(数値のみならSteamID64、それ以外は通常のIP)の
  判定自体は`SocketSubsystemSteamIP`/`SteamSockets`側の解析に委ねている。
  ただし当初想定していた「`open <文字列>`をそのままコンソールコマンドとして
  実行するだけで自動判別される」という設計は誤りだった。UEの`open`コマンド
  自体がドット無しの数値文字列をマップ名と誤解釈してしまうため、
  `FURL`を直接組み立てて`GEngine->Browse()`を呼ぶ方式に変更している
  (上記「全体アーキテクチャ」参照)。この変更はロビー画面のテキスト入力欄
  自体には影響しない(ユーザーはIP/SteamIDのどちらも同じ欄に入力する)
- 自分のSteamID表示は`IOnlineSubsystem::Get()->GetIdentityInterface()->
  GetUniquePlayerId(0)->ToString()`(`UCGLobbyHUD::GetLocalSteamIdString()`)。
  `FUniqueNetIdSteam::ToString()`は`%llu`形式の10進数を返すため、上記の
  アドレス解析とそのまま噛み合う
- `steam_appid.txt`(中身は`480`)をプロジェクトルートに配置。exeを直接
  起動する場合(パッケージ後の配布・エディタの`-game`起動)、Steamの
  ライブラリ経由で起動されない限りこのファイルが無いとSteam APIの初期化に
  失敗する。**パッケージ後に配布する際は、この`steam_appid.txt`をexeと
  同じフォルダにコピーし忘れないこと**(現状パッケージング設定に自動コピー
  対象として登録していないため、手動コピーが必要)

### 検証状況

開発機でSteamクライアントが起動していたため、`-game`単体起動で以下を
ログで確認できた(2台目のPC・別アカウントでのP2P接続自体はこのサンドボックス
環境からは検証できていない):

- `LogOnline: STEAM: [AppId: 480] Client API initialized 1` (Steam APIの
  初期化成功)
- `LogSockets: SteamSockets: Initializing Network Relay` +
  世界各地のSDRリレーサーバーとの疎通確認ログ(香港・シンガポール・
  シドニー等)、有効な証明書取得(`AuthStatus (steamid:...): OK`)
- `FUniqueNetIdSteam::ToString()`が`%llu`形式(純粋な10進数)を返すことを
  ソースコードで確認済み。`SocketSubsystemSteamIP`/`SteamSockets`側の
  アドレス解析(`IsNumeric()`判定)とフォーマットが一致することを確認した

実際に2台の別PC・別Steamアカウントで接続を試したところ、上記の`open`コマンドの
SteamID誤解釈バグに加えて、パッケージにマップ(`L_DeckBuilder`/
`L_Card_GamePrototype`)が正しく含まれていない、背景テクスチャが未クックなど
複数の実地でしか見つからない問題があった。いずれも修正済みで、ローカルでの
2プロセス(ホスト側`-game`実行ファイル+ゲスト側パッケージ版exe)によるSteamID
接続・デッキ提出・`StartTurn()`実行までの再現テストでは成功を確認している。

## 検証方法

1. **エディタ内(同一PC)**: エディタの「マルチプレイヤーオプション」で
   プレイヤー数2・ネットモードを「Play As Listen Server」に設定してPIE起動し、
   2つのウィンドウで実際に1試合最後まで進められることを確認する
2. **同一LAN内の2台のPC**: 実機でLAN内IPアドレスを`JoinIPBox`に入力して
   接続を確認する
3. **異なるネットワークの2台のPC**: 双方のPCでSteamクライアントを起動した
   状態で、ホストの`YourSteamIdText`に表示されるSteamIDをゲストが
   `JoinIPBox`へ入力して接続する(SteamSockets経由、ポート開放不要)。
   ポート開放によるIP直接接続(既定7777/UDP)も引き続き利用可能。**実施済み**
   (online-play-design.md「検証状況」参照。実地テストでのみ発覚したバグは
   architecture.md「オンライン対戦移行時の実装メモ」に記録)

## 今後の検討事項(スコープ外)

- Steamフレンドリストからの招待/オーバーレイ経由参加(セッション・ロビー
  機能の実装が必要、現状は手動でのID入力のみ)
- EOS(Epic Online Services)への対応(Steamを持たない相手向け)
- 通信切断時の再接続
- 観戦機能
- チート対策の強化(現状はサーバー権威止まりで、パケット改ざん等の高度な
  攻撃までは想定しない)
