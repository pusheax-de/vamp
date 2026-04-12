param(
    [Parameter(Mandatory = $true)]
    [string]$InDir,

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

$sourceRoot = [System.IO.Path]::GetFullPath($InDir)
$destinationRoot = [System.IO.Path]::GetFullPath($OutDir)

if (-not (Test-Path -LiteralPath $sourceRoot)) {
    throw "Input directory does not exist: $sourceRoot"
}

New-Item -ItemType Directory -Force -Path $destinationRoot | Out-Null

$archive = Get-ChildItem -LiteralPath $sourceRoot -Filter "vampire_sources_*.7z" -File |
    Sort-Object LastWriteTimeUtc, Name |
    Select-Object -Last 1

if ($null -eq $archive) {
    throw "No archive matching 'vampire_sources_*.7z' was found in: $sourceRoot"
}

$archivePath = $archive.FullName

$arguments = @(
    "x",
    "-y",
    "-aoa",
    $archivePath,
    "-o$destinationRoot"
)

& $sevenZipCommand @arguments
if ($LASTEXITCODE -ne 0) {
    throw "$sevenZipCommand failed with exit code $LASTEXITCODE."
}

Write-Host "Extracted archive: $archivePath"
Write-Host "Destination: $destinationRoot"
