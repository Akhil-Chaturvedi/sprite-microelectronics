# Sprite One - Build Configurations
Week 4 Day 26

Three optimized build configurations for different use cases.

## Configuration Files

### 1. Debug Build (Development)

**File:** `platformio_debug.ini`

```ini
[env:sprite_one_debug]
platform = raspberrypi
board = pico
framework = arduino

lib_deps = 
    AIfES
    LittleFS

build_flags = 
    -DSPRITE_DEBUG_ENABLED=true
    -DSPRITE_VERBOSE=true
    -DENABLE_PROGRESS_BARS=true
    -O2                          # Optimize for speed
    -g                           # Include debug symbols

monitor_speed = 115200
```

**Use for:** Development, testing, debugging

### 2. Release Build (Production)

**File:** `platformio_release.ini`

```ini
[env:sprite_one_release]
platform = raspberrypi
board = pico
framework = arduino

lib_deps = 
    AIfES
    LittleFS

build_flags = 
    -DSPRITE_DEBUG_ENABLED=false
    -DSPRITE_VERBOSE=false
    -DENABLE_PROGRESS_BARS=false
    -Os                          # Optimize for size
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections           # Remove unused code
    -flto                        # Link-time optimization
    -DNDEBUG                     # Disable assertions

monitor_speed = 115200
```

**Use for:** Production deployment, end products

### 3. Minimal Build (Constrained)

**File:** `platformio_minimal.ini`

```ini
[env:sprite_one_minimal]
platform = raspberrypi
board = pico
framework = arduino

lib_deps = 
    AIfES
    LittleFS

build_flags = 
    -DSPRITE_DEBUG_ENABLED=false
    -DSPRITE_VERBOSE=false
    -DENABLE_PROGRESS_BARS=false
    -DAI_MAX_EPOCHS=50           # Reduce max training
    -DPROTOCOL_MAX_PAYLOAD=128   # Smaller buffers
    -Os
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections
    -flto
    -DNDEBUG

monitor_speed = 115200
```

**Use for:** Memory-constrained applications

---

## Build Commands

### Using PlatformIO

```bash
# Debug build
pio run -e sprite_one_debug

# Release build
pio run -e sprite_one_release

# Minimal build
pio run -e sprite_one_minimal

# Upload
pio run -e sprite_one_release --target upload
```

### Using Arduino CLI

```bash
# Debug build (default, no special flags)
arduino-cli compile --fqbn rp2040:rp2040:rpipico

# Release build
arduino-cli compile --fqbn rp2040:rp2040:rpipico \
    --build-property "compiler.c.extra_flags=-Os -flto" \
    --build-property "compiler.cpp.extra_flags=-Os -flto"
```

---

## Expected Memory Usage

| Configuration | Flash | RAM | Features |
|---------------|-------|-----|----------|
| Debug | 109KB | 12.5KB | Full debugging |
| Release | 105KB | 12.4KB | Optimized |
| Minimal | 100KB | 10.5KB | Core only |

---

## Feature Comparison

| Feature | Debug | Release | Minimal |
|---------|-------|---------|---------|
| AI Training | ✅ | ✅ | ✅ (limited) |
| AI Inference | ✅ | ✅ | ✅ |
| Model Persistence | ✅ | ✅ | ✅ |
| Graphics | ✅ | ✅ | ✅ |
| Protocol | ✅ | ✅ | ✅ |
| Debug Logging | ✅ | ❌ | ❌ |
| Progress Bars | ✅ | ❌ | ❌ |
| Verbose Output | ✅ | ❌ | ❌ |
| Assertions | ✅ | ❌ | ❌ |

---

## Switching Configurations

### Option 1: Conditional Compilation

In code:
```cpp
#ifndef SPRITE_DEBUG_ENABLED
  #define SPRITE_DEBUG_ENABLED false
#endif

#if SPRITE_DEBUG_ENABLED
  #define LOG_INFO(msg) Serial1.println(msg)
#else
  #define LOG_INFO(msg) // No-op
#endif
```

### Option 2: Separate Sketch Files

- `sprite_one_debug.ino` - Full features
- `sprite_one_release.ino` - Optimized
- `sprite_one_minimal.ino` - Minimal

### Option 3: PlatformIO Environments

Use `platformio.ini` with multiple environments (recommended).

---

## Recommendations

**Development:** Use Debug build
- Full visibility into system state
- Easy debugging
- Performance is adequate

**Production:** Use Release build
- 4KB flash savings
- Cleaner serial output
- Still has all features

**Constrained:** Use Minimal build only if needed
- Absolute minimum footprint
- Some features limited
- Still fully functional

---

## Performance Impact

### Compile Times

| Config | First Compile | Rebuild |
|--------|---------------|---------|
| Debug | ~45s | ~10s |
| Release | ~55s | ~15s |
| Minimal | ~50s | ~12s |

LTO adds ~10s to compile time but reduces flash by ~2-3KB.

### Runtime Performance

| Config | Training (100 epochs) | Inference |
|--------|----------------------|-----------|
| Debug | ~3000ms | ~0.5ms |
| Release | ~2700ms | ~0.4ms |
| Minimal | ~2700ms | ~0.4ms |

Release build is ~10% faster due to optimizations.

---

## Build Script Example

**`build.sh`** (Linux/Mac):
```bash
#!/bin/bash

echo "Building all Sprite One configurations..."

echo "1. Debug build..."
pio run -e sprite_one_debug

echo "2. Release build..."
pio run -e sprite_one_release

echo "3. Minimal build..."
pio run -e sprite_one_minimal

echo "Done! Binaries in .pio/build/"
```

**`build.bat`** (Windows):
```batch
@echo off
echo Building all Sprite One configurations...

echo 1. Debug build...
pio run -e sprite_one_debug

echo 2. Release build...
pio run -e sprite_one_release

echo 3. Minimal build...
pio run -e sprite_one_minimal

echo Done! Binaries in .pio\build\
```
