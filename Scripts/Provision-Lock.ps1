#Requires -Version 5.1
<#
.SYNOPSIS
    Provisions the Lock sample application with HomeKit credentials

.DESCRIPTION
    Generates setup codes, setup IDs, and configuration files for the Lock
    HomeKit accessory. Creates the necessary key-value store structure.

.PARAMETER SetupCode
    8-digit HomeKit setup code (format: XXX-XX-XXX)

.PARAMETER SetupID
    4-character setup ID for QR codes (optional)

.PARAMETER Name
    Accessory name (default: "HomeKit Lock")

.PARAMETER StoragePath
    Path to key-value store (default: ./db-lock)

.EXAMPLE
    .\Provision-Lock.ps1 -SetupCode "987-65-432" -Name "Front Door Lock"

.NOTES
    Date: November 15, 2025
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^\d{3}-\d{2}-\d{3}$')]
    [string]$SetupCode,

    [Parameter()]
    [ValidatePattern('^[A-Z0-9]{4}$')]
    [string]$SetupID = '',

    [Parameter()]
    [string]$Name = 'HomeKit Lock',

    [Parameter()]
    [string]$StoragePath = 'db-lock'
)

$ErrorActionPreference = 'Stop'

function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info { Write-Host $args -ForegroundColor Cyan }

Write-Info "========================================="
Write-Info "HomeKit Lock Provisioning"
Write-Info "========================================="

if (-not (Test-Path $StoragePath)) {
    New-Item -ItemType Directory -Path $StoragePath | Out-Null
    Write-Success "✓ Created storage directory: $StoragePath"
}

$setupCodeFile = Join-Path $StoragePath "00.00"
$setupCodeBytes = [System.Text.Encoding]::ASCII.GetBytes($SetupCode)
[System.IO.File]::WriteAllBytes($setupCodeFile, $setupCodeBytes)
Write-Success "✓ Saved setup code: $SetupCode"

if ($SetupID) {
    $setupIDFile = Join-Path $StoragePath "00.01"
    $setupIDBytes = [System.Text.Encoding]::ASCII.GetBytes($SetupID)
    [System.IO.File]::WriteAllBytes($setupIDFile, $setupIDBytes)
    Write-Success "✓ Saved setup ID: $SetupID"

    $setupPayload = "X-HM://$SetupID$($SetupCode.Replace('-',''))"
    Write-Info "`n📱 Setup Payload: $setupPayload"
}

$configPath = "Applications\Lock\Config\setup.json"
$configDir = Split-Path $configPath -Parent
if (-not (Test-Path $configDir)) {
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
}

$config = @{
    name = $Name
    setupCode = $SetupCode
    setupID = $SetupID
    category = "Lock"
    storagePath = $StoragePath
    manufacturer = "HomeKit ADK"
    model = "Lock1,1"
    serialNumber = "00000002"
    firmwareVersion = "1.0.0"
} | ConvertTo-Json

$config | Out-File -FilePath $configPath -Encoding UTF8
Write-Success "✓ Configuration saved"

Write-Info "`n✓ Provisioning complete! Run: .\Applications\Lock\bin\Release\Lock.exe"
