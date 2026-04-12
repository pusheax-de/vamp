param(
    [Parameter(Mandatory = $true)]
    [string]$OutDir
)

$ErrorActionPreference = "Stop"

function Test-CommandAvailable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

$sevenZipCommand = $null
if (Test-CommandAvailable -Name "7z.exe") {
    $sevenZipCommand = "7z.exe"
}
elseif (Test-CommandAvailable -Name "7z") {
    $sevenZipCommand = "7z"
}

if ($null -eq $sevenZipCommand) {
    throw "7z.exe was not found in PATH."
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = [System.IO.Path]::GetFullPath($scriptDir)
$destinationRoot = [System.IO.Path]::GetFullPath($OutDir)

if (-not (Test-Path -LiteralPath $projectRoot)) {
    throw "Project root does not exist: $projectRoot"
}

New-Item -ItemType Directory -Force -Path $destinationRoot | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$archiveName = "vampire_sources_$timestamp.7z"
$archivePath = Join-Path $destinationRoot $archiveName

$includeEntries = @(
    "engine",
    "game",
    "editor",
    "ui",
    "assets",
    "AGENTS.md",
    "README.md",
    "vampire.sln",
    "vampire.vcxproj",
    "vampire.vcxproj.filters",
    "vampire.cpp",
    "vampire.h",
    "vampire.rc",
    "framework.h",
    "copy_assets.ps1",
    "copyToUSB.ps1"
    "copyFromUSB.ps1"
)

$existingEntries = @()
foreach ($entry in $includeEntries) {
    $fullPath = Join-Path $projectRoot $entry
    if (Test-Path -LiteralPath $fullPath) {
        $existingEntries += $entry
    }
}

if ($existingEntries.Count -eq 0) {
    throw "No project files were found to archive."
}

Push-Location $projectRoot
try {
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }

    $arguments = @(
        "a",
        "-t7z",
        "-mx=9",
        $archivePath
    ) + $existingEntries

    & $sevenZipCommand @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$sevenZipCommand failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host "Created archive: $archivePath"
