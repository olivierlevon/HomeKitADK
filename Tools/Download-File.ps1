#Requires -Version 5.1
<#
.SYNOPSIS
    Downloads a file from a URL with SHA-256 verification

.DESCRIPTION
    PowerShell equivalent of Tools/download.sh
    Downloads files with optional SHA-256 hash verification and progress reporting

.PARAMETER FilePath
    Destination file path

.PARAMETER Url
    Source URL to download from

.PARAMETER Sha256
    Expected SHA-256 hash (optional verification)

.EXAMPLE
    .\Download-File.ps1 -FilePath "openssl.zip" -Url "https://example.com/openssl.zip"

.EXAMPLE
    .\Download-File.ps1 -FilePath "file.zip" -Url "https://..." -Sha256 "abc123..."

.NOTES
    Date: November 15, 2025
    Windows equivalent of Tools/download.sh
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$FilePath,

    [Parameter(Mandatory=$true)]
    [string]$Url,

    [Parameter()]
    [string]$Sha256 = ''
)

$ErrorActionPreference = 'Stop'

function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Failure { Write-Host $args -ForegroundColor Red }

Write-Info "Downloading: $Url"
Write-Info "Destination: $FilePath"

# Create directory if needed
$dir = Split-Path -Parent $FilePath
if ($dir -and -not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

# Check if file exists and hash matches
if ((Test-Path $FilePath) -and $Sha256) {
    Write-Info "File exists, verifying hash..."
    $existingHash = (Get-FileHash -Path $FilePath -Algorithm SHA256).Hash
    if ($existingHash -eq $Sha256) {
        Write-Success "✓ File already downloaded and verified"
        exit 0
    } else {
        Write-Info "Hash mismatch, re-downloading..."
        Remove-Item $FilePath
    }
}

# Download with progress
try {
    $webClient = New-Object System.Net.WebClient
    $webClient.Headers.Add("User-Agent", "HomeKitADK-Windows/1.0")

    # Progress callback
    Register-ObjectEvent -InputObject $webClient -EventName DownloadProgressChanged -SourceIdentifier WebClient.DownloadProgressChanged -Action {
        Write-Progress -Activity "Downloading" -Status "$($EventArgs.ProgressPercentage)% complete" -PercentComplete $EventArgs.ProgressPercentage
    } | Out-Null

    $webClient.DownloadFile($Url, $FilePath)

    Unregister-Event -SourceIdentifier WebClient.DownloadProgressChanged
    Write-Progress -Activity "Downloading" -Completed

    Write-Success "✓ Download complete"

} catch {
    Write-Failure "✗ Download failed: $_"
    exit 1
}

# Verify hash if provided
if ($Sha256) {
    Write-Info "Verifying SHA-256 hash..."
    $actualHash = (Get-FileHash -Path $FilePath -Algorithm SHA256).Hash

    if ($actualHash -eq $Sha256) {
        Write-Success "✓ Hash verified: $actualHash"
    } else {
        Write-Failure "✗ Hash mismatch!"
        Write-Failure "  Expected: $Sha256"
        Write-Failure "  Actual:   $actualHash"
        Remove-Item $FilePath
        exit 1
    }
}

Write-Success "✓ File ready: $FilePath"
