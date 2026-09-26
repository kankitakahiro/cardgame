# パッケージ化(配布用ビルド)の手順

エディタを介さずに単体実行できるビルド(`CardGame.exe`一式)を作る手順。
「他人に渡すときはどうしたらいいか」「パッケージにすると画面遷移ができない」
という実際のトラブルを踏まえてまとめている。**今後パッケージ化を行う際は
必ずこのドキュメントを参照すること。**

## 実行コマンド

```bash
"/c/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun \
  -project="C:\Users\kanki\Documents\app-develop\game\cardgame\CardGame\CardGame.uproject" \
  -noP4 -platform=Win64 -clientconfig=Development -cook -build -stage -pak -utf8output
```

- 出力先: `CardGame/Saved/StagedBuilds/Windows/`(`CardGame.exe`を含む一式)。
- 実行には2〜3分程度かかる(コンテンツ量に応じて増える)。
- `-clientconfig=Development`は動作確認・社内配布向け。不特定多数への配布
  (Steam等)を予定する場合は`Shipping`を検討する(下記「配布コンフィグの選び方」参照)。

### `-Map=`引数は使わない

BuildCookRunには特定のマップだけをクックする`-Map=/Game/.../L_Xxx+/Game/.../L_Yyy`
という引数もあるが、**この環境(Git Bash経由でのRunUAT.bat呼び出し)では
`-Map=`を渡すと`'C:\Program' は、内部コマンドまたは外部コマンド、
操作可能なプログラムまたはバッチファイルとして認識されていません。`という
エラーで即座に失敗することを確認済み**(原因未特定、`=`と`/`を含む引数が
バッチファイル内部の引用符処理を壊していると推測される)。マップの選定は
必ず後述の`DefaultGame.ini`側の設定で行うこと。

## 重要: 含めるマップを`DefaultGame.ini`に明示する

### 起きた不具合

「オンライン対戦」「デッキ構築」ボタンを押しても画面が遷移しない、
「CPU対戦(対戦開始)」も動かない、という不具合が発生した。原因は
パッケージに`L_Lobby`しか含まれておらず、`L_Card_GamePrototype`
(対戦画面。CPU対戦・オンライン対戦のホスト両方がここへ遷移する)と
`L_DeckBuilder`(デッキ構築画面)が含まれていなかったこと。

`UCGLobbyHUD`側の`UGameplayStatics::OpenLevel(...)`呼び出し自体は
正常だが、パッケージ内に該当マップのパッケージが無いため
`LogLevel: Warning: WARNING: The map '...' does not exist.`
`LogStreaming: Warning: LoadPackage: SkipPackage: ... does not exist on disk`
という警告と共にサイレントに遷移が失敗し、ボタンを押しても何も
起こらないように見えていた(`CardGame/Saved/Logs/CardGame.log`で確認できる)。

`+DirectoriesToAlwaysCook=(Path="/Game/CardGame")`という既存設定
(背景画像等、コード内のハードコードされたパス参照でしかロードされない
アセット向けに以前追加されたもの)だけでは、**マップパッケージ自体は
含まれない**らしいことも分かった(マップはこの設定と別扱いになる)。

### 対処(実施済み、今後も維持すること)

`CardGame/Config/DefaultGame.ini`の`[/Script/UnrealEd.ProjectPackagingSettings]`
セクションに、含める全マップを`+MapsToCook`で明示的に列挙している。

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/Game/CardGame")
+MapsToCook=(FilePath="/Game/CardGame/Maps/L_Lobby")
+MapsToCook=(FilePath="/Game/CardGame/Maps/L_Card_GamePrototype")
+MapsToCook=(FilePath="/Game/CardGame/Maps/L_DeckBuilder")
```

**新しいマップ(レベル)を追加したときは、必ずこのリストにも追加すること。**
追加を忘れると、そのマップへ遷移するボタン・機能だけがパッケージ版で
サイレントに動かなくなる(エディタの`-game`実行では全マップがディスク上に
存在するため再現しない。**エディタでは問題なくても、パッケージ版で
初めて発覚するバグ**という点に注意)。

## パッケージ後の動作確認(必須)

パッケージ後は必ず以下を確認してから相手に渡す。

1. `CardGame.exe`を起動し、ロビー画面が表示されることを確認する。
2. 「デッキ構築」「オンライン対戦」「対戦開始(CPU対戦)」の各ボタンを
   実際に押し、それぞれの画面/対戦が問題なく開始できることを確認する。
3. `CardGame/Saved/StagedBuilds/Windows/CardGame/Saved/Logs/CardGame.log`
   を`grep`し、`does not exist`・`SkipPackage`・`TravelFailure`が
   出ていないことを確認する。

```bash
grep -i "does not exist\|SkipPackage\|TravelFailure" \
  "C:/Users/kanki/Documents/app-develop/game/cardgame/CardGame/Saved/StagedBuilds/Windows/CardGame/Saved/Logs/CardGame.log"
```

手動でボタンを押す代わりに、対象マップへ直接起動して`LoadMap`が
`Load map complete`まで到達するかだけを機械的に確認することもできる
(全画面を触るわけではないので、あくまで簡易チェック)。

```bash
"C:/Users/kanki/Documents/app-develop/game/cardgame/CardGame/Saved/StagedBuilds/Windows/CardGame.exe" \
  L_Card_GamePrototype -windowed -log
```

## 配布コンフィグの選び方

| | Development(既定) | Shipping |
|---|---|---|
| 用途 | 動作確認・社内テスト | 不特定多数への配布(Steam等) |
| 実行速度・サイズ | やや遅い/大きい | 最適化済みで速い/小さい |
| コンソール・デバッグ機能 | 有効 | 無効 |
| ログ出力 | 詳細 | 最小限 |

Shippingでパッケージする場合は`-clientconfig=Development`を
`-clientconfig=Shipping`に変える(他の引数は同じでよい)。

## 他人に渡すとき

`CardGame/Saved/StagedBuilds/Windows/`フォルダ**ごと**ZIP圧縮して渡す
(`CardGame.exe`単体では動かない。同階層の`CardGame/`・`Engine/`
フォルダやManifestファイルも実行に必要)。
