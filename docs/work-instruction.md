# カードゲーム作業手順書（UE 5.8）

## 目的
Unreal Engine 5.8 でカードゲームの初期プレイアブル版を作るための、実作業向け手順をまとめる。

## 前提
- Unreal Engine 5.8 をインストール済み
- このリポジトリを Git で管理している
- Blueprint中心で開始する
- 重い処理のみ後から C++ に切り出す

## 参照ドキュメント
- [UE 5.8 実装チェックリスト](ue58-implementation-checklist.md)
- [Blueprint設計書](blueprint-architecture.md)
- [最小ルール仕様](game-rules-minimum.md)
- [初期カード一覧 v0.1](initial-cards-v0.1.md)

## 作業開始前の確認
1. `CardGame/CardGame.uproject` が存在することを確認する。
2. Unreal Engine 5.8 でプロジェクトを開けることを確認する。
3. `L_Card_GamePrototype` を開けることを確認する。
4. Git の作業ツリーを確認する。
5. 既存のテンプレート由来ファイルを消す前に、必要なものかどうかを確認する。

## 実装順

### 1. レベルと起点の確認
1. `L_Card_GamePrototype` を開く。
2. 一度 `Save All` を実行する。
3. そのレベルを起点として作業を進める。

### 2. Blueprint 基盤の作成
1. `BP_CG_GameMode` を作る。
2. `BP_CG_GameState` を作る。
3. `BP_CG_PlayerState` を作る。
4. `BP_CG_GameMode` に初期化処理を置く。
5. `BP_CG_GameState` に公開状態を置く。
6. `BP_CG_PlayerState` に各プレイヤーの状態を置く。

### 3. UI の作成
1. `WBP_CG_HandWidget` を作る。
2. `WBP_CG_BoardWidget` を作る。
3. `WBP_CG_MarketWidget` を作る。
4. それぞれに更新関数を用意する。
5. 画面に表示できる最小の見た目を先に作る。

### 4. データの作成
1. `ST_CG_CardRow` を作る。
2. `DT_CG_Cards` を作る。
3. `DT_CG_Market` を作る。
4. [初期カード一覧 v0.1](initial-cards-v0.1.md) を元にデータを入力する。
5. まずは 24 枚すべての定義を入れる。

### 5. ゲームループの作成
1. 初期手札を配る。
2. ターン開始でドローする。
3. 手札からプレイする。
4. マーケットから購入する。
5. 攻撃処理を行う。
6. 勝敗判定を行う。
7. ターン終了で相手へ渡す。

### 6. 最低限の遊べる状態にする
1. 1対1で動くことを確認する。
2. 先攻後攻をランダムにする。
3. マーケット5枚を常時表示する。
4. `play / buy / attack / end` が成立することを確認する。
5. 勝敗が最後までつくことを確認する。

## 作業ルール
- Blueprint は `BP_CG_`、Widget は `WBP_CG_`、DataAsset は `DA_CG_`、Struct/Enum は `ST_CG_` / `E_CG_` を使う。
- 1機能ずつ作る。
- 途中で動作確認を挟む。
- まずは見た目より処理の流れを優先する。
- 理解しづらい処理はコメントよりノードのまとまりで整理する。

## Git ルール
1. 大きな区切りごとにコミットする。
2. Unreal 生成物のうち、コミットすべきものと除外すべきものを毎回確認する。
3. `DerivedDataCache`、`Intermediate`、`Saved` の扱いに注意する。

## 迷ったときの判断
- ルールが曖昧なら [最小ルール仕様](game-rules-minimum.md) を優先する。
- カードの強さが分からなければ [初期カード一覧 v0.1](initial-cards-v0.1.md) のレシオを見る。
- 実装順が分からなければ [UE 5.8 実装チェックリスト](ue58-implementation-checklist.md) に戻る。
