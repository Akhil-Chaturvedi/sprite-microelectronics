# Changelog

All notable changes to Sprite One.

---

## [2.2.0] - 2026-02-26

### Added

- **Industrial API Primitives (0xA0–0xA7)**
  Eight open-API commands for signal processing primitives: device identity (`WHO_IS_THERE`, `PING_ID`), a 60-sample floating-point circular buffer (`BUFFER_WRITE`, `BUFFER_SNAPSHOT`), baseline mean capture and delta measurement (`BASELINE_CAPTURE`, `BASELINE_RESET`, `GET_DELTA`), and normalized cross-correlation (`CORRELATE`). The circular buffer stores up to 60 `float32` samples and evicts the oldest on overflow.

- **Hardware Device ID** (`sprite_one_unified.ino`)
  `pico_get_unique_board_id()` is called at boot and stored as the 8-byte device identity returned by `WHO_IS_THERE` (0xA0). The ID is board-specific and persistent.

- **Dynamic Model Training (on-device backprop)**
  `DynamicModel::prepare_training(lr)` allocates gradient and momentum memory in the static arena. `DynamicModel::train_step(inputs, targets)` runs one forward + backward pass and weight update via the AIfES Adam optimizer. Supported layer types for backprop: Dense, ReLU, Sigmoid. Conv2D is inference-only.

- **`CMD_AI_TRAIN` (0x51) wired to dynamic models**
  If a V3 dynamic model is loaded, `CMD_AI_TRAIN` delegates to `DynamicModel::train_step()`. Falls back to the legacy static XOR model if no dynamic model is active.

- **Fine-tuning commands fully implemented (0x65–0x67)**
  `FINETUNE_START` allocates the optimizer. `FINETUNE_DATA` runs one training step and returns the loss. `FINETUNE_STOP` tears down the session. Previously these were stubs that returned ACK without executing any training logic.

- **`CMD_AI_STATUS` extended to 12 bytes**
  Response now includes `input_dim` (u16) and `output_dim` (u16) after the existing 8-byte body. `model_loaded` field is now an enum: 0=none, 1=legacy static, 2=dynamic V3.

- **Python host library — new methods**
  `get_device_id()`, `ping_id()`, `buffer_write()`, `buffer_snapshot()`, `baseline_capture()`, `baseline_reset()`, `get_delta()`, `correlate()`.

- **Hardware verification script** (`host/python/verify_hardware.py`)
  Exercises CRC validation, industrial primitives, and fine-tuning end-to-end against a real or mock device.

- **Mock device (`tools/mock_device.py`) updated**
  Added handlers for 0xA0–0xA7, updated `AI_STATUS` to 12 bytes, added `_cmd_finetune_data` with simulated loss decay.

### Changed

- `ai_status()` Python method now returns `model_type` (`'None'`, `'Static'`, `'Dynamic'`) and `input_dim` / `output_dim` fields when available.
- `finetune_data()` payload format changed from prefixed `[count_B][floats]` to raw `[inputs floats][targets floats]` — aligns with firmware expectation.
- Async file operations in `fs_task()` unified: `FS_LOAD_PENDING` and `FS_SAVE_PENDING` now both delegate to `load_model()` / `save_model()` rather than managing their own streaming logic. This eliminates the dual read-path for dynamic vs. legacy models.
- Protocol note about CRC inconsistency removed — both request and response now carry CRC32 end-to-end on all paths.

---

## [2.1.0] - 2026-02-19

### Added

- **Dynamic Model Loading (V3 format)**
  `.aif32` now includes a layer descriptor table — the firmware loads arbitrary sequential topologies at runtime without recompilation.

- **Universal AIfES Interpreter** (`sprite_dynamic.h`)
  `DynamicModel::loadV3()` iterates layer descriptors and allocates the AIfES layer graph into a static arena.

- **Layer types added:** Sigmoid (4), Softmax (5), Conv2D (6), Flatten (7), MaxPool (8)

- **Model generator** (`tools/gen_sentinel_model.py`)
  Builds complete `.aif32` V3 files from Python.

- **Simulator Backprop Engine** (`webapp/js/mock_device.js`)
  Implements backpropagation for Dense, ReLU, and Sigmoid layers in JavaScript.

- **Mock Mode** (Web Trainer)
  Toggle `Mock` in the web UI header for a fully simulated device in the browser.

- **Protocol hardening**
  CRC32 checksum appended to every packet (request and response). Per-chunk ACK handshake during model upload.

- **Conv2D support** — channels-first (C, H, W) tensors across Conv2D → MaxPool → Flatten → Dense → Softmax pipelines.

- **Test runner** (`test_runner.js`)
  Node.js test suite validating all model types against the mock device, including XOR convergence.

### Changed

- `CMD_AI_TRAIN (0x51)` repurposed from legacy hardcoded training to one training step on active dynamic model.
- `CMD_AI_INFER (0x50)` payload driven by model's actual `input_count` / `output_count`.
- `sprite_dynamic.h::loadV3()` replaces the old V2 format parser. V2 models are no longer supported.

---

## [2.0.0] - 2026-02-09

### Added

- Web Trainer — browser-based training with real-time loss chart, webcam/file/canvas data capture, one-click deploy
- TFLite Converter (`tools/converter/`) — converts `.tflite`, `.h5`, `.pb`, TF.js to `.aif32`
- Model Management commands (0x60–0x67) — `MODEL_INFO`, `MODEL_LIST`, `MODEL_SELECT`, `MODEL_UPLOAD`, `MODEL_DELETE`, `FINETUNE_START/DATA/STOP`
- Python library extensions — `model_info()`, `model_list()`, `model_select()`, `model_upload()`, `model_delete()`
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

- RP2040 firmware with graphics and AI (AIfES)
- 128×64 framebuffer, SSD1306 driver
- AIfES on-device training (hardcoded 2-4-1 XOR network, F32)
- LittleFS persistence
- Python and C host libraries
- Protocol over UART: `[0xAA] [CMD] [LEN] [PAYLOAD] [XOR checksum]`
