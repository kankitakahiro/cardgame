# UE 5.8 カードゲーム実装チェックリスト

## 目的
Unreal Engine 5.8 上で、カードゲームの初期プレイアブル版を最短で組む。

## 先に作るもの
1. Map
   - `L_CardGamePrototype`
   - カードゲーム用の空レベル
2. Blueprint Classes
   - `BP_CG_GameMode`
   - `BP_CG_GameState`
   - `BP_CG_PlayerState`
3. Widgets
   - `WBP_CG_HandWidget`
   - `WBP_CG_BoardWidget`
   - `WBP_CG_MarketWidget`
4. Data
   - `ST_CG_CardRow`
   - `DT_CG_Cards`
   - `DT_CG_Market`

## 実装順
1. `BP_CG_GameMode` でターン制御を作る
2. `BP_CG_GameState` に公開情報を置く
3. `BP_CG_PlayerState` にHP・手札・デッキ・マナを置く
4. `WBP_CG_HandWidget` で手札を表示する
5. `WBP_CG_BoardWidget` で盤面を表示する
6. `WBP_CG_MarketWidget` で市場5枚を表示する
7. DataTable からカードを読み込む
8. `play / buy / attack / end` のコマンドに相当するボタンを実装する
9. 1対1のプロトタイプを成立させる

## この段階でのカードデータ参照先
- [docs/initial-cards-v0.1.md](initial-cards-v0.1.md)

## 最低限の完成条件
- 先攻後攻で開始できる
- ドローできる
- 手札からカードを出せる
- マーケットからカードを買える
- 攻撃でHPを削れる
- 勝敗が決まる

## 補足
- Blueprint作成後は、テンプレート由来の FirstPerson 系アセットを減らしていく。
- 重い処理は後から C++ に切り出す。

## 進捗メモ(2026-08-29時点)
- Map / BP_CG_GameMode・GameState・PlayerState / カードデータ24枚: 作成済み。
- **ゲームループ・UIともにBlueprintのイベントグラフではなく、C++
  (`CardGame/Source/CardGame/`)で実装済み**。Unreal 5.8のPython Editor Scripting
  APIがBlueprintグラフへのノード追加・配線もUMG WidgetTreeの編集も公開していない
  ため(詳細: [automation-notes.md](automation-notes.md))、`BP_CG_GameMode`/
  `GameState`/`PlayerState`はC++クラス(`CGGameMode`/`CGGameState`/`CGPlayerState`)
  を親に持つ薄いBlueprintになっている。UIは`WBP_CG_*`アセットを使わず、
  `UCGGameHUD`/`UCGCardSlotWidget`というC++の`UUserWidget`派生クラスが
  `NativeConstruct`ではなく`RebuildWidget`内でウィジェット階層を組み立て、
  `ACGGameMode::BeginPlay`から`AddToViewport`で直接表示している。
- **`play/buy/attack/end`のUIボタンから実際に操作できることを、ユーザー自身の
  PIE確認で検証済み**(手札からのプレイ、マーケット購入判定、攻撃によるHP減少、
  ターン交代とマナの個別増加、すべて正常動作)。「最低限の完成条件」は全て満たした。
- カード固有効果(24枚中、基本3効果=単体ダメージ/単体回復/死亡時ドロー以外)は
  `EffectId=TODO_*`のままデータのみで未実装。
- `WBP_CG_HandWidget`/`BoardWidget`/`MarketWidget`(Content/CardGame/UI)は
  Python自動化時代の名残で空の器のまま残っているが、実際のUIはC++の
  `UCGGameHUD`が担っているため実質未使用。整理は今後の課題。
