# Sprite-Stream Protocol v1.2

Binary protocol between a host (PC, MCU, or browser via WebSerial) and a Sprite One device.

---

## Packet Format

### Request (host → device)

```
[ 0xAA ] [ CMD ] [ LEN ] [ PAYLOAD × LEN bytes ] [ CRC32 (4 bytes) ]
```

### Response (device → host)

```
[ 0xAA ] [ CMD ] [ STATUS ] [ LEN ] [ DATA × LEN bytes ] [ CRC32 (4 bytes) ]
```

| Field | Size | Notes |
|---|---|---|
| Header | 1 B | Always `0xAA` |
| CMD | 1 B | Command opcode |
| STATUS (response) | 1 B | `0x00` = OK, `0x01` = Error, `0x02` = Not Found |
| LEN | 1 B | Payload / data length (0–255) |
| PAYLOAD / DATA | 0–255 B | Command-specific |
| CRC32 | 4 B | CRC32 of all bytes from CMD through end of PAYLOAD/DATA, little-endian |

CRC32 is appended to both requests and responses. The Python library (`host/python/sprite_one.py`) and the Python mock device (`tools/mock_device.py`) both compute and validate CRC32 on every packet.

**STATUS byte values:**
- `0x00` — OK / success
- `0x01` — Error
- `0x02` — Not Found

All multi-byte integers in payloads are **little-endian** unless noted.

> **Queued command payload cap:** The dual-core command queue stores up to 64 bytes of payload per entry. Commands with larger payloads (e.g. `MODEL_UPLOAD 0x63`) are parsed synchronously on Core 0 and bypass the queue.

---

## Command Reference

### System (0x00–0x0F)

| Cmd | Name | Payload | Response | Description |
|---|---|---|---|---|
| `0x00` | `NOP` | — | ACK | Alive check |
| `0x02` | `RESET` | — | ACK | Soft-reset device state |
| `0x0E` | `BUFFER_STATUS` | — | `free_space` (u16) | RX buffer space for flow control |
| `0x0F` | `VERSION` | — | 3 bytes: `major, minor, patch` | Firmware version |

---

### Graphics (0x10–0x2F)

Display is 128×64 monochrome (SSD1306). Coordinates in pixels. Color is `0` (off) or `1` (on).

| Cmd | Name | Payload | Description |
|---|---|---|---|
| `0x10` | `CLEAR` | `color` (u8) | Fill entire display |
| `0x11` | `PIXEL` | `x, y, color` (u8 each) | Draw single pixel |
| `0x12` | `RECT` | `x, y, w, h, color` (u8 each) | Filled rectangle |
| `0x21` | `TEXT` | `x, y, color` (u8 each), then ASCII string | Draw text using built-in font |
| `0x2F` | `FLUSH` | — | Push framebuffer to display |

Changes to the framebuffer are not visible until `FLUSH` is sent.

---

### Sprite System (0x30–0x3F)

Up to 8 sprites, Z-ordered by slot ID. All positions use signed 16-bit integers.

| Cmd | Name | Payload | Description |
|---|---|---|---|
| `0x30` | `SPRITE_CREATE` | `id(1), x(2), y(2), w(1), h(1), layer(1), [flags(1)], [pixels(ceil(w*h/8) bytes)]` | Create sprite |
| `0x31` | `SPRITE_MOVE` | `id(1), x(2), y(2)` | Move sprite |
| `0x32` | `SPRITE_DELETE` | `id` (u8) | Free slot |
| `0x33` | `SPRITE_VISIBLE` | `id, visible` (u8 each) | Show or hide |
| `0x34` | `SPRITE_COLLISION` | `id_a, id_b` (u8 each) | Returns `1` if bounding boxes overlap |
| `0x35` | `SPRITE_RENDER` | — | Composite visible sprites into framebuffer |
| `0x36` | `SPRITE_CLEAR` | — | Remove all sprites |

---

### AI / Inference (0x50–0x5F)

Commands operate on the active model. Load a model with `MODEL_SELECT` (`0x62`) before using these.

#### `0x50` — `AI_INFER`

Run a single forward pass.

- **Payload:** two `float32` values (`in0`, `in1`) — legacy 2-input signature. For models with other input sizes, use `FINETUNE_DATA` (0x66) which infers dimensions from the loaded model.
- **Response data:** `float32` output

#### `0x51` — `AI_TRAIN`

Run one training step on the active model (100 epochs over the hardcoded XOR dataset if no dynamic model is loaded).

- **Payload:** `epochs` (u8)
- **Response data:** `float32` final loss

#### `0x52` — `AI_STATUS`

- **Response data:** 12 bytes

| Offset | Field | Type | Notes |
|---|---|---|---|
| 0 | `state` | u8 | 0=idle, 1=training |
| 1 | `model_loaded` | u8 | 0=none, 1=static, 2=dynamic V3 |
| 2 | `epochs` | u16 | Total training steps executed |
| 4 | `last_loss` | f32 | Loss from most recent training step |
| 8 | `input_dim` | u16 | Active model input count |
| 10 | `output_dim` | u16 | Active model output count |

#### `0x53` — `AI_SAVE` / `0x54` — `AI_LOAD`

- **Payload:** filename string (max 31 chars, no null terminator required)
- Operates on the legacy static model slot. Use `MODEL_UPLOAD` + `MODEL_SELECT` for V3 dynamic models.

#### `0x55` — `AI_LIST`

- **Response data:** length-prefixed filename list (each entry: `len(u8)` then ASCII bytes)

#### `0x56` — `AI_DELETE`

- **Payload:** filename string

---

### Model Management (0x60–0x6F)

Dynamic V3 model management. Models are stored in LittleFS.

#### `0x60` — `MODEL_INFO`

- **Response data:** 32 bytes (ModelHeader structure): magic (u32), version (u16), input/output/hidden counts (u8), model type (u8), reserved (u16), CRC32 (u32), name (16 bytes ASCII)

#### `0x61` — `MODEL_LIST`

- **Response data:** length-prefixed filename list (same format as `AI_LIST`)

#### `0x62` — `MODEL_SELECT`

- **Payload:** filename string
- Reads the file from flash, loads it as a V3 dynamic model, and activates it for inference and training. Returns error if the file does not exist or the format is invalid.

#### `0x63` — `MODEL_UPLOAD` (start)

Begin a chunked file upload. The host sends the filename, then follows with `CMD_UPLOAD_CHUNK` (0x68) packets, then `CMD_UPLOAD_END` (0x69).

- **Payload:** filename string

#### `0x68` — `UPLOAD_CHUNK`

- **Payload:** raw bytes (up to 200 bytes per chunk)
- Device ACKs each chunk before the host sends the next.

#### `0x69` — `UPLOAD_END`

- **Payload:** expected CRC32 (u32, little-endian) — device verifies the full file CRC before ACKing

#### `0x64` — `MODEL_DELETE`

- **Payload:** filename string

#### `0x65` — `FINETUNE_START`

Allocate gradient and momentum memory for the Adam optimizer on the active model. Must be called before `FINETUNE_DATA`.

- **Payload:** `learning_rate` (f32). Default 0.01 if payload is omitted.

#### `0x66` — `FINETUNE_DATA`

Run one forward + backward + weight-update step. The device uses the loaded model's `input_count` and `output_count` to parse the payload.

- **Payload:** `input_count × float32` inputs followed by `output_count × float32` targets
- **Response data:** `float32` loss for this step

#### `0x67` — `FINETUNE_STOP`

Finalize the training session. Currently a no-op placeholder — the AIfES optimizer does not require explicit teardown. Future versions may persist updated weights here.

---

### Industrial API Primitives (0xA0–0xA7)

Low-level signal processing primitives. These are general-purpose building blocks; the interpretation of the data is left to the host.

| Cmd | Name | Payload | Response | Description |
|---|---|---|---|---|
| `0xA0` | `WHO_IS_THERE` | — | 8 bytes device ID | Returns the board-unique 64-bit ID from `pico_get_unique_board_id()` |
| `0xA1` | `PING_ID` | 8 bytes ID | ACK or Error | ACK only if payload matches this device's ID |
| `0xA2` | `BUFFER_WRITE` | `float32` sample | ACK | Push one sample into the 60-slot circular buffer |
| `0xA3` | `BUFFER_SNAPSHOT` | — | `N × float32` | Return ordered buffer contents (oldest first). Empty payload if buffer is empty. |
| `0xA4` | `BASELINE_CAPTURE` | — | `float32` mean | Freeze current buffer mean as baseline |
| `0xA5` | `BASELINE_RESET` | — | ACK | Clear baseline |
| `0xA6` | `GET_DELTA` | — | `float32` delta | `abs(live_mean - baseline)`. Error if no baseline is set. |
| `0xA7` | `CORRELATE` | `N × float32` reference | `float32` score | Normalized cross-correlation of buffer vs reference. Score mapped to [0.0, 1.0]. |

The circular buffer holds up to 60 `float32` samples. Writes beyond 60 evict the oldest entry.

---

### Batch (0x70)

#### `0x70` — `BATCH`

Execute multiple commands in a single packet.

- **Payload:** one or more sub-commands, each prefixed with `CMD (u8)` and `LEN (u8)` then their payload bytes
- Commands are executed sequentially. Each sub-command sends its own response before the next executes.

---

## Model Format (.aif32 V3)

```
[ Header — 64 bytes ]
  magic:       0x33464941 ("AIF3")
  version:     3
  layer_count: N
  weight_size: total weight blob bytes
  name:        32-byte null-padded ASCII string

[ Layer Descriptors × layer_count ]
  Each descriptor is 18 bytes:
  type   (u8):  1=Input, 2=Dense, 3=ReLU, 4=Sigmoid, 5=Softmax,
                6=Conv2D, 7=Flatten, 8=MaxPool
  flags  (u8):  reserved
  param1–param6 (u16 each): layer-specific parameters

[ Weight Blob ]
  Concatenated float32 weights and biases for all parametric layers.
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
# Request: AI_INFER (0x50), two float32 inputs: 1.0, 0.0
AA 50 08 00 00 80 3F 00 00 00 00 [CRC32 4 bytes]

# Response: OK + one float32 output: ~0.98
AA 50 00 04 71 3D 7A 3F [CRC32 4 bytes]
```

---

## Notes

- No flow control. For chunked uploads, wait for an ACK after each chunk before sending the next.
- WebSerial (Chrome and Edge only) is required for browser-based communication.
- The Python library (`host/python/sprite_one.py`) handles all packet framing, CRC calculation, and chunked upload internally.
