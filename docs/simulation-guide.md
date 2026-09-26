# バランス検証シミュレーションの使い方

カードのステータス調整やキーワードの変更を行うたびに、手動でのプレイ確認
だけでなく、AI同士を大量に自己対戦させてログから勝率を読む方法で検証する。
毎回コマンドを再構築しなくて済むよう、このドキュメントに手順をまとめておく。
実装は`ACGGameMode::RunSelfPlaySimulation()`(`CGGameMode.cpp`)。

シミュレーションが完了すると`RunSelfPlaySimulation()`が
`FPlatformMisc::RequestExit()`でエンジンを自動終了させるため、GUIを手動で
閉じる必要は無い。プロセスの終了を待ってからログを集計すればよい。

**シミュレーションを実行したら、毎回その結果をまとめて出力/報告すること。**
「毎回シミュレーションの結果をまとめて出力してくれるようにしてほしい」という
フィードバックへの対応。`scripts/run_simulation.ps1`経由なら自動でこれを行う
(下記「実行コマンド」参照、`SimSummary`行を省略せず全て表示する)。手動で
UnrealEditor.exeを直接起動した場合(下記「手動で実行する場合」)も、完了後に
必ず`SimSummary`行を`grep`して、色ごとの総合勝率・対面ごとの相性・カードごとの
勝率(CardStats)まで含めた全体をまとめてユーザーへ報告する。色ごとの総合勝率
だけを抜粋して終わらない。

## 実行コマンド(推奨: スクリプト経由)

```bash
pwsh -File "C:/Users/kanki/Documents/app-develop/game/cardgame/scripts/run_simulation.ps1" -Matches 1000
```

`scripts/run_simulation.ps1`が以下を自動で行う。

1. 前回のログ(`CardGame/Saved/Logs/CardGame.log`)を削除する。
2. UnrealEditor.exeを`-SimulateMatches=N -game -log`で起動する
   (`-Wait`でプロセスの終了を同期的に待つ)。
3. シミュレーション完了時にUE側がエンジン終了を要求するので、プロセスが
   終了し次第、`SimSummary`行を自動抽出して整形表示する。

オプション:

- `-Matches <N>`: 対戦数(既定1000。「シミュレーションの回数を1000回に減らして
  ほしい」というフィードバックを受け、以前の既定3000から変更した)。軽い確認は
  500、カードごとの勝率(下記CardStatsの母数が20試合以上必要)まで見たい/色の
  相性まで見たいときは3000まで増やす。
- `-DisableBuy`: マーケット購入を無効化する診断用フラグ(`-SimDisableBuy`)を付与する。

## 手動で実行する場合

スクリプトを使わず直接起動することもできる。この場合もシミュレーション
完了時にエンジンが自動終了するため、待機後にログをそのまま`grep`できる。

```bash
rm -f "C:/Users/kanki/Documents/app-develop/game/cardgame/CardGame/Saved/Logs/CardGame.log"

"/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" \
  "C:/Users/kanki/Documents/app-develop/game/cardgame/CardGame/CardGame.uproject" \
  L_Card_GamePrototype -game -windowed -resX=800 -resY=600 \
  -SimulateMatches=1000 -log
```

- 起動元のシェルは、上記コマンドがエンジンの終了(自動)まで制御を返さない
  同期実行になる。数百戦なら数秒〜数十秒、3000戦でも数分程度で完了する。
- ビルドし直す前に、プロセスが完全に終了しているか一応確認すること
  (`tasklist //FI "IMAGENAME eq UnrealEditor.exe"`)。万一残っていると次の
  ビルドが`Unable to build while Live Coding is active`で失敗するので、
  その場合は`taskkill //F //PID <PID>`で終了させてからビルドし直す。

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
SimSummary   G14(世界樹の加護): PlayedGames=68 WinRate=66.2%
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

## デッキ構築の最適化(-OptimizeDecks)

上記のシミュレーションは`GetBasicColorDeckCardIds()`の固定デッキ(1色5種類を
3枚・残り10種類を1枚)を前提にした検証だが、そのデッキ自体が各色にとって
本当に最適かどうかは別問題。`ACGGameMode::RunDeckOptimization()`
(`CGGameMode.cpp`)は、色ごとに「今のベストデッキから1枚だけ別のカードに
入れ替える」山登り法(hill climbing)でデッキ構成そのものを探索する。

### 仕組み

1. 各色の探索の起点は`GetBasicColorDeckCardIds()`の現行デッキ。
2. 1色につき、現在のベストデッキを他4色の「現時点のベストデッキ」と
   総当たりでMatchesPerEvaluation回対戦させ、勝率を計測する。
3. デッキから1枚をランダムに抜き、同じ色の別カードに差し替えた候補デッキを
   作り、同じ方法で評価する(同名カード3枚までの制約は守る)。
4. 候補の勝率が現在のベストを上回っていれば採用し、そうでなければ捨てる。
   これをIterationsPerColor回繰り返す。
5. 赤→橙→緑→青→紫の順に2〜4を行うのを1ラウンドとし、Rounds回繰り返す。
   対戦相手のデッキも毎ラウンド更新されていくため、5色のデッキが互いに
   適応し合いながら同時に育っていく。

完了すると、色ごとに「ベースライン(元のGetBasicColorDeckCardIds)の勝率」
「最終デッキの勝率」「その差分」と、最終デッキの構成(カードIdごとの採用枚数)
をログへ出力し、`RunSelfPlaySimulation()`と同様にエンジンを自動終了する。

### 実行コマンド

```bash
pwsh -File "C:/Users/kanki/Documents/app-develop/game/cardgame/scripts/run_deck_optimization.ps1" -IterationsPerColor 30 -MatchesPerEvaluation 200 -Rounds 3
```

オプション:

- `-IterationsPerColor <N>`: 1色・1ラウンドあたりの入れ替え試行回数(既定30)。
- `-MatchesPerEvaluation <N>`: デッキ1つを評価するときの対戦数(既定200)。
  少ないと結果がノイジーになる(下記「数値の目安」参照)。
- `-Rounds <N>`: 5色を何周探索するか(既定3)。

手動起動する場合は`-SimulateMatches=N`の代わりに
`-OptimizeDecks=<IterationsPerColor> -OptimizeDeckMatches=<MatchesPerEvaluation> -OptimizeDeckRounds=<Rounds>`
を渡す(`-OptimizeDeckMatches`/`-OptimizeDeckRounds`省略時はそれぞれ200/3)。

### ログの読み方

```
=== DeckOptimization: 3 round(s), 30 iteration(s)/color/round, 200 match(es)/evaluation ===
DeckOptimization Round=1 Color=ECGColor::Red start WinRate=48.5%
DeckOptimization Round=1 Color=ECGColor::Red Iter=1/30 WinRate=52.0% -> accepted (was 48.5%)
DeckOptimization Round=1 Color=ECGColor::Red Iter=2/30 WinRate=46.0% -> rejected (best 52.0%)
...
=== DeckOptimization: final decks ===
DeckOptimization Result Color=ECGColor::Red BaselineWinRate=48.5% FinalWinRate=58.0% Delta=+9.5%
DeckOptimization   R01(黒鉄拾いの悪童) x3
DeckOptimization   R07(疾風の抜き手) x3
...
```

抽出コマンド:
```bash
grep "DeckOptimization Result" CardGame.log            # 色ごとの最終勝率・改善幅のみ
grep -A16 "DeckOptimization Result Color=ECGColor::Red" CardGame.log  # 赤の最終デッキ構成まで含めて見る
```

### 数値の目安・注意点

- `MatchesPerEvaluation`が小さい(数十戦程度)と1回の評価のノイズが大きく、
  「たまたま勝率が高く出ただけ」の入れ替えを採用してしまいやすい。実際に
  デッキ構成を見直す判断材料にするなら200戦以上を推奨(既定値)。
- 対戦相手側のデッキもラウンドをまたいで更新され続けるため、`Delta`が
  マイナスになることもある(評価時点でたまたま相手側が強くなっていた等)。
  1回の実行結果だけで判断せず、複数回実行するか`Rounds`を増やして傾向を見る。
  `-SimulateMatches`による通常のバランス検証と同様、最終的な採否は
  `GetBasicColorDeckCardIds()`側を手動で書き換えてから改めてシミュレーション
  (`-SimulateMatches`)で確認すること(このコマンドはデッキを自動では
  書き換えない、あくまで探索・提案のみ)。
- 探索は色内のカードだけを入れ替える(無色カードは対象外)ため、色の基礎性能
  そのものが弱い場合(紫など)は、デッキ構築の最適化だけでは埋まらない
  ギャップが残ることがある。その場合はカード単位の調整(`next-ruleset-cards-v1.md`)
  と併用する。
