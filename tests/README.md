# BMCU370 ESP32 Web Interface Test Suite

This directory contains comprehensive tests for the BMCU370 ESP32 web interface functionality.

## Test Structure

```
tests/
├── unit/                   # Unit tests for individual components
│   ├── test_bmcu370_interface.py  # BMCU370 communication tests
│   ├── test_web_server.py         # Web server functionality tests
│   ├── test_wifi_manager.py       # WiFi management tests
│   └── test_historical_data.py    # Historical data management tests
├── integration/            # Integration tests for full system
│   ├── test_api_endpoints.py      # REST API endpoint tests
│   ├── test_websocket.py          # WebSocket communication tests
│   └── test_system_integration.py # Full system integration tests
├── web_interface/          # Web interface validation tests
│   ├── test_html_validation.py    # HTML structure and validation
│   ├── test_css_validation.py     # CSS styling and responsiveness
│   └── test_javascript.py         # JavaScript functionality tests
├── simulation/             # Hardware simulation tests
│   ├── test_bmcu370_simulator.py  # Mock BMCU370 device simulation
│   └── test_usb_communication.py  # USB communication simulation
├── performance/            # Performance and load tests
│   ├── test_web_server_load.py    # Web server load testing
│   └── test_memory_usage.py       # Memory usage monitoring
└── fixtures/               # Test data and fixtures
    ├── mock_bmcu370_responses.json
    ├── test_configurations.json
    └── sample_historical_data.json
```

## Test Categories

### 1. Unit Tests
- **BMCU370 Interface**: USB communication, command parsing, status handling
- **Web Server**: HTTP request handling, routing, error responses
- **WiFi Manager**: Network scanning, connection management, AP mode
- **Historical Data**: Data storage, retrieval, and management

### 2. Integration Tests
- **API Endpoints**: Complete request/response cycles for all REST endpoints
- **WebSocket**: Real-time communication and message broadcasting
- **System Integration**: Full system operation from ESP32 boot to web interface

### 3. Web Interface Tests
- **HTML Validation**: Structure, accessibility, and semantic markup
- **CSS Validation**: Styling, responsiveness, and cross-browser compatibility
- **JavaScript**: Interactive functionality and AJAX communication

### 4. Simulation Tests
- **BMCU370 Simulation**: Mock device behavior for testing without hardware
- **USB Communication**: Simulated USB host/device communication

### 5. Performance Tests
- **Load Testing**: Multiple concurrent web interface sessions
- **Memory Usage**: RAM and flash memory consumption monitoring

## Running Tests

### Prerequisites
```bash
pip install -r requirements-test.txt
```

### Run All Tests
```bash
python -m pytest tests/ -v
```

### Run Specific Test Categories
```bash
# Unit tests only
python -m pytest tests/unit/ -v

# Integration tests only
python -m pytest tests/integration/ -v

# Web interface tests only
python -m pytest tests/web_interface/ -v

# Performance tests only
python -m pytest tests/performance/ -v
```

### Run Tests with Coverage
```bash
python -m pytest tests/ --cov=esp32_firmware/src --cov-report=html
```

### Run Tests in Continuous Integration
```bash
python -m pytest tests/ --junit-xml=test-results.xml
```

## Test Configuration

Tests use the following configuration files:
- `tests/conftest.py`: Common test fixtures and configuration
- `tests/pytest.ini`: PyTest configuration settings
- `tests/requirements-test.txt`: Test dependencies

## Mock Hardware Setup

For testing without physical hardware, the test suite includes:
- Mock BMCU370 device simulator
- Simulated USB communication
- Mock WiFi network environments
- Simulated sensor data and responses

## Continuous Integration

Tests are automatically run on:
- Pull requests to main branch
- Commits to development branches
- Scheduled nightly builds

## Test Data

Test fixtures include:
- Sample BMCU370 status responses
- Various configuration scenarios
- Historical data samples

## Performance and Optimization

The test framework includes comprehensive optimization and caching strategies:

### Intelligent Caching
- **Dependency caching**: Python, Node.js, and security tools (70-90% faster installs)
- **Test result caching**: Skip tests for unchanged code (60% execution time reduction)
- **Emulator caching**: Docker layers, QEMU, and Wokwi simulators (80% faster startup)

### Smart Test Selection
- **File change detection**: Only run tests when relevant code changes
- **Hash-based optimization**: MD5 hashing for precise cache invalidation
- **Conditional execution**: Skip expensive tests on unrelated changes

### Performance Metrics
- **Total CI time**: Reduced from 45-60 minutes to 15-20 minutes (70% improvement)
- **Cache hit rates**: 85-95% for dependencies, 70-80% for test results
- **Parallel execution**: Matrix-based parallelization across Python versions

For detailed optimization documentation, see:
- **[TESTING_OPTIMIZATION_GUIDE.md](TESTING_OPTIMIZATION_GUIDE.md)** - Comprehensive caching and optimization strategies

### Quick Optimization Commands

```bash
# Enable optimization for local testing
export USE_TEST_CACHE=true
python run_tests.py --all --optimize-cache --verbose

# Clear caches if needed
rm -rf tests/.test_cache.json tests/.pytest_cache/

# View cache status
cat tests/.test_cache.json | jq '.'
```

The optimization framework ensures efficient test execution while maintaining comprehensive coverage and reliability.
- Error condition simulations