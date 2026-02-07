# 🚀 Sprite One

**The world's cheapest open-source hardware accelerator for microcontrollers.**

Give your $3 Arduino the power of a dedicated graphics and AI co-processor—for just $5.

![Status](https://img.shields.io/badge/status-in%20development-yellow)
![License](https://img.shields.io/badge/license-MIT-blue)
![BOM Cost](https://img.shields.io/badge/BOM-$1.80-green)

---

## 🎯 What is Sprite One?

Sprite One is a tiny hardware module that connects to any Arduino, ESP32, or other microcontroller via SPI. It offloads graphics rendering and (eventually) AI inference from your main chip, making everything faster and smoother.

### The Problem
- Your ESP32 is fast, but it chokes when driving a display AND running your code
- Professional solutions cost $30-60+ (Google Coral, Nextion, etc.)
- They're closed-source black boxes

### The Solution
- **$10-15 retail** (BOM ~$1.80 at volume)
- **100% open source** - hardware, firmware, everything
- **Dead simple API** - 5 lines of code to get started
- **Multi-display support** - ILI9341, ST7789, ST7735, and more

---

## ⚡ Quick Start

```cpp
#include <Sprite.h>

Sprite gfx;

void setup() {
    gfx.begin(10);  // CS on pin 10
    gfx.initDisplay(DISPLAY_ILI9341, 320, 240);
}

void loop() {
    gfx.clear(BLACK);
    gfx.rect(10, 10, 50, 50, RED);
    gfx.text(100, 100, "Hello World!");
    gfx.flush();
    delay(16);
}
```

---

## 📁 Project Structure

```
Sprite One/
├── firmware/           # RP2040 accelerator firmware
│   ├── src/
│   │   ├── main.cpp            # Dual-core main program
│   │   └── display_drivers.cpp # Multi-display support
│   └── platformio.ini
│
├── library/            # Arduino host library
│   ├── src/Sprite.h    # Just include this!
│   └── examples/
│
├── docs/
│   └── protocol.md     # SPI protocol specification
│
└── wokwi/              # Hardware simulation
    └── diagram.json
```

---

## 🛠 Hardware

### Bill of Materials (~$1.80)

| Component | Part | Cost |
|-----------|------|------|
| MCU | RP2040 | $0.80 |
| Flash | W25Q16 2MB | $0.15 |
| Crystal | 12MHz | $0.05 |
| LDO | AP2112K-3.3 | $0.10 |
| USB-C | Connector | $0.15 |
| Passives | Caps/resistors | $0.25 |
| PCB | 25x25mm 2-layer | $0.30 |

### Why RP2040?

- **$0.80** at volume (cheapest option)
- **PIO** - Programmable I/O for blazing fast display driving
- **Dual-core** - One for SPI, one for rendering
- **264KB RAM** - Enough for full framebuffer

---

## 🎴 Supported Displays

| Display | Resolution | Status |
|---------|------------|--------|
| ILI9341 | 320×240 | ✅ Supported |
| ST7789 | 240×240 / 320×240 | ✅ Supported |
| ST7735 | 128×160 | ✅ Supported |
| SSD1306 | 128×64 OLED | 🔜 Coming |
| ILI9488 | 480×320 | 🔜 Coming |

---

## 🗺 Roadmap

- [x] **Phase 1**: Protocol & architecture design
- [ ] **Phase 2**: Core firmware (in progress)
- [ ] **Phase 3**: Wokwi simulation validation
- [ ] **Phase 4**: Physical prototype
- [ ] **Phase 5**: AI/ML inference support
- [ ] **Phase 6**: Kickstarter launch

---

## 🤝 Contributing

This is a community-driven project! We welcome contributions:

1. Fork the repo
2. Create a feature branch
3. Submit a PR

---

## 📜 License

MIT License - Use it, modify it, sell products with it. Just give credit.

---

## 💬 Community

- **Discord**: Coming soon
- **Hackaday**: Coming soon
- **Twitter**: @SpriteElectro

---

*Built with ❤️ by Sprite Microelectronics*  
*"Silicon for the Rest of Us"*
