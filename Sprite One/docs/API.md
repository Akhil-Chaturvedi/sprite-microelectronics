# API Reference

**Version:** 2.0.0

## Python API

### Connection

```python
from sprite_one import SpriteOne

# Auto-detect transport
sprite = SpriteOne('COM3')

# Explicit USB-CDC
sprite = SpriteOne('/dev/ttyACM0', mode='usb')

# With context manager
with SpriteOne('COM3') as sprite:
    pass
```

**Parameters:**
- `port` - Serial port
- `baudrate` - Default 115200 (ignored for USB-CDC)
- `timeout` - Read timeout in seconds (default: 2.0)
- `mode` - `'auto'`, `'usb'`, or `'uart'`

---

### System Commands

#### get_version()
Returns firmware version as `(major, minor, patch)`.

```python
v = sprite.get_version()  # (2, 0, 0)
```

---

### Graphics Commands

```python
sprite.clear(color=0)          # Clear display
sprite.pixel(x, y, color=1)    # Draw pixel
sprite.rect(x, y, w, h, color=1)  # Draw filled rectangle
sprite.text(x, y, "text")      # Draw text
sprite.flush()                 # Update display
```

---

### AI Commands

#### ai_train(epochs=100)
Train model on device. Returns final loss.

```python
loss = sprite.ai_train(100)  # Returns 0.012
```

#### ai_infer(input0, input1)
Run inference. Returns output value.

```python
result = sprite.ai_infer(1.0, 0.0)  # Returns ~0.98
```

#### ai_status()
Returns dict: `{state, model_loaded, epochs, last_loss}`

#### ai_save(filename) / ai_load(filename)
```python
sprite.ai_save("/xor.aif32")
sprite.ai_load("/xor.aif32")
```

#### ai_list() / ai_delete(filename)
```python
models = sprite.ai_list()  # ['/xor.aif32', ...]
sprite.ai_delete("/old.aif32")
```

---

### Model Management (v2.0+)

#### model_info()
Returns active model header info or None.

```python
info = sprite.model_info()
# {'name': 'xor', 'input_size': 2, 'output_size': 1, ...}
```

#### model_list()
Returns list of model filenames.

#### model_select(filename)
Activates specified model.

#### model_upload(filename, data)
Uploads .aif32 model to device.

```python
with open('model.aif32', 'rb') as f:
    sprite.model_upload('custom.aif32', f.read())
```

#### model_delete(filename)
Deletes model from device.

#### finetune_start(lr=0.01) / finetune_data(inputs, outputs) / finetune_stop(save=True)
Incremental training (stubs - not fully implemented).

---

## C API

```c
#include "sprite_one.h"

// Init
sprite_context_t ctx;
sprite_init(&ctx, uart_write, uart_read, uart_available, 2000);

// Version
uint8_t major, minor, patch;
sprite_get_version(&ctx, &major, &minor, &patch);

// Graphics
sprite_clear(&ctx, 0);
sprite_rect(&ctx, 10, 10, 50, 30, 1);
sprite_flush(&ctx);

// AI
float loss, result;
sprite_ai_train(&ctx, 100, &loss);
sprite_ai_infer(&ctx, 1.0, 0.0, &result);
sprite_ai_save(&ctx, "/model.aif32");
```

---

## Error Handling

**Python:** All methods raise `SpriteOneError` on failure.

```python
from sprite_one import SpriteOneError

try:
    sprite.ai_load("/missing.aif32")
except SpriteOneError as e:
    print(f"Error: {e}")
```

**C:** All functions return `bool` (true = success).

---

## Protocol

| Byte | Description |
|------|-------------|
| 0 | Command |
| 1 | Length |
| 2-N | Payload |
| N+1 | Checksum |

See [protocol.md](protocol.md) for details.

---

## Performance

| Operation | Time |
|-----------|------|
| ai_train(100) | ~3s |
| ai_infer() | <1ms |
| ai_save/load | ~10ms |
| model_upload (1KB) | ~100ms |

---

## Limits

| Parameter | Value |
|-----------|-------|
| Display | 128x64 |
| Max payload | 255 bytes |
| Max model name | 15 chars |
| Flash storage | ~1.9MB |
