# BMCU370 Testing Framework

This directory contains the comprehensive testing framework for the BMCU370 USB-ESP interface system.

## Testing Structure

### Unit Tests (`unit/`)
- Platform-specific unit tests for both BMCU370 and ESP32
- Firmware binary validation
- Memory usage verification
- Configuration validation

### Protocol Tests (`protocol/`)
- USB CDC communication testing
- BambuBus protocol validation
- Web interface API testing
- Network communication tests

### Integration Tests (`integration/`)
- End-to-end system testing
- Multi-component interaction testing
- Data flow validation

### Simulation Tests (`simulation/`)
- QEMU-based firmware simulation
- Virtual hardware testing
- Protocol simulation

### Hardware-in-the-Loop Tests (`hil/`)
- Real hardware testing framework
- Automated hardware interaction
- Physical interface validation

## Test Execution

Tests are automatically executed in the CI pipeline:

1. **Unit Tests**: Run for each firmware build
2. **Protocol Tests**: Validate communication protocols
3. **Integration Tests**: Test system interactions
4. **Simulation Tests**: Virtual environment testing
5. **HIL Tests**: Real hardware validation (when available)

## Adding New Tests

### Unit Tests
```python
# test/unit/[target]/test_[feature].py
import unittest

class TestFeature(unittest.TestCase):
    def test_specific_functionality(self):
        # Test implementation
        pass
```

### Protocol Tests
```python
# test/protocol/[target]/test_[protocol].py
import unittest

class TestProtocol(unittest.TestCase):
    def test_protocol_compliance(self):
        # Protocol validation
        pass
```

## Test Configuration

- Test runner: pytest
- Report format: JUnit XML
- Coverage tracking: Enabled
- CI integration: GitHub Actions

## Manual Testing

Run tests locally:
```bash
# All tests
python -m pytest test/ -v

# Specific target
python -m pytest test/unit/bmcu370/ -v
python -m pytest test/unit/esp32/ -v

# Specific test type
python -m pytest test/protocol/ -v
```

## Test Requirements

- Python 3.11+
- pytest
- unittest (built-in)
- platformio (for firmware analysis)
- Additional dependencies as needed per test type