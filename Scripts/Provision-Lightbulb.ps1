#Requires -Version 5.1
<#
.SYNOPSIS
    Provisions the Lightbulb sample application with HomeKit credentials

.DESCRIPTION
    Generates setup codes, setup IDs, and configuration files for the Lightbulb
    HomeKit accessory. Creates the necessary key-value store structure.

.PARAMETER SetupCode
    8-digit HomeKit setup code (format: XXX-XX-XXX)

.PARAMETER SetupID
    4-character setup ID for QR codes (optional)

.PARAMETER Name
    Accessory name (default: "HomeKit Lightbulb")

.PARAMETER StoragePath
    Path to key-value store (default: ./db)

.PARAMETER Port
    TCP port for HAP server (default: 0 for auto-assign)

.EXAMPLE
    .\Provision-Lightbulb.ps1 -SetupCode "123-45-678" -Name "My Lightbulb"

.EXAMPLE
    .\Provision-Lightbulb.ps1 -SetupCode "111-22-333" -SetupID "AB12" -Port 8080

.NOTES
    Date: November 15, 2025
    The setup code must be a valid HomeKit setup code
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^\d{3}-\d{2}-\d{3}$')]
    [string]$SetupCode,

    [Parameter()]
    [ValidatePattern('^[A-Z0-9]{4}$')]
    [string]$SetupID = '',

    [Parameter()]
    [string]$Name = 'HomeKit Lightbulb',

    [Parameter()]
    [string]$StoragePath = 'db',

    [Parameter()]
    [ValidateRange(0, 65535)]
    [int]$Port = 0
)

$ErrorActionPreference = 'Stop'

function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }

Write-Info "========================================="
Write-Info "HomeKit Lightbulb Provisioning"
Write-Info "========================================="

# Create storage directory
if (-not (Test-Path $StoragePath)) {
    New-Item -ItemType Directory -Path $StoragePath | Out-Null
    Write-Success "✓ Created storage directory: $StoragePath"
} else {
    Write-Info "Using existing storage directory: $StoragePath"
}

# Generate setup info structure
# This is a simplified version - real implementation would use HAP structures
$setupInfo = @{
    setupCode = $SetupCode
    setupID = $SetupID
    name = $Name
    category = 5  # Lightbulb category
    port = $Port
}

# Create setup code file (domain 0x00, key 0x00)
$setupCodeFile = Join-Path $StoragePath "00.00"
$setupCodeBytes = [System.Text.Encoding]::ASCII.GetBytes($SetupCode)
[System.IO.File]::WriteAllBytes($setupCodeFile, $setupCodeBytes)
Write-Success "✓ Saved setup code: $SetupCode"

# Create setup ID file if provided (domain 0x00, key 0x01)
if ($SetupID) {
    $setupIDFile = Join-Path $StoragePath "00.01"
    $setupIDBytes = [System.Text.Encoding]::ASCII.GetBytes($SetupID)
    [System.IO.File]::WriteAllBytes($setupIDFile, $setupIDBytes)
    Write-Success "✓ Saved setup ID: $SetupID"

    # Generate QR code payload
    $setupPayload = "X-HM://$SetupID$($SetupCode.Replace('-',''))"
    Write-Info "`n📱 Setup Payload for QR Code:"
    Write-Host "   $setupPayload" -ForegroundColor Yellow
}

# Create configuration JSON
$configPath = "Applications\Lightbulb\Config\setup.json"
$configDir = Split-Path $configPath -Parent
if (-not (Test-Path $configDir)) {
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
}

$config = @{
    name = $Name
    setupCode = $SetupCode
    setupID = $SetupID
    category = "Lightbulb"
    port = $Port
    storagePath = $StoragePath
    manufacturer = "HomeKit ADK"
    model = "Lightbulb1,1"
    serialNumber = "00000001"
    firmwareVersion = "1.0.0"
    hardwareVersion = "1.0.0"
} | ConvertTo-Json -Depth 10

$config | Out-File -FilePath $configPath -Encoding UTF8
Write-Success "✓ Created configuration: $configPath"

# Display summary
Write-Info "`n========================================="
Write-Info "Provisioning Complete!"
Write-Info "========================================="
Write-Info "Accessory Name:  $Name"
Write-Info "Setup Code:      $SetupCode"
if ($SetupID) {
    Write-Info "Setup ID:        $SetupID"
}
Write-Info "Storage Path:    $StoragePath"
Write-Info "Port:            $(if ($Port -eq 0) {'Auto-assign'} else {$Port})"
Write-Info ""
Write-Info "📱 To pair with iOS:"
Write-Info "   1. Run: .\Applications\Lightbulb\bin\Release\Lightbulb.exe"
Write-Info "   2. Open Home app on iPhone"
Write-Info "   3. Tap 'Add Accessory'"
if ($SetupID) {
    Write-Info "   4. Scan QR code or enter setup code: $SetupCode"
} else {
    Write-Info "   4. Enter setup code: $SetupCode"
}
Write-Info "========================================="
