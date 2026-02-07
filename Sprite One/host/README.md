# Sprite One Host Libraries

Host-side libraries for controlling Sprite One from PC or embedded systems.

## Directory Structure

```
host/
├── python/              # Python library
│   ├── sprite_one.py    # Main library
│   ├── examples.py      # Usage examples
│   └── requirements.txt # Dependencies
│
└── c/                   # C library for embedded
    ├── sprite_one.h     # Header file
    └── sprite_one.c     # Implementation
```

## Python Library

### Installation

```bash
cd host/python
pip install -r requirements.txt
```

### Quick Start

```python
from sprite_one import SpriteOne

# Connect
with SpriteOne('COM3') as sprite:  # or '/dev/ttyUSB0'
    # Train AI
    sprite.ai_train(epochs=100)
    
    # Run inference
    result = sprite.ai_infer(1.0, 0.0)
    print(f"1 XOR 0 = {result}")
    
    # Graphics
    sprite.clear()
    sprite.rect(10, 10, 50, 30)
    sprite.flush()
```

### API Reference

**System:**
- `sprite.get_version()` - Get firmware version

**Graphics:**
- `sprite.clear(color=0)` - Clear display
- `sprite.pixel(x, y, color=1)` - Draw pixel
- `sprite.rect(x, y, w, h, color=1)` - Draw rectangle
- `sprite.text(x, y, text, color=1)` - Draw text
- `sprite.flush()` - Update display

**AI:**
- `sprite.ai_infer(in0, in1)` - Run inference
- `sprite.ai_train(epochs=100)` - Train model
- `sprite.ai_status()` - Get AI status
- `sprite.ai_save(filename)` - Save model
- `sprite.ai_load(filename)` - Load model
- `sprite.ai_list_models()` - List saved models

## C Library

### Platform Integration

The C library uses function pointers for UART operations, making it platform-agnostic.

```c
#include "sprite_one.h"

// Implement these for your platform
void uart_write(uint8_t byte) { /* ... */ }
uint8_t uart_read(void) { /* ... */ }
bool uart_available(void) { /* ... */ }

// Initialize
sprite_context_t sprite;
sprite_init(&sprite, uart_write, uart_read, uart_available, 2000);

// Use it
float result;
sprite_ai_infer(&sprite, 1.0, 0.0, &result);
```

### Example (ESP32)

```c
// ESP32 UART wrapper
void esp32_write(uint8_t b) {
    Serial2.write(b);
}

uint8_t esp32_read(void) {
    return Serial2.read();
}

bool esp32_available(void) {
    return Serial2.available() > 0;
}

void setup() {
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    
    sprite_context_t sprite;
    sprite_init(&sprite, esp32_write, esp32_read, 
                esp32_available, 2000);
    
    // Train model
    float loss;
    sprite_ai_train(&sprite, 100, &loss);
    
    // Test
    float result;
    sprite_ai_infer(&sprite, 1.0, 0.0, &result);
}
```

## Protocol Format

All commands use this packet structure:

**Request:**
```
[0xAA] [CMD] [LEN] [PAYLOAD...] [CHECKSUM]
```

**Response:**
```
[0xAA] [CMD] [STATUS] [LEN] [DATA...] [CHECKSUM]
```

## License

MIT License - See main project README
