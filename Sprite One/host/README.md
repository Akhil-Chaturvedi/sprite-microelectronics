# Sprite One Host Libraries

Host-side libraries for controlling Sprite One from a PC or embedded MCU.

## Directory Structure

```
host/
├── python/
│   ├── sprite_one.py      # Main library
│   ├── examples.py        # Usage examples
│   ├── verify_hardware.py # End-to-end hardware verification script
│   ├── test_suite.py      # Integration tests (requires hardware)
│   ├── unit_tests.py      # Unit tests (no hardware needed)
│   └── requirements.txt   # pyserial
└── c/
    ├── sprite_one.h       # Header
    └── sprite_one.c       # Implementation
```

---

## Python Library

### Installation

```bash
cd host/python
pip install -r requirements.txt   # installs pyserial
```

### API Summary

**System**
- `get_version()` → `(major, minor, patch)`

**Graphics**
- `clear(color=0)` — fill display
- `pixel(x, y, color=1)` — single pixel
- `rect(x, y, w, h, color=1)` — filled rectangle
- `text(x, y, string, color=1)` — draw text
- `flush()` — push framebuffer to OLED

**AI — Inference**
- `ai_infer(in0, in1)` → `float` — forward pass, legacy 2-input signature
- `ai_status()` → `dict` — state, model_type, epochs, last_loss, input_dim, output_dim

**Model Management** (V3 dynamic models)
- `model_info()` → `dict | None` — active model metadata
- `model_list()` → `list[str]` — models stored in flash
- `model_select(filename)` — load a stored model
- `model_upload(filename, data)` — upload `.aif32` to device flash
- `model_delete(filename)` — delete from flash

**On-Device Training**
- `finetune_start(learning_rate=0.01)` — allocate optimizer memory
- `finetune_data(inputs, targets)` → `float` loss — one training step
- `finetune_stop()` — end session

**Industrial API Primitives**
- `get_device_id()` → `bytes` — 8-byte board-unique ID
- `ping_id(id_bytes)` → `bool` — ID match check
- `buffer_write(value)` — push one float32 into circular buffer (max 60)
- `buffer_snapshot()` → `list[float]` — ordered buffer contents
- `baseline_capture()` → `float` — freeze buffer mean as baseline
- `baseline_reset()` — clear baseline
- `get_delta()` → `float` — abs(live_mean - baseline)
- `correlate(ref_data)` → `float` — normalized cross-correlation [0.0, 1.0]

**Legacy file operations**
- `ai_save(filename)` / `ai_load(filename)` — save/load the static model slot
- `ai_list_models()` / `ai_delete(filename)`

See [docs/API.md](../docs/API.md) for full signatures and examples.

---

## C Library

Platform-agnostic. Provide three I/O function pointers at init time.

```c
#include "sprite_one.h"

void    platform_write(uint8_t b) { /* write one byte */ }
uint8_t platform_read(void)       { /* blocking read one byte */ }
bool    platform_available(void)  { /* true if data ready */ }

sprite_context_t ctx;
sprite_init(&ctx, platform_write, platform_read, platform_available, 2000);

// Graphics
sprite_clear(&ctx, 0);
sprite_rect(&ctx, 10, 10, 50, 30, 1);
sprite_flush(&ctx);

// Inference
float result;
sprite_ai_infer(&ctx, 1.0f, 0.0f, &result);

// Model management
sprite_model_upload(&ctx, "model.aif32", data_ptr, data_len);
sprite_model_select(&ctx, "model.aif32");
```

### ESP32 Example

```c
void esp32_write(uint8_t b) { Serial2.write(b); }
uint8_t esp32_read(void)    { return Serial2.read(); }
bool esp32_available(void)  { return Serial2.available() > 0; }

void setup() {
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    sprite_context_t ctx;
    sprite_init(&ctx, esp32_write, esp32_read, esp32_available, 2000);

    float result;
    sprite_ai_infer(&ctx, 1.0f, 0.0f, &result);
}
```

---

## License

MIT — see [LICENSE](../LICENSE)
