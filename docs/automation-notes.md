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
3. **DataTable運用は`DT_CG_Cards`ではなく`BP_CG_CardDefinition`派生の
   `DA_CG_C001`〜`C024`で代替**(`UserDefinedStruct`のフィールド編集APIも
   非公開のため)。
