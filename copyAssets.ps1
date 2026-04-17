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

$sourceRoot = [System.IO.Path]::GetFullPath($assetSource)
$targetRoot = [System.IO.Path]::GetFullPath($assetTarget)

Get-ChildItem -Path $sourceRoot -Recurse -Directory | ForEach-Object {
    $relativePath = $_.FullName.Substring($sourceRoot.Length).TrimStart('\')
    $destinationDir = Join-Path $targetRoot $relativePath
    New-Item -ItemType Directory -Force -Path $destinationDir | Out-Null
}

Get-ChildItem -Path $sourceRoot -Recurse -File | ForEach-Object {
    $relativePath = $_.FullName.Substring($sourceRoot.Length).TrimStart('\')
    $destinationPath = Join-Path $targetRoot $relativePath
    $destinationDir = Split-Path -Parent $destinationPath

    if (-not (Test-Path -LiteralPath $destinationDir)) {
        New-Item -ItemType Directory -Force -Path $destinationDir | Out-Null
    }

    Copy-Item -LiteralPath $_.FullName -Destination $destinationPath -Force
}

Write-Host "Copied assets to: $assetTarget"
