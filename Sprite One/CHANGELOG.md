# Changelog

All notable changes to Sprite One will be documented in this file.

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [2.0.0] - 2026-02-09

### Added
- **Web Trainer** - Browser-based training interface
  - WebGPU/CPU training with real-time chart
  - Webcam, file, and canvas data input
  - Model architecture visualization
  - One-click device deployment
  
- **TFLite Converter** (`tools/converter/`)
  - Converts `.tflite`, `.h5`, `.pb`, TF.js to `.aif32`
  - Lightweight FlatBuffer parser (no TensorFlow for .tflite)
  - Dequantization support (INT8, UINT8 → F32)
  - Browser and Python versions
  
- **Model Management** (firmware commands 0x60-0x67)
  - `model_info`, `model_list`, `model_select`
  - `model_upload`, `model_delete`
  - `finetune_start`, `finetune_data`, `finetune_stop` (stubs)

- **Python Library Extensions**
  - 8 new model management methods
  - Chunked upload support
  
- **Unit Tests** - 27 Python tests for converter

### Changed
- Webapp UI redesigned with training charts
- README updated to v2.0
- Documentation consistency pass

---

## [1.5.0] - 2026-02-09

### Added
- SSD1306 OLED display driver (128x64, I2C)
- Hardware sprite system (8 sprites, Z-ordering)
- Model hot-swap (switch models without reboot)
- LittleFS model persistence

### Display
- I2C on GP4 (SDA), GP5 (SCL)
- 30 FPS display refresh
- Sprite collision detection (AABB)

---

## [1.2.0] - 2026-02-08

### Added
- USB-CDC transport (12 Mbps vs 115200 bps UART)
- Transport auto-detection
- Python `mode` parameter: `'auto'`, `'usb'`, `'uart'`

### Performance
- USB: ~100x faster than UART
- Memory: +508B flash, +24B RAM

---

## [1.1.0] - 2026-02-08

### Added
- Dirty rectangle tracking
- `CMD_BUFFER_STATUS` (0x0E)
- Code cleanup (removed 80% duplicate code)

---

## [1.0.0] - 2026-02-07

### Initial Release
- RP2040 firmware with graphics + AI
- 128x64 framebuffer
- AIfES on-device training (F32, Q7)
- LittleFS persistence
- Python and C host libraries
- Protocol over UART

### Specifications
- Flash: 109KB (5.2%)
- RAM: 12.5KB (4.7%)
- Training: ~3s for 100 epochs
- Inference: <1ms
