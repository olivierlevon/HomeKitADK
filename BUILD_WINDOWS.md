# Building HomeKit ADK on Windows

This guide explains how to build the HomeKit Accessory Development Kit (ADK) on Windows using modern tooling.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Build Methods](#build-methods)
  - [Visual Studio 2022](#visual-studio-2022)
  - [CMake Command Line](#cmake-command-line)
  - [vcpkg Integration](#vcpkg-integration)
- [Build Options](#build-options)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required Software

1. **Visual Studio 2022** (or later) with C++ development tools
   - Download: https://visualstudio.microsoft.com/downloads/
   - Required components:
     - Desktop development with C++
     - CMake tools for Windows
     - Windows 10/11 SDK

2. **CMake 3.20 or later**
   - Download: https://cmake.org/download/
   - Or install via Visual Studio Installer

3. **vcpkg** (Package Manager)
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
   - Set environment variable: `VCPKG_ROOT=C:\path\to\vcpkg`

4. **Git for Windows**
   - Download: https://git-scm.com/download/win

### Optional Software

5. **Bonjour SDK for Windows** (Required for mDNS service discovery)
   - Download from Apple Developer Portal
   - https://developer.apple.com/bonjour/
   - Install to default location: `C:\Program Files\Bonjour SDK`

6. **Ninja Build System** (Recommended for faster builds)
   ```powershell
   choco install ninja
   # Or download from: https://ninja-build.org/
   ```

---

## Quick Start

### Method 1: Visual Studio 2022 (Recommended for Windows)

1. **Clone the repository:**
   ```powershell
   git clone https://github.com/apple/HomeKitADK.git
   cd HomeKitADK
   ```

2. **Open in Visual Studio:**
   - Launch Visual Studio 2022
   - Choose "Open a local folder"
   - Select the HomeKitADK directory
   - Visual Studio will automatically detect `CMakeLists.txt` and `CMakePresets.json`

3. **Select Configuration:**
   - In Visual Studio, use the configuration dropdown
   - Choose: `vs2022` or `windows-x64-debug`

4. **Build:**
   - Select `Build` → `Build All` (or press `Ctrl+Shift+B`)
   - Executables will be in: `build/vs2022/bin/`

### Method 2: Command Line with CMake

1. **Set up environment:**
   ```powershell
   # Ensure VCPKG_ROOT is set
   $env:VCPKG_ROOT = "C:\path\to\vcpkg"
   ```

2. **Configure and build:**
   ```powershell
   # Configure
   cmake --preset windows-x64-debug

   # Build
   cmake --build --preset windows-x64-debug

   # Or using Ninja directly
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

3. **Run applications:**
   ```powershell
   cd build\windows-x64-debug\bin
   .\Lightbulb.exe
   ```

---

## Build Methods

### Visual Studio 2022

#### Opening the Project

Visual Studio 2022 has native CMake support. Simply open the folder containing `CMakeLists.txt`.

#### Available Configurations

The project includes several pre-configured CMake presets:

| Preset | Description |
|--------|-------------|
| `vs2022` | Standard Visual Studio build (Debug/Release switchable in VS) |
| `windows-x64-debug` | Debug build with Ninja generator |
| `windows-x64-release` | Optimized release build |
| `windows-x64-debug-nfc` | Debug build with NFC support enabled |
| `windows-x64-mbedtls` | Release build using MbedTLS instead of OpenSSL |

#### Building

- **Build All:** `Build` → `Build All` (Ctrl+Shift+B)
- **Build Specific Target:** Right-click target in Solution Explorer → `Build`
- **Clean:** `Build` → `Clean All`

#### Debugging

1. Select the application to debug (e.g., `Lightbulb.exe`)
2. Right-click → `Set as Startup Item`
3. Press F5 to start debugging

### CMake Command Line

#### Configure

```powershell
# Using preset (recommended)
cmake --preset windows-x64-debug

# Manual configuration
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DBUILD_APPLICATIONS=ON `
  -DHAVE_DISPLAY=ON
```

#### Build

```powershell
# Build using preset
cmake --build --preset windows-x64-debug

# Build manually
cmake --build build --config Debug

# Build specific target
cmake --build build --target Lightbulb --config Release
```

#### Install

```powershell
cmake --install build --prefix install
```

### vcpkg Integration

#### Installing Dependencies

vcpkg will automatically install required dependencies based on `vcpkg.json`:

**Core dependencies:**
- OpenSSL 3.x (or MbedTLS 4.x if using `-DUSE_MBEDTLS=ON`)

**Optional dependencies:**
- libnfc (enable with `-DHAVE_NFC=ON`)

#### Manual Installation

If you need to install dependencies manually:

```powershell
cd $env:VCPKG_ROOT

# Install OpenSSL
.\vcpkg install openssl:x64-windows

# Install MbedTLS 4.x (alternative crypto library)
.\vcpkg install mbedtls:x64-windows

# Install libnfc (for NFC support)
.\vcpkg install libnfc:x64-windows
```

#### Using NuGet Instead of vcpkg

You can also use NuGet packages:

1. In Visual Studio, right-click the project → `Manage NuGet Packages`
2. Search for and install:
   - `openssl` or `mbedtls`
   - `libnfc` (if available)

---

## Build Options

Configure the build using CMake cache variables:

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_APPLICATIONS` | `ON` | Build example applications (Lightbulb, Lock) |
| `BUILD_TESTING` | `ON` | Build unit tests |
| `HAVE_NFC` | `OFF` | Enable NFC support (requires libnfc) |
| `HAVE_DISPLAY` | `ON` | Enable QR code display support |
| `HAVE_MFI_HW_AUTH` | `OFF` | Enable MFi hardware authentication (not supported on Windows) |
| `USE_MBEDTLS` | `OFF` | Use MbedTLS instead of OpenSSL |
| `HAP_LOG_LEVEL` | `2` | Logging level (0=None, 1=Default, 2=Info, 3=Debug) |

### Example: Building with NFC Support

```powershell
cmake --preset windows-x64-debug-nfc
cmake --build --preset windows-x64-debug-nfc
```

### Example: Building with MbedTLS

```powershell
cmake -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DUSE_MBEDTLS=ON `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Project Structure

```
HomeKitADK/
├── Applications/          # Example applications
│   ├── Lightbulb/        # Smart lightbulb example
│   └── Lock/             # Smart lock example
├── HAP/                   # HomeKit Accessory Protocol core library
├── PAL/                   # Platform Abstraction Layer
│   └── Windows/          # Windows-specific implementation
├── External/              # Third-party utilities
│   ├── Base64/           # Base64 encoding/decoding
│   ├── JSON/             # JSON parser
│   └── HTTP/             # HTTP parser
├── Tests/                 # Unit tests
├── build/                 # Build output (gitignored)
├── CMakeLists.txt        # Root CMake configuration
├── CMakePresets.json     # CMake presets for easy configuration
└── vcpkg.json            # Dependency manifest
```

---

## Troubleshooting

### "Bonjour SDK not found"

**Problem:** CMake warning about missing Bonjour SDK.

**Solution:**
1. Download Bonjour SDK from Apple: https://developer.apple.com/bonjour/
2. Install to the default location: `C:\Program Files\Bonjour SDK`
3. Or specify custom path:
   ```powershell
   cmake -B build -DDNSSD_LIBRARY="C:\custom\path\dnssd.lib" `
                  -DDNSSD_INCLUDE_DIR="C:\custom\path\include"
   ```

### "OpenSSL not found"

**Problem:** vcpkg can't find OpenSSL.

**Solution:**
```powershell
cd $env:VCPKG_ROOT
.\vcpkg install openssl:x64-windows
```

### "libnfc not found"

**Problem:** NFC support enabled but libnfc not available.

**Solution:**
Either disable NFC support or install libnfc:
```powershell
cd $env:VCPKG_ROOT
.\vcpkg install libnfc:x64-windows
```

### Visual Studio Can't Find CMake

**Problem:** VS doesn't detect CMake configuration.

**Solution:**
1. Ensure `CMake tools for Windows` is installed via Visual Studio Installer
2. Delete `build/` and `.vs/` folders
3. Restart Visual Studio
4. `Project` → `Delete Cache and Reconfigure`

### Build Fails with "winsock2.h not found"

**Problem:** Missing Windows SDK.

**Solution:**
Install Windows 10/11 SDK via Visual Studio Installer:
- Open Visual Studio Installer
- Modify your installation
- Check "Windows 10/11 SDK" under Individual Components

---

## Running the Applications

### Lightbulb Example

```powershell
cd build\windows-x64-debug\bin
.\Lightbulb.exe
```

The application will:
1. Start a HomeKit accessory server
2. Display setup code and QR code payload
3. Advertise via mDNS (if Bonjour SDK is installed)
4. Wait for HomeKit pairing

### Lock Example

```powershell
cd build\windows-x64-debug\bin
.\Lock.exe
```

### Pairing with iOS

1. Open the Home app on iPhone/iPad
2. Tap "Add Accessory"
3. Scan the QR code or enter the setup code manually
4. Follow the pairing instructions

---

## Advanced Topics

### Using Different Crypto Libraries

**OpenSSL (default):**
```powershell
cmake --preset windows-x64-release
```

**MbedTLS 4.x (with TLS 1.3):**
```powershell
cmake --preset windows-x64-mbedtls
```

### Cross-Compilation

To cross-compile for ARM64 Windows:
```powershell
cmake -B build-arm64 -G "Visual Studio 17 2022" -A ARM64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=arm64-windows
cmake --build build-arm64
```

### Static vs Dynamic Linking

By default, the project builds static libraries. To build shared libraries:
```powershell
cmake -B build -DBUILD_SHARED_LIBS=ON
```

---

## Additional Resources

- **HomeKit ADK Documentation:** https://github.com/apple/HomeKitADK
- **vcpkg Documentation:** https://vcpkg.io/
- **CMake Documentation:** https://cmake.org/documentation/
- **Visual Studio CMake Support:** https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio

---

## Support

For issues specific to the Windows port:
- Check the [GitHub Issues](https://github.com/apple/HomeKitADK/issues)
- Review build logs in `build/CMakeFiles/CMakeOutput.log`
- Enable verbose logging: `cmake --build build --verbose`

For HomeKit protocol questions:
- Review the HomeKit Accessory Protocol Specification
- Check Apple Developer Forums
