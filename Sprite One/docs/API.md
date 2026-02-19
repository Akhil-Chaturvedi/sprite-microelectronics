# API Reference

**Firmware version:** 2.1.0  
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
major, minor, patch = sprite.get_version()  # e.g. (2, 1, 0)
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

Run inference on the active model. This signature assumes a 2-input model (e.g. XOR). For arbitrary input sizes, use the model management + finetune workflow instead.

```python
result = sprite.ai_infer(1.0, 0.0)  # Returns ~0.98 for trained XOR
```

#### `ai_status() → dict`

```python
s = sprite.ai_status()
# {'state': 0, 'model_loaded': True, 'epochs': 100, 'last_loss': 0.012}
```

---

### AI — File-based Model Storage (legacy)

These commands use the old hardcoded model slot. For dynamic model loading, see **Model Management** below.

```python
sprite.ai_save("/xor.aif32")      # Save active model to flash
sprite.ai_load("/xor.aif32")      # Load model from flash into fixed slot
sprite.ai_list_models()           # Returns list of filenames on flash
sprite.ai_delete("/old.aif32")    # Delete file from flash
```

---

### Model Management

Dynamic model loading — upload arbitrary `.aif32` V3 models without recompiling firmware. Multiple models can be stored in flash simultaneously.

#### `model_info() → dict`

Returns metadata for the currently active model.

```python
info = sprite.model_info()
# {
#   'name': 'sentinel_god_v3',
#   'input_count': 128,
#   'output_count': 5,
#   'layer_count': 5
# }
```

Returns `None` if no model is active.

#### `model_list() → list[str]`

Returns list of model filenames stored on flash.

```python
models = sprite.model_list()
# ['sentinel_god_v3.aif32', 'xor.aif32']
```

#### `model_select(filename: str) → bool`

Load a stored model and make it the active inference target.

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

### On-Device Training (Finetune)

Train or fine-tune the active model directly on the device using AIfES (Adam optimizer, MSE loss). The model must already be loaded via `model_select()` or `model_upload()`.

#### `finetune_start(learning_rate: float = 0.01) → bool`

Allocate gradient/momentum memory and initialise the optimizer. Must be called before `finetune_data()`.

```python
sprite.finetune_start(learning_rate=0.01)
```

#### `finetune_data(inputs: list, targets: list) → float`

Run a single training step. Returns the loss for that step.

```python
for epoch in range(500):
    loss = sprite.finetune_data([0.0, 1.0], [1.0])
    loss = sprite.finetune_data([1.0, 0.0], [1.0])
    loss = sprite.finetune_data([0.0, 0.0], [0.0])
    loss = sprite.finetune_data([1.0, 1.0], [0.0])
```

#### `finetune_stop(save: bool = True) → dict`

End the training session. If `save=True`, writes updated weights back to flash.

```python
sprite.finetune_stop(save=True)
```

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
sprite_text(&ctx, 0, 0, 1, "hello");   // x, y, color, string
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

Measured on RP2040 at 133 MHz with a 2-4-1 dense+sigmoid XOR model.

| Operation | Time |
|---|---|
| `ai_infer()` | < 1 ms |
| `finetune_data()` (one step) | ~1–3 ms |
| `model_upload()` (1 KB) | ~100–200 ms (USB-CDC) |
| `model_select()` (load from flash) | ~10–50 ms (model-size dependent) |

---

## Limits

| Parameter | Value |
|---|---|
| Display | 128×64, 1-bit |
| Max packet payload | 255 bytes |
| Max model filename | 15 chars |
| Flash storage | ~1.9 MB (LittleFS) |
| Hardware sprites | 8 slots |
| Concurrent models in RAM | 1 (loaded model) |
