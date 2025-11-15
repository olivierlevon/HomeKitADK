# GitHub PR Fixes Applied - November 15, 2025
## Critical Bug Fixes from Upstream

## ✅ PR #126 - TLVReader Integer Parsing Fix (APPLIED)

### Problem
**Undefined behavior in bit-shift operations causing integer overflow:**
- `uint64_t`: Shifting `255 << 32` exceeded type capacity
- `int32_t`: Shifting `255 << 24` caused overflow
- Result: Incomplete parsing (uint64 only parsed 32 bits, int32 behaved as "int31")
- Address Sanitizer flagged these as errors

### Fix Applied
**File:** HAP/HAPTLVReader.c

**Change:** Updated `PROCESS_INTEGER_FORMAT` macro (line 979, 995)
- Added `intermediateTypeName` parameter to macro
- Cast byte to intermediate type BEFORE shifting:
  ```c
  // Before (BROKEN):
  *value |= (typeName)(((const uint8_t*) bytes)[i] << (i * CHAR_BIT));
  
  // After (FIXED):
  *value |= (typeName)(((intermediateTypeName)((const uint8_t*) bytes)[i]) << (i * CHAR_BIT));
  ```

**Updated calls:**
- `UInt8`: intermediate type = `unsigned int`
- `UInt16`: intermediate type = `unsigned int`
- `UInt32`: intermediate type = `unsigned long`
- `UInt64`: intermediate type = `uint64_t` ← Critical fix\!
- `Int8`: intermediate type = `int`
- `Int16`: intermediate type = `int`
- `Int32`: intermediate type = `long`
- `Int64`: intermediate type = `int64_t` ← Critical fix\!

**Impact:** ✅ Fixes integer parsing bugs, prevents ASAN errors, ensures full 64-bit value parsing

---

## ✅ PR #117 - IP Camera Characteristics (APPLIED)

### Problem
Missing HomeKit characteristics for IP camera accessories

### Fix Applied
**File:** HAP/HAPCharacteristicTypes.h

**Added 8 new characteristics** for IP camera support:

1. **Streaming Status** (0x120)
   - TLV format, Paired Read/Notify
   - Values: Available, In Use, Unavailable

2. **Supported Audio Stream Configuration** (0x115)
   - TLV format, Paired Read
   - Codec configuration for microphone/speaker

3. **Supported Data Stream Transport Configuration** (0x130)
   - TLV format, Paired Read
   - HomeKit Data Stream over TCP (iOS 12+)

4. **Supported RTP Configuration** (0x116)
   - TLV format, Paired Read
   - SRTP crypto suites (AES variants)

5. **Supported Video Stream Configuration** (0x114)
   - TLV format, Paired Read
   - H.264 codec parameters

6. **Selected RTP Stream Configuration** (0x117)
   - TLV8 format, Paired Read/Write
   - Active stream control

7. **Setup Endpoints** (0x118)
   - TLV8 format, Paired Read/Write
   - IP/Port exchange for streaming

8. **Mute** (0x11A)
   - Bool format, Paired Read/Write/Notify
   - Audio control

**Impact:** ✅ Enables IP camera accessory development with HomeKit ADK

**Note:** UUIDs already existed in HAPCharacteristicTypes.c - only header declarations were missing\!

---

## 📊 SUMMARY

| PR | Files Modified | Lines Changed | Status |
|----|----------------|---------------|--------|
| #126 | HAP/HAPTLVReader.c | 1 macro + 8 calls | ✅ Applied |
| #117 | HAP/HAPCharacteristicTypes.h | +166 lines | ✅ Applied |

**Total:** 2 files, ~180 lines changed/added

---

## ✅ BENEFITS

### PR #126 Benefits:
- ✅ Fixes critical parsing bugs
- ✅ Prevents undefined behavior
- ✅ Eliminates ASAN warnings
- ✅ Ensures correct 64-bit value parsing

### PR #117 Benefits:
- ✅ Enables IP camera accessories
- ✅ Adds video/audio streaming support
- ✅ HomeKit Data Stream support
- ✅ RTP stream management
- ✅ Better spec compliance

---

## 🚀 READY TO BUILD

All PR fixes applied\! These fix:
- Critical TLV parsing bugs (PR #126)
- Missing IP camera support (PR #117)

**Rebuild now to test\!**

