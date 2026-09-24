# Card Game (Unreal Engine)

Unreal Engineでカードゲームを開発するための初期リポジトリです。
アンリアルエンジンでカードゲームを作ります。

## 前提
- Windows
- Git
- Git LFS
- Unreal Engine 5.x

このPCでは `UE_5.8` が検出されています。

## 初回セットアップ
1. Git LFSを有効化
   - `git lfs install`
2. Unreal Engineで新規プロジェクトを作成
   - このリポジトリ配下に作成してください（例: `CardGame/`）
3. 生成された不要ファイルが`.gitignore`で除外されることを確認
4. 初回コミット
   - `git add .`
   - `git commit -m "chore: initialize Unreal card game project"`

## GitHub連携
1. GitHubで空のリポジトリを作成（例: `cardgame`）
2. リモートを追加
   - `git remote add origin <YOUR_GITHUB_REPO_URL>`
3. 最初のpush
   - `git branch -M main`
   - `git push -u origin main`

## Unrealプロジェクト作成のおすすめ
1. Epic Games Launcherから Unreal Engine 5.8 を起動
1. Epic Games Launcherから Unreal Engine 5.8 を起動
2. New Project を選択
3. 保存先をこのフォルダ配下に設定（例: `CardGame/`）
4. プロジェクト作成後、このリポジトリで `git status` を確認

## 推奨構成（Unreal Content Browser）
- `Content/CardGame/Core`
- `Content/CardGame/Cards`
- `Content/CardGame/UI`
- `Content/CardGame/Effects`
- `Content/CardGame/Data`

## 実装方針
- 初期開発は Blueprint 中心で進めます。
- 負荷が高い処理、再利用性が高い共通処理、複雑なアルゴリズムは C++ へ段階的に移行します。
- まずはゲーム仕様の確定を優先し、早い試作を重視します。

## ブランチ運用（例）
- `main`: 常に安定
- `develop`: 統合ブランチ
- `feature/*`: 機能開発

## 設計ドキュメント
- `docs/work-instruction.md`

## 注意
- `.uasset` / `.umap` はGit LFSで管理します。
- `DerivedDataCache` や `Intermediate` はコミットしません。
- 大きなアセット追加前に `git lfs track` 状態を確認してください。
