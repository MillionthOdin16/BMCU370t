# BMCU370t Testing Infrastructure

This directory contains the comprehensive test suite for the BMCU370t embedded firmware project.

## Directory Structure

```
tests/
├── unit/                    # Unit tests for individual components
│   ├── bmcu370/            # BMCU370 (CH32V203) unit tests  
│   │   ├── test_bambu_bus.cpp
│   │   ├── test_usb_protocol.cpp
│   │   ├── test_adc_dma.cpp
│   │   ├── test_flash_saves.cpp
│   │   └── test_motion_control.cpp
│   └── esp32/              # ESP32-S3 unit tests
│       ├── test_web_server.cpp
│       ├── test_usb_host.cpp
│       ├── test_data_logger.cpp
│       ├── test_wifi_config.cpp
│       └── test_json_api.cpp
├── simulation/             # Simulation and mock testing
│   ├── mock_bmcu370.py     # Python script simulating BMCU370 responses
│   ├── esp32_sim_tests.py  # ESP32 simulation tests
│   ├── protocol_validation.py  # USB CDC protocol validation
│   └── conftest.py         # Pytest fixtures for simulation
├── integration/            # Integration testing 
│   ├── test_web_interface.py    # Web UI functional tests
│   ├── test_api_endpoints.py    # REST API validation
│   ├── test_system_integration.py  # Cross-component tests
│   ├── conftest.py              # Pytest configuration
│   └── requirements.txt         # Python dependencies
├── hil/                    # Hardware-in-the-Loop testing
│   ├── test_hardware_flash.py    # Automated firmware flashing
│   ├── test_live_communication.py # Real hardware communication
│   ├── test_system_validation.py  # End-to-end system tests
│   ├── hardware_setup.py         # HIL test setup utilities
│   └── conftest.py               # HIL-specific fixtures
└── config/                 # Static analysis and tool configuration
    ├── cppcheck.cfg        # Cppcheck configuration
    ├── clang-tidy.yaml     # Clang-tidy rules
    ├── pytest.ini         # Pytest configuration
    └── requirements.txt    # Python testing dependencies
```

## Test Categories

### Unit Tests
- **Framework**: Unity (PlatformIO native)
- **Target**: Individual functions and modules
- **Scope**: BMCU370 and ESP32 components in isolation
- **Execution**: Fast, automated, no hardware dependencies

### Simulation Tests  
- **Framework**: Python pytest + custom mocks
- **Target**: System behavior with simulated hardware
- **Scope**: Protocol validation, state machine testing
- **Execution**: CI-friendly, reproducible results

### Integration Tests
- **Framework**: Python pytest + Selenium/requests
- **Target**: Component interactions and web interface
- **Scope**: API endpoints, web UI, data flow
- **Execution**: Automated in CI, browser-based

### Hardware-in-the-Loop (HIL) Tests
- **Framework**: Python pytest + hardware control
- **Target**: Real hardware behavior validation  
- **Scope**: Complete system with actual ESP32/CH32V203
- **Execution**: Self-hosted runner with physical devices

## Running Tests

### Local Development
```bash
# Install dependencies
pip install -r tests/config/requirements.txt

# Run unit tests
cd tests/unit
pio test

# Run simulation tests  
cd tests/simulation
python -m pytest -v

# Run integration tests
cd tests/integration  
python -m pytest -v
```

### CI Execution
Tests are automatically executed in GitHub Actions workflows:
- Unit tests: In build workflow after compilation
- Simulation tests: In dedicated simulation workflow
- Integration tests: In integration workflow  
- HIL tests: In HIL workflow (manual/scheduled)

## Configuration

### Static Analysis
- **cppcheck.cfg**: Suppression rules and analysis options
- **clang-tidy.yaml**: Modern C++ style and bug detection rules

### Test Execution
- **pytest.ini**: Test discovery, markers, and execution options
- **conftest.py**: Shared fixtures and test configuration

### Hardware Setup
HIL tests require specific hardware configuration documented in each test module.

## Contributing

1. Add unit tests for all new functionality
2. Update simulation tests for protocol changes
3. Extend integration tests for new API endpoints
4. Document HIL test requirements for hardware changes

See individual test files for specific testing guidelines and examples.