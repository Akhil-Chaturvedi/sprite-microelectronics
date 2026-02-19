# Sprite One Tools

## `gen_sentinel_model.py`

Generates `.aif32` V3 model files from Python. Used to produce all reference models in the project.

**What it generates:**

| Output file | Architecture | Purpose |
|---|---|---|
| `sentinel_god_v3.aif32` | Conv2D → MaxPool → Flatten → Dense → Sigmoid | Main vision model |
| `vision_demo.aif32` | Deeper CNN variant | Vision pipeline demo |
| `test_sigmoid.aif32` | Dense → Sigmoid | Activation sanity check |
| `test_comprehensive.aif32` | Dense → ReLU → Dense → Sigmoid | Multi-layer test |
| `xor_random.aif32` | Dense → Sigmoid (randomized weights) | Training convergence test |

**Usage:**

```bash
python gen_sentinel_model.py
```

All output files are written to the project root. Copy them to the `webapp/` folder or upload directly via the web trainer.

---

## `mock_device.py`

Python-based simulation of the Sprite One protocol. Mirrors the JavaScript mock device (`webapp/js/mock_device.js`) for testing the Python host library without hardware.

Implements the same command table as the firmware (0x50–0x67), including CRC32 validation and per-chunk ACK on model upload.

**Usage:**

```bash
# Run the test script against the mock
python mock_device.py
```

---

## `converter/`

Converts TensorFlow Lite (`.tflite`) and Keras (`.h5`) models to `.aif32` V3 format.

```bash
python converter/convert.py model.tflite -o output.aif32
python converter/convert.py model.h5 -o output.aif32
```

See `converter/README.md` for details.
