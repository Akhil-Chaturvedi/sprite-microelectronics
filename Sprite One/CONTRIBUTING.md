# Contributing to Sprite One

Thank you for your interest in contributing to Sprite One! This document provides guidelines for contributing to the project.

---

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help others learn and grow
- Maintain a welcoming environment

---

## How to Contribute

### Reporting Bugs

1. Check if the bug has already been reported in [Issues](https://github.com/sprite-microelectronics/sprite-one/issues)
2. Create a new issue with:
   - Clear title
   - Steps to reproduce
   - Expected vs actual behavior
   - Your environment (OS, firmware version, etc.)
   - Serial output logs if applicable

### Suggesting Features

1. Check [Discussions](https://github.com/sprite-microelectronics/sprite-one/discussions) first
2. Create a new discussion or issue with:
   - Clear use case
   - Proposed implementation (if you have ideas)
   - Benefits to the project

### Submitting Code

1. **Fork the repository**
2. **Create a feature branch:**
   ```bash
   git checkout -b feature/my-new-feature
   ```
3. **Make your changes:**
   - Follow the coding style (see below)
   - Add tests if applicable
   - Update documentation
4. **Test your changes:**
   ```bash
   cd host/python
   python test_suite.py --port COM3
   ```
5. **Commit with clear messages:**
   ```bash
   git commit -m "Add feature: description"
   ```
6. **Push and create a Pull Request**

---

## Coding Style

### C/C++ (Firmware)

```cpp
// Use snake_case for functions and variables
void my_function(int param_name);
uint8_t my_variable;

// Use PascalCase for types
typedef struct {
    uint32_t field_name;
} MyStruct;

// Constants in UPPER_CASE
#define MAX_BUFFER_SIZE 256

// Comments for non-obvious code
// Clear explanation of what and why
```

### Python (Host Library)

Follow PEP 8:
```python
# Use snake_case
def my_function(param_name):
    my_variable = 0
    
# Type hints
def process_data(input: float) -> float:
    return input * 2.0
    
# Docstrings
def important_function(param):
    """Brief description.
    
    Args:
        param: Description
        
    Returns:
        Description of return value
    """
```

---

## Testing

### Before Submitting

- [ ] Code compiles without errors
- [ ] All existing tests pass
- [ ] New tests added for new features
- [ ] Documentation updated
- [ ] CHANGELOG.md updated

### Running Tests

**Firmware:**
```bash
cd firmware
arduino-cli compile --fqbn rp2040:rp2040:rpipico
```

**Python:**
```bash
cd host/python
python unit_tests.py  # Unit tests (no hardware)
python test_suite.py --port COM3  # Integration tests
```

---

## Documentation

### When to Update Docs

- Adding new API functions
- Changing behavior
- Adding examples
- Fixing bugs that affect usage

### Where to Update

- `docs/API.md` - API changes
- `docs/GETTING_STARTED.md` - Setup changes
- `README.md` - Major features
- `CHANGELOG.md` - All changes

---

## Pull Request Process

1. **Ensure CI passes** (when available)
2. **Update documentation**
3. **Add to CHANGELOG.md**
4. **Request review** from maintainers
5. **Address feedback**
6. **Squash commits** if requested

### PR Title Format

```
[Category] Brief description

Categories:
- Feature: New functionality
- Fix: Bug fixes
- Docs: Documentation only
- Refactor: Code restructuring
- Test: Test additions/changes
- Perf: Performance improvements
```

---

## Development Setup

### Prerequisites

- Arduino IDE 2.0+ or PlatformIO
- Python 3.7+
- RP2040 board support
- Serial terminal (PuTTY, screen, etc.)

### Quick Start

```bash
# Clone your fork
git clone https://github.com/Akhil-Chaturvedi/sprite-microelectronics.git
cd sprite-microelectronics

# Install Python dependencies
cd host/python
pip install -r requirements.txt

# Compile firmware
cd ../../firmware
arduino-cli compile --fqbn rp2040:rp2040:rpipico
```

---

## Project Structure

```
sprite-one/
├── firmware/       # RP2040 firmware
│   ├── include/   # Headers
│   └── examples/  # Example sketches
├── host/          # Host libraries
│   ├── python/   # Python library
│   └── c/        # C library
├── docs/          # Documentation
└── examples/      # More examples
```

---

## Areas for Contribution

### Good First Issues

- Documentation improvements
- Additional examples
- Test coverage
- Bug fixes

### Advanced Areas

- AI model architectures
- Display drivers
- Protocol extensions
- Performance optimization
- Hardware design

---

## Questions?

- Open a [Discussion](https://github.com/sprite-microelectronics/sprite-one/discussions)
- Ask in community chat (coming soon)

---

Thank you for contributing to Sprite One! 🚀
