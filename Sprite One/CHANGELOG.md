# Changelog

All notable changes to Sprite One.

---

## [2.1.0] - 2026-02-19

### Added

- **Dynamic Model Loading (V3 format)**  
  `.aif32` now includes a layer descriptor table — the firmware loads arbitrary sequential topologies at runtime without recompilation. See `docs/protocol.md` for the file format spec.

- **Universal AIfES Interpreter** (`sprite_dynamic.h`)  
  `DynamicModel::loadV3()` iterates layer descriptors and allocates the AIfES layer graph into a static arena. Previously, the model topology was hardcoded.

- **Layer types added:** Sigmoid (4), Softmax (5), Conv2D (6), Flatten (7), MaxPool (8)

- **Model generator** (`tools/gen_sentinel_model.py`)  
  Builds complete `.aif32` V3 files from Python — input, dense, activation, conv2d, maxpool, softmax layers. Used to generate all reference models (`sentinel_god_v3.aif32`, `vision_demo.aif32`, test models).

- **On-device Training**  
  `CMD_AI_TRAIN (0x51)` and `CMD_FINETUNE_START (0x65)` — runs one backprop step on-device using AIfES (Adam optimizer, MSE loss). `DynamicModel::prepare_training()` allocates gradient/momentum memory; `train_step()` executes forward + backward + weight update.

- **Simulator Backprop Engine** (`webapp/js/mock_device.js`)  
  Implements backpropagation for Dense, ReLU, and Sigmoid layers in JavaScript, enabling XOR and similar models to be trained entirely in-browser with no device attached.

- **Mock Mode** (Web Trainer)  
  Toggle `Mock` in the web UI header to get a fully simulated Sprite One device in the browser. No hardware needed to develop or test model upload/inference/training workflows.

- **Protocol hardening**  
  - CRC32 checksum appended to every packet (request and response)
  - Per-chunk ACK handshake during model upload (`CMD_MODEL_UPLOAD 0x63`)
  - Python library and mock device enforce CRC on both sides

- **Conv2D support** — both firmware and simulator correctly handle channels-first (C, H, W) tensors across Conv2D → MaxPool → Flatten → Dense → Softmax pipelines

- **Test runner** (`test_runner.js`)  
  Node.js test suite that validates all model types against the mock device, including the training loop (XOR convergence test).

### Changed

- `CMD_AI_TRAIN (0x51)` repurposed from "legacy hardcoded training" to "one training step on active dynamic model". Payload: `N×f32 inputs + M×f32 targets`. Response: `f32 loss`.
- `CMD_AI_INFER (0x50)` payload now driven by model's actual `input_count` / `output_count` rather than being hardcoded to 2-in / 1-out.
- `sprite_dynamic.h::loadV3()` replaces the old V2 format parser. V2 models are no longer supported.

---

## [2.0.0] - 2026-02-09

### Added

- **Web Trainer** — browser-based training with WebGPU/CPU backend, real-time loss chart, webcam/file/canvas data capture, one-click deploy
- **TFLite Converter** (`tools/converter/`) — converts `.tflite`, `.h5`, `.pb`, TF.js to `.aif32` with FlatBuffer parser (no TensorFlow needed for `.tflite` input)
- **Model Management commands** (0x60–0x67) — `MODEL_INFO`, `MODEL_LIST`, `MODEL_SELECT`, `MODEL_UPLOAD`, `MODEL_DELETE`, `FINETUNE_START/DATA/STOP`
- **Python library extensions** — `model_info()`, `model_list()`, `model_select()`, `model_upload()`, `model_delete()`, `finetune_start/data/stop()`
- Chunked upload (`MODEL_UPLOAD`) — 200-byte chunks with per-chunk ACK

---

## [1.5.0] - 2026-02-09

### Added

- SSD1306 OLED driver (128×64, I2C on GP4/GP5)
- Hardware sprite system (8 slots, AABB collision detection, Z-ordering)
- Model hot-swap — switch active model without rebooting
- LittleFS model persistence

---

## [1.2.0] - 2026-02-08

### Added

- USB-CDC transport (12 Mbps vs 115200 bps UART)
- Transport auto-detection
- Python `mode` parameter: `'auto'`, `'usb'`, `'uart'`

---

## [1.1.0] - 2026-02-08

### Added

- Dirty rectangle tracking (skip unchanged display regions on flush)
- `CMD_BUFFER_STATUS (0x0E)`

---

## [1.0.0] - 2026-02-07

### Initial Release

- RP2040 firmware with graphics + AI (AIfES)
- 128×64 framebuffer, SSD1306 driver
- AIfES on-device training (hardcoded 2-4-1 XOR network, F32)
- LittleFS persistence
- Python and C host libraries
- Protocol over UART: `[0xAA] [CMD] [LEN] [PAYLOAD] [XOR checksum]`
