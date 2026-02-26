# Sprite One

RP2040-based coprocessor for graphics rendering and neural network inference/training, controlled via UART or USB-CDC.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-RP2040-orange.svg)](https://www.raspberrypi.com/products/rp2040/)
[![Version](https://img.shields.io/badge/version-2.2-green.svg)](CHANGELOG.md)

## What It Does

A host MCU (or PC) sends binary commands over UART or USB-CDC. Sprite One handles:

- 128x64 OLED display (SSD1306) with a software framebuffer and 8 hardware sprite slots
- Neural network inference — load any `.aif32` V3 model, run forward passes
- On-device training — backpropagation via AIfES (Adam + MSE), one step per command
- Incremental fine-tuning via `FINETUNE_START` / `FINETUNE_DATA` / `FINETUNE_STOP`
- LittleFS flash storage for model files
- Dual transport: UART (115200 baud) and USB-CDC
- Industrial signal processing primitives: device identity, rolling buffer, baseline delta, cross-correlation

## Hardware

```
Sprite One (RP2040/Pico)      Host
────────────────────────      ────────────────
GP0 (UART TX)          →      RX
GP1 (UART RX)          ←      TX
GND                    ─      GND
USB                    →      USB (CDC, faster)

GP4 (I2C SDA)          →      SSD1306 SDA  (optional display)
GP5 (I2C SCL)          →      SSD1306 SCL
```

## Firmware Upload

```bash
# Arduino CLI
cd firmware
arduino-cli core install rp2040:rp2040
arduino-cli compile --fqbn rp2040:rp2040:rpipico ../examples/sprite_one_unified
arduino-cli upload -p COM3 --fqbn rp2040:rp2040:rpipico

# Arduino IDE
# Board: Raspberry Pi Pico (from earlephilhower/arduino-pico)
# Open: examples/sprite_one_unified/sprite_one_unified.ino
```

Dependencies: AIfES for Arduino, LittleFS, Wire (for SSD1306).

## Python Quick Start

```bash
cd host/python
pip install pyserial
```

```python
from sprite_one import SpriteOne

with SpriteOne('COM3') as sprite:
    print(sprite.get_version())          # (2, 2, 0)

    # Upload a model and run inference
    with open("sentinel_god_v3.aif32", "rb") as f:
        sprite.model_upload("sentinel_god_v3.aif32", f.read())
    sprite.model_select("sentinel_god_v3.aif32")
    result = sprite.ai_infer(1.0, 0.0)

    # Incremental fine-tune (one training step per call)
    sprite.finetune_start(learning_rate=0.01)
    loss = sprite.finetune_data(inputs=[0.0, 1.0], targets=[1.0])
    sprite.finetune_stop()
```

## Web Trainer

Open `webapp/index.html` in Chrome or Edge.

- Toggle "Mock" in the header to simulate a device in-browser (no hardware needed)
- Click "Refresh" to scan for a real USB device
- Upload `.aif32` models, run inference, collect training samples, deploy

Requires WebSerial API — Chrome and Edge only. Firefox and Safari are not supported.

## Model Conversion

Convert a TFLite or Keras model to `.aif32`:

```bash
python tools/converter/convert.py model.tflite -o output.aif32
python tools/converter/convert.py model.h5 -o output.aif32
```

Generate reference models from scratch:

```bash
python tools/gen_sentinel_model.py
```

This writes `sentinel_god_v3.aif32`, `vision_demo.aif32`, and several test variants to the project root.

## Hardware Verification

Run the verification script against a connected device:

```bash
py host/python/verify_hardware.py COM3
```

For mock-mode testing (no hardware):

```bash
py tools/mock_device.py --loopback        # Protocol + upload
py tools/mock_device.py --test-api        # Industrial primitives
```

## Project Structure

```
sprite-one/
├── examples/sprite_one_unified/   # Main firmware (.ino + headers)
│   ├── sprite_one_unified.ino
│   ├── sprite_dynamic.h           # V3 dynamic model loader + trainer
│   ├── sprite_model.h             # Model management (upload, select)
│   ├── sprite_inference.h         # Legacy INT8 inference path
│   ├── sprite_transport.h         # UART/USB-CDC abstraction
│   ├── sprite_display.h           # SSD1306 driver
│   └── sprite_engine.h            # Sprite system
├── firmware/                      # PlatformIO config, shared headers
├── host/
│   ├── python/                    # Python library and test scripts
│   └── c/                         # C library for embedded hosts
├── webapp/                        # Browser-based training UI
│   ├── index.html
│   ├── style.css
│   └── js/
├── tools/
│   ├── converter/                 # TFLite / Keras to .aif32
│   ├── mock_device.py             # Python protocol simulator
│   └── gen_sentinel_model.py      # Reference model generator
└── docs/                          # Protocol spec, API reference
```

## Specifications

| | Value |
|---|---|
| MCU | RP2040 (133 MHz, 264 KB RAM) |
| Transport | UART 115200 baud or USB-CDC |
| Display | SSD1306 128x64, 1-bit |
| Hardware sprites | 8 slots |
| Model format | `.aif32` V3 (dynamic layer table) |
| On-device training | Adam + MSE, Dense/ReLU/Sigmoid layers |
| Industrial buffer | 60 float32 samples, circular |

## Known Limitations

- **Display:** Software renderer only — no DMA or hardware acceleration.
- **AI training:** Backprop supports Dense, ReLU, and Sigmoid layers. Conv2D is inference-only.
- **Protocol:** No flow control. For chunked uploads the host must wait for a per-chunk ACK before sending the next chunk.
- **WebSerial:** Chrome and Edge only.
- **Fine-tuning session state:** The optimizer is stateless between `FINETUNE_STOP` and the next `FINETUNE_START`. Accumulated momentum is lost if the session is interrupted.

## Documentation

- [Getting Started](docs/GETTING_STARTED.md)
- [API Reference](docs/API.md)
- [Protocol Specification](docs/protocol.md)
- [Changelog](CHANGELOG.md)

## License

MIT — see [LICENSE](LICENSE).

## Credits

- [AIfES](https://github.com/Fraunhofer-IMS/AIfES_for_Arduino) — embedded ML framework
- [LittleFS](https://github.com/littlefs-project/littlefs) — embedded filesystem
- [earlephilhower/arduino-pico](https://github.com/earlephilhower/arduino-pico) — RP2040 Arduino core