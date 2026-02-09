# Getting Started

## Requirements

**Hardware:**
- Raspberry Pi Pico (RP2040)
- USB cable
- *(Optional)* SSD1306 OLED display (128x64, I2C)

**Software:**
- Python 3.7+
- Arduino IDE 2.0+ or PlatformIO
- Chrome/Edge (for web trainer - needs WebSerial)

---

## Hardware Connections

### USB-CDC (Default)
Connect Pico via USB. No other wiring needed.
- Windows: `COM3`, `COM4`, etc.
- Linux: `/dev/ttyACM0`
- macOS: `/dev/cu.usbmodem*`

### UART (Optional)
```
Pico       →  Host
GP0 (TX)   →  RX
GP1 (RX)   →  TX
GND        →  GND
```

### Display (Optional)
```
Pico       →  SSD1306
GP4 (SDA)  →  SDA
GP5 (SCL)  →  SCL
3.3V       →  VCC
GND        →  GND
```

---

## Firmware Upload

### Arduino CLI
```bash
arduino-cli core install rp2040:rp2040
cd examples/sprite_one_unified
arduino-cli compile --fqbn rp2040:rp2040:rpipico
arduino-cli upload -p COM3 --fqbn rp2040:rp2040:rpipico
```

### Arduino IDE
1. Add board URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. Install "Raspberry Pi Pico/RP2040" from Board Manager
3. Open `examples/sprite_one_unified/sprite_one_unified.ino`
4. Upload

---

## Python Library

```bash
cd host/python
pip install pyserial
```

### Basic Usage
```python
from sprite_one import SpriteOne

with SpriteOne('COM3') as sprite:
    # Version
    v = sprite.get_version()
    print(f"v{v[0]}.{v[1]}.{v[2]}")
    
    # Train
    loss = sprite.ai_train(epochs=100)
    print(f"Loss: {loss:.4f}")
    
    # Inference
    result = sprite.ai_infer(1.0, 0.0)
    print(f"1 XOR 0 = {result:.3f}")
```

---

## Web Trainer

Open `webapp/index.html` in Chrome/Edge.

1. Click **Refresh** to scan for devices
2. Add training samples via webcam/files/drawing
3. Click **Train**
4. Click **Deploy** to upload to device

### Import TFLite Model
1. Go to **Import** tab
2. Drop `.tflite` file
3. Click **Convert & Deploy**

---

## Model Conversion (CLI)

```bash
cd tools/converter

# TFLite → AIFes
py convert.py model.tflite -o output.aif32

# Keras → AIFes (requires TensorFlow)
py convert.py model.h5 -o output.aif32
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Port not found | Check Device Manager (Windows) or `ls /dev/tty*` (Linux) |
| Timeout | Verify USB cable, try different port |
| Model not loaded | Train or load a model first: `sprite.ai_train(100)` |
| WebSerial not available | Use Chrome/Edge, not Firefox/Safari |

---

## Next Steps

- [API Reference](API.md) - All commands
- [Protocol Spec](protocol.md) - Binary protocol details
- [Converter Docs](../tools/converter/README.md) - TFLite conversion
