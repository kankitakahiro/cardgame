<#
.SYNOPSIS
  各色ごとに勝率の高いデッキ構成を、山登り法(1枚ずつ入れ替えて評価)で
  探索する(-OptimizeDecks)。UEの終了を待ってから、色ごとの最終デッキ構成と
  ベースラインからの勝率変化を表示する。

.DESCRIPTION
  ACGGameMode::RunDeckOptimization()はGetBasicColorDeckCardIds()の基本デッキを
  起点に、色ごとに「1枚だけ別のカードに入れ替えたデッキ」を作って他4色の
  現時点のベストデッキと対戦させ、勝率が上がれば採用する処理をIterationsPerColor
  回繰り返す。これをRounds周回すると、5色のデッキが互いに適応し合いながら
  同時に育っていく。完了時にFPlatformMisc::RequestExit()でエンジンが自動終了
  するため、run_simulation.ps1と同様にプロセスの終了を待つだけでよい。

.PARAMETER IterationsPerColor
  1色・1ラウンドあたりの入れ替え試行回数(既定30)。

.PARAMETER MatchesPerEvaluation
  デッキ1つを評価するときの対戦数(既定200)。少ないと結果がノイジーになる。

.PARAMETER Rounds
  5色を何周探索するか(既定3)。ラウンドを重ねるほど、相手デッキの変化に
  合わせて再適応する。

.EXAMPLE
  ./scripts/run_deck_optimization.ps1
  ./scripts/run_deck_optimization.ps1 -IterationsPerColor 50 -MatchesPerEvaluation 300 -Rounds 5
#>
param(
    [int]$IterationsPerColor = 30,
    [int]$MatchesPerEvaluation = 200,
    [int]$Rounds = 3
)

$ErrorActionPreference = "Stop"

$repoRoot = "C:\Users\kanki\Documents\app-develop\game\cardgame"
$project = "$repoRoot\CardGame\CardGame.uproject"
$editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$logPath = "$repoRoot\CardGame\Saved\Logs\CardGame.log"

if (Test-Path $logPath) {
    Remove-Item $logPath -Force
}

$arguments = @(
    $project,
    "L_Card_GamePrototype",
    "-game",
    "-windowed",
    "-resX=800",
    "-resY=600",
    "-OptimizeDecks=$IterationsPerColor",
    "-OptimizeDeckMatches=$MatchesPerEvaluation",
    "-OptimizeDeckRounds=$Rounds",
    "-log"
)

Write-Host "Optimizing decks (IterationsPerColor=$IterationsPerColor, MatchesPerEvaluation=$MatchesPerEvaluation, Rounds=$Rounds)..."
Write-Host "Command: & '$editor' $($arguments -join ' ')"

$process = Start-Process -FilePath $editor -ArgumentList $arguments -PassThru -Wait

if (-not (Test-Path $logPath)) {
    Write-Error "Log file not found: $logPath"
    exit 1
}

Write-Host ""
Write-Host "=== Deck optimization finished (exit code $($process.ExitCode)). Extracting results ==="
Write-Host ""

$logLines = Get-Content -Path $logPath -Encoding UTF8
$resultLines = $logLines | Where-Object { $_ -match "LogCardGame: (=== DeckOptimization|DeckOptimization Result|DeckOptimization\s+[A-Z0-9_]+\()" }

if (-not $resultLines) {
    Write-Warning "No DeckOptimization result lines found in log. The run may not have completed."
    exit 1
}

foreach ($line in $resultLines) {
    $trimmed = $line -replace '^.*LogCardGame:\s*', ''
    Write-Host $trimmed
}
