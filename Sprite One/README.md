# Sprite One

Graphics and AI accelerator for embedded systems.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-RP2040-orange.svg)](https://www.raspberrypi.com/products/rp2040/)

## What It Does

Sprite One is an RP2040-based coprocessor that handles graphics rendering and neural network operations via a simple UART protocol. Designed for microcontrollers that need to offload computationally intensive tasks.

**Current capabilities:**
- 128x64 framebuffer with basic primitives (pixel, rect, text simulation)
- On-device neural network training (F32 and Q7 quantization)
- Model persistence to flash storage
- Binary protocol over UART (115200 baud)

**What it doesn't do:**
- No real display drivers yet (framebuffer is logged to serial for testing)
- No hardware graphics acceleration (software rendering)
- Limited to simple neural network architectures (XOR demo included)

## Quick Start

### Hardware

Connect via UART:
```
Sprite One (RP2040)    →    Host
TX (GPIO 0)            →    RX
RX (GPIO 1)            →    TX  
GND                    →    GND
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
    # Train XOR neural network
    loss = sprite.ai_train(epochs=100)
    print(f"Trained. Loss: {loss:.6f}")
    
    # Run inference
    result = sprite.ai_infer(1.0, 0.0)
    print(f"1 XOR 0 = {result:.3f}")
```

## Specifications

| Feature | Current State |
|---------|---------------|
| MCU | RP2040 (133 MHz, 264KB RAM) |
| Flash usage | 109KB (5%) |
| RAM usage | 12.5KB (5%) |
| Protocol | UART, 115200 baud |
| Framebuffer | 128x64, 1-bit (simulated) |
| AI training | ~3s for 100 epochs (XOR) |
| AI inference | <1ms |
| Model storage | LittleFS on flash |

## Project Structure

```
sprite-one/
├── examples/
│   └── sprite_one_unified/    # Main demo firmware
├── firmware/
│   └── include/               # Core headers
├── host/
│   ├── python/               # Python library
│   └── c/                    # C library for embedded hosts
└── docs/
    ├── GETTING_STARTED.md
    └── API.md
```

## Documentation

- **[Getting Started](docs/GETTING_STARTED.md)** - Setup and first program
- **[API Reference](docs/API.md)** - Complete command reference
- **[Build Configurations](firmware/BUILD_CONFIGS.md)** - Compiler flags and variants

## Current Limitations

1. **Graphics:** Framebuffer is simulated (serial output), no real display drivers
2. **Protocol:** No flow control implementation yet (command added, not used)
3. **AI Models:** Limited to simple feedforward networks
4. **Performance:** Software rendering, no DMA for UART
5. **Testing:** Requires physical hardware for integration tests

## Use Cases

**Good for:**
- Prototyping AI at the edge
- Learning TinyML concepts
- Offloading simple graphics tasks

**Not suitable for:**
- High-framerate graphics (UART bottleneck)
- Complex deep learning models
- Production systems without further development

## Development

Requires:
- Arduino IDE 2.0+ or PlatformIO
- RP2040 board support
- Python 3.7+ (for host library)

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup.

## License

MIT License - See [LICENSE](LICENSE)

## Acknowledgments

- **AIfES** - TinyML framework
- **LittleFS** - Embedded filesystem
- **RP2040** - Raspberry Pi Foundation

---

**Author:** Akhil Chaturvedi  
**Repository:** [github.com/Akhil-Chaturvedi/sprite-microelectronics](https://github.com/Akhil-Chaturvedi/sprite-microelectronics)
