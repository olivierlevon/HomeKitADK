#Requires -Version 5.1
<#
.SYNOPSIS
    Installs HomeKit ADK application to a target location

.DESCRIPTION
    PowerShell equivalent of Tools/install.sh
    Deploys built application with all dependencies to specified location

.PARAMETER Application
    Application name (Lightbulb, Lock, or AccessorySetupGenerator)

.PARAMETER Destination
    Installation destination path (default: C:\Program Files\HomeKitADK)

.PARAMETER Configuration
    Build configuration: Debug or Release (default: Release)

.EXAMPLE
    .\Install-Application.ps1 -Application Lightbulb

.EXAMPLE
    .\Install-Application.ps1 -Application Lock -Destination "C:\HomeKit" -Configuration Debug

.NOTES
    Date: November 15, 2025
    Requires Administrator privileges for Program Files installation
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Lightbulb', 'Lock', 'AccessorySetupGenerator')]
    [string]$Application,

    [Parameter()]
    [string]$Destination = "C:\Program Files\HomeKitADK",

    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Failure { Write-Host $args -ForegroundColor Red }

Write-Info "========================================="
Write-Info "HomeKit ADK Application Installer"
Write-Info "========================================="

# Check if running as administrator for Program Files
if ($Destination -like "*Program Files*") {
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Write-Failure "Administrator privileges required for Program Files installation"
        Write-Info "Rerun as Administrator or choose different destination"
        exit 1
    }
}

# Find built executable
$exeName = "$Application.exe"
$buildPaths = @(
    "build\vs2026\bin\$Configuration\$exeName",
    "build\windows-x64-$($Configuration.ToLower())\bin\$exeName",
    "build\bin\$Configuration\$exeName"
)

$exePath = $null
foreach ($path in $buildPaths) {
    if (Test-Path $path) {
        $exePath = $path
        break
    }
}

if (-not $exePath) {
    Write-Failure "✗ Built application not found. Build first!"
    Write-Info "Expected locations:"
    $buildPaths | ForEach-Object { Write-Info "  - $_" }
    exit 1
}

Write-Success "✓ Found: $exePath"

# Create destination
$appDest = Join-Path $Destination $Application
if (-not (Test-Path $appDest)) {
    New-Item -ItemType Directory -Path $appDest -Force | Out-Null
}

# Copy executable
Write-Info "Installing to: $appDest"
Copy-Item $exePath $appDest -Force
Write-Success "✓ Copied $exeName"

# Copy dependencies (DLLs)
$vcpkgBin = "$env:VCPKG_ROOT\installed\x64-windows\bin"
if (Test-Path $vcpkgBin) {
    Write-Info "Copying runtime dependencies..."

    $dlls = @(
        "libcrypto-*.dll",
        "libssl-*.dll"
    )

    foreach ($dll in $dlls) {
        $files = Get-ChildItem -Path $vcpkgBin -Filter $dll -ErrorAction SilentlyContinue
        if ($files) {
            Copy-Item $files.FullName $appDest -Force
            Write-Success "✓ Copied $($files.Name)"
        }
    }
}

# Create storage directory
$storagePath = Join-Path $appDest "db"
if (-not (Test-Path $storagePath)) {
    New-Item -ItemType Directory -Path $storagePath -Force | Out-Null
    Write-Success "✓ Created storage directory"
}

# Create launcher script
$launcherPath = Join-Path $Destination "$Application.bat"
$launcherContent = @"
@echo off
cd /d "%~dp0$Application"
$exeName
pause
"@

$launcherContent | Out-File -FilePath $launcherPath -Encoding ASCII
Write-Success "✓ Created launcher: $launcherPath"

# Summary
Write-Info "`n========================================="
Write-Info "Installation Complete!"
Write-Info "========================================="
Write-Info "Application: $Application"
Write-Info "Location: $appDest"
Write-Info "Launcher: $launcherPath"
Write-Info ""
Write-Info "To run:"
Write-Info "  $launcherPath"
Write-Info "  OR"
Write-Info "  cd `"$appDest`""
Write-Info "  .\$exeName"
Write-Info "========================================="
