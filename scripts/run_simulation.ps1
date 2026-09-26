<#
.SYNOPSIS
  バランス検証シミュレーション(-SimulateMatches)を実行し、UEの終了を待ってから
  ログを自動集計して結果を表示する。

.DESCRIPTION
  ACGGameMode::RunSelfPlaySimulation()はシミュレーション完了時に
  FPlatformMisc::RequestExit()でエンジンを自動終了するようになっているため、
  このスクリプトはプロセスの終了を待つだけでよい(手動でウィンドウを閉じる
  必要は無い)。終了後、CardGame.logから`SimSummary`行を抽出して整形表示する。

.PARAMETER Matches
  シミュレーションする対戦数(既定1000。docs/simulation-guide.md参照)。

.PARAMETER DisableBuy
  マーケット購入を無効化する診断用フラグ(-SimDisableBuy)を付与する。

.EXAMPLE
  ./scripts/run_simulation.ps1
  ./scripts/run_simulation.ps1 -Matches 3000
#>
param(
    [int]$Matches = 1000,
    [switch]$DisableBuy
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
    "-SimulateMatches=$Matches",
    "-log"
)
if ($DisableBuy) {
    $arguments += "-SimDisableBuy"
}

Write-Host "Running $Matches self-play matches..."
Write-Host "Command: & '$editor' $($arguments -join ' ')"

# シミュレーション完了時にゲーム側がエンジン終了を要求するので、ここでは
# プロセスの終了を待つだけでよい。
$process = Start-Process -FilePath $editor -ArgumentList $arguments -PassThru -Wait

if (-not (Test-Path $logPath)) {
    Write-Error "Log file not found: $logPath"
    exit 1
}

Write-Host ""
Write-Host "=== Simulation finished (exit code $($process.ExitCode)). Aggregating log ==="
Write-Host ""

$logLines = Get-Content -Path $logPath -Encoding UTF8

$summaryLines = $logLines | Where-Object { $_ -match "LogCardGame: (=== SimSummary|SimSummary)" }

if (-not $summaryLines) {
    Write-Warning "No SimSummary lines found in log. The simulation may not have completed."
    exit 1
}

foreach ($line in $summaryLines) {
    $trimmed = $line -replace '^.*LogCardGame:\s*', ''
    Write-Host $trimmed
}
