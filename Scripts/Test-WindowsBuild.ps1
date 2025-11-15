#Requires -Version 5.1
<#
.SYNOPSIS
    Automated build testing script for HomeKit ADK on Windows

.DESCRIPTION
    Tests all build configurations to ensure the Windows port builds correctly.
    Supports CMake and Visual Studio builds with various feature combinations.

.PARAMETER Configuration
    Build configuration: Debug, Release, or All

.PARAMETER Features
    Comma-separated list of features: NFC,BLE,QRCode,MbedTLS

.PARAMETER Clean
    Clean build directories before building

.PARAMETER Verbose
    Enable verbose output

.EXAMPLE
    .\Test-WindowsBuild.ps1 -Configuration All

.EXAMPLE
    .\Test-WindowsBuild.ps1 -Configuration Release -Features "BLE,QRCode"

.NOTES
    Requires: Visual Studio 2022+, CMake 3.20+, vcpkg
    Date: November 15, 2025
#>

param(
    [Parameter()]
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All',

    [Parameter()]
    [string]$Features = '',

    [Parameter()]
    [switch]$Clean,

    [Parameter()]
    [switch]$Verbose
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Colors
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }
function Write-Failure { Write-Host $args -ForegroundColor Red }

Write-Info "========================================="
Write-Info "HomeKit ADK Windows Build Test"
Write-Info "Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Info "========================================="

# Check prerequisites
Write-Info "`nChecking prerequisites..."

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Failure "CMake not found! Install from https://cmake.org/"
    exit 1
}
Write-Success "✓ CMake found: $(cmake --version | Select-Object -First 1)"

if (-not $env:VCPKG_ROOT) {
    Write-Warning "⚠ VCPKG_ROOT not set. Trying to auto-detect..."
    $vcpkgPaths = @("C:\vcpkg", "C:\Tools\vcpkg", "$env:USERPROFILE\vcpkg")
    foreach ($path in $vcpkgPaths) {
        if (Test-Path "$path\vcpkg.exe") {
            $env:VCPKG_ROOT = $path
            Write-Success "✓ Found vcpkg at: $path"
            break
        }
    }
    if (-not $env:VCPKG_ROOT) {
        Write-Failure "vcpkg not found! Install from https://github.com/microsoft/vcpkg"
        exit 1
    }
}
Write-Success "✓ vcpkg found: $env:VCPKG_ROOT"

# Parse features
$enableNFC = $Features -match 'NFC'
$enableBLE = $Features -match 'BLE'
$enableQR = $Features -match 'QR'
$useMbedTLS = $Features -match 'MbedTLS'

Write-Info "`nBuild Features:"
Write-Info "  NFC Support:    $enableNFC"
Write-Info "  BLE Support:    $enableBLE"
Write-Info "  QR Codes:       $enableQR"
Write-Info "  MbedTLS:        $useMbedTLS"

# Install dependencies
Write-Info "`nInstalling dependencies..."
$packages = @('openssl:x64-windows')
if ($enableNFC) { $packages += 'libnfc:x64-windows' }
if ($enableBLE) { $packages += 'btstack:x64-windows' }
if ($enableQR) { $packages += 'qrencode:x64-windows' }
if ($useMbedTLS) { $packages += 'mbedtls:x64-windows' }

foreach ($pkg in $packages) {
    Write-Info "  Installing $pkg..."
    & "$env:VCPKG_ROOT\vcpkg.exe" install $pkg
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Failed to install $pkg (may already be installed)"
    }
}

# Clean if requested
if ($Clean) {
    Write-Info "`nCleaning build directories..."
    if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
    Write-Success "✓ Build directories cleaned"
}

# Test configurations
$configs = if ($Configuration -eq 'All') { @('Debug', 'Release') } else { @($Configuration) }
$results = @()

foreach ($config in $configs) {
    Write-Info "`n========================================="
    Write-Info "Testing $config Configuration"
    Write-Info "========================================="

    # Determine preset
    $preset = if ($useMbedTLS) {
        "windows-x64-mbedtls"
    } elseif ($enableBLE -and $enableNFC) {
        "windows-x64-debug-ble-nfc"
    } elseif ($enableBLE) {
        "windows-x64-debug-ble"
    } elseif ($enableNFC) {
        "windows-x64-debug-nfc"
    } else {
        "windows-x64-$($config.ToLower())"
    }

    try {
        # Configure
        Write-Info "Configuring with preset: $preset"
        $configArgs = @(
            "--preset", $preset
        )
        if ($enableBLE) { $configArgs += "-DHAVE_BLE=ON" }
        if ($enableQR) { $configArgs += "-DHAVE_QR_CODE=ON" }

        & cmake @configArgs
        if ($LASTEXITCODE -ne 0) { throw "Configuration failed" }

        # Build
        Write-Info "Building..."
        $buildArgs = @("--build", "build/$preset", "--config", $config)
        if ($Verbose) { $buildArgs += "--verbose" }

        & cmake @buildArgs
        if ($LASTEXITCODE -ne 0) { throw "Build failed" }

        Write-Success "✓ $config build succeeded"
        $results += [PSCustomObject]@{
            Configuration = $config
            Features = $Features
            Status = 'Success'
            Time = Get-Date
        }

    } catch {
        Write-Failure "✗ $config build failed: $_"
        $results += [PSCustomObject]@{
            Configuration = $config
            Features = $Features
            Status = 'Failed'
            Error = $_.Exception.Message
            Time = Get-Date
        }
    }
}

# Summary
Write-Info "`n========================================="
Write-Info "Build Test Summary"
Write-Info "========================================="
$results | Format-Table -AutoSize

$successCount = ($results | Where-Object { $_.Status -eq 'Success' }).Count
$totalCount = $results.Count

if ($successCount -eq $totalCount) {
    Write-Success "`n✓ All builds passed ($successCount/$totalCount)"
    exit 0
} else {
    Write-Failure "`n✗ Some builds failed ($successCount/$totalCount succeeded)"
    exit 1
}
