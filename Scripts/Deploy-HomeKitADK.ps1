#Requires -Version 5.1
<#
.SYNOPSIS
    Deploys HomeKit ADK binaries and libraries

.DESCRIPTION
    Packages and deploys HomeKit ADK applications, libraries, and dependencies
    for distribution or installation.

.PARAMETER Configuration
    Build configuration to deploy: Debug or Release

.PARAMETER Destination
    Deployment destination directory

.PARAMETER IncludeDependencies
    Include runtime dependencies (DLLs)

.EXAMPLE
    .\Deploy-HomeKitADK.ps1 -Configuration Release -Destination C:\HomeKitApps

.NOTES
    Date: November 15, 2025
#>

param(
    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [Parameter()]
    [string]$Destination = 'dist',

    [Parameter()]
    [switch]$IncludeDependencies
)

$ErrorActionPreference = 'Stop'

function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info { Write-Host $args -ForegroundColor Cyan }

Write-Info "========================================="
Write-Info "HomeKit ADK Deployment"
Write-Info "========================================="

$preset = "windows-x64-$($Configuration.ToLower())"
$buildDir = "build\$preset"

if (-not (Test-Path $buildDir)) {
    Write-Host "Build not found. Building now..." -ForegroundColor Yellow
    & cmake --preset $preset
    & cmake --build --preset $preset --config $Configuration
}

# Create destination
if (Test-Path $Destination) {
    Write-Info "Cleaning existing deployment..."
    Remove-Item -Recurse -Force $Destination
}
New-Item -ItemType Directory -Path $Destination | Out-Null

# Install using CMake
Write-Info "Installing binaries..."
& cmake --install $buildDir --prefix $Destination --config $Configuration

# Copy runtime dependencies if requested
if ($IncludeDependencies) {
    Write-Info "Copying runtime dependencies..."

    # OpenSSL DLLs
    if (Test-Path "$env:VCPKG_ROOT\installed\x64-windows\bin\libssl-*.dll") {
        Copy-Item "$env:VCPKG_ROOT\installed\x64-windows\bin\libssl-*.dll" "$Destination\bin\"
        Copy-Item "$env:VCPKG_ROOT\installed\x64-windows\bin\libcrypto-*.dll" "$Destination\bin\"
        Write-Success "✓ Copied OpenSSL DLLs"
    }

    # Bonjour DLL (if installed)
    $bonjourDLL = "C:\Program Files\Bonjour\mdnsNSP.dll"
    if (Test-Path $bonjourDLL) {
        Copy-Item $bonjourDLL "$Destination\bin\"
        Write-Success "✓ Copied Bonjour DLL"
    }
}

# Create README
$readme = @"
HomeKit ADK for Windows - Deployment Package
Date: $(Get-Date -Format 'yyyy-MM-dd')
Configuration: $Configuration

CONTENTS:
  bin/          - Executables (Lightbulb.exe, Lock.exe)
  lib/          - Static libraries
  include/      - Header files

USAGE:
  1. Run: bin\Lightbulb.exe or bin\Lock.exe
  2. Note the setup code displayed
  3. Open iOS Home app
  4. Add accessory using setup code

REQUIREMENTS:
  - Windows 10 1809+ or Windows 11
  - Bonjour Service (for mDNS discovery)
  - WiFi or Ethernet connection

For more information, see BUILD_WINDOWS.md
"@

$readme | Out-File -FilePath "$Destination\README.txt" -Encoding UTF8

Write-Success "`n✓ Deployment complete: $Destination"
Write-Info "`nPackage contents:"
Get-ChildItem -Recurse $Destination | Select-Object -First 20 | Format-Table Name, Length -AutoSize
