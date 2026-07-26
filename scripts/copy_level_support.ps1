$pairs = @(
  @{
    Source = 'C:\Users\kanki\Documents\app-develop\game\cardgame\CardGame\Content\__ExternalActors__\DemoTemplate\_Core\Lvl_IntroRoom';
    Dest = 'C:\Users\kanki\Documents\app-develop\game\cardgame\CardGame\Content\__ExternalActors__\CardGame\Maps\L_Card_GamePrototype'
  },
  @{
    Source = 'C:\Users\kanki\Documents\app-develop\game\cardgame\CardGame\Content\__ExternalObjects__\DemoTemplate\_Core\Lvl_IntroRoom';
    Dest = 'C:\Users\kanki\Documents\app-develop\game\cardgame\CardGame\Content\__ExternalObjects__\CardGame\Maps\L_Card_GamePrototype'
  }
)

foreach ($pair in $pairs) {
  New-Item -ItemType Directory -Force -Path $pair.Dest | Out-Null
  $count = 0
  Get-ChildItem -Path $pair.Source -Recurse -File | ForEach-Object {
    $relative = $_.FullName.Substring($pair.Source.Length).TrimStart('\\')
    $target = Join-Path $pair.Dest $relative
    $targetDir = Split-Path $target -Parent
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
    Copy-Item $_.FullName $target -Force
    $count++
  }
  Write-Host "$($pair.Source) -> $($pair.Dest): $count files"
}
