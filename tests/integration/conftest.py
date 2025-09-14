"""
Integration test configuration and fixtures

Provides shared test fixtures and configuration for integration testing.
"""

import pytest
import time
import threading
import requests
from selenium import webdriver
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from tests.integration.mock_esp32_server import run_mock_server


@pytest.fixture(scope="session")
def mock_server():
    """Start mock ESP32 server for testing"""
    server_port = 8080
    
    # Start server in background thread
    server_thread = threading.Thread(
        target=run_mock_server,
        args=(server_port,),
        daemon=True
    )
    server_thread.start()
    
    # Wait for server to start
    max_retries = 10
    for _ in range(max_retries):
        try:
            response = requests.get(f"http://localhost:{server_port}/api/status", timeout=1)
            if response.status_code == 200:
                break
        except requests.exceptions.RequestException:
            time.sleep(0.5)
    else:
        pytest.fail("Mock server failed to start")
    
    yield f"http://localhost:{server_port}"
    
    # Server will stop when test session ends


@pytest.fixture(scope="function")
def web_driver():
    """Create Chrome WebDriver for browser testing"""
    chrome_options = Options()
    chrome_options.add_argument("--headless")
    chrome_options.add_argument("--no-sandbox")
    chrome_options.add_argument("--disable-dev-shm-usage")
    chrome_options.add_argument("--disable-gpu")
    chrome_options.add_argument("--window-size=1920,1080")
    
    driver = webdriver.Chrome(options=chrome_options)
    driver.implicitly_wait(10)
    
    yield driver
    
    driver.quit()


@pytest.fixture(scope="function")
def api_client(mock_server):
    """HTTP client configured for mock server"""
    class APIClient:
        def __init__(self, base_url):
            self.base_url = base_url
            self.session = requests.Session()
            self.session.headers.update({
                'Content-Type': 'application/json',
                'User-Agent': 'BMCU370-Test-Client/1.0'
            })
        
        def get(self, endpoint, **kwargs):
            return self.session.get(f"{self.base_url}{endpoint}", **kwargs)
        
        def post(self, endpoint, **kwargs):
            return self.session.post(f"{self.base_url}{endpoint}", **kwargs)
        
        def put(self, endpoint, **kwargs):
            return self.session.put(f"{self.base_url}{endpoint}", **kwargs)
        
        def delete(self, endpoint, **kwargs):
            return self.session.delete(f"{self.base_url}{endpoint}", **kwargs)
    
    return APIClient(mock_server)


@pytest.fixture(scope="function")
def wait_for_element():
    """Helper fixture for waiting for web elements"""
    def _wait_for_element(driver, locator, timeout=10):
        return WebDriverWait(driver, timeout).until(
            EC.presence_of_element_located(locator)
        )
    return _wait_for_element


@pytest.fixture(scope="function")
def sample_config_data():
    """Sample configuration data for testing"""
    return {
        "wifi": {
            "ssid": "TestWiFi",
            "password": "test123",
            "security": "WPA2"
        },
        "device": {
            "name": "BMCU370-Test-Device",
            "location": "Test Lab",
            "description": "Test configuration"
        },
        "settings": {
            "auto_refresh": True,
            "refresh_interval": 3000,
            "debug_mode": True
        }
    }


@pytest.fixture(scope="function")
def sample_control_commands():
    """Sample device control commands for testing"""
    return [
        {"action": "toggle_led", "led": "status"},
        {"action": "toggle_led", "led": "error"},
        {"action": "move_motor", "motor": 0, "direction": "cw", "steps": 90},
        {"action": "move_motor", "motor": 1, "direction": "ccw", "steps": 45},
        {"action": "reset_system"},
        {"action": "enter_dfu_mode"},
        {"action": "exit_dfu_mode"}
    ]


# Test markers for different test categories
pytest_plugins = []

def pytest_configure(config):
    """Configure pytest with custom markers"""
    config.addinivalue_line("markers", "api: API endpoint tests")
    config.addinivalue_line("markers", "ui: User interface tests")
    config.addinivalue_line("markers", "slow: Slow-running tests")
    config.addinivalue_line("markers", "network: Tests requiring network access")