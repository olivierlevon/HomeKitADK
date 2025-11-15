# Building Optional Dependencies from Source on Windows
## November 15, 2025

## ⚠️ IMPORTANT NOTE

The following libraries are **not available in vcpkg for Windows** and must be built from source:
- **BTstack** - Bluetooth LE peripheral stack
- **libnfc** - NFC library
- **libqrencode** - QR code generation

This guide explains how to build them properly for HomeKit ADK.

---

## 🚀 QUICK START (Automated)

### One Command to Build All:
```powershell
.\Scripts\Build-Dependencies.ps1
```

This will:
1. Clone BTstack, libnfc, and libqrencode from GitHub
2. Build all three with CMake and Visual Studio
3. Install to `C:\HomeKitADK-deps`
4. Set `HOMEKIT_DEPS` environment variable

**After completion:**
- Restart Visual Studio
- Build HomeKitADK with BLE/NFC/QR support enabled

---

## 📋 MANUAL BUILD (Step by Step)

### Prerequisites
- Visual Studio 2022/2026 with C++ tools
- CMake 3.20+
- Git

### Build BTstack (for BLE Support)

```powershell
# Clone
git clone https://github.com/bluekitchen/btstack.git C:\build-deps\btstack
cd C:\build-deps\btstack

# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX=C:\HomeKitADK-deps `
  -DBUILD_SHARED_LIBS=OFF

# Build
cmake --build build --config Release

# Install
cmake --install build --config Release
```

**Result:** BTstack installed to `C:\HomeKitADK-deps\`
- Headers: `C:\HomeKitADK-deps\include\btstack\`
- Library: `C:\HomeKitADK-deps\lib\btstack.lib`

### Build libnfc (for NFC Support)

```powershell
# Clone
git clone https://github.com/nfc-tools/libnfc.git C:\build-deps\libnfc
cd C:\build-deps\libnfc

# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX=C:\HomeKitADK-deps `
  -DLIBNFC_DRIVER_PN532_UART=ON `
  -DLIBNFC_DRIVER_PN532_SPI=ON `
  -DLIBNFC_DRIVER_PN533_USB=ON `
  -DLIBNFC_DRIVER_PN532_I2C=ON `
  -DBUILD_SHARED_LIBS=OFF

# Build
cmake --build build --config Release

# Install
cmake --install build --config Release
```

**Result:** libnfc installed to `C:\HomeKitADK-deps\`
- Headers: `C:\HomeKitADK-deps\include\nfc\`
- Library: `C:\HomeKitADK-deps\lib\nfc.lib`

### Build libqrencode (for QR Code Support)

```powershell
# Clone
git clone https://github.com/fukuchi/libqrencode.git C:\build-deps\libqrencode
cd C:\build-deps\libqrencode

# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX=C:\HomeKitADK-deps `
  -DWITH_TOOLS=OFF `
  -DBUILD_SHARED_LIBS=OFF

# Build
cmake --build build --config Release

# Install
cmake --install build --config Release
```

**Result:** libqrencode installed to `C:\HomeKitADK-deps\`
- Headers: `C:\HomeKitADK-deps\include\qrencode.h`
- Library: `C:\HomeKitADK-deps\lib\qrencode.lib`

---

## 🔧 SET ENVIRONMENT VARIABLE

After building, set the environment variable:

```powershell
# Temporary (current session)
$env:HOMEKIT_DEPS = "C:\HomeKitADK-deps"

# Permanent (User scope)
[System.Environment]::SetEnvironmentVariable('HOMEKIT_DEPS', 'C:\HomeKitADK-deps', 'User')

# Permanent (System scope - requires admin)
[System.Environment]::SetEnvironmentVariable('HOMEKIT_DEPS', 'C:\HomeKitADK-deps', 'Machine')
```

**Restart Visual Studio** after setting environment variables!

---

## 🎯 USING IN YOUR BUILD

### CMake Build
The build system will automatically find libraries via our custom Find modules:
- `cmake/Modules/FindBTstack.cmake`
- `cmake/Modules/Findlibnfc.cmake`
- `cmake/Modules/Findqrencode.cmake`

These search in order:
1. `$(HOMEKIT_DEPS)\include` and `$(HOMEKIT_DEPS)\lib`
2. `$(VCPKG_ROOT)\installed\x64-windows\`
3. System paths

### Visual Studio Build
Projects are configured to use:
- Include: `$(HOMEKIT_DEPS)\include`
- Library: `$(HOMEKIT_DEPS)\lib`

All .vcxproj files already have these paths configured!

---

## 📊 AUTOMATED BUILD SCRIPT OPTIONS

### Build Specific Libraries
```powershell
# Build only BTstack
.\Scripts\Build-Dependencies.ps1 -Libraries "BTstack"

# Build only QR code support
.\Scripts\Build-Dependencies.ps1 -Libraries "qrencode"

# Build BTstack and libnfc
.\Scripts\Build-Dependencies.ps1 -Libraries "BTstack,libnfc"

# Build everything (default)
.\Scripts\Build-Dependencies.ps1
```

### Custom Install Location
```powershell
.\Scripts\Build-Dependencies.ps1 -InstallPrefix "D:\MyLibs"

# Then set:
$env:HOMEKIT_DEPS = "D:\MyLibs"
[System.Environment]::SetEnvironmentVariable('HOMEKIT_DEPS', 'D:\MyLibs', 'User')
```

### Debug Builds
```powershell
.\Scripts\Build-Dependencies.ps1 -BuildType Debug
```

### Clean Build
```powershell
.\Scripts\Build-Dependencies.ps1 -Clean
```

---

## 🐛 TROUBLESHOOTING

### CMake doesn't find libraries

**Check environment variable:**
```powershell
$env:HOMEKIT_DEPS
# Should output: C:\HomeKitADK-deps
```

**Verify installation:**
```powershell
dir C:\HomeKitADK-deps\include
dir C:\HomeKitADK-deps\lib
```

**Restart Visual Studio** to reload environment!

### Build fails during clone

**Network issues:**
```powershell
# Clone manually first
git clone https://github.com/bluekitchen/btstack.git
git clone https://github.com/nfc-tools/libnfc.git
git clone https://github.com/fukuchi/libqrencode.git

# Then run script
.\Scripts\Build-Dependencies.ps1
```

### Visual Studio can't find headers

**Verify project settings:**
- Open project properties
- C/C++ → General → Additional Include Directories
- Should include: `$(HOMEKIT_DEPS)\include`

**Check environment in VS:**
- Tools → Options → Projects and Solutions → Build and Run
- MSBuild project build output verbosity: Detailed
- Look for HOMEKIT_DEPS in output

---

## 📁 INSTALLATION DIRECTORY STRUCTURE

After running `Build-Dependencies.ps1`, you'll have:

```
C:\HomeKitADK-deps\
├── include\
│   ├── btstack\
│   │   ├── btstack.h
│   │   ├── ble\
│   │   └── ... (BTstack headers)
│   ├── nfc\
│   │   ├── nfc.h
│   │   └── nfc-types.h
│   └── qrencode.h
│
└── lib\
    ├── btstack.lib
    ├── nfc.lib
    └── qrencode.lib
```

---

## ✅ VERIFICATION

### Test Library Detection (CMake)
```powershell
cmake -B build-test -G Ninja `
  -DHAVE_BLE=ON `
  -DHAVE_NFC=ON `
  -DHAVE_QR_CODE=ON

# Check output for:
# -- BLE support enabled with BTstack: C:/HomeKitADK-deps/lib/btstack.lib
# -- NFC support enabled with libnfc: C:/HomeKitADK-deps/lib/nfc.lib
# -- QR code generation enabled with libqrencode: C:/HomeKitADK-deps/lib/qrencode.lib
```

### Test in Visual Studio
1. Open HomeKitADK.sln
2. Build HAPPlatform_Windows project
3. Check Output window for include paths
4. Should see: `$(HOMEKIT_DEPS)\include` in the paths

---

## 🎁 COMPLETE WORKFLOW

```powershell
# 1. Setup base environment
.\Scripts\Setup-Environment.ps1

# 2. Build optional dependencies
.\Scripts\Build-Dependencies.ps1

# 3. Restart Visual Studio
# Close and reopen VS

# 4. Build HomeKit ADK
start HomeKitADK.sln
# Select configuration with features you want
# Build → Rebuild Solution

# 5. Run with all features!
# BLE, NFC, QR codes all working
```

---

## 📝 SUMMARY

| Library | Source | Build Tool | Install Location |
|---------|--------|------------|------------------|
| BTstack | GitHub | CMake + VS | C:\HomeKitADK-deps |
| libnfc | GitHub | CMake + VS | C:\HomeKitADK-deps |
| libqrencode | GitHub | CMake + VS | C:\HomeKitADK-deps |

**Environment Variable:** `HOMEKIT_DEPS=C:\HomeKitADK-deps`

**Script:** `Scripts\Build-Dependencies.ps1` (fully automated)

**CMake Find Modules:** `cmake\Modules\Find*.cmake` (automatic detection)

---

**All optional dependencies can now be properly built and integrated on Windows!** 🎉

Run `Scripts\Build-Dependencies.ps1` to get started!
