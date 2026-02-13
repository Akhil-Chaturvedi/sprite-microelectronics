# Sprite One Tools

This directory contains utility scripts for model generation, conversion, and device simulation.

## Scripts

### `gen_sentinel_model.py`

Generates a custom `.aif32` binary model for the **Sentinel God Mode** demo. This script demonstrates how to manually pack a V2-compatible AIfES model binary, including:
- Header creation (Magic, Version, Input/Output sizes)
- Packing weights and biases for Dense layers
- CRC32 checksum calculation

**Usage:**
```bash
python gen_sentinel_model.py
```
This will create `sentinel_god.aif32` in the current directory, ready to be uploaded to the Sprite One WebApp or copied to the device's LittleFS.

### `mock_device.py`

A Python-based simulation of the Sprite One device protocol. Useful for testing host-side libraries without physical hardware.

## Subdirectories

- **[converter/](converter/)**: Tools for converting TensorFlow Lite (`.tflite`) and Keras (`.h5`) models to the `.aif32` format.
