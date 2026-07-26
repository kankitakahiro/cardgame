# プロトタイプ実装の遊び方

## 概要
- 実装ファイル: `prototype/cardgame_cli.py`
- 形式: ターミナル上で遊ぶ1対1（You vs CPU）
- ルール: 初期デッキ12枚、マーケット5枚公開、購入とプレイは共通マナ

## 起動方法
1. ワークスペースのルートで次を実行
   - `python prototype/cardgame_cli.py`

## コマンド
- `play N`: 手札インデックスNのカードをプレイ
- `buy N`: マーケットインデックスNのカードを購入
- `attack`: 攻撃可能な味方Unitで自動攻撃
- `end`: ターン終了
- `help`: コマンド一覧表示

## 画面の見方
- `[G|R]`: Guardあり / Ready（攻撃可能）
- `[G|S]`: Guardあり / Summoning sickness（召喚酔い）
- `Mana x/y`: 今使えるマナ / 最大マナ

## 補足
- 一部カード効果はCLI向けに簡略化しています。
- 表のカードIDとカード名は `docs/initial-cards-v0.1.md` に対応しています。
