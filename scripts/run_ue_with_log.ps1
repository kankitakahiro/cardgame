$project = "C:\Users\kanki\Documents\app-develop\game\cardgame\CardGame\CardGame.uproject"
$editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$pythonScript = "C:\Users\kanki\Documents\app-develop\game\cardgame\scripts\create_blueprint_ai_factory.py"
$logPath = "C:\Users\kanki\Documents\app-develop\game\cardgame\CardGame\Saved\Logs\CardGame.log"

$arguments = @(
    $project,
    "-ExecutePythonScript=$pythonScript",
    "-log",
    "-windowed",
    "-stdout",
    "-FullStdOutLogOutput"
)

Write-Host "Launching Unreal Editor with Python automation..."
Write-Host "Project: $project"
Write-Host "Python script: $pythonScript"
Write-Host "Log file: $logPath"
Write-Host "Command: & '$editor' @arguments"

& $editor @arguments

if (Test-Path $logPath) {
    Write-Host "Log file exists: $logPath"
} else {
    Write-Host "Log file not created yet: $logPath"
}
