#Requires -Version 5.1
<#
.SYNOPSIS
    Sets up environment variables for HomeKit ADK Windows build

.DESCRIPTION
    Configures VCPKG_ROOT and BONJOUR_SDK environment variables required
    for building HomeKit ADK on Windows with Visual Studio.

.PARAMETER VcpkgRoot
    Path to vcpkg installation (auto-detected if not specified)

.PARAMETER BonjourSDK
    Path to Bonjour SDK installation (auto-detected if not specified)

.PARAMETER Scope
    Environment variable scope: User or Machine (default: User)

.EXAMPLE
    .\Setup-Environment.ps1

.EXAMPLE
    .\Setup-Environment.ps1 -VcpkgRoot "C:\vcpkg" -BonjourSDK "C:\Program Files\Bonjour SDK"

.NOTES
    Date: November 15, 2025
    Run this script ONCE before opening Visual Studio
#>

param(
    [Parameter()]
    [string]$VcpkgRoot = '',

    [Parameter()]
    [string]$BonjourSDK = '',

    [Parameter()]
    [ValidateSet('User', 'Machine')]
    [string]$Scope = 'User'
)

$ErrorActionPreference = 'Stop'

function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }
function Write-Failure { Write-Host $args -ForegroundColor Red }

Write-Info "========================================="
Write-Info "HomeKit ADK Environment Setup"
Write-Info "========================================="

# Auto-detect vcpkg
if (-not $VcpkgRoot) {
    Write-Info "`nAuto-detecting vcpkg installation..."
    $vcpkgPaths = @(
        "C:\vcpkg",
        "C:\Tools\vcpkg",
        "$env:USERPROFILE\vcpkg",
        "$env:LOCALAPPDATA\vcpkg"
    )

    foreach ($path in $vcpkgPaths) {
        if (Test-Path "$path\vcpkg.exe") {
            $VcpkgRoot = $path
            Write-Success "✓ Found vcpkg at: $VcpkgRoot"
            break
        }
    }

    if (-not $VcpkgRoot) {
        Write-Warning "⚠ vcpkg not found. Please install:"
        Write-Info "   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg"
        Write-Info "   cd C:\vcpkg"
        Write-Info "   .\bootstrap-vcpkg.bat"
        $VcpkgRoot = Read-Host "Enter vcpkg path (or press Enter to skip)"
        if (-not $VcpkgRoot) {
            Write-Warning "Skipping vcpkg setup"
        }
    }
}

# Auto-detect Bonjour SDK
if (-not $BonjourSDK) {
    Write-Info "`nAuto-detecting Bonjour SDK installation..."
    $bonjourPaths = @(
        "C:\Program Files\Bonjour SDK",
        "C:\Program Files (x86)\Bonjour SDK",
        "$env:ProgramFiles\Bonjour SDK",
        "${env:ProgramFiles(x86)}\Bonjour SDK"
    )

    foreach ($path in $bonjourPaths) {
        if (Test-Path "$path\Include\dns_sd.h") {
            $BonjourSDK = $path
            Write-Success "✓ Found Bonjour SDK at: $BonjourSDK"
            break
        }
    }

    if (-not $BonjourSDK) {
        Write-Warning "⚠ Bonjour SDK not found. mDNS service discovery will not work."
        Write-Info "   Download from: https://developer.apple.com/bonjour/"
        Write-Info "   Install to: C:\Program Files\Bonjour SDK"
        $BonjourSDK = Read-Host "Enter Bonjour SDK path (or press Enter to skip)"
        if (-not $BonjourSDK) {
            Write-Warning "Skipping Bonjour SDK setup"
        }
    }
}

# Set environment variables
Write-Info "`nConfiguring environment variables..."

if ($VcpkgRoot) {
    [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $VcpkgRoot, $Scope)
    $env:VCPKG_ROOT = $VcpkgRoot
    Write-Success "✓ Set VCPKG_ROOT = $VcpkgRoot"
}

if ($BonjourSDK) {
    [System.Environment]::SetEnvironmentVariable('BONJOUR_SDK', $BonjourSDK, $Scope)
    $env:BONJOUR_SDK = $BonjourSDK
    Write-Success "✓ Set BONJOUR_SDK = $BonjourSDK"
}

# Set HOMEKIT_DEPS for optional dependencies (BTstack, libnfc, qrencode)
$homekitDeps = "C:\HomeKitADK-deps"
if (Test-Path "$homekitDeps\include") {
    [System.Environment]::SetEnvironmentVariable('HOMEKIT_DEPS', $homekitDeps, $Scope)
    $env:HOMEKIT_DEPS = $homekitDeps
    Write-Success "✓ Set HOMEKIT_DEPS = $homekitDeps"
    Write-Info "  (BTstack, libnfc, qrencode will be found here)"
} else {
    Write-Warning "⚠ HOMEKIT_DEPS directory not found: $homekitDeps"
    Write-Info "  To build optional dependencies (BLE, NFC, QR codes):"
    Write-Info "  Run: .\Scripts\Build-Dependencies.ps1"
}

# Install vcpkg dependencies
if ($VcpkgRoot -and (Test-Path "$VcpkgRoot\vcpkg.exe")) {
    Write-Info "`nInstalling vcpkg dependencies..."

    $packages = @(
        'openssl:x64-windows'
    )

    Write-Info "Installing base dependencies..."
    foreach ($pkg in $packages) {
        Write-Info "  - $pkg"
        & "$VcpkgRoot\vcpkg.exe" install $pkg --triplet x64-windows
    }

    Write-Success "✓ Base dependencies installed (OpenSSL)"

    Write-Info "`nOptional dependencies:"
    Write-Info "  vcpkg install mbedtls:x64-windows        # For MbedTLS 4.x crypto (TLS 1.3)"
    Write-Info ""
    Write-Info "For BLE, NFC, and QR code support (not in vcpkg):"
    Write-Info "  .\Scripts\Build-Dependencies.ps1         # Builds BTstack, libnfc, qrencode from source"
}

# Summary
Write-Info "`n========================================="
Write-Info "Environment Setup Complete!"
Write-Info "========================================="
Write-Info ""
Write-Info "Environment Variables ($Scope):"
if ($env:VCPKG_ROOT) { Write-Success "  VCPKG_ROOT = $env:VCPKG_ROOT" }
if ($env:BONJOUR_SDK) { Write-Success "  BONJOUR_SDK = $env:BONJOUR_SDK" }
if ($env:HOMEKIT_DEPS) { Write-Success "  HOMEKIT_DEPS = $env:HOMEKIT_DEPS" }
Write-Info ""
Write-Info "Next Steps:"
Write-Info "  1. Close and reopen Visual Studio (to reload environment)"
Write-Info "  2. Open: HomeKitADK.sln"
Write-Info "  3. Build → Rebuild Solution (Ctrl+Shift+B)"
Write-Info "  4. Run Lightbulb (F5)"
Write-Info ""
Write-Success "Ready to build! 🚀"
