# Python自動化に関する既知の制約(UE 5.8)

## 背景
`scripts/`配下のPythonスクリプトは、Unreal Editorを`-ExecutePythonScript=<path>`付きで
起動し、Editor Scripting API経由でBlueprintアセットを無人で編集するための仕組み。
起動コマンドの型は `scripts/run_ue_with_log.ps1` を参照。

## 確認できた制約(2026-08-23 実機検証済み)
- `unreal.EdGraph` に `add_node` に相当するメソッドが存在しない。
- `EdGraph.Nodes` プロパティは protected でPythonから読み取りすらできない
  (`get_editor_property('nodes')` はエラーになる)。
- `unreal.BlueprintEditorLibrary` にはノード生成・ピン接続用の関数が存在しない
  (`add_event_override` / `add_function_graph` / `add_member_variable` などの
  “構造編集”系はあるが、グラフ内のノード配置・配線は非公開)。
- `unreal.new_object(unreal.K2Node_CustomEvent, outer=graph)` でノードオブジェクト自体は
  生成できるが、`CustomFunctionName`のようなノード固有プロパティやピン割当関数
  (`allocate_default_pins`等)がPythonに公開されておらず、グラフの`Nodes`配列にも
  正式登録されない(=Blueprintエディタ上で有効なノードにならない)。

## 結論
**Blueprintのイベントグラフへのノード追加・配線は、標準のUnreal Python Editor
Scripting APIでは自動化できない。** これは実装スキルの問題ではなく、UE 5.8の
Python公開範囲が「アセット/変数/関数グラフの器」までで、Kismetグラフ編集
(`FBlueprintEditorUtils` / `FKismetEditorUtilities`相当)はエディタC++内部にしか
存在しないことに起因する。

前回セッションの `dump_*` / `inspect_*` / `probe_*` 系スクリプトが大量に存在した
理由もこれで説明できる(この壁に繰り返し当たっていた)。

## 自動化できる範囲(実績あり)
- Blueprintクラスの新規作成(`AssetTools.create_asset` + 各種Factory)
- メンバ変数の追加(`BlueprintEditorLibrary.add_member_variable` + `EdGraphPinType.import_text`)
- イベントのオーバーライド追加(`add_event_override`。BeginPlay等の既存イベントの
  “器”を作るだけで、中身の配線は別問題)
- PrimaryDataAsset派生クラスのインスタンス生成 + フィールド値の設定
  (`set_editor_property`)
- UMG WidgetBlueprintアセットの新規作成(ただし`WidgetTree`もprotectedのため、
  Python側からのウィジェット配置は不可。空のUserWidgetの器を作るところまで)

## 自動化できない範囲
- イベントグラフ内のノード配置・ピン接続(Branch, Sequence, 変数Get/Set,
  関数呼び出しノードなどをコードから組み立てること)
- UMG WidgetTreeへの子ウィジェット追加(TextBlock等をPythonから挿入すること)

## 今後の選択肢
1. **手動配線**: 詳細なノード単位の手順書を用意し、エディタ上で人力配線する
   (Blueprint中心方針に最も忠実)。
2. **C++移行**: ゲームループ部分をC++で実装し、Blueprintからは呼び出すだけにする
   (Sourceモジュール新設・ビルド環境が必要。方針転換になるため要合意)。
   → **2026-08-23〜24に実施済み**。詳細は下記「C++移行の実施記録」を参照。
3. **DataTable運用は`DT_CG_Cards`ではなく`BP_CG_CardDefinition`派生の
   `DA_CG_C001`〜`C024`で代替**(`UserDefinedStruct`のフィールド編集APIも
   非公開のため)。
   → C++移行後は `UCGCardDatabase`(`Source/CardGame/CGCardDatabase.cpp`)の
   静的データが正データ。`DA_CG_C001`〜`C024`は参考用に残置。

## C++移行の実施記録(2026-08-23〜24)

### ビルド環境
- このマシンには元々Visual Studioが未インストールだった。
  `winget install --id Microsoft.VisualStudio.2022.Community --override "--add
  Microsoft.VisualStudio.Workload.NativeGame --add
  Microsoft.VisualStudio.Workload.ManagedDesktop --includeRecommended --quiet --wait"`
  で導入(ワークロードが反映されない場合は `vs_installer.exe modify` で個別に追加)。
- 物理メモリ15.8GBと少なめのため、デフォルトの並列ビルド(3並列)では
  `cl.exe`が内部コンパイラエラー(C1001)でクラッシュした。
  `Build.bat ... -MaxParallelActions=1` で1並列にしたところ安定してビルドできた。
  **今後このマシンでC++をビルドする際は必ず `-MaxParallelActions=1` を付けること。**
- `Target.cs`の`DefaultBuildSettings`は`V5`ではなく`BuildSettingsVersion.Latest`を
  使うこと(UE5.8の既定値と食い違うと`UnrealEditor`と警告レベル設定が衝突しビルド
  不能になる)。

### 実装構成
- `CardGame/Source/CardGame/` にモジュール新設。`CardGame.uproject`に`Modules`追加。
- `CGTypes.h`: `ECGCardType` / `ECGPhase` / `FCGCardDef`
- `CGCardDatabase.h/.cpp`: 24枚のカードマスタ(静的データ)、初期デッキ12枚取得
- `CGPlayerState.h/.cpp`: 片側プレイヤーのHP/マナ/手札/デッキ/場。ドロー・プレイ・
  購入・攻撃・死亡処理・基本3効果(単体ダメージ/単体回復/死亡時ドロー)を実装
- `CGGameState.h/.cpp`: ターン/フェーズ/勝者/マーケット公開状態
- `CGGameMode.h/.cpp`: 試合初期化、ターン進行、Play/Buy/Attack/EndTurnの受付、
  勝敗判定。`LogCardGame`カテゴリでログ出力

### Blueprintとの接続
- `BP_CG_GameMode` / `BP_CG_GameState` / `BP_CG_PlayerState` は削除して、
  C++クラス(`CGGameMode`/`CGGameState`/`CGPlayerState`)を親に再作成した
  (`scripts/step6_recreate_core_blueprints.py`)。
  Python版`add_member_variable`で追加していた同名の動的変数を残したまま
  reparentすると名前衝突のリスクがあるため、削除→再作成を選んだ。
- `EditorAssetLibrary.delete_asset`はアセットレジストリ上は消えても
  物理ファイルが残ることがある(unattended実行では上書き不可でエラーになる)。
  再作成前に `.uasset` を直接ファイル削除するのが確実。
- `BP_CG_GameMode`のCDOに対して`GameStateClass`/`PlayerStateClass`を
  `BP_CG_GameState`/`BP_CG_PlayerState`に設定済み(C++既定クラスではなく
  Blueprint版を使う。将来Blueprint側で拡張できるように)。

### 動作確認
`-game`スタンドアロン実行(`UnrealEditor.exe <uproject> -game -windowed -log`)で
実際に対戦初期化〜ターン開始まで走ることを確認済み(ログ例):
```
LogCardGame: InitializeMatch: Sides=2 Side0 HP=20 Hand=5 Deck=7 / Side1 HP=20 Hand=5 Deck=7 / Market=5 FirstPlayer=1
LogCardGame: StartTurn: TurnCount=1 ActiveSide=1 Mana=1/1 HandSize=6
```
Play/Buy/Attack/EndTurnの各`Request*`関数はUI(ボタン等)からの呼び出しが
必要だが、UMG WidgetTreeがPythonから編集できない制約(前述)のため、
ボタン配置とOnClickedからの関数呼び出しはUnrealエディタ上で手動作業が必要。

### 追記: UIボタンの配線はC++なら自動化できる(2026-08-25)

上記の制約は「Python Editor Scripting API」から見たBlueprintグラフ編集の話であり、
**C++コードそのものには及ばない**。UMGの`WidgetTree`はC++からは通常のUPROPERTYと
して直接アクセス可能(protectedはPython reflection側のガードであり、C++の派生
クラスからは通常のアクセス指定子どおりアクセスできる)。

これを利用し、UI一式(`CGCardSlotWidget` / `CGGameHUD`)を完全にC++の
`NativeConstruct()`で構築し、ボタンクリックも`OnClicked.AddDynamic(...)`で
C++関数に直接バインドする方式で実装した。WidgetBlueprintアセット(`WBP_CG_*`)
を経由せず、`ACGGameMode::BeginPlay`から`CreateWidget<UCGGameHUD>(...)`で
直接生成・`AddToViewport()`している。

- `-game`スタンドアロン実行で、ログ上は
  `NativeConstruct built widget tree` → `RefreshUI ... Sides=2` →
  `HUD created and added to viewport` まで全て正常に完了することを確認済み。
- **視覚的なスクリーンショット確認は本セッションの自動化環境では失敗した**
  (BitBlt/PrintWindow(PW_RENDERFULLCONTENT)のどちらもDX11/DX12問わず黒画面、
  SendKeysでのコンソールコマンド(`Shot`)もウィンドウにフォーカスが渡らず
  実行されなかった)。これはWindowsのフォアグラウンドロックやリモート
  セッションでのGPU画面キャプチャの既知の制約によるものと推測され、
  ログで確認できるコード側の動作(ウィジェット構築・ビューポート追加・
  ゲーム状態反映)には問題が見られない。実機での目視確認を推奨する。
