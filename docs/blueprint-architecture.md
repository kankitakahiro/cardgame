# Blueprint設計書（初期）

## 目的
Blueprint中心でカードゲームの最小実装を作り、重い処理のみ後からC++に切り出す。

## 推奨Blueprintクラス
- `BP_CG_GameMode`
  - 対戦全体の進行管理
  - マッチ開始、ターン開始、ターン終了を制御
- `BP_CG_GameState`
  - 公開情報の保持（現在ターン、フェーズ、先攻後攻）
- `BP_CG_PlayerState`
  - プレイヤー単位データ（ライフ、手札枚数、デッキ枚数）
- `BP_CG_CardActor`
  - カードの表示と入力受付
  - カードIDを持ち、詳細データはDataAsset/DataTableから参照
- `BP_CG_HandWidget`
  - 手札UI表示、カード選択、プレイ操作
- `BP_CG_BoardWidget`
  - 場、山札、墓地など主要エリア表示
- `BP_CG_TurnManager`（Actor Component推奨）
  - ターンとフェーズ進行を状態遷移で管理
- `BP_CG_EffectResolver`（Actor Component推奨）
  - カード効果解決順序の管理

## データ設計
- カードマスタは `DataTable` または `PrimaryDataAsset`
- 各カードは以下を最低限保持
  - `CardId`（一意ID）
  - `CardName`
  - `Cost`
  - `CardType`
  - `EffectId`（効果解決ロジック参照キー）

## イベントフロー（最小）
1. マッチ開始時に各プレイヤーへ初期手札配布
2. ターン開始で1枚ドロー
3. プレイヤーがカードをプレイ
4. 効果解決
5. 勝敗条件判定
6. ターン終了して相手へ遷移

## C++移行候補（重い部分）
- 効果解決ロジック（分岐と連鎖が多い処理）
- AI思考処理（探索、評価関数）
- デッキシャッフルや抽選の共通ロジック
- ルール検証（プレイ可能判定、対象選択判定）

## 命名ルール（推奨）
- Blueprint: `BP_CG_` プレフィックス
- Widget: `WBP_CG_` プレフィックス
- DataAsset: `DA_CG_` プレフィックス
- Enum/Struct: `E_CG_`, `ST_CG_` プレフィックス

## 最初に作る順番
1. `BP_CG_GameMode`
2. `BP_CG_GameState`
3. `BP_CG_PlayerState`
4. `WBP_CG_HandWidget`
5. `WBP_CG_BoardWidget`
6. `BP_CG_EffectResolver`
