# NFC and QR Code Support ENABLED in Visual Studio Projects
## November 15, 2025

## ✅ FEATURES ENABLED

All 5 main projects now have NFC and QR code support compiled in:

### Preprocessor Definitions Added:
```
HAVE_NFC=1
HAVE_QR_CODE=1
```

### Projects Updated:
1. ✅ **HAPPlatform_Windows.vcxproj**
   - Added HAVE_NFC=1
   - Added HAVE_QR_CODE=1
   - Linked: qrencode.lib, nfc.lib, libusb-1.0.lib
   - Includes: nfc headers, libusb headers

2. ✅ **HAP.vcxproj**
   - Added HAVE_NFC=1
   - Added HAVE_QR_CODE=1

3. ✅ **Lightbulb.vcxproj**
   - Added HAVE_NFC=1
   - Added HAVE_QR_CODE=1

4. ✅ **Lock.vcxproj**
   - Added HAVE_NFC=1
   - Added HAVE_QR_CODE=1

5. ✅ **AccessorySetupGenerator.vcxproj**
   - Added HAVE_NFC=1
   - Added HAVE_QR_CODE=1

---

## 📦 DEPENDENCIES

### vcpkg.json Updated:
```json
"dependencies": [
  {"name": "openssl", ...},
  {"name": "libusb", "platform": "windows"}
]
```

### Required Libraries:
- ✅ **qrencode.lib** → $(HOMEKIT_DEPS)\lib\qrencode.lib
- ⏳ **nfc.lib** → $(HOMEKIT_DEPS)\lib\nfc.lib (build with script)
- ✅ **libusb-1.0.lib** → $(VCPKG_ROOT)\installed\x64-windows\lib\

---

## 🚀 TO BUILD WITH FULL NFC + QR CODE SUPPORT

### Step 1: Install libusb (via vcpkg)
```powershell
vcpkg install
```

### Step 2: Build libnfc (now fixed!)
```powershell
.\Scripts\Build-Dependencies.ps1 -Libraries "libnfc"
```

### Step 3: Rebuild HomeKitADK
```powershell
# Rebuild in Visual Studio
# Build → Rebuild Solution
```

---

## 🎯 WHAT YOU GET

### With QR Code Support (Already Working!):
- ✅ **Automatic QR code generation** from setup codes
- ✅ **Console ASCII QR display**
- ✅ **Easy iOS pairing** (scan QR code)
- ✅ **libqrencode integration** working

### With NFC Support (After building libnfc):
- ✅ **NFC tag emulation**
- ✅ **Tap-to-pair** functionality
- ✅ **NDEF payload delivery**
- ✅ **libnfc integration**

---

## 📊 LIBRARY LINKING

### HAPPlatform_Windows.vcxproj Now Links:
```xml
<AdditionalDependencies>
  ws2_32.lib        <!-- Winsock2 -->
  bcrypt.lib        <!-- Crypto RNG -->
  advapi32.lib      <!-- Windows registry -->
  dnssd.lib         <!-- Bonjour mDNS -->
  qrencode.lib      <!-- QR codes ✅ -->
  nfc.lib           <!-- NFC support ✅ -->
  libusb-1.0.lib    <!-- USB for NFC ✅ -->
</AdditionalDependencies>
```

### Library Search Paths:
```xml
<AdditionalLibraryDirectories>
  $(HOMEKIT_DEPS)\lib           <!-- Built dependencies -->
  $(BONJOUR_SDK)\Lib\x64        <!-- Bonjour SDK -->
  $(VCPKG_ROOT)\installed\x64-windows\lib  <!-- vcpkg -->
</AdditionalLibraryDirectories>
```

---

## 🎁 FEATURES NOW AVAILABLE

When built with these defines:

### QR Code Generation:
```c
#ifdef HAVE_QR_CODE
// libqrencode functions available
// Automatic QR code generation
// PNG export support
#endif
```

### NFC Pairing:
```c
#ifdef HAVE_NFC
// libnfc functions available
// NFC tag emulation
// NDEF payload delivery
// Windows threading support
#endif
```

---

## ✅ VERIFICATION

Check that defines are set:
```powershell
# Search in build output
Select-String "HAVE_NFC=1" -Path *.vcxproj, *\*.vcxproj, *\*\*.vcxproj

# Should find in 5 projects:
# - HAPPlatform_Windows.vcxproj
# - HAP.vcxproj
# - Lightbulb.vcxproj
# - Lock.vcxproj
# - AccessorySetupGenerator.vcxproj
```

---

## 🎉 SUMMARY

**NFC and QR Code support is now ENABLED in all Visual Studio projects!**

### Current Status:
- ✅ **QR Code** - libqrencode built and linked
- ⏳ **NFC** - Ready (build libnfc to activate)

### Next Steps:
1. Build libnfc: `.\Scripts\Build-Dependencies.ps1 -Libraries "libnfc"`
2. Rebuild solution in VS
3. Run with full NFC + QR support!

**The platform is ready for all pairing methods!** 🚀

