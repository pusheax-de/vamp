param(
    [string]$ProjectDir,
    [string]$OutDir
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ProjectDir)) {
    throw "ProjectDir is required."
}

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    throw "OutDir is required."
}

$projectRoot = [System.IO.Path]::GetFullPath($ProjectDir)
$outputRoot = [System.IO.Path]::GetFullPath($OutDir)

$shaderSource = Join-Path $projectRoot "engine\\shaders"
$shaderTarget = Join-Path $outputRoot "shaders"
$assetSource = Join-Path $projectRoot "assets"
$assetTarget = Join-Path $outputRoot "assets"

New-Item -ItemType Directory -Force -Path $shaderTarget | Out-Null
New-Item -ItemType Directory -Force -Path $assetTarget | Out-Null

Copy-Item -Path (Join-Path $shaderSource "*") -Destination $shaderTarget -Recurse -Force
Copy-Item -Path (Join-Path $assetSource "*") -Destination $assetTarget -Recurse -Force
