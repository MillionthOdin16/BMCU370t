"""
Test configuration and fixtures for BMCU370 ESP32 Web Interface tests.
"""

import pytest
import json
import os
from unittest.mock import Mock, MagicMock
from typing import Dict, Any, List

# Test data directory
TEST_DATA_DIR = os.path.join(os.path.dirname(__file__), 'fixtures')

@pytest.fixture
def mock_bmcu370_device():
    """Mock BMCU370 device for testing without hardware."""
    device = Mock()
    device.is_connected.return_value = True
    device.get_status.return_value = {
        "system": {
            "uptime": 123456,
            "version": "00.00.06.49",
            "bambubus_status": "online",
            "device_type": "AMS",
            "error_count": 0
        },
        "channels": [
            {
                "id": 0,
                "filament": {
                    "status": "online",
                    "name": "PLA Basic",
                    "color": {"r": 255, "g": 255, "b": 255, "a": 255},
                    "temperature": {"min": 190, "max": 220},
                    "meters_remaining": 125.5
                },
                "motion": {
                    "state": "idle",
                    "position": 1024,
                    "speed": 0.0,
                    "pressure": 1650
                },
                "rgb": {
                    "brightness": 15,
                    "current_color": {"r": 0, "g": 255, "b": 0}
                },
                "sensors": {
                    "hall_position": 1024,
                    "filament_present": True
                },
                "errors": []
            }
        ]
    }
    return device

@pytest.fixture
def mock_wifi_config():
    """Mock WiFi configuration for testing."""
    return {
        "ssid": "test_network",
        "password": "test_password",
        "ip": "192.168.1.100",
        "gateway": "192.168.1.1",
        "subnet": "255.255.255.0",
        "dns": "8.8.8.8"
    }

@pytest.fixture
def mock_web_server():
    """Mock web server for testing."""
    server = Mock()
    server.port = 80
    server.is_running.return_value = True
    server.client_count = 0
    return server

@pytest.fixture
def sample_api_responses():
    """Sample API responses for testing."""
    return {
        "status": {
            "success": True,
            "data": {
                "system": {"uptime": 123456, "version": "1.0.0"},
                "channels": []
            }
        },
        "config": {
            "success": True,
            "data": {
                "wifi": {"ssid": "test_network"},
                "led_brightness": 50
            }
        },
        "error": {
            "success": False,
            "error": "Device not connected",
            "code": 500
        }
    }

@pytest.fixture
def historical_data_sample():
    """Sample historical data for testing."""
    return {
        "timestamp": 1640995200,
        "data": [
            {
                "channel": 0,
                "filament_present": True,
                "hall_position": 1024,
                "temperature": 25.5,
                "pressure": 1650
            }
        ]
    }

@pytest.fixture
def mock_websocket_client():
    """Mock WebSocket client for testing."""
    client = Mock()
    client.connected = True
    client.send = Mock()
    client.receive = Mock()
    return client

@pytest.fixture(scope="session")
def test_files_dir():
    """Directory containing test HTML/CSS/JS files."""
    return os.path.join(os.path.dirname(__file__), 'fixtures', 'web_files')

@pytest.fixture
def mock_usb_device():
    """Mock USB device for BMCU370 communication testing."""
    device = Mock()
    device.vendor_id = 0x1234
    device.product_id = 0x5678
    device.serial_number = "BMCU370-001"
    device.is_open = True
    device.read = Mock(return_value=b'{"status": "ok"}')
    device.write = Mock(return_value=10)
    return device

@pytest.fixture
def esp32_memory_limits():
    """ESP32-S3 memory limits for testing."""
    return {
        "flash_size": 4 * 1024 * 1024,  # 4MB
        "psram_size": 2 * 1024 * 1024,  # 2MB
        "heap_warning_threshold": 50 * 1024,  # 50KB
        "heap_critical_threshold": 20 * 1024   # 20KB
    }

@pytest.fixture(autouse=True)
def setup_test_environment():
    """Setup test environment before each test."""
    # Ensure test data directory exists
    os.makedirs(TEST_DATA_DIR, exist_ok=True)
    
    # Set test environment variables
    os.environ['BMCU370_TEST_MODE'] = '1'
    os.environ['ESP32_TEST_CONFIG'] = 'test'
    
    yield
    
    # Cleanup after test
    if 'BMCU370_TEST_MODE' in os.environ:
        del os.environ['BMCU370_TEST_MODE']
    if 'ESP32_TEST_CONFIG' in os.environ:
        del os.environ['ESP32_TEST_CONFIG']

def load_test_data(filename: str) -> Dict[Any, Any]:
    """Load test data from JSON file."""
    filepath = os.path.join(TEST_DATA_DIR, filename)
    if os.path.exists(filepath):
        with open(filepath, 'r') as f:
            return json.load(f)
    return {}

def save_test_data(filename: str, data: Dict[Any, Any]) -> None:
    """Save test data to JSON file."""
    filepath = os.path.join(TEST_DATA_DIR, filename)
    with open(filepath, 'w') as f:
        json.dump(data, f, indent=2)

# Custom assertions for ESP32 testing
def assert_valid_json_response(response_text: str) -> Dict[Any, Any]:
    """Assert that response is valid JSON and return parsed data."""
    try:
        data = json.loads(response_text)
        assert isinstance(data, dict), "Response must be a JSON object"
        return data
    except json.JSONDecodeError as e:
        pytest.fail(f"Invalid JSON response: {e}")

def assert_bmcu370_status_format(status_data: Dict[Any, Any]) -> None:
    """Assert that status data follows BMCU370 format."""
    required_keys = ['system', 'channels']
    for key in required_keys:
        assert key in status_data, f"Missing required key: {key}"
    
    assert isinstance(status_data['channels'], list), "Channels must be a list"
    
    if status_data['channels']:
        channel = status_data['channels'][0]
        required_channel_keys = ['id', 'filament', 'motion', 'rgb', 'sensors']
        for key in required_channel_keys:
            assert key in channel, f"Missing required channel key: {key}"

def assert_memory_usage_acceptable(current_usage: int, limit: int, threshold: float = 0.8) -> None:
    """Assert that memory usage is within acceptable limits."""
    usage_ratio = current_usage / limit
    assert usage_ratio < threshold, f"Memory usage too high: {usage_ratio:.2%} of {limit} bytes"