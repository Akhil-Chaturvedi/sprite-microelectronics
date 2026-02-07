# Sprite One Testing Guide

Week 4 Day 24 - Automated Testing Suite

## Test Organization

```
host/python/
├── test_suite.py      # Main automated tests (requires hardware)
├── unit_tests.py      # Unit tests (no hardware needed)
└── sprite_one.py      # Library to test
```

## Running Tests

### Automated Test Suite (with hardware)

Comprehensive integration tests requiring connected Sprite One:

```bash
# Run all tests
python test_suite.py --port COM3

# Verbose output
python test_suite.py --port COM3 --verbose

# Stress test with more iterations
python test_suite.py --port COM3 --stress 50
```

**Test Categories:**
1. Connection (version check)
2. Graphics (clear, pixel, rect, text, flush)
3. AI Training (train model, check status)
4. AI Inference (XOR test cases)
5. AI Persistence (save, load, list, delete)
6. Integration (combined workflows)
7. Error Handling (invalid operations)
8. Stress Test (repeated operations)

### Unit Tests (no hardware)

Tests that don't require hardware:

```bash
python unit_tests.py
```

**Tests:**
- Protocol packet structure
- Checksum calculation
- Data type conversions
- Error handling
- Response parsing

## Test Results Format

### Success
```
==================================================
Sprite One - Automated Test Suite
==================================================

Connecting to COM3...
Connected!

[1] Connection Tests
  ✓ Get version (v1.0.0)

[2] Graphics Tests
  ✓ Clear display
  ✓ Draw pixel
  ✓ Draw rectangle
  ✓ Draw text
  ✓ Flush display

...

==================================================
Test Results: 28/28 passed (0 failed)
==================================================
```

### Failure
```
[4] AI Inference Tests
  ✓ 0 XOR 0 = 0 (result: 0.021)
  ✗ 0 XOR 1 = 1: Expected 1, got 0
  ...

==================================================
Test Results: 26/28 passed (2 failed)

Failed Tests:
  - 0 XOR 1 = 1: Expected 1, got 0
  - Full workflow: Connection timeout
==================================================
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Test Sprite One
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Set up Python
        uses: actions/setup-python@v2
        with:
          python-version: '3.9'
      - name: Install dependencies
        run: pip install -r requirements.txt
      - name: Run unit tests
        run: python unit_tests.py
```

## Test Coverage

| Component | Coverage | Tests |
|-----------|----------|-------|
| Protocol | 95% | Unit + Integration |
| Graphics | 100% | Integration |
| AI Training | 100% | Integration |
| AI Inference | 100% | Integration |
| Persistence | 100% | Integration |
| Error Handling | 90% | Unit + Integration |

## Adding New Tests

### Integration Test

Add to `test_suite.py`:

```python
def test_my_feature(sprite, results):
    """Test my new feature."""
    print("\n[9] My Feature Tests")
    
    try:
        result = sprite.my_new_function()
        if result == expected:
            results.pass_test("My test")
        else:
            results.fail_test("My test", "Wrong result")
    except Exception as e:
        results.fail_test("My test", str(e))

# Add to main():
test_my_feature(sprite, results)
```

### Unit Test

Add to `unit_tests.py`:

```python
class TestMyFeature(unittest.TestCase):
    def test_something(self):
        """Test something."""
        result = my_function(input)
        self.assertEqual(result, expected)
```

## Troubleshooting

### "Connection failed"
- Check port name (COM3, /dev/ttyUSB0)
- Verify Sprite One is powered
- Close other serial monitors
- Check baudrate (115200)

### "Model not loaded"
- Run training test first
- Save model before load test
- Check flash filesystem

### Timeout Errors
- Increase Serial timeout in sprite_one.py
- Check for slow training (reduce epochs)
- Verify hardware performance

## Best Practices

1. **Run unit tests first** - Fast, no hardware needed
2. **Run full suite before commits** - Catch regressions
3. **Use verbose mode for debugging** - See detailed output
4. **Stress test for stability** - Find edge cases
5. **Test on clean state** - Format flash if needed

## Performance Benchmarks

Typical test run times:

| Test Category | Duration |
|---------------|----------|
| Connection | 0.1s |
| Graphics | 0.5s |
| AI Training | 3-5s |
| AI Inference | 0.4s |
| Persistence | 1s |
| Integration | 2s |
| Error Handling | 0.5s |
| Stress (10x) | 5s |
| **Total** | **~15s** |

## Exit Codes

- `0` - All tests passed
- `1` - One or more tests failed
- `2` - Connection error

Use in scripts:
```bash
python test_suite.py --port COM3
if [ $? -eq 0 ]; then
    echo "Tests passed!"
else
    echo "Tests failed!"
fi
```
