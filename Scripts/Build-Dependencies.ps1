#Requires -Version 5.1
<#
.SYNOPSIS
    Builds BTstack, libnfc, and libqrencode from source for Windows

.DESCRIPTION
    Master script to build all optional dependencies that are not available
    in vcpkg for Windows. Clones, builds, and installs:
    - BTstack (Bluetooth LE stack)
    - libnfc (NFC library)
    - libqrencode (QR code generation)

.PARAMETER Libraries
    Comma-separated list: BTstack,libnfc,qrencode,All (default: All)

.PARAMETER InstallPrefix
    Installation prefix (default: C:\HomeKitADK-deps)

.PARAMETER BuildType
    Build type: Debug or Release (default: Release)

.PARAMETER Architecture
    Target architecture: x64 or ARM64 (default: x64)

.PARAMETER Clean
    Clean existing builds before building

.EXAMPLE
    .\Build-Dependencies.ps1

.EXAMPLE
    .\Build-Dependencies.ps1 -Libraries "BTstack,qrencode" -InstallPrefix "C:\deps"

.EXAMPLE
    .\Build-Dependencies.ps1 -Architecture ARM64

.NOTES
    Date: November 15, 2025
    Requires: CMake, Git, Visual Studio Build Tools
#>

param(
    [Parameter()]
    [string]$Libraries = 'All',

    [Parameter()]
    [string]$InstallPrefix = 'C:\HomeKitADK-deps',

    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$BuildType = 'Release',

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Architecture = 'x64',

    [Parameter()]
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }
function Write-Failure { Write-Host $args -ForegroundColor Red }

Write-Info "============================================="
Write-Info "HomeKit ADK Dependency Builder for Windows"
Write-Info "============================================="

# Determine which libraries to build
$buildAll = $Libraries -eq 'All'
$buildBTstack = $buildAll -or ($Libraries -like '*BTstack*')
$buildLibnfc = $buildAll -or ($Libraries -like '*libnfc*')
$buildQRencode = $buildAll -or ($Libraries -like '*qrencode*')

Write-Info "`nLibraries to build:"
if ($buildBTstack) { Write-Info "  ✓ BTstack (Bluetooth LE)" }
if ($buildLibnfc) { Write-Info "  ✓ libnfc (NFC)" }
if ($buildQRencode) { Write-Info "  ✓ libqrencode (QR codes)" }

# Check prerequisites
Write-Info "`nChecking prerequisites..."

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Failure "CMake not found! Install from https://cmake.org/"
    exit 1
}
Write-Success "✓ CMake: $(cmake --version | Select-Object -First 1)"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Failure "Git not found! Install from https://git-scm.com/"
    exit 1
}
Write-Success "✓ Git: $(git --version)"

# Check for Visual Studio
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    Write-Success "✓ Visual Studio: $vsPath"
} else {
    Write-Warning "Visual Studio not detected via vswhere"
}

# Create build directory
$buildDir = Join-Path $PSScriptRoot "..\build-deps"
$sourceDir = Join-Path $buildDir "src"
$installDir = $InstallPrefix

if ($Clean -and (Test-Path $buildDir)) {
    Write-Info "Cleaning build directory..."
    Remove-Item -Recurse -Force $buildDir
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
New-Item -ItemType Directory -Path $sourceDir -Force | Out-Null
New-Item -ItemType Directory -Path $installDir -Force | Out-Null

Write-Info "`nBuild directory: $buildDir"
Write-Info "Source directory: $sourceDir"
Write-Info "Install prefix: $installDir"

#============================================
# Build BTstack
#============================================
if ($buildBTstack) {
    Write-Info "`n============================================="
    Write-Info "Building BTstack"
    Write-Info "============================================="

    $btstackSrc = Join-Path $sourceDir "btstack"
    $btstackBuild = Join-Path $buildDir "btstack-build"

    if (-not (Test-Path $btstackSrc)) {
        Write-Info "Cloning BTstack..."
        Push-Location $sourceDir
        git clone https://github.com/bluekitchen/btstack.git
        Pop-Location
    } else {
        Write-Info "BTstack source exists, updating..."
        Push-Location $btstackSrc
        git pull
        Pop-Location
    }

    # BTstack may have CMakeLists in port/windows or use different build system
    $btstackCMake = if (Test-Path "$btstackSrc\CMakeLists.txt") {
        $btstackSrc
    } elseif (Test-Path "$btstackSrc\port\windows\CMakeLists.txt") {
        "$btstackSrc\port\windows"
    } elseif (Test-Path "$btstackSrc\port\CMakeLists.txt") {
        "$btstackSrc\port"
    } else {
        $null
    }

    if ($btstackCMake) {
        Write-Info "Configuring BTstack for $Architecture from: $btstackCMake"
        $cmakeArgs = @(
            "-S", $btstackCMake,
            "-B", $btstackBuild,
            "-G", "Visual Studio 17 2022",
            "-A", $Architecture,
            "-DCMAKE_BUILD_TYPE=$BuildType",
            "-DCMAKE_INSTALL_PREFIX=$installDir",
            "-DBUILD_SHARED_LIBS=OFF"
        )
        & cmake @cmakeArgs

        if ($LASTEXITCODE -eq 0) {
            Write-Info "Building BTstack..."
            & cmake --build $btstackBuild --config $BuildType

            Write-Info "Installing BTstack..."
            & cmake --install $btstackBuild --config $BuildType

            if ($LASTEXITCODE -eq 0) {
                Write-Success "✓ BTstack built and installed"
            } else {
                Write-Failure "✗ BTstack build failed"
            }
        } else {
            Write-Failure "✗ BTstack configure failed"
        }
    } else {
        Write-Warning "⚠ BTstack doesn't have CMakeLists.txt"
        Write-Info "BTstack may need manual build. Check: https://github.com/bluekitchen/btstack"
        Write-Info "For Windows: BTstack provides pre-built libraries or requires manual MSBuild"
    }
}

#============================================
# Build libnfc
#============================================
if ($buildLibnfc) {
    Write-Info "`n============================================="
    Write-Info "Building libnfc"
    Write-Info "============================================="

    # libnfc requires libusb - check if it's installed
    Write-Info "libnfc requires libusb..."

    $triplet = if ($Architecture -eq "ARM64") { "arm64-windows" } else { "x64-windows" }

    # Check both vcpkg locations (manifest mode uses vcpkg_installed in project root)
    $libusbCheck1 = "$PSScriptRoot\..\vcpkg_installed\$triplet\include\libusb-1.0\libusb.h"
    $libusbCheck2 = "$env:VCPKG_ROOT\installed\$triplet\include\libusb-1.0\libusb.h"

    $libusbFound = (Test-Path $libusbCheck1) -or (Test-Path $libusbCheck2)

    if (-not $libusbFound) {
        Write-Warning "⚠ libusb not found in vcpkg!"
        Write-Info "Installing libusb now..."
        Push-Location $PSScriptRoot\..
        & vcpkg install
        Pop-Location

        $libusbFound = (Test-Path $libusbCheck1) -or (Test-Path $libusbCheck2)
        if (-not $libusbFound) {
            Write-Failure "✗ libusb installation failed"
            Write-Info "Try manually: cd to project root and run 'vcpkg install'"
            return
        }
    }
    Write-Success "✓ libusb found"

    $libnfcSrc = Join-Path $sourceDir "libnfc"
    $libnfcBuild = Join-Path $buildDir "libnfc-build"

    if (-not (Test-Path $libnfcSrc)) {
        Write-Info "Cloning libnfc..."
        Push-Location $sourceDir
        git clone https://github.com/nfc-tools/libnfc.git
        Pop-Location
    } else {
        Write-Info "libnfc source exists, updating..."
        Push-Location $libnfcSrc
        git pull
        Pop-Location
    }

    Write-Info "Configuring libnfc for $Architecture..."
    $cmakeArgs = @(
        "-S", $libnfcSrc,
        "-B", $libnfcBuild,
        "-G", "Visual Studio 17 2022",
        "-A", $Architecture,
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCMAKE_INSTALL_PREFIX=$installDir",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DCMAKE_POLICY_DEFAULT_CMP0074=NEW",
        "-DLIBNFC_DRIVER_PN532_UART=OFF",
        "-DLIBNFC_DRIVER_PN532_SPI=OFF",
        "-DLIBNFC_DRIVER_PN533_USB=ON",
        "-DLIBNFC_DRIVER_PN532_I2C=OFF",
        "-DLIBNFC_DRIVER_ACR122_USB=ON",
        "-DBUILD_SHARED_LIBS=OFF"
    )

    # Add vcpkg toolchain and libusb paths
    if ($env:VCPKG_ROOT) {
        $triplet = if ($Architecture -eq "ARM64") { "arm64-windows" } else { "x64-windows" }
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
        $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=$triplet"

        # Check both vcpkg locations for libusb
        $libusbRoot1 = "$PSScriptRoot\..\vcpkg_installed\$triplet"
        $libusbRoot2 = "$env:VCPKG_ROOT\installed\$triplet"

        $libusbRoot = if (Test-Path "$libusbRoot1\include\libusb-1.0") { $libusbRoot1 } elseif (Test-Path "$libusbRoot2\include\libusb-1.0") { $libusbRoot2 } else { $null }

        if ($libusbRoot) {
            # Find the actual libusb library file
            $libusbLib = Get-ChildItem "$libusbRoot\lib" -Filter "libusb*.lib" -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($libusbLib) {
                # Set variables that libnfc's FindLIBUSB.cmake looks for
                $cmakeArgs += "-DLIBUSB_ROOT=$libusbRoot"
                $cmakeArgs += "-DLIBUSB_INCLUDE_DIR=$libusbRoot\include\libusb-1.0"
                $cmakeArgs += "-DLIBUSB_LIBRARY=$($libusbLib.FullName)"
                $cmakeArgs += "-DLIBUSB_INCLUDE_DIRS=$libusbRoot\include\libusb-1.0"
                $cmakeArgs += "-DLIBUSB_LIBRARIES=$($libusbLib.FullName)"
                $cmakeArgs += "-DLIBUSB_1_INCLUDE_DIRS=$libusbRoot\include\libusb-1.0"
                $cmakeArgs += "-DLIBUSB_1_LIBRARIES=$($libusbLib.FullName)"
                $cmakeArgs += "-DLIBUSB_FOUND=TRUE"
                Write-Info "Using libusb from: $libusbRoot"
                Write-Info "  Include: $libusbRoot\include\libusb-1.0"
                Write-Info "  Library: $($libusbLib.FullName)"
            } else {
                Write-Warning "⚠ libusb library not found in $libusbRoot\lib"
            }
        } else {
            Write-Warning "⚠ libusb paths not found"
        }
    }

    $configureResult = & cmake @cmakeArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Failure "✗ libnfc configuration failed"
        Write-Warning "Make sure libusb is installed: vcpkg install"
        return
    }

    Write-Info "Building libnfc..."
    & cmake --build $libnfcBuild --config $BuildType

    Write-Info "Installing libnfc..."
    & cmake --install $libnfcBuild --config $BuildType

    if ($LASTEXITCODE -eq 0) {
        Write-Success "✓ libnfc built and installed"
    } else {
        Write-Failure "✗ libnfc build failed"
    }
}

#============================================
# Build libqrencode
#============================================
if ($buildQRencode) {
    Write-Info "`n============================================="
    Write-Info "Building libqrencode"
    Write-Info "============================================="

    $qrencodeSrc = Join-Path $sourceDir "libqrencode"
    $qrencodeBuild = Join-Path $buildDir "qrencode-build"

    if (-not (Test-Path $qrencodeSrc)) {
        Write-Info "Cloning libqrencode..."
        Push-Location $sourceDir
        git clone https://github.com/fukuchi/libqrencode.git
        Pop-Location
    } else {
        Write-Info "libqrencode source exists, updating..."
        Push-Location $qrencodeSrc
        git pull
        Pop-Location
    }

    Write-Info "Configuring libqrencode for $Architecture..."
    $cmakeArgs = @(
        "-S", $qrencodeSrc,
        "-B", $qrencodeBuild,
        "-G", "Visual Studio 17 2022",
        "-A", $Architecture,
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCMAKE_INSTALL_PREFIX=$installDir",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DWITH_TOOLS=OFF",
        "-DBUILD_SHARED_LIBS=OFF"
    )
    & cmake @cmakeArgs

    Write-Info "Building libqrencode..."
    & cmake --build $qrencodeBuild --config $BuildType

    Write-Info "Installing libqrencode..."
    & cmake --install $qrencodeBuild --config $BuildType

    if ($LASTEXITCODE -eq 0) {
        Write-Success "✓ libqrencode built and installed"
    } else {
        Write-Failure "✗ libqrencode build failed"
    }
}

#============================================
# Set Environment Variable
#============================================
Write-Info "`n============================================="
Write-Info "Configuring Environment"
Write-Info "============================================="

[System.Environment]::SetEnvironmentVariable('HOMEKIT_DEPS', $installDir, 'User')
$env:HOMEKIT_DEPS = $installDir
Write-Success "✓ Set HOMEKIT_DEPS=$installDir"

#============================================
# Summary
#============================================
Write-Info "`n============================================="
Write-Info "Build Complete!"
Write-Info "============================================="
Write-Info "Installation directory: $installDir"
Write-Info ""
Write-Info "Directory structure:"
Write-Info "  $installDir\include  - Header files"
Write-Info "  $installDir\lib      - Static libraries"
Write-Info "  $installDir\bin      - DLLs (if any)"
Write-Info ""
Write-Info "Environment variable set:"
Write-Success "  HOMEKIT_DEPS=$installDir"
Write-Info ""
Write-Info "Next steps:"
Write-Info "  1. Restart Visual Studio (to reload environment)"
Write-Info "  2. Open HomeKitADK.sln"
Write-Info "  3. Build with BLE/NFC/QRCode support enabled"
Write-Info ""
Write-Info "The build system will automatically find libraries in:"
Write-Info "  `$(HOMEKIT_DEPS)\include"
Write-Info "  `$(HOMEKIT_DEPS)\lib"
Write-Info "============================================="
