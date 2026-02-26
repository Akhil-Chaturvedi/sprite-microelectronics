# Sprite One Tools

## `mock_device.py`

Python-based protocol simulator. Mirrors the firmware command table for testing the Python host library without hardware. Implements CRC32 validation, per-chunk ACK for model uploads, and handlers for all command groups including Industrial API Primitives (0xA0–0xA7).

**Built-in test modes:**

```bash
# Protocol correctness + chunked upload
py mock_device.py --loopback

# Industrial API Primitives (0xA0-0xA7) with assertions
py mock_device.py --test-api

# Listen on a real serial port (e.g. via virtual COM pair)
py mock_device.py COM10
```

---

## `gen_sentinel_model.py`

Generates `.aif32` V3 model files from Python. Produces all reference models used by the test suite and web trainer.

**Output files:**

| File | Architecture | Purpose |
|---|---|---|
| `sentinel_god_v3.aif32` | Conv2D → MaxPool → Flatten → Dense → Sigmoid | Main vision model |
| `vision_demo.aif32` | Deeper CNN variant | Vision pipeline demo |
| `test_sigmoid.aif32` | Dense → Sigmoid | Activation sanity check |
| `test_comprehensive.aif32` | Dense → ReLU → Dense → Sigmoid | Multi-layer test |
| `xor_random.aif32` | Dense → Sigmoid (randomized weights) | Training convergence test |

All output files are written to the project root.

```bash
py gen_sentinel_model.py
```

---

## `converter/`

Converts TensorFlow Lite (`.tflite`) and Keras (`.h5`) models to `.aif32` V3 format. No TensorFlow installation required for `.tflite` input (uses a built-in FlatBuffer parser).

```bash
py converter/convert.py model.tflite -o output.aif32
py converter/convert.py model.h5 -o output.aif32
```

See `converter/README.md` for supported layer types and known limitations.
