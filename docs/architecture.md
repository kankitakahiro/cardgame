# カードゲーム アーキテクチャ設計書

Unreal Engine 5.8上のカードゲーム実装(`CardGame/Source/CardGame/`)の設計を
まとめた恒久ドキュメント。過去の一時的な作業手順書(リファクタリング計画・
機能実装計画)は完了後に削除する運用のため、そこで確定した設計のうち今後も
価値のある部分はここに集約している。

実装上の技術的な制約・落とし穴(UMGのRebuildWidgetの罠、Python自動化の限界等)
は [automation-notes.md](automation-notes.md) を参照。ルール仕様は
[game-rules-minimum.md](game-rules-minimum.md)、カード一覧・バランスは
[initial-cards-v0.1.md](initial-cards-v0.1.md) を参照。

## 全体構成

UIも含めて全てC++で実装している(UMG WidgetTreeがPython Editor Scripting APIから
編集できないため。詳細は automation-notes.md)。BlueprintクラスはC++クラスを親に
持つ薄いラッパー(`BP_CG_GameMode`/`GameState`/`PlayerState`、
`Content/CardGame/Core/`)のみ残っており、ロジックは全てC++側にある。

### レベルと画面遷移

3つのレベルを `UGameplayStatics::OpenLevel` で行き来する構成(ウィジェット切り替え
ではない)。

```
L_Lobby ──(デッキ構築)──> L_DeckBuilder ──(保存)──> L_Lobby ──(バトル開始)──> L_Card_GamePrototype ──(勝敗確定後、ロビーへ戻る)──> L_Lobby
```

| レベル | GameMode | HUD | 役割 |
|---|---|---|---|
| `L_Lobby` | `ACGLobbyGameMode` | `UCGLobbyHUD` | 「デッキ構築」「バトル開始」の2ボタンのみ |
| `L_DeckBuilder` | `ACGDeckBuilderGameMode` | `UCGDeckBuilderHUD` | 24種から12枚を選ぶデッキ編成画面 |
| `L_Card_GamePrototype` | `ACGGameMode` | `UCGGameHUD` | 対人1vsAI1の対戦本編 |

`ACGLobbyGameMode`/`ACGDeckBuilderGameMode` は対戦ロジックを持たない軽量GameMode
で、`BeginPlay`から1フレーム遅延させてHUDを`AddToViewport`する点以外はほぼ同じ
構成(`SetupHUD`+`FTimerHandle`)。

### デッキの永続化

レベルをまたいで生存させる必要があるデータ(プレイヤーが選んだデッキ)は
`UGameInstance`派生の `UCGGameInstance` が保持する(GameMode/GameStateはレベル
遷移で破棄されるため)。

- `UCGGameInstance::PlayerDeckCardIds`: 現在選択中のデッキ(12枚)。
  `UCGDeckBuilderHUD`が更新し、`ACGGameMode::InitializeMatch()`がここから読み取る。
  未設定/不正(12枚でない)ならスターターデッキへフォールバックする。
- `UCGDeckSaveGame`(`USaveGame`派生): ディスク保存用。`SaveDeckToDisk()`で
  書き出し、`UCGGameInstance::Init()`で起動時に読み込む(無ければスターター
  デッキがデフォルト)。

## 対戦ロジックのクラス責務

| クラス | 責務 |
|---|---|
| `ACGGameMode` | 対戦進行の中核。マッチ初期化、ターン進行、`RequestPlayCard`/`RequestBuyCard`/`RequestAttack`/`RequestEndTurn`の受付、勝敗判定。手番がAI側になったら`UCGAIOpponent`を呼ぶだけで、AIの思考ロジック自体は持たない |
| `ACGGameState` | 対戦全体の公開状態(ターン数、フェーズ、勝者、マーケット公開5枚とその補充プール、両陣営の`ACGPlayerState`への参照) |
| `ACGPlayerState` | 片側プレイヤーの全データ(HP/マナ/手札/デッキ/捨て札/場)と、ドロー・プレイ・購入・攻撃・死亡処理・カード効果ディスパッチ |
| `UCGAIOpponent` | AI側(Side 1固定)の意思決定ロジック。`RunTurn()`1回で購入→プレイ→攻撃→EndTurnまで同期的に完結させる。`ACGGameMode`の公開APIのみを呼ぶため、進行ルール自体はGameMode側に残る |
| `UCGCardDatabase` | 24枚のカードマスタ(静的データ)を保持する`BlueprintFunctionLibrary`。`GetAllCards()`/`FindCard()`/`GetStarterDeckCardIds()` |

`ACGPlayerState`と`UCGAIOpponent`を分けているのは、AIをより賢くする・難易度を
分けるといった将来の拡張でGameMode本体を肥大化させないため。

### 場のユニットの状態管理

`FCGBoardUnit`(`CGTypes.h`)1構造体に、場のユニット1体分の状態
(`CardId`/`Atk`/`Hp`/`bCanAttack`/`bHasGuard`)をまとめている。新しい状態
(毒/バフ/沈黙等)を足す場合はこの構造体にフィールドを追加する。

### カード効果ディスパッチ(EffectId → ハンドラ関数)

`FCGCardDef::EffectId`(FName)を経由するテーブル駆動方式。カードが増えるほど
分岐が際限なく伸びるif/elseの塊を避けるための設計。実装は`CGPlayerState.cpp`の
無名namespace内。

- ハンドラは4種類、それぞれ `EffectId -> 関数ポインタ` の `TMap` で管理:
  - `FSpellEffectHandler`(`GetSpellEffectHandlers()`): Spellカードをプレイした
    ときの効果(例: `Handle_OnPlayDamageTarget`)
  - `FUnitOnPlayEffectHandler`(`GetUnitOnPlayEffectHandlers()`): Unitが場に
    出た瞬間の効果(例: `Handle_ScoutTop1`)
  - `FUnitOnDeathEffectHandler`(`GetUnitOnDeathEffectHandlers()`): Unitが
    死亡した瞬間の効果(例: `Handle_OnDeathDraw`)
  - `FEndTurnAuraEffectHandler`(`GetEndTurnAuraEffectHandlers()`): 場にいる間
    ずっと有効な常在効果のうち、ターン終了時に判定するもの
    (例: `Handle_OnBuyEndTurnDiscardDraw`)。常在効果全般は
    `ACGPlayerState::HasBoardUnitWithEffect(EffectId)`で都度判定している
    (例: 追撃の射手・市場監督官・連鎖術の教授は攻撃/購入/Spell発動のタイミングで
    直接`HasBoardUnitWithEffect`を呼んでいる)
- ハンドラは全て状態を持たない静的関数(関数ポインタ)なので、Side0/Side1どちらの
  `ACGPlayerState`インスタンスに対しても同じテーブルを使い回せる。
- 未登録のEffectId(将来ここに新しいカードタイプ用の値を置いた場合等)は何も
  起きない(テーブルに見つからなければ無視するだけ)。

**新しいカード効果を追加する手順**:
1. `CGTypes.h`の`CGEffectId`namespaceに定数を1行追加する。
2. `CGPlayerState.cpp`に`Handle_XXX`関数を1つ書く(効果の種類に応じて4種類の
   シグネチャのいずれかに合わせる)。
3. 対応する`Get*Handlers()`のテーブルに1行追加する。
4. `CGCardDatabase.cpp`の`MakeCard()`呼び出しで、対象カードの`EffectId`に
   その定数を指定する。

### カードデータモデル(`FCGCardDef`, `CGTypes.h`)

| フィールド | 用途 |
|---|---|
| `CardId`/`CardName`/`CardType`/`Cost`/`Atk`/`Hp` | 基本情報 |
| `Description` | 効果テキスト(UI表示用) |
| `Tribe` | 部族/系統(例: 戦士、アンデッド)。**将来のシナジー効果実装を見込んだ予約フィールド**で、現状は参照するロジックが無く、UI表示と一部カードのサンプル値のみ |
| `FlavorText` | カード下部の短いフレーバーテキスト(世界観演出用、UI表示のみ) |
| `EffectId`/`EffectValue` | 上記のカード効果ディスパッチで使うキーと数値パラメータ |
| `Ratio` | バランス調整用の補助値(`initial-cards-v0.1.md`のManaRatio)。ゲームロジックの判定には使わない |
| `Tags`(カンマ区切り文字列) + `HasTag()` | キーワード能力。現状 `"Haste"`(速攻)/`"Guard"`(守護)のみロジックが参照する |

**新しいキーワード(タグ)を追加する手順**: `Tags`に新しい文字列を足し、
`ACGPlayerState::PlayCardFromHand()`(登場時の`bCanAttack`/`bHasGuard`相当の
初期化)や`ACGGameMode::RequestAttack()`(守護の対象強制ロジック)など、
参照すべき箇所に`Def.HasTag(TEXT("新タグ"))`の判定を足す。

**新しい部族(Tribe)シナジーを追加する場合**: `Tribe`は現状文字列比較の
自由形式データ。シナジー効果(例:「戦士が2体以上いれば」)を実装する場合は、
`ACGPlayerState`に「指定Tribeを持つBoardUnit数を数える」ヘルパーを追加し、
上記のカード効果ディスパッチのハンドラから呼び出す形になる見込み。

**新しいカードタイプ(`ECGCardType`)を追加する場合**: 現状Unit/Spellの2値
前提で分岐している箇所は主に次の3箇所。
- `ACGPlayerState::PlayCardFromHand()`(`if (Def.CardType == ECGCardType::Unit) ... else ...`という二択分岐)
- `ACGPlayerState::TryReturnRandomSpellFromDiscardToHand()`/`TryMoveRandomDiscardUnitToDeckTop()`(型で絞り込む効果ヘルパー)
- `UCGCardSlotWidget::ApplyCardTypeColor()`(種別ごとの枠色)

いずれも「Unitなら/Spellなら」という二択のif/elseになっているため、3種類目を
足す場合はswitch文への書き換えを検討する。

## カードUIの設計

### 共通コンポーネント

手札・マーケット・場のユニット・相手の裏向き手札・デッキ構築画面(上段/下段)、
カードを表示する箇所は全て`UCGCardSlotWidget`1つを共通利用している。**カードは
どの画面でも同じレイアウト・同じ情報を表示する**という方針(画面ごとに
「デッキ内」「攻撃可能」等の文字注記を出し分けることはしない)。文脈依存の
状態は`SetRenderOpacity()`による非文字表現(暗くする)のみで表現する
(例: 攻撃不可能な自分のユニット、デッキ構築画面で既に選択済みのカード)。

`SetCardData(Def, OverrideAtk, OverrideHp)` でカード内容を反映する
(場のユニットはバフ等で現在値がカード基本値と異なることがあるため、
Override引数で上書きできる)。相手の裏向き手札は`SetFaceDown()`で名前欄に
"?"だけを表示する。

カードサイズは一般的なトレーディングカードの比率(2.5:3.5インチ、約0.714)に
合わせた `240x336`(`UCGCardSlotWidget::CardWidth`/`CardHeight`)に固定。

外側から: 境界線(`OuterBorder`)→ Unit/Spell種別の色枠(`TypeBorder`)→
レアリティ/雰囲気用の枠(`RarityBorder`、現状データなしの中間色のみ。将来の
レアリティ概念導入に備えた予約枠)。中身は上から: ヘッダー(コストバッジ+
カード名)→ イラスト欄(プレースホルダー)→ 部族/キーワード行 → 効果テキスト
→ フレーバーテキスト → 右下ATK/HP(Unitのみ)。

### ホバー拡大とZ順序

`UHorizontalBox`等の通常のUMGパネルは常に子の追加順で描画され、個々の子だけを
Z順序で前面へ出す簡単な方法がない。そのため拡大表示は「カード自身を大きくする」
のではなく、次の方式にしている。

1. `UCGCardSlotWidget`はホバー状態が変わると`OnHoverChanged`デリゲートを
   ブロードキャストするだけで、自分自身の見た目(レイアウトサイズ)は変えない。
2. `CGGameHUD`/`CGDeckBuilderHUD`の共通基底クラス`UCGCardHostWidget`が、
   画面全体に重ねた最前面専用の`UCanvasPanel`レイヤー(`BuildRootOverlay()`で
   `UOverlay`の一番最後の子として追加、常に最前面)にカードの複製を1枚だけ
   遅延生成して使い回す。
3. ホバーされたカードの内容を`CopyCardDataTo()`で複製へコピーし、
   `FGeometry::AbsoluteToLocal`でそのカードと同じ画面上の位置を計算して
   `UCanvasPanelSlot`に配置、`SetRenderScale`で拡大する。

派生HUDは`BuildRootOverlay(Root)`の戻り値を`WidgetTree->RootWidget`に設定し、
カードウィジェットを1枚作るたびに`RegisterCardHoverPreview(SlotWidget)`を
呼ぶだけでこの挙動が有効になる。行のレイアウト自体は拡大分の余白を確保する
必要がないため、カード間隔は見た目用の小さな固定値(`CardGap`)のみで良い。

## 今後の拡張ポイント(チェックリスト)

- **カード効果を増やす**: 上記「カード効果ディスパッチ」の4手順。
- **キーワード(タグ)を増やす**: `Tags`に文字列を足し、`HasTag()`で参照する
  箇所を追加。
- **部族(Tribe)シナジーを実装する**: `ACGPlayerState`に部族カウントヘルパーを
  追加し、ハンドラから呼び出す。
- **カードタイプを増やす**: 上記「新しいカードタイプを追加する場合」の3箇所
  (`PlayCardFromHand`/discard-graveyardヘルパー/`ApplyCardTypeColor`)を
  switch文に書き換える。
- **カードを1枚増やす**: `CGCardDatabase.cpp`の`BuildAllCards()`に
  `MakeCard(...)`を1行追加するだけ(実データはC++側のみが正。
  `Content/CardGame/Data`にDataAsset等は置いていない)。
- **場のユニットに新しい状態を足す**: `FCGBoardUnit`にフィールドを追加。
