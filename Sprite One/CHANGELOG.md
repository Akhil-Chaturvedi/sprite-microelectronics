# Changelog

All notable changes to Sprite One will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-02-07

### Added

#### Firmware (RP2040)
- Graphics engine with 128x64 framebuffer
- AI engine with on-device training (AIfES)
- F32 and Q7 model quantization support
- Model persistence via LittleFS
- Binary protocol over UART (115200 baud)
- Enhanced debug logging and progress indicators
- Memory monitoring utilities
- CRC32 data integrity checks

#### Host Libraries
- **Python library** (`host/python/sprite_one.py`)
  - Full API wrapper for all commands
  - Context manager support
  - Error handling with custom exceptions
  - Type hints for better IDE support
- **C library** (`host/c/`) for embedded hosts
  - Platform-agnostic UART abstraction
  - Works with ESP32, STM32, Arduino
  - Clean API design

#### Examples
- `sprite_one_unified.ino` - Complete system demo
- `aifes_xor_training.ino` - Neural network training
- `aifes_q7_demo.ino` - Q7 quantization
- `aifes_persistence_demo.ino` - Model save/load
- `examples.py` - Python examples
- `test_suite.py` - Automated testing

#### Documentation
- Getting Started Guide
- Complete API Reference
- Testing Guide
- Build Configurations
- Optimization Report
- Protocol Specification

#### Development Tools
- Automated test suite with 28+ tests
- Unit tests (no hardware required)
- Build configurations (debug, release, minimal)
- Performance profiling tools

### Performance
- Flash usage: 109KB (5.2% of 2MB)
- RAM usage: 12.5KB (4.7% of 264KB)
- Training speed: 100 epochs in ~3 seconds
- Inference latency: <1ms
- 75% memory savings with Q7 quantization

### Security
- CRC32 checksums for model files
- Model corruption detection
- Safe buffer handling

---

## [Unreleased]

### Planned
- Hardware PCB design
- Additional AI model architectures
- More display driver support
- Bluetooth connectivity
- Web-based configuration tool

---

## Development History

This project was developed over 4 weeks (Days 1-28):

- **Week 1 (Days 1-7):** Protocol design & graphics engine
- **Week 2 (Days 8-14):** AI integration & persistence
- **Week 3 (Days 15-21):** Advanced features & optimization
- **Week 4 (Days 22-28):** Integration, testing, documentation, release

For detailed development history, see the `development/` directory.
