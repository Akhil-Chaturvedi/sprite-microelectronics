# Sprite One - API Reference

Complete API documentation for Sprite One host libraries.

---

## Python API

### SpriteOne Class

```python
class SpriteOne:
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 2.0)
```

Creates connection to Sprite One.

**Parameters:**
- `port` - Serial port (e.g., `'COM3'`, `'/dev/ttyUSB0'`)
- `baudrate` - Serial baudrate (default: 115200)
- `timeout` - Read timeout in seconds (default: 2.0)

**Example:**
```python
sprite = SpriteOne('COM3')
# or with context manager:
with SpriteOne('/dev/ttyUSB0') as sprite:
    # Use sprite...
```

---

### System Commands

#### get_version()

Get firmware version.

**Returns:** `tuple[int, int, int]` - (major, minor, patch)

**Example:**
```python
version = sprite.get_version()
print(f"v{version[0]}.{version[1]}.{version[2]}")  # v1.0.0
```

---

### Graphics Commands

#### clear(color=0)

Clear display to specified color.

**Parameters:**
- `color` - Fill color (0 or 1)

**Example:**
```python
sprite.clear()  # Clear to black
sprite.clear(1)  # Clear to white
```

#### pixel(x, y, color=1)

Draw single pixel.

**Parameters:**
- `x`, `y` - Coordinates
- `color` - Pixel color (0 or 1)

**Example:**
```python
sprite.pixel(64, 32, 1)  # Draw white pixel at center
```

#### rect(x, y, w, h, color=1)

Draw filled rectangle.

**Parameters:**
- `x`, `y` - Top-left corner
- `w`, `h` - Width and height
- `color` - Fill color (0 or 1)

**Example:**
```python
sprite.rect(10, 10, 50, 30, 1)  # 50x30 white rectangle
```

#### text(x, y, text, color=1)

Draw text string.

**Parameters:**
- `x`, `y` - Starting position
- `text` - String to draw
- `color` - Text color (0 or 1)

**Example:**
```python
sprite.text(5, 5, "Hello World!", 1)
```

#### flush()

Update display with drawn content.

**Example:**
```python
sprite.clear()
sprite.rect(10, 10, 20, 20)
sprite.flush()  # Display changes
```

---

### AI Commands

#### ai_train(epochs=100)

Train AI model.

**Parameters:**
- `epochs` - Number of training epochs (default: 100)

**Returns:** `float` - Final loss value

**Raises:** `SpriteOneError` on failure

**Example:**
```python
loss = sprite.ai_train(epochs=100)
print(f"Final loss: {loss:.6f}")
```

**Time:** ~3 seconds for 100 epochs

#### ai_infer(input0, input1)

Run inference on loaded model.

**Parameters:**
- `input0`, `input1` - Input values (float)

**Returns:** `float` - Output value

**Raises:** 
- `SpriteOneError` if model not loaded

**Example:**
```python
result = sprite.ai_infer(1.0, 0.0)
print(f"1 XOR 0 = {result:.3f}")  # ~0.978
```

**Time:** <1ms

#### ai_status()

Get AI engine status.

**Returns:** `dict` with keys:
- `state` (int) - Current state (0=idle, 1=training, etc.)
- `model_loaded` (bool) - Whether model is loaded
- `epochs` (int) - Total epochs trained
- `last_loss` (float) - Most recent loss value

**Example:**
```python
status = sprite.ai_status()
if status['model_loaded']:
    print(f"Model ready! Loss: {status['last_loss']:.6f}")
```

#### ai_save(filename="/model.aif32")

Save current model to flash.

**Parameters:**
- `filename` - Model filename (default: `"/model.aif32"`)

**Raises:**
- `SpriteOneError` if no model loaded or save fails

**Example:**
```python
sprite.ai_train(100)
sprite.ai_save("/xor.aif32")
```

#### ai_load(filename="/model.aif32")

Load model from flash.

**Parameters:**
- `filename` - Model filename

**Raises:**
- `SpriteOneError` if file not found or corrupted

**Example:**
```python
sprite.ai_load("/xor.aif32")
result = sprite.ai_infer(1.0, 0.0)
```

#### ai_list_models()

List all saved models.

**Returns:** `list[str]` - Filenames of saved models

**Example:**
```python
models = sprite.ai_list_models()
print(f"Found {len(models)} models:")
for model in models:
    print(f"  - {model}")
```

#### ai_delete(filename)

Delete a saved model.

**Parameters:**
- `filename` - Model file to delete

**Raises:**
- `SpriteOneError` if file not found

**Example:**
```python
sprite.ai_delete("/old_model.aif32")
```

---

## C API

### Initialization

#### sprite_init()

```c
void sprite_init(sprite_context_t* ctx,
                 sprite_uart_write_fn write_fn,
                 sprite_uart_read_fn read_fn,
                 sprite_uart_available_fn available_fn,
                 uint32_t timeout_ms);
```

Initialize Sprite One context.

**Parameters:**
- `ctx` - Context structure to initialize
- `write_fn` - Function to write byte to UART
- `read_fn` - Function to read byte from UART
- `available_fn` - Function to check if data available
- `timeout_ms` - Response timeout

**Example:**
```c
sprite_context_t sprite;
sprite_init(&sprite, uart_write, uart_read, uart_available, 2000);
```

---

### System

#### sprite_get_version()

```c
bool sprite_get_version(sprite_context_t* ctx, 
                        uint8_t* major, uint8_t* minor, uint8_t* patch);
```

Get firmware version.

**Returns:** `true` on success

**Example:**
```c
uint8_t major, minor, patch;
if (sprite_get_version(&sprite, &major, &minor, &patch)) {
    printf("v%d.%d.%d\n", major, minor, patch);
}
```

---

### Graphics

#### sprite_clear()

```c
bool sprite_clear(sprite_context_t* ctx, uint8_t color);
```

#### sprite_pixel()

```c
bool sprite_pixel(sprite_context_t* ctx, int16_t x, int16_t y, uint8_t color);
```

#### sprite_rect()

```c
bool sprite_rect(sprite_context_t* ctx, 
                 int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
```

#### sprite_text()

```c
bool sprite_text(sprite_context_t* ctx, 
                 int16_t x, int16_t y, const char* text, uint8_t color);
```

#### sprite_flush()

```c
bool sprite_flush(sprite_context_t* ctx);
```

---

### AI

#### sprite_ai_train()

```c
bool sprite_ai_train(sprite_context_t* ctx, 
                     uint8_t epochs, float* final_loss);
```

Train model.

**Example:**
```c
float loss;
if (sprite_ai_train(&sprite, 100, &loss)) {
    printf("Loss: %f\n", loss);
}
```

#### sprite_ai_infer()

```c
bool sprite_ai_infer(sprite_context_t* ctx, 
                     float input0, float input1, float* output);
```

Run inference.

**Example:**
```c
float result;
if (sprite_ai_infer(&sprite, 1.0, 0.0, &result)) {
    printf("Result: %f\n", result);
}
```

#### sprite_ai_status()

```c
bool sprite_ai_status(sprite_context_t* ctx, sprite_ai_status_t* status);
```

Get AI status.

**Example:**
```c
sprite_ai_status_t status;
if (sprite_ai_status(&sprite, &status)) {
    printf("Model loaded: %d\n", status.model_loaded);
}
```

#### sprite_ai_save() / sprite_ai_load()

```c
bool sprite_ai_save(sprite_context_t* ctx, const char* filename);
bool sprite_ai_load(sprite_context_t* ctx, const char* filename);
```

Save/load models.

**Example:**
```c
sprite_ai_save(&sprite, "/model.aif32");
sprite_ai_load(&sprite, "/model.aif32");
```

---

## Error Handling

### Python

All functions raise `SpriteOneError` on failure:

```python
from sprite_one import SpriteOneError

try:
    sprite.ai_load("/nonexistent.aif32")
except SpriteOneError as e:
    print(f"Error: {e}")
```

### C

All functions return `bool`:
- `true` - Success
- `false` - Failure

```c
if (!sprite_ai_load(&sprite, "/model.aif32")) {
    printf("Load failed!\n");
}
```

---

## Protocol Details

See [PROTOCOL.md](PROTOCOL.md) for low-level protocol specification.

---

## Performance

| Operation | Time | Notes |
|-----------|------|-------|
| `ai_train(100)` | ~3s | XOR network, 100 epochs |
| `ai_infer()` | <1ms | Single inference |
| `ai_save()` | ~10ms | Save to flash |
| `ai_load()` | ~10ms | Load from flash |
| `flush()` | <1ms | Display update |

---

## Limits

| Parameter | Limit |
|-----------|-------|
| Display Size | 128x64 |
| Model Inputs | 2 (current XOR example) |
| Model Outputs | 1 (current XOR example) |
| Max Epochs | 1000 |
| Max Payload | 255 bytes |
| Flash Models | Limited by filesystem (~1.9MB) |

---

## Examples

See `examples/` directory for complete working examples.
