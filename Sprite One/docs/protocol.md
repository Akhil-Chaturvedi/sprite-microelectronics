# Sprite-Stream Protocol v1.0

The communication protocol between Host MCU and Sprite One accelerator.

## Packet Format

```
┌────────┬─────────┬────────┬─────────────┬──────────┐
│ HEADER │ COMMAND │ LENGTH │   PAYLOAD   │ CHECKSUM │
│  0xAA  │ 1 byte  │ 1 byte │ 0-255 bytes │   XOR    │
└────────┴─────────┴────────┴─────────────┴──────────┘
```

- **HEADER**: Always `0xAA` (start of packet marker)
- **COMMAND**: Operation code (see table below)
- **LENGTH**: Payload length (0-255)
- **PAYLOAD**: Command-specific data
- **CHECKSUM**: XOR of all bytes from COMMAND to end of PAYLOAD

## Command Reference

### System Commands (0x00-0x0F)

| Cmd  | Name       | Payload              | Response      | Description |
|------|------------|----------------------|---------------|-------------|
| 0x00 | `NOP`      | -                    | ACK           | Ping/alive check |
| 0x01 | `INIT`     | driver_id, w, h, rot | ACK           | Initialize display |
| 0x02 | `RESET`    | -                    | ACK           | Soft reset |
| 0x0F | `VERSION`  | -                    | 3 bytes       | Protocol version |

### Graphics Commands (0x10-0x3F)

| Cmd  | Name       | Payload                    | Description |
|------|------------|----------------------------|-------------|
| 0x10 | `CLEAR`    | color_hi, color_lo         | Clear screen (RGB565) |
| 0x11 | `PIXEL`    | x_hi, x_lo, y_hi, y_lo, c_hi, c_lo | Draw pixel |
| 0x12 | `RECT`     | x, y, w, h, c_hi, c_lo     | Filled rectangle |
| 0x13 | `RECT_OUT` | x, y, w, h, c_hi, c_lo     | Rectangle outline |
| 0x14 | `LINE`     | x1, y1, x2, y2, c_hi, c_lo | Draw line |
| 0x15 | `CIRCLE`   | cx, cy, r, c_hi, c_lo      | Filled circle |
| 0x20 | `SPRITE`   | id, x_hi, x_lo, y_hi, y_lo | Draw sprite at position |
| 0x21 | `TEXT`     | x, y, color_hi, color_lo, len, chars... | Draw text |
| 0x2F | `FLUSH`    | -                          | Push framebuffer |

### Asset Commands (0x40-0x4F)

| Cmd  | Name         | Payload                       | Description |
|------|--------------|-------------------------------|-------------|
| 0x40 | `LOAD_SPRITE`| id, w, h, data...             | Load sprite to RAM |
| 0x41 | `LOAD_FONT`  | id, char_w, char_h, data...   | Load font |
| 0x42 | `STORE_FLASH`| id                            | Move asset to flash |

### AI/Math Commands (0x50-0x5F) - Future

| Cmd  | Name         | Payload               | Description |
|------|--------------|----------------------|-------------|
| 0x50 | `LOAD_MODEL` | id, len, weights...  | Load neural network |
| 0x51 | `INFER`      | model_id, input_len, data... | Run inference |
| 0x52 | `GET_RESULT` | -                    | Get last inference result |

## Display Driver IDs

| ID   | Display         | Resolution | Interface |
|------|-----------------|------------|-----------|
| 0x01 | ILI9341         | 320x240    | SPI       |
| 0x02 | ST7789          | 240x240    | SPI       |
| 0x03 | SSD1306         | 128x64     | I2C/SPI   |
| 0x04 | ST7735          | 128x160    | SPI       |
| 0x05 | ILI9488         | 480x320    | SPI       |
| 0x10 | Generic SPI     | Custom     | SPI       |

## Response Format

```
┌────────┬────────┬─────────────┐
│ STATUS │ LENGTH │    DATA     │
└────────┴────────┴─────────────┘
```

- `0x00`: ACK (success)
- `0x01`: NAK (error)
- `0x02`: BUSY (try again)
- `0xFF`: DATA (followed by response payload)

## Example: Draw a Red Rectangle

```
Host sends: AA 12 06 10 20 30 40 F8 00 [checksum]
            ││ ││ ││ ││ ││ ││ ││ ││ ││
            ││ ││ ││ ││ ││ ││ ││ └┴─── Color: 0xF800 (red in RGB565)
            ││ ││ ││ ││ ││ ││ └─────── Height: 64
            ││ ││ ││ ││ ││ └────────── Width: 48
            ││ ││ ││ ││ └───────────── Y: 32
            ││ ││ ││ └──────────────── X: 16
            ││ ││ └─────────────────── Payload length: 6 bytes
            ││ └────────────────────── Command: RECT
            └───────────────────────── Header
```
