# Sprite One - Getting Started Guide

Welcome to Sprite One! This guide will help you get up and running quickly.

---

## Table of Contents

1. [Hardware Setup](#hardware-setup)
2. [Firmware Upload](#firmware-upload)
3. [Host Library Installation](#host-library-installation)
4. [First Program](#first-program)
5. [Next Steps](#next-steps)

---

## Hardware Setup

### Requirements

- Sprite One board (RP2040-based)
- USB-C cable for programming
- Host device (PC, ESP32, Arduino, etc.)
- 3 jumper wires for UART connection

**Note:** Current firmware simulates graphics output to serial. No physical display is connected yet.

### Connections

Connect Sprite One to your host device via UART:

```
Sprite One (RP2040)    →    Host Device
─────────────────────────────────────────
TX  (GPIO 0)           →    RX
RX  (GPIO 1)           →    TX  
GND                    →    GND
3.3V                   →    3.3V (optional)
```

**Baudrate:** 115200

---

## Firmware Upload

### Option 1: Arduino IDE

1. Install Arduino IDE 2.0+
2. Add RP2040 board support:
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
   - Tools → Board → Board Manager → Install "Raspberry Pi Pico/RP2040"

3. Open firmware:
   ```
   examples/sprite_one_unified/sprite_one_unified.ino
   ```

4. Select board:
   - Tools → Board → Raspberry Pi RP2040 Boards → Raspberry Pi Pico

5. Upload:
   - Connect USB-C cable
   - Click Upload

### Option 2: Arduino CLI

```bash
# Install RP2040 support
arduino-cli core install rp2040:rp2040

# Compile
cd firmware
arduino-cli compile --fqbn rp2040:rp2040:rpipico

# Upload
arduino-cli upload -p COM3 --fqbn rp2040:rp2040:rpipico
```

### Option 3: PlatformIO

```bash
cd firmware
pio run --target upload
```

---

## Host Library Installation

### Python

```bash
cd host/python
pip install -r requirements.txt
```

Requirements:
- Python 3.7+
- pyserial

### C/C++ (Embedded)

Copy files to your project:
```
host/c/sprite_one.h
host/c/sprite_one.c
```

See `host/README.md` for integration details.

---

## First Program

### Python

Create `test.py`:

```python
from sprite_one import SpriteOne

# Connect to Sprite One
with SpriteOne('COM3') as sprite:  # Change port as needed
    # Get version
    version = sprite.get_version()
    print(f"Firmware: v{version[0]}.{version[1]}.{version[2]}")
    
    # Draw graphics
    sprite.clear()
    sprite.rect(10, 10, 50, 30, color=1)
    sprite.text(5, 5, "HELLO")
    sprite.flush()
    print("Graphics drawn!")
    
    # Train AI model
    print("Training AI...")
    loss = sprite.ai_train(epochs=100)
    print(f"Training complete! Loss: {loss:.6f}")
    
    # Test XOR
    result = sprite.ai_infer(1.0, 0.0)
    print(f"1 XOR 0 = {result:.3f}")
```

Run it:
```bash
python test.py
```

### Arduino (Host)

```cpp
// Coming soon - Arduino host library in development
```

### ESP32 (Host)

```c
#include "sprite_one.h"

// UART functions
void esp_write(uint8_t b) { Serial2.write(b); }
uint8_t esp_read(void) { return Serial2.read(); }
bool esp_available(void) { return Serial2.available() > 0; }

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 16, 17);  // RX=16, TX=17
    
    // Initialize Sprite One
    sprite_context_t sprite;
    sprite_init(&sprite, esp_write, esp_read, esp_available, 2000);
    
    // Get version
    uint8_t major, minor, patch;
    if (sprite_get_version(&sprite, &major, &minor, &patch)) {
        Serial.printf("Firmware: v%d.%d.%d\n", major, minor, patch);
    }
    
    // Train AI
    float loss;
    sprite_ai_train(&sprite, 100, &loss);
    Serial.printf("Trained! Loss: %f\n", loss);
    
    // Run inference
    float result;
    sprite_ai_infer(&sprite, 1.0, 0.0, &result);
    Serial.printf("1 XOR 0 = %f\n", result);
}

void loop() {
    delay(1000);
}
```

---

## Expected Output

After running the first program, you should see:

```
Firmware: v1.1.0
Graphics drawn!  # Note: Framebuffer logged to serial, not displayed
Training AI...
Training complete! Loss: 0.012345
1 XOR 0 = 0.978
```

On Sprite One's serial output (115200 baud):
```
╔════════════════════════════════════════╗
║     SPRITE ONE v1.1.0                  ║
║     Graphics & AI Accelerator          ║
╚════════════════════════════════════════╝

[INFO] System Information:
  CPU: RP2040 @ 133MHz
  Flash: 2MB
  RAM: 256KB

[INFO] Initializing LittleFS...
✓ LittleFS mounted

[INFO] AI Engine ready
Ready for commands!

[FLUSH] Dirty region: 8192 pixels  # Full screen
```

**Note:** Graphics commands are processed but framebuffer output is currently simulated via serial logging.

---

## Troubleshooting

### "No such file or directory: 'COM3'"

**Problem:** Wrong serial port

**Solution:** 
- Windows: Check Device Manager (usually COM3, COM4, etc.)
- Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
- Mac: `/dev/cu.usbmodem*`

### "Timeout waiting for response"

**Problem:** UART not connected or wrong baud rate

**Solutions:**
1. Check UART connections (TX↔RX, RX↔TX, GND↔GND)
2. Verify 115200 baud on both sides
3. Check if Sprite One is powered
4. Try different USB cable

### "Model not loaded" error

**Problem:** No model in flash memory

**Solution:**
```python
# Train and save a model first
sprite.ai_train(100)
sprite.ai_save("/model.aif32")
```

### Compilation errors

**Problem:** Missing libraries

**Solution:**
- Arduino: Install "AIfES for Arduino" from Library Manager
- PlatformIO: Libraries auto-installed from `platformio.ini`

---

## Next Steps

### Learn More

- **[API Reference](API.md)** - Complete API documentation
- **[Examples](../examples/)** - More code examples
- **[Protocol](PROTOCOL.md)** - Technical protocol details

### Try Advanced Features

1. **Q7 Quantization** - Reduce model size by 75%
   ```python
   # Coming soon in examples
   ```

2. **Model Persistence** - Save/load trained models
   ```python
   sprite.ai_save("/my_model.aif32")
   sprite.ai_load("/my_model.aif32")
   ```

3. **Multiple Models** - Switch between different models
   ```python
   sprite.ai_list_models()
   # ['/xor.aif32', '/gesture.aif32', '/pattern.aif32']
   ```

### Join Community

- GitHub Discussions
- Discord (coming soon)
- Share your projects!

---

## Quick Reference

### Python API

```python
# System
sprite.get_version()

# Graphics
sprite.clear(color=0)
sprite.pixel(x, y, color=1)
sprite.rect(x, y, w, h, color=1)
sprite.text(x, y, "text", color=1)
sprite.flush()

# AI
sprite.ai_train(epochs=100)
sprite.ai_infer(input0, input1)
sprite.ai_status()
sprite.ai_save(filename)
sprite.ai_load(filename)
sprite.ai_list_models()
```

### Serial Monitor

Open serial monitor (115200 baud) on Sprite One for debug output.

---

**You're all set! Happy coding with Sprite One!** 🚀
