# API Reference

**Firmware version:** 2.2.0
**Python library:** `host/python/sprite_one.py`

---

## Python Library

### Connection

```python
from sprite_one import SpriteOne

# Auto-detect transport (USB-CDC preferred, falls back to UART)
with SpriteOne('COM3') as sprite:
    pass

# Explicit USB-CDC
sprite = SpriteOne('/dev/ttyACM0', mode='usb')

# Explicit UART
sprite = SpriteOne('/dev/ttyUSB0', mode='uart', baudrate=115200)
```

**Constructor parameters:**

| Parameter | Type | Default | Description |
|---|---|---|---|
| `port` | str | — | Serial port path |
| `baudrate` | int | 115200 | Baud rate (UART only; USB-CDC ignores this) |
| `timeout` | float | 2.0 | Read timeout in seconds |
| `mode` | str | `'auto'` | `'auto'`, `'usb'`, or `'uart'` |

---

### System

#### `get_version() → (int, int, int)`

Returns firmware version as `(major, minor, patch)`.

```python
major, minor, patch = sprite.get_version()  # e.g. (2, 2, 0)
```

---

### Graphics

The display is a 128×64 monochrome OLED (SSD1306). All drawing calls modify an in-memory framebuffer. Changes are not visible until `flush()` is called.

```python
sprite.clear(color=0)               # Fill display (0=black, 1=white)
sprite.pixel(x, y, color=1)        # Single pixel
sprite.rect(x, y, w, h, color=1)   # Filled rectangle
sprite.text(x, y, "hello", color=1) # Draw text at position
sprite.flush()                      # Push buffer to display
```

---

### AI — Inference

#### `ai_infer(input0, input1) → float`

Run inference using the legacy 2-float-input signature. If a V3 dynamic model is loaded, the two inputs are placed at positions 0 and 1 of the model's input vector (remaining inputs zero-padded). For models with other input shapes, send raw data via `finetune_data()`.

```python
result = sprite.ai_infer(1.0, 0.0)
```

#### `ai_status() → dict`

```python
s = sprite.ai_status()
# {
#   'state': 0,
#   'model_loaded': True,
#   'model_type': 'Dynamic',   # 'None', 'Static', or 'Dynamic'
#   'epochs': 100,
#   'last_loss': 0.012,
#   'input_dim': 2,
#   'output_dim': 1
# }
```

---

### AI — Legacy File Operations

These operate on the hardcoded static model slot. For V3 dynamic models, use the Model Management commands below.

```python
sprite.ai_save("/xor.aif32")     # Save active static model weights to flash
sprite.ai_load("/xor.aif32")     # Load weights from flash into static slot
sprite.ai_list_models()          # Returns list of filenames on flash
sprite.ai_delete("/old.aif32")   # Delete file from flash
```

---

### Model Management

Upload arbitrary `.aif32` V3 models without recompiling firmware. Multiple models can coexist in flash.

#### `model_info() → dict | None`

Returns metadata for the currently active model, or `None` if no model is loaded.

```python
info = sprite.model_info()
# {
#   'name': 'sentinel_god_v3',
#   'input_size': 128,
#   'output_size': 5
# }
```

#### `model_list() → list[str]`

Returns list of model filenames stored on flash.

```python
models = sprite.model_list()
# ['sentinel_god_v3.aif32', 'xor.aif32']
```

#### `model_select(filename: str) → bool`

Load a stored model from flash and make it the active inference and training target.

```python
sprite.model_select("sentinel_god_v3.aif32")
```

#### `model_upload(filename: str, data: bytes) → bool`

Upload a `.aif32` file to device flash. Handles chunking and per-chunk ACK handshake internally.

```python
with open("custom_model.aif32", "rb") as f:
    sprite.model_upload("custom_model.aif32", f.read())
```

#### `model_delete(filename: str) → bool`

Delete a model file from flash.

---

### On-Device Training

Train or fine-tune the active model directly on the device using AIfES (Adam optimizer, MSE loss). The model must be loaded via `model_select()` or uploaded with `model_upload()`.

Supported layer types for backprop: Dense, ReLU, Sigmoid. Conv2D layers are inference-only.

#### `finetune_start(learning_rate: float = 0.01)`

Allocate gradient and momentum memory and initialize the Adam optimizer. Must be called before `finetune_data()`.

```python
sprite.finetune_start(learning_rate=0.01)
```

#### `finetune_data(inputs: list, targets: list) → float`

Run a single training step. Returns the loss for that step.

- `inputs`: list of floats, length must match the model's `input_count`
- `targets`: list of floats, length must match the model's `output_count`

```python
for epoch in range(500):
    loss = sprite.finetune_data([0.0, 1.0], [1.0])
    loss = sprite.finetune_data([1.0, 0.0], [1.0])
    loss = sprite.finetune_data([0.0, 0.0], [0.0])
    loss = sprite.finetune_data([1.0, 1.0], [0.0])
```

#### `finetune_stop()`

End the training session.

```python
sprite.finetune_stop()
```

---

### Industrial API Primitives

Low-level signal processing primitives implemented on-device. The device stores up to 60 `float32` samples in a circular buffer. Baseline and delta operations work against that buffer.

#### `get_device_id() → bytes`

Returns the 8-byte board-unique ID from the RP2040 one-time-programmable memory.

#### `ping_id(device_id: bytes) → bool`

Returns `True` if the provided 8-byte ID matches the device.

#### `buffer_write(value: float)`

Push one sample into the circular buffer. Oldest entry is evicted once the buffer exceeds 60 samples.

#### `buffer_snapshot() → list[float]`

Return all buffered samples as a list, ordered oldest-to-newest.

#### `baseline_capture() → float`

Freeze the current buffer mean as the baseline. Returns the captured mean.

#### `baseline_reset()`

Clear the baseline.

#### `get_delta() → float`

Returns `abs(live_mean - baseline)`. Raises `SpriteOneError` if no baseline has been captured.

#### `correlate(ref_data: list[float]) → float`

Normalized cross-correlation of the buffer contents against `ref_data`. Returns a score in [0.0, 1.0]. Score is 1.0 for identical signals, 0.0 for anti-correlated signals.

---

## C Library

Header: `host/c/sprite_one.h`

```c
#include "sprite_one.h"

// Provide platform-specific I/O callbacks
void    uart_write(uint8_t b)  { /* write one byte */ }
uint8_t uart_read(void)        { /* read one byte, blocking */ }
bool    uart_available(void)   { /* return true if data available */ }

// Initialise
sprite_context_t ctx;
sprite_init(&ctx, uart_write, uart_read, uart_available, /* timeout_ms */ 2000);

// System
uint8_t major, minor, patch;
sprite_get_version(&ctx, &major, &minor, &patch);

// Graphics
sprite_clear(&ctx, 0);
sprite_rect(&ctx, 10, 10, 50, 30, 1);
sprite_text(&ctx, 0, 0, 1, "hello");
sprite_flush(&ctx);

// Inference
float result;
sprite_ai_infer(&ctx, 1.0f, 0.0f, &result);

// Model management
sprite_model_upload(&ctx, "model.aif32", data, data_len);
sprite_model_select(&ctx, "model.aif32");
```

---

## Error Handling

**Python:** All methods raise `SpriteOneError` on command failure or timeout.

```python
from sprite_one import SpriteOneError

try:
    sprite.model_select("missing.aif32")
except SpriteOneError as e:
    print(f"Error: {e}")
```

**C:** All functions return `bool` (`true` = success).

---

## Performance

Measured on RP2040 at 133 MHz.

| Operation | Model | Time |
|---|---|---|
| `ai_infer()` | 2-4-1 dense+sigmoid | < 1 ms |
| `finetune_data()` (one step) | 2-4-1 dense+sigmoid | ~1–3 ms |
| `model_upload()` | 1 KB | ~100–200 ms (USB-CDC) |
| `model_select()` | load from flash | ~10–50 ms (model-size dependent) |
| `get_device_id()` | — | < 1 ms |

---

## Limits

| Parameter | Value |
|---|---|
| Display | 128×64, 1-bit |
| Max packet payload | 255 bytes |
| Max model filename | 31 chars |
| Flash storage | ~1.9 MB (LittleFS) |
| Hardware sprites | 8 slots |
| Concurrent models in RAM | 1 |
| Industrial buffer depth | 60 float32 samples |
