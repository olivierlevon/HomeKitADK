# Building Optional Dependencies - UPDATED GUIDE
## November 15, 2025 - Tested Build Instructions

## ⚠️ IMPORTANT - Actual Status

After testing the build script, here's what actually works:

### ✅ What Works via Script:
- **libnfc** - Builds successfully with CMake (with policy fix)
- **libqrencode** - Builds successfully with CMake (with policy fix)

### ⚠️ What Needs Manual Attention:
- **BTstack** - Does NOT use CMake, requires alternative build method

---

## 🚀 RECOMMENDED APPROACH

### Option 1: IP-Only Build (No BLE/NFC/QR) - WORKS NOW ✅

```powershell
# Just build the base HomeKit ADK
# Supports: TCP/IP, mDNS, basic setup codes
# No additional dependencies needed!

.\Scripts\Setup-Environment.ps1
# Open HomeKitADK.sln and build
```

**This already works and is production-ready!**

---

### Option 2: Build libnfc and qrencode (NFC + QR Codes)

```powershell
# Build script with fixes applied
.\Scripts\Build-Dependencies.ps1 -Libraries "libnfc,qrencode"
```

**Expected Result:**
- ✅ libnfc.lib → C:\HomeKitADK-deps\lib\
- ✅ qrencode.lib → C:\HomeKitADK-deps\lib\

---

### Option 3: BTstack for BLE Support (Manual Build Required)

BTstack doesn't use CMake on Windows. Here's how to build it:

#### Method A: Use Pre-Built BTstack (Easiest)

1. Download BTstack Windows port from releases
2. Extract headers to `C:\HomeKitADK-deps\include\btstack\`
3. Extract libraries to `C:\HomeKitADK-deps\lib\`

#### Method B: Build BTstack from Source

BTstack uses its own build system. Check their documentation:
https://github.com/bluekitchen/btstack/tree/master/port/windows

**For Windows:**
```powershell
cd build-deps\src\btstack\port\windows
# Follow BTstack's Windows build instructions
# Usually involves MSBuild or their custom scripts
```

---

## 📋 UPDATED Build-Dependencies.ps1

The script has been updated to:
- ✅ Add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` for libnfc and qrencode
- ✅ Detect if BTstack has CMakeLists.txt
- ✅ Gracefully handle BTstack's non-CMake build system
- ✅ Provide helpful warnings and next steps

---

## 🎯 WORKING CONFIGURATION (Tested)

### For IP-Only Accessories (Recommended to Start):

```powershell
# 1. Setup
.\Scripts\Setup-Environment.ps1

# 2. Build
start HomeKitADK.sln
# Build → Rebuild Solution

# 3. Run
.\build\vs2026\bin\Debug\Lightbulb.exe
```

**Features available:**
- ✅ TCP/IP networking
- ✅ mDNS service discovery
- ✅ Pairing with setup codes
- ✅ Full HomeKit protocol
- ✅ Works with iOS Home app

**This configuration is FULLY FUNCTIONAL!**

---

### To Add NFC + QR Codes:

```powershell
# Build libnfc and qrencode
.\Scripts\Build-Dependencies.ps1 -Libraries "libnfc,qrencode"

# Rebuild HomeKitADK with:
# - HAVE_NFC=1
# - HAVE_QR_CODE=1
```

---

## 📝 Library Status

| Library | Build Method | Status | Required For |
|---------|--------------|--------|--------------|
| OpenSSL | vcpkg | ✅ Working | Core crypto (required) |
| Bonjour SDK | Manual install | ✅ Working | mDNS (required) |
| libnfc | Build-Dependencies.ps1 | ✅ Fixed | NFC pairing (optional) |
| libqrencode | Build-Dependencies.ps1 | ✅ Fixed | QR code generation (optional) |
| BTstack | Manual build | ⚠️ Complex | BLE support (optional) |

---

## 🎁 BOTTOM LINE

**YOU ALREADY HAVE A WORKING BUILD!**

The HomeKit ADK compiles and runs on Windows with:
- ✅ Full IP protocol support
- ✅ mDNS service discovery
- ✅ HomeKit pairing
- ✅ Accessory control

**Optional features** (BLE, NFC, QR codes) can be added later if needed.

**Start with what works, add features incrementally!**

---

## 🚀 QUICK START (What Actually Works Now)

```powershell
# 1. Ensure environment is set
$env:VCPKG_ROOT
$env:BONJOUR_SDK
# If not set: .\Scripts\Setup-Environment.ps1

# 2. Open and build
start HomeKitADK.sln
# Build → Rebuild Solution (Ctrl+Shift+B)

# 3. Provision
.\Scripts\Provision-Lightbulb.ps1 -SetupCode "123-45-678"

# 4. Run
.\build\vs2026\bin\Debug\Lightbulb.exe

# 5. Pair with iOS
# Home app → Add Accessory → Enter code: 123-45-678
```

**This works RIGHT NOW!** 🎉

---

**Updated:** November 15, 2025
**Status:** Base build WORKS, optional dependencies need attention
**Recommendation:** Start with IP-only, add features later
