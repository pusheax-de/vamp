param()

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Get-Location).Path)
$solutionPath = Join-Path $projectRoot "vampire.sln"
$assetSource = Join-Path $projectRoot "assets"
$assetTarget = Join-Path $projectRoot "x64\\Debug\\assets"

if (-not (Test-Path -LiteralPath $solutionPath)) {
    Write-Error "Current directory does not look like the vampire project root: missing vampire.sln"
    exit 1
}

if (-not (Test-Path -LiteralPath $assetSource)) {
    Write-Error "Source assets directory was not found: $assetSource"
    exit 1
}

New-Item -ItemType Directory -Force -Path $assetTarget | Out-Null
Copy-Item -Path (Join-Path $assetSource "*") -Destination $assetTarget -Recurse -Force

Write-Host "Copied assets to: $assetTarget"
