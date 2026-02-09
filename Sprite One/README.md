# Sprite One

Graphics and AI accelerator for embedded systems.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-RP2040-orange.svg)](https://www.raspberrypi.com/products/rp2040/)
[![Version](https://img.shields.io/badge/version-2.0-green.svg)]()

## What It Does

Sprite One is an RP2040-based coprocessor that handles graphics rendering and neural network operations via UART/USB-CDC. Designed for microcontrollers that need to offload computationally intensive tasks.

**Capabilities:**
- 128x64 OLED display (SSD1306) with hardware sprites
- On-device neural network training (F32 and Q7)
- Hot-swap multi-model system
- TFLite model converter
- Web-based training interface
- Dual transport: UART + USB-CDC (100x faster)

## Quick Start

### Hardware

```
Sprite One (RP2040)    →    Host
TX (GPIO 0)            →    RX
RX (GPIO 1)            →    TX  
GND                    →    GND

Optional Display (SSD1306):
SDA (GPIO 4)           →    OLED SDA
SCL (GPIO 5)           →    OLED SCL
```

### Firmware Upload

```bash
cd firmware
arduino-cli compile --fqbn rp2040:rp2040:rpipico ../examples/sprite_one_unified
arduino-cli upload -p COM3 --fqbn rp2040:rp2040:rpipico
```

### Python Example

```python
from sprite_one import SpriteOne

with SpriteOne('COM3') as sprite:
    # Get version
    v = sprite.get_version()
    print(f"v{v[0]}.{v[1]}.{v[2]}")
    
    # Train XOR neural network
    loss = sprite.ai_train(epochs=100)
    print(f"Trained. Loss: {loss:.6f}")
    
    # Run inference
    result = sprite.ai_infer(1.0, 0.0)
    print(f"1 XOR 0 = {result:.3f}")
    
    # Model management (v1.5+)
    models = sprite.model_list()
    info = sprite.model_info()
    sprite.model_upload("custom.aif32", model_data)
```

### Web Trainer

Open `webapp/index.html` in Chrome/Edge for:
- Device auto-detection
- WebGPU/CPU training
- TFLite import and conversion
- One-click deploy

## Specifications

| Feature | v2.0 |
|---------|------|
| MCU | RP2040 (133 MHz, 264KB RAM) |
| Flash usage | ~115KB |
| RAM usage | ~13KB |
| Transport | UART + USB-CDC |
| Display | SSD1306 128x64 OLED |
| Sprites | 8 hardware sprites |
| AI inference | <1ms |
| Model format | .aif32 (AIFes) |

## Project Structure

```
sprite-one/
├── examples/
│   └── sprite_one_unified/    # Main firmware
├── firmware/
│   └── include/               # Core headers
├── host/
│   ├── python/               # Python library
│   └── c/                    # C library
├── webapp/                   # Web training UI
│   ├── index.html
│   ├── style.css
│   └── js/
└── tools/
    └── converter/            # TFLite → AIFes
```

## Features by Version

| Version | Features |
|---------|----------|
| v1.2 | USB-CDC transport |
| v1.3 | SSD1306 display driver |
| v1.4 | Hardware sprites (8) |
| v1.5 | Multi-model hot-swap |
| v2.0 | Web trainer, TFLite converter |

## Model Conversion

Convert TFLite models to AIFes format:

```bash
# Python (lightweight, no TensorFlow needed for .tflite)
python tools/converter/convert.py model.tflite output.aif32

# From Keras (requires TensorFlow)
python tools/converter/convert.py model.h5 output.aif32
```

Supports: `.tflite`, `.h5`, `.keras`, `.pb`, TF.js

## Documentation

- **[Getting Started](docs/GETTING_STARTED.md)** - Setup and first program
- **[API Reference](docs/API.md)** - Complete command reference
- **[Model Format](tools/converter/README.md)** - AIFes format specification

## Current Limitations

1. **Display:** Software rendering (no GPU)
2. **AI:** Limited to dense/conv layers
3. **Protocol:** No flow control yet
4. **Testing:** Hardware required for full tests

## Development

Requires:
- Arduino IDE 2.0+ or PlatformIO
- RP2040 board support
- Python 3.7+ (for host library)
- Chrome/Edge (for webapp WebSerial)

## License

MIT License - See [LICENSE](LICENSE)

## Acknowledgments

- **AIfES** - TinyML framework
- **LittleFS** - Embedded filesystem
- **TensorFlow Lite** - Model format

---

**Author:** Akhil Chaturvedi  
**Repository:** [github.com/Akhil-Chaturvedi/sprite-microelectronics](https://github.com/Akhil-Chaturvedi/sprite-microelectronics)
