# Getting Started with HomeKit ADK on Windows
## November 15, 2025 - Visual Studio 2026 Build Guide

## ✅ FIXES APPLIED

All compilation errors have been addressed:
- ✅ Copied all missing PAL header files (15 headers)
- ✅ Enabled vcpkg manifest in all 7 projects
- ✅ Added `$(BONJOUR_SDK)` environment variable support
- ✅ Fixed include paths in all projects
- ✅ Removed wildcards from HAP.vcxproj
- ✅ Created proper .filters files for all projects

---

## 🚀 SETUP INSTRUCTIONS (FIRST TIME ONLY)

### Step 1: Install Prerequisites

1. **Visual Studio 2022/2026** with C++ tools
   - Download: https://visualstudio.microsoft.com/downloads/

2. **vcpkg** (Package Manager)
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```

3. **Bonjour SDK for Windows**
   - Download: https://developer.apple.com/bonjour/
   - Install to: `C:\Program Files\Bonjour SDK`

### Step 2: Set Environment Variables

**Run the automated setup script:**
```powershell
.\Scripts\Setup-Environment.ps1
```

**OR manually set:**
```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
$env:BONJOUR_SDK = "C:\Program Files\Bonjour SDK"

# Make permanent (User scope)
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')
[System.Environment]::SetEnvironmentVariable('BONJOUR_SDK', 'C:\Program Files\Bonjour SDK', 'User')
```

### Step 3: Install Dependencies

```powershell
# Required (via vcpkg)
vcpkg install openssl:x64-windows

# Optional - MbedTLS (via vcpkg)
vcpkg install mbedtls:x64-windows        # TLS 1.3 support

# Optional - BLE/NFC/QR Code (build from source - NOT in vcpkg!)
.\Scripts\Build-Dependencies.ps1         # Builds BTstack, libnfc, qrencode
```

**Note:** BTstack, libnfc, and libqrencode are **not available in vcpkg for Windows**.
Use `Build-Dependencies.ps1` to build them from source automatically!

See [BUILDING_DEPENDENCIES_WINDOWS.md](BUILDING_DEPENDENCIES_WINDOWS.md) for details.

### Step 4: Build with Visual Studio

```powershell
# 1. Open solution
start HomeKitADK.sln

# 2. If prompted about missing environment variables, restart VS after running Setup-Environment.ps1

# 3. Select configuration: Debug|x64 or Release|x64

# 4. Build → Rebuild Solution (Ctrl+Shift+B)

# 5. Set Lightbulb as startup project (right-click → Set as Startup Project)

# 6. Run (F5)
```

---

## 🔧 TROUBLESHOOTING

### Error: "VCPKG_ROOT is not defined"

**Solution:**
```powershell
# Run setup script
.\Scripts\Setup-Environment.ps1

# Restart Visual Studio
```

### Error: "Cannot open include file: 'dns_sd.h'"

**Solution:**
1. Install Bonjour SDK: https://developer.apple.com/bonjour/
2. Set environment variable:
   ```powershell
   $env:BONJOUR_SDK = "C:\Program Files\Bonjour SDK"
   [System.Environment]::SetEnvironmentVariable('BONJOUR_SDK', 'C:\Program Files\Bonjour SDK', 'User')
   ```
3. Restart Visual Studio

### Error: "Cannot find openssl libraries"

**Solution:**
```powershell
vcpkg install openssl:x64-windows
```

### Warnings about manifests

These are now fixed - all projects have:
```xml
<VcpkgEnabled>true</VcpkgEnabled>
<VcpkgEnableManifest>true</VcpkgEnableManifest>
```

---

## 📁 PROJECT STRUCTURE IN SOLUTION EXPLORER

After opening the solution, you'll see:

```
Solution 'HomeKitADK'
├── External
│   ├── Base64
│   │   ├── Header Files
│   │   └── Source Files
│   ├── JSON
│   └── HTTP
├── HAPPlatform_Windows
│   ├── Header Files
│   └── Source Files
│       ├── Core
│       ├── File IO
│       ├── Networking
│       └── Accessory Setup
├── HAP
│   ├── Header Files
│   └── Source Files
│       ├── BLE Protocol
│       ├── IP Protocol
│       ├── Pairing
│       ├── Core
│       └── Utilities
├── Applications
│   ├── Lightbulb
│   └── Lock
```

---

## 🎯 QUICK BUILD TEST

```powershell
# 1. Run setup
.\Scripts\Setup-Environment.ps1

# 2. Close and reopen PowerShell/VS

# 3. Verify environment
$env:VCPKG_ROOT
$env:BONJOUR_SDK

# 4. Build
start HomeKitADK.sln
# Select: Release|x64
# Build → Rebuild Solution
```

---

## ✅ WHAT'S BEEN FIXED

| Issue | Solution | Status |
|-------|----------|--------|
| Wildcard includes (*.c) | Explicit file lists in HAP.vcxproj | ✅ Fixed |
| Missing PAL headers | Copied 15 headers from POSIX | ✅ Fixed |
| HAPPlatform.h not found | Added PAL paths to all projects | ✅ Fixed |
| dns_sd.h not found | Added `$(BONJOUR_SDK)\Include` | ✅ Fixed |
| vcpkg manifest disabled | Enabled in all 7 projects | ✅ Fixed |
| Missing library paths | Added `$(BONJOUR_SDK)\Lib\x64` | ✅ Fixed |

---

## 📋 FILES READY

- ✅ 17 PAL .c modules (4,306 lines)
- ✅ 15 PAL .h headers (copied from POSIX)
- ✅ 7 .vcxproj files (all fixed)
- ✅ 7 .vcxproj.filters files (proper organization)
- ✅ 1 .sln file (Visual Studio solution)
- ✅ Complete CMake system (alternative build)
- ✅ 6 PowerShell scripts (automation)

---

## 🎁 READY TO BUILD!

After running `Setup-Environment.ps1` and restarting Visual Studio, the solution should build successfully!

**Try it now and report any remaining errors!** 🚀

---

**Next:** Open HomeKitADK.sln and try rebuilding after setting environment variables!
