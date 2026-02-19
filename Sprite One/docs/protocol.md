# Sprite-Stream Protocol v1.1

Binary protocol between a host (PC, MCU, or browser via WebSerial) and a Sprite One device.

---

## Packet Format

### Request (host → device)

```
[ 0xAA ] [ CMD ] [ LEN ] [ PAYLOAD × LEN bytes ] [ CRC32 × 4 bytes ]
```

### Response (device → host)

```
[ 0xAA ] [ CMD ] [ STATUS ] [ LEN ] [ DATA × LEN bytes ] [ CRC32 × 4 bytes ]
```

| Field | Size | Notes |
|---|---|---|
| Header | 1 B | Always `0xAA` |
| CMD | 1 B | Command opcode |
| LEN | 1 B | Payload length (0–255) |
| PAYLOAD | 0–255 B | Command-specific |
| CRC32 | 4 B | CRC32 of CMD + LEN + PAYLOAD, little-endian |

**STATUS byte values:**
- `0x00` — ACK / success
- `0x01` — Error / NAK
- `0x02` — Not found
- `0xFF` — DATA follows (response includes a data payload)

All multi-byte integers in payloads are **little-endian** unless noted.

---

## Command Reference

### System (0x00–0x0F)

| Cmd | Name | Payload | Response | Description |
|---|---|---|---|---|
| `0x00` | `NOP` | — | ACK | Ping / alive check |
| `0x02` | `RESET` | — | ACK | Soft-reset device state |
| `0x0E` | `BUFFER_STATUS` | — | 4 bytes: `dirty_x1, dirty_y1, dirty_x2, dirty_y2` (u8 each) | Query dirty rect |
| `0x0F` | `VERSION` | — | 3 bytes: `major, minor, patch` | Firmware version |

---

### Graphics (0x10–0x2F)

Display is 128×64 monochrome (SSD1306). Coordinates are in pixels. Color is `0` (off) or `1` (on).

| Cmd | Name | Payload | Description |
|---|---|---|---|
| `0x10` | `CLEAR` | `color` (u8) | Fill entire display |
| `0x11` | `PIXEL` | `x, y, color` (u8 each) | Draw single pixel |
| `0x12` | `RECT` | `x, y, w, h, color` (u8 each) | Filled rectangle |
| `0x21` | `TEXT` | `x, y, color` (u8 each), then ASCII string | Draw text using built-in font |
| `0x2F` | `FLUSH` | — | Push framebuffer to display |

Changes to the framebuffer are **not visible** until `FLUSH` is sent.

---

### Sprite System (0x30–0x3F)

Hardware sprite layer on top of the framebuffer. Up to 8 sprites, Z-ordered by ID.

| Cmd | Name | Payload | Description |
|---|---|---|---|
| `0x30` | `SPRITE_CREATE` | `id, w, h` (u8), then pixel data | Load sprite bitmap into slot `id` |
| `0x31` | `SPRITE_MOVE` | `id, x, y` (u8 each) | Move sprite to position |
| `0x32` | `SPRITE_DELETE` | `id` (u8) | Free sprite slot |
| `0x33` | `SPRITE_VISIBLE` | `id, visible` (u8 each) | Show or hide sprite |
| `0x34` | `SPRITE_COLLISION` | `id_a, id_b` (u8 each) | Returns `1` if bounding boxes overlap |
| `0x35` | `SPRITE_RENDER` | — | Composite all visible sprites then flush |
| `0x36` | `SPRITE_CLEAR` | — | Remove all sprites |

---

### AI / Inference (0x50–0x5F)

Commands operate on the **active model**. Switch models with `MODEL_SELECT` (`0x62`) before using these.

#### `0x50` — `AI_INFER`

Run a single forward pass on the active model.

- **Payload:** `N × float32` input values (little-endian), where N = model input count
- **Response data:** `M × float32` output values, where M = model output count

#### `0x51` — `AI_TRAIN`

Run one gradient-descent training step on the active model.

- **Payload:** `N × float32` inputs followed by `M × float32` targets
- **Response data:** `float32` loss value for this step
- Requires `FINETUNE_START` (`0x65`) to be called first to initialise the optimizer

#### `0x52` — `AI_STATUS`

- **Response data:** 9 bytes: `state` (u8), `model_loaded` (u8), `epochs` (u16), `last_loss` (f32)

#### `0x53` — `AI_SAVE` / `0x54` — `AI_LOAD`

- **Payload:** filename string (e.g. `/model.aif32`)

#### `0x55` — `AI_LIST`

- **Response data:** newline-separated filename list

#### `0x56` — `AI_DELETE`

- **Payload:** filename string

---

### Model Management (0x60–0x6F)

These commands work with the V3 `.aif32` dynamic model format. Models are stored in LittleFS flash.

#### `0x60` — `MODEL_INFO`

Returns metadata of the currently active model.
- **Response data:** `input_count` (u16), `output_count` (u16), `layer_count` (u16), `name` (null-terminated string)

#### `0x61` — `MODEL_LIST`

Returns newline-separated list of stored model filenames.

#### `0x62` — `MODEL_SELECT`

- **Payload:** filename string
- Loads the specified model from flash and makes it active

#### `0x63` — `MODEL_UPLOAD`

Chunked upload of a `.aif32` file. The host sends the file in 200-byte chunks. The device sends an ACK after each chunk is written.

- **Payload:** `filename` (null-terminated, max 15 chars), then `chunk_data`
- The simulator (`mock_device.js`) and Python library handle chunking automatically

#### `0x64` — `MODEL_DELETE`

- **Payload:** filename string

#### `0x65` — `FINETUNE_START`

Initialise the Adam optimizer + MSE loss function for the active model. Must be called before `AI_TRAIN` (`0x51`).

- **Payload:** `learning_rate` (f32)

#### `0x66` — `FINETUNE_DATA`

Alias for `AI_TRAIN` (`0x51`). Provided for API consistency.

#### `0x67` — `FINETUNE_STOP`

Tear down training memory. Optionally saves the model weights to flash.

---

### Batch (0x70)

#### `0x70` — `BATCH`

Execute multiple commands in a single packet to reduce round-trip overhead.

- **Payload:** one or more sub-commands, each prefixed with their own CMD and LEN bytes
- The device executes them sequentially and returns a combined response

---

## Model Format (.aif32 V3)

```
[ Header 64 bytes ]
  magic:       0x33464941 ("AIF3")
  version:     3
  layer_count: N
  weight_size: total weight blob bytes
  name:        32-byte string

[ Layer Descriptors × layer_count ]
  Each descriptor is 18 bytes:
  type   (u8):  1=Input, 2=Dense, 3=ReLU, 4=Sigmoid, 5=Softmax,
                6=Conv2D, 7=Flatten, 8=MaxPool
  flags  (u8):  reserved
  param1–param6 (u16 each): layer-specific parameters (see below)

[ Weight Blob ]
  Concatenated float32 weights and biases for all parametric layers
```

**Layer parameter map:**

| Type | param1 | param2 | param3 | param4 | param5 | param6 |
|---|---|---|---|---|---|---|
| Input (1D) | size | 0 | 0 | — | — | — |
| Input (3D) | height | width | channels | — | — | — |
| Dense | neurons | — | — | — | — | — |
| Conv2D | filters | k_h | k_w | s_h | s_w | padding |
| MaxPool | — | k_h | k_w | s_h | s_w | padding |

Weight layout for Dense: `[weights: out×in float32][biases: out float32]`
Weight layout for Conv2D: `[weights: filters×in_c×k_h×k_w float32][biases: filters float32]`

---

## Example: Run Inference

```
# Request: CMD_AI_INFER (0x50), 8 bytes payload (two float32 inputs: 1.0, 0.0)
AA 50 08 00 00 80 3F 00 00 00 00 [CRC32]

# Response: ACK + 4 bytes (one float32 output: ~0.98)
AA 50 00 04 71 3D 7A 3F [CRC32]
```

---

## Notes

- The protocol has no flow control. For chunked uploads, wait for an ACK packet after each chunk before sending the next.
- WebSerial (Chrome/Edge only) is required for browser-based communication.
- The Python library (`host/python/sprite_one.py`) implements all framing, CRC, and chunking automatically.
