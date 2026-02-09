# Model Converter Tools

Universal converter for ML models to AIFes (.aif32) format.

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Input Formats                         │
│  .h5  .keras  .pb  .json (TF.js)  .tflite                │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  Step 1: Normalize (normalize.py)                        │
│  Convert all formats to .tflite                          │
│  Requires: tensorflow (500MB+)                           │
│  Only runs if input is not .tflite                       │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  Step 2: Extract (tflite_to_aif32.py)                    │
│  Parse .tflite using FlatBuffers                         │
│  Requires: numpy only (~15MB)                            │
│  Lightweight: runs in milliseconds                       │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│                    .aif32 Output                         │
│  Ready for Sprite One device                             │
└──────────────────────────────────────────────────────────┘
```

## Files

| File | Purpose | Dependencies |
|------|---------|--------------|
| `convert.py` | Universal entry point | numpy |
| `tflite_to_aif32.py` | Lightweight TFLite parser | numpy |
| `normalize.py` | Format normalization | tensorflow |

## Usage

### Direct .tflite (Fastest)
```bash
# No TensorFlow needed
python convert.py model.tflite output.aif32
```

### From Keras .h5
```bash
pip install tensorflow
python convert.py model.h5 output.aif32
```

### From TF.js
```bash
pip install tensorflow tensorflowjs
python convert.py model.json output.aif32
```

## .aif32 Format

```
Header (32 bytes):
  0x00: magic      (4 bytes) - "SPRT" = 0x54525053
  0x04: version    (2 bytes) - Format version
  0x06: input_size (1 byte)  - Number of inputs
  0x07: output_size(1 byte)  - Number of outputs
  0x08: hidden_size(1 byte)  - Hidden layer neurons
  0x09: model_type (1 byte)  - 0=F32, 1=Q7
  0x0A: num_layers (1 byte)  - Number of layers
  0x0B: reserved   (1 byte)  - Padding
  0x0C: weights_crc(4 bytes) - CRC32 of weights
  0x10: name       (16 bytes)- Model name

Weights (variable):
  Float32 array of all weights flattened
```

## Quantization Handling

The converter automatically handles:
- Float32 models (passthrough)
- Int8/UInt8 quantized models (dequantized to F32)
- Int16 quantized models (dequantized to F32)

Dequantization formula:
```
RealValue = (QuantizedValue - ZeroPoint) * Scale
```

## Supported TFLite Operations

| Op | Name | Status |
|----|------|--------|
| 3 | Conv2D | Weights extracted |
| 4 | DepthwiseConv2D | Weights extracted |
| 9 | FullyConnected | Weights extracted |
| 19 | ReLU | Activation |
| 21 | ReLU6 | Activation |
| 25 | Softmax | Activation |
| 14 | Sigmoid | Activation |
| 28 | Tanh | Activation |

## Debugging

Use [Netron](https://netron.app/) to visualize .tflite structure and verify extraction.
