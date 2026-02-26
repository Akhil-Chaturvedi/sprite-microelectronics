# Getting Started

## Requirements

**Hardware:**
- Raspberry Pi Pico (RP2040)
- USB cable
- (Optional) SSD1306 OLED display (128x64, I2C)

**Software:**
- Python 3.7+
- Arduino IDE 2.0+ or Arduino CLI
- Chrome or Edge (for the web trainer — requires WebSerial API)

---

## Hardware Connections

### USB-CDC (default)

Connect the Pico directly via USB. No extra wiring needed.

- Windows: shows as `COM3`, `COM4`, etc.
- Linux: `/dev/ttyACM0`
- macOS: `/dev/cu.usbmodem*`

### UART (optional)

```
Pico GP0 (TX)  →  Host RX
Pico GP1 (RX)  ←  Host TX
Pico GND       ─  Host GND
```

115200 baud, 8N1.

### OLED Display (optional)

```
Pico GP4 (SDA)  →  SSD1306 SDA
Pico GP5 (SCL)  →  SSD1306 SCL
Pico 3.3V       →  SSD1306 VCC
Pico GND        →  SSD1306 GND
```

---

## Firmware Upload

### Arduino CLI

```bash
arduino-cli core install rp2040:rp2040
cd examples/sprite_one_unified
arduino-cli compile --fqbn rp2040:rp2040:rpipico .
arduino-cli upload -p COM3 --fqbn rp2040:rp2040:rpipico .
```

### Arduino IDE

1. Add board manager URL: `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
2. Install **Raspberry Pi Pico/RP2040** from Board Manager
3. Install libraries: **AIfES for Arduino**, **LittleFS**
4. Open `examples/sprite_one_unified/sprite_one_unified.ino`
5. Select board: `Raspberry Pi Pico`, port: your COM port
6. Upload

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
    print(sprite.get_version())   # (2, 2, 0)

    # Upload a model and run inference
    with open("sentinel_god_v3.aif32", "rb") as f:
        sprite.model_upload("sentinel_god_v3.aif32", f.read())
    sprite.model_select("sentinel_god_v3.aif32")

    info = sprite.model_info()
    print(info)   # {'name': 'sentinel_god_v3', 'input_size': 128, ...}

    # Graphics
    sprite.clear()
    sprite.text(0, 0, "Hello")
    sprite.flush()
```

### On-Device Training

```python
with SpriteOne('COM3') as sprite:
    # Load the model to train
    sprite.model_select("xor.aif32")

    # Init Adam optimizer
    sprite.finetune_start(learning_rate=0.01)

    # Run training steps (one sample at a time)
    for epoch in range(500):
        sprite.finetune_data([0.0, 1.0], [1.0])
        sprite.finetune_data([1.0, 0.0], [1.0])
        sprite.finetune_data([0.0, 0.0], [0.0])
        sprite.finetune_data([1.0, 1.0], [0.0])

    sprite.finetune_stop()
```

### Industrial Primitives

```python
with SpriteOne('COM3') as sprite:
    # Board identity
    dev_id = sprite.get_device_id()
    assert sprite.ping_id(dev_id)

    # Rolling buffer and delta
    for sample in sensor_readings:
        sprite.buffer_write(sample)

    sprite.baseline_capture()
    # ... acquire new readings ...
    delta = sprite.get_delta()

    # Pattern match
    score = sprite.correlate(reference_pattern)
```

---

## Web Trainer

Open `webapp/index.html` in Chrome or Edge.

**Without hardware (Mock Mode):**
1. Toggle **Mock** in the top-right header
2. The simulated device appears in the Devices panel
3. Upload models, run inference, and train — all in the browser

**With hardware:**
1. Plug in the Pico via USB
2. Click **Refresh** to scan for devices
3. Select the device, then upload models or start training

### Importing a TFLite Model

1. Go to the **Import** tab
2. Drop a `.tflite` or `.h5` file
3. Review the converted architecture
4. Click **Convert & Deploy**

---

## Model Conversion (CLI)

```bash
# TFLite to .aif32
python tools/converter/convert.py model.tflite -o output.aif32

# Keras to .aif32 (requires TensorFlow)
python tools/converter/convert.py model.h5 -o output.aif32

# Generate reference models (xor, sentinel, vision demo, test models)
python tools/gen_sentinel_model.py
```

---

## Verification

Run the built-in mock tests to verify the protocol stack without hardware:

```bash
py tools/mock_device.py --loopback     # CRC + chunked upload
py tools/mock_device.py --test-api     # Industrial primitives (0xA0-0xA7)
```

Run against real hardware:

```bash
py host/python/verify_hardware.py COM3
```

---

## Troubleshooting

| Problem | Check |
|---|---|
| Port not found | Device Manager (Windows) or `ls /dev/tty*` (Linux/macOS) |
| Timeout on command | Try USB-CDC mode if using UART; check for a stalled firmware upload |
| `SpriteOneError: no model` | Call `model_select()` or upload a model first |
| WebSerial not available | Use Chrome or Edge — Firefox and Safari do not support WebSerial |
| Mock device not showing | Hard-refresh the page (Ctrl+Shift+R) then toggle Mock again |
| Large model upload fails | Check LittleFS free space; delete unused models first |

---

## Next Steps

- [API Reference](API.md) — all commands and their parameters
- [Protocol Specification](protocol.md) — packet format, opcodes, `.aif32` file format
- [Model Converter](../tools/converter/) — TFLite and Keras conversion tools
