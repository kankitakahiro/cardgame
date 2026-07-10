param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryUrl
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path ".git")) {
    throw "Git repository is not initialized."
}

$currentRemote = git remote get-url origin 2>$null
if ($LASTEXITCODE -eq 0 -and $currentRemote) {
    Write-Host "origin is already configured: $currentRemote"
} else {
    git remote add origin $RepositoryUrl
    Write-Host "origin added: $RepositoryUrl"
}

git branch -M main
Write-Host "current branch renamed to main"

Write-Host "Run the following command when ready:"
Write-Host "  git push -u origin main"
