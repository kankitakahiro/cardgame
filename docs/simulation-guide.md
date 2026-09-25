# バランス検証シミュレーションの使い方

カードのステータス調整やキーワードの変更を行うたびに、手動でのプレイ確認
だけでなく、AI同士を大量に自己対戦させてログから勝率を読む方法で検証する。
毎回コマンドを再構築しなくて済むよう、このドキュメントに手順をまとめておく。
実装は`ACGGameMode::RunSelfPlaySimulation()`(`CGGameMode.cpp`)。

## 実行コマンド

```bash
# 1. 前回のログが残っていると新しい結果と混ざって読みにくいので、先に削除する
rm -f "C:/Users/kanki/Documents/app-develop/game/cardgame/CardGame/Saved/Logs/CardGame.log"

# 2. ビルド済みのUnrealEditor.exeを-gameモードで起動し、指定回数の自己対戦を
#    同期的に実行させる(GUIは実質使わないが-windowedを付けないと起動に失敗する
#    環境がある)。数百戦なら数秒〜数十秒、3000戦でも数分程度で完了する。
"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" \
  "C:/Users/kanki/Documents/app-develop/game/cardgame/CardGame/CardGame.uproject" \
  L_Card_GamePrototype -game -windowed -resX=800 -resY=600 \
  -SimulateMatches=3000 -log
```

- `-SimulateMatches=N`: N戦のAI対AI(基本単色デッキ同士、色はランダムに毎回選び直す)を実行する。
- 目安: 軽い確認は500戦、カードごとの勝率(下記CardStatsの母数が20試合以上必要)まで
  しっかり見たい/色の相性まで見たいときは3000戦にする。
- `-SimDisableBuy`: マーケット購入を無効化する診断用フラグ(通常は付けない)。
- 実行後、コマンド自体はすぐ制御を返さずゲームプロセスが起動したままになることがある。
  次にビルドし直す前に、そのプロセスが完全に終了しているか確認すること
  (`tasklist //FI "IMAGENAME eq UnrealEditor.exe"`)。残っていると次のビルドが
  `Unable to build while Live Coding is active` で失敗するので、その場合は
  `taskkill //F //PID <PID>` で終了させてからビルドし直す。

## ログの読み方(`CardGame/Saved/Logs/CardGame.log`)

全て`LogCardGame: SimSummary ...`または`LogCardGame: SimMatch ...`という
プレフィックスで出力される。主な行は以下の4種類。

### 1. 全体サマリ(先頭1行)

```
=== SimSummary: 3000 matches, 42 draws, avg 14.3 turns/match ===
```

引き分け数と平均ターン数。平均ターン数が極端に短い/長い場合、何らかの
壊れたコンボや無限ループに近い状態が無いか疑う材料になる。

### 2. 先攻/後攻の勝率(色に依らない集計)

```
SimSummary FirstPlayer Games=3000 Wins=1580 Losses=1390 Draws=30 WinRate=52.7%
```

抽出コマンド:
```bash
grep "SimSummary FirstPlayer" CardGame.log
```

### 3. 色ごとの勝率+相性(色の数だけ繰り返す)

```
SimSummary Color=ECGColor::Red Games=1205 Wins=650 Losses=555 Draws=0 WinRate=53.9%
SimSummary   Red vs Orange: 120-95
SimSummary   Red vs Green: 140-110
...
```

- 1行目がその色の総合勝率。
- 続く`A vs B: W-L`の行が色同士の相性(Aから見た対Bの勝敗数)。
  `next-ruleset-simulation-v1.md`のような相性分析はここから拾う。
- 抽出コマンド:
  ```bash
  grep "SimSummary Color=" CardGame.log        # 色ごとの総合勝率のみ
  grep -A5 "SimSummary Color=ECGColor::Red" CardGame.log  # 赤の相性まで含めて見る
  ```

### 4. カードごとの勝率(色の中で強い順、20試合以上のみ)

```
SimSummary CardStats Color=ECGColor::Green (プレイした試合の勝率、20試合以上のみ、強い順)
SimSummary   G14(大地の祝福): PlayedGames=68 WinRate=66.2%
...
```

- 「そのカードをプレイした試合のうち勝てた割合」。**色をまたいだ比較は
  地力差(色ごとの総合勝率)と混ざるので意味を持たない**。同じ色の中での
  相対的な強弱だけを見ること。
- 抽出コマンド:
  ```bash
  grep -A16 "SimSummary CardStats Color=ECGColor::Green" CardGame.log
  ```

## 数値の目安

- これまでの計測では、健全な状態は色ごとの総合勝率が概ね45〜52%のレンジに
  収まっている状態(`game-rules-minimum.md`「色ごとの一目比較」も参照)。
  55%を超える、または45%を割り込む色があれば調整候補。
- カード単位は同じ色の中で60%を超える/40%を割り込むあたりが調整候補の目安
  (母数20試合はノイズが大きいので、可能なら3000戦規模で見て50試合以上ある
  カードを優先的に見る)。
- 1回の調整では大きく振りすぎない。過去に緑のステータスを2段階連続で下げたら
  59.9%→40.4%まで振れすぎた例があるため、1項目ずつ小さく調整して
  再計測するサイクルを回すこと。

## この方式の限界(既知の制約)

- 各対戦は**基本単色デッキ同士**(`UCGCardDatabase::GetBasicColorDeckCardIds`)
  でのみ行われる。実際のプレイヤーは複数色を混ぜたデッキを組めるため、
  混色デッキでの強さやしきい値(17/25枚)ちょうど付近の挙動はこの
  シミュレーションでは検証できない。
- AI同士の対戦のため、人間のプレイヤーがAIより上手く/下手く立ち回った場合の
  差は反映されない(`UCGAIOpponent`の判断ロジックの範囲内での強さ)。
- 引き分けは100ターンで打ち切った試合(`ACGGameMode::StartTurn`の安全装置)。
  引き分けが多い場合はどこかで手が止まっている可能性があるため、
  `SimMatch`行を`grep`して該当試合の直前ログを追う。
