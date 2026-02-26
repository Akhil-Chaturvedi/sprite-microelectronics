# Sprite One Testing Guide

## Test Organization

```
host/python/
├── verify_hardware.py   # Hardware verification script (requires device or mock)
├── test_suite.py        # Integration tests (requires hardware)
├── unit_tests.py        # Unit tests (no hardware needed)
└── sprite_one.py        # Library under test

tools/
├── mock_device.py       # Python protocol simulator (no hardware needed)
```

---

## Running Tests

### Unit Tests (no hardware)

Tests the protocol layer — packet construction, CRC calculation, response parsing, error handling.

```bash
cd host/python
py unit_tests.py
```

### Mock Device Tests (no hardware)

Built-in loopback and API tests in the mock device, covering protocol framing and Industrial API Primitives.

```bash
py tools/mock_device.py --loopback     # CRC validation + chunked upload
py tools/mock_device.py --test-api     # Industrial primitives (0xA0-0xA7)
```

All assertions print pass/fail and exit with code 0 on success.

### Integration Tests (requires hardware)

Full test suite against a connected Sprite One device.

```bash
py host/python/test_suite.py --port COM3
py host/python/test_suite.py --port COM3 --verbose
py host/python/test_suite.py --port COM3 --stress 50
```

**Test categories:**
1. Connection (version check)
2. Graphics (clear, pixel, rect, text, flush)
3. AI Training (train, check status, loss value)
4. AI Inference (XOR test cases)
5. AI Persistence (save, load, list, delete)
6. Model Management (upload, select, delete)
7. Fine-tuning (start, data, stop)
8. Industrial Primitives (ID, buffer, baseline, delta, correlate)
9. Error Handling (invalid operations, bad CRC)

### Hardware Verification Script

End-to-end verification covering all implemented features.

```bash
py host/python/verify_hardware.py COM3
```

---

## Test Results Format

```
[1] Connection
  OK Version (2.2.0)

[2] Industrial API
  OK Device ID: xx:xx:xx:xx:xx:xx:xx:xx
  OK Ping ID match
  OK Buffer write x10
  OK Snapshot match
  OK Baseline capture
  OK Delta non-zero
  OK Baseline reset
  OK Correlation score

...

RESULT: SUCCESS
```

---

## CI Integration

The unit tests and mock device tests do not require hardware and are suitable for CI:

```yaml
name: Test
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      - run: pip install -r host/python/requirements.txt
      - run: py host/python/unit_tests.py
      - run: py tools/mock_device.py --loopback
      - run: py tools/mock_device.py --test-api
```

---

## Troubleshooting

| Problem | Check |
|---|---|
| Connection failed | Port name correct? Firmware uploaded? Other serial monitors closed? |
| Model not loaded | Run `model_select()` before inference or training tests |
| Timeout on training | Reduce epoch count; AI training blocks the command loop |
| CRC errors | Corrupted cable or USB hub; connect Pico directly |
