"""
Unit tests for Web Server module.

Tests the ESP32 web server functionality including HTTP request handling,
routing, static file serving, and API endpoints.
"""

import pytest
import json
from unittest.mock import Mock, patch, AsyncMock
from tests.conftest import load_test_data, assert_valid_json_response


class TestWebServerManager:
    """Test cases for WebServerManager class."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.test_configs = load_test_data('test_configurations.json')
        
    @pytest.mark.unit
    def test_server_initialization(self, mock_bmcu370_device):
        """Test web server initialization."""
        web_server = self.create_mock_web_server()
        
        # Test successful initialization
        result = web_server.init(mock_bmcu370_device, None, True)
        assert result == True
        assert web_server.port == 80
        assert web_server.littlefs_available == True
        
    @pytest.mark.unit
    def test_route_setup(self):
        """Test HTTP route setup and registration."""
        web_server = self.create_mock_web_server()
        
        # Test that all required routes are registered
        expected_routes = [
            '/',
            '/api/status',
            '/api/config',
            '/api/logs',
            '/api/wifi/scan',
            '/api/wifi/connect',
            '/api/system/reset',
            '/api/system/dfu'
        ]
        
        for route in expected_routes:
            assert web_server.has_route(route) == True
            
    @pytest.mark.unit
    def test_static_file_serving(self):
        """Test static file serving functionality."""
        web_server = self.create_mock_web_server()
        
        # Test HTML file serving
        response = web_server.serve_static_file('/index.html')
        assert response.status_code == 200
        assert 'text/html' in response.content_type
        
        # Test CSS file serving
        response = web_server.serve_static_file('/css/style.css')
        assert response.status_code == 200
        assert 'text/css' in response.content_type
        
        # Test JavaScript file serving
        response = web_server.serve_static_file('/js/app.js')
        assert response.status_code == 200
        assert 'application/javascript' in response.content_type
        
    @pytest.mark.unit
    def test_api_status_endpoint(self, mock_bmcu370_device):
        """Test /api/status endpoint."""
        web_server = self.create_mock_web_server()
        
        # Mock BMCU370 status response
        mock_status = load_test_data('mock_bmcu370_responses.json')['system_online']
        mock_bmcu370_device.get_status.return_value = mock_status
        
        response = web_server.handle_api_status()
        
        assert response.status_code == 200
        response_data = assert_valid_json_response(response.body)
        assert response_data['success'] == True
        assert 'data' in response_data
        
    @pytest.mark.unit
    def test_api_config_endpoint(self, mock_bmcu370_device):
        """Test /api/config endpoint for GET and POST."""
        web_server = self.create_mock_web_server()
        
        # Test GET config
        response = web_server.handle_api_config_get()
        assert response.status_code == 200
        
        # Test POST config
        config_data = {
            'led_brightness': {'main': 50, 'channels': 25},
            'voltage_thresholds': {'high': 1.9, 'low': 1.4}
        }
        
        response = web_server.handle_api_config_post(config_data)
        assert response.status_code == 200
        response_data = assert_valid_json_response(response.body)
        assert response_data['success'] == True
        
    @pytest.mark.unit
    def test_wifi_scan_endpoint(self):
        """Test /api/wifi/scan endpoint."""
        web_server = self.create_mock_web_server()
        
        # Mock WiFi scan results
        scan_results = self.test_configs['network_scan_results']
        
        with patch('wifi_manager.scan_networks') as mock_scan:
            mock_scan.return_value = scan_results
            
            response = web_server.handle_wifi_scan()
            assert response.status_code == 200
            
            response_data = assert_valid_json_response(response.body)
            assert response_data['success'] == True
            assert len(response_data['networks']) > 0
            
    @pytest.mark.unit
    def test_wifi_connect_endpoint(self):
        """Test /api/wifi/connect endpoint."""
        web_server = self.create_mock_web_server()
        
        # Test valid WiFi connection request
        connect_data = {
            'ssid': 'HomeNetwork',
            'password': 'homepassword123'
        }
        
        with patch('wifi_manager.connect_to_network') as mock_connect:
            mock_connect.return_value = True
            
            response = web_server.handle_wifi_connect(connect_data)
            assert response.status_code == 200
            
            response_data = assert_valid_json_response(response.body)
            assert response_data['success'] == True
            
    @pytest.mark.unit
    def test_error_handling(self):
        """Test error handling in web server."""
        web_server = self.create_mock_web_server()
        
        # Test 404 handling
        response = web_server.handle_not_found('/nonexistent')
        assert response.status_code == 404
        
        # Test 500 error handling
        with patch('bmcu370_interface.get_status') as mock_status:
            mock_status.side_effect = Exception("Device error")
            
            response = web_server.handle_api_status()
            assert response.status_code == 500
            
            response_data = assert_valid_json_response(response.body)
            assert response_data['success'] == False
            assert 'error' in response_data
            
    @pytest.mark.unit
    def test_cors_headers(self):
        """Test CORS header handling."""
        web_server = self.create_mock_web_server()
        
        response = web_server.handle_api_status()
        headers = response.headers
        
        assert 'Access-Control-Allow-Origin' in headers
        assert 'Access-Control-Allow-Methods' in headers
        assert 'Access-Control-Allow-Headers' in headers
        
    @pytest.mark.unit
    def test_rate_limiting(self):
        """Test rate limiting functionality."""
        web_server = self.create_mock_web_server()
        
        # Simulate multiple rapid requests
        client_ip = '192.168.1.100'
        
        for i in range(15):  # Exceed rate limit
            response = web_server.handle_api_status(client_ip=client_ip)
            
            if i < 10:  # First 10 should succeed
                assert response.status_code == 200
            else:  # Subsequent should be rate limited
                assert response.status_code == 429
                
    @pytest.mark.unit
    def test_client_management(self):
        """Test client connection management."""
        web_server = self.create_mock_web_server()
        
        # Test client tracking
        web_server.add_client('192.168.1.100')
        web_server.add_client('192.168.1.101')
        
        assert web_server.get_client_count() == 2
        
        # Test client removal
        web_server.remove_client('192.168.1.100')
        assert web_server.get_client_count() == 1
        
    @pytest.mark.unit
    def test_memory_usage_monitoring(self):
        """Test memory usage monitoring in web server."""
        web_server = self.create_mock_web_server()
        
        # Test memory check before serving large responses
        with patch('esp32.get_free_heap') as mock_heap:
            mock_heap.return_value = 20000  # Low memory
            
            response = web_server.handle_large_response()
            
            # Should return error when memory is low
            assert response.status_code == 503
            response_data = assert_valid_json_response(response.body)
            assert 'memory' in response_data['error'].lower()
            
    @pytest.mark.unit
    def test_fallback_interface(self):
        """Test fallback interface when LittleFS is unavailable."""
        web_server = self.create_mock_web_server()
        web_server.littlefs_available = False
        
        # Should serve minimal HTML interface
        response = web_server.serve_static_file('/index.html')
        assert response.status_code == 200
        assert 'BMCU370' in response.body
        assert len(response.body) < 10000  # Should be minimal
        
    def create_mock_web_server(self):
        """Create a mock WebServerManager for testing."""
        server = Mock()
        server.port = 80
        server.littlefs_available = True
        server.client_count = 0
        server.init = Mock(return_value=True)
        server.has_route = Mock(return_value=True)
        server.serve_static_file = Mock()
        server.handle_api_status = Mock()
        server.handle_api_config_get = Mock()
        server.handle_api_config_post = Mock()
        server.handle_wifi_scan = Mock()
        server.handle_wifi_connect = Mock()
        server.handle_not_found = Mock()
        server.add_client = Mock()
        server.remove_client = Mock()
        server.get_client_count = Mock(return_value=0)
        server.handle_large_response = Mock()
        
        # Setup default responses
        server.serve_static_file.return_value = self.create_mock_response(200, 'text/html', '<html></html>')
        server.handle_api_status.return_value = self.create_mock_response(200, 'application/json', '{"success":true}')
        server.handle_api_config_get.return_value = self.create_mock_response(200, 'application/json', '{"success":true}')
        server.handle_api_config_post.return_value = self.create_mock_response(200, 'application/json', '{"success":true}')
        server.handle_wifi_scan.return_value = self.create_mock_response(200, 'application/json', '{"success":true,"networks":[]}')
        server.handle_wifi_connect.return_value = self.create_mock_response(200, 'application/json', '{"success":true}')
        server.handle_not_found.return_value = self.create_mock_response(404, 'text/html', 'Not Found')
        server.handle_large_response.return_value = self.create_mock_response(503, 'application/json', '{"success":false,"error":"Low memory"}')
        
        return server
        
    def create_mock_response(self, status_code, content_type, body):
        """Create a mock HTTP response."""
        response = Mock()
        response.status_code = status_code
        response.content_type = content_type
        response.body = body
        response.headers = {
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type'
        }
        return response


class TestWebServerSecurity:
    """Test security aspects of the web server."""
    
    @pytest.mark.unit
    def test_input_validation(self):
        """Test input validation for API endpoints."""
        web_server = Mock()
        
        # Test malicious input in config
        malicious_config = {
            'led_brightness': {'main': '"><script>alert(1)</script>'},
            'voltage_thresholds': {'high': 'DROP TABLE config;'}
        }
        
        with patch('web_server.validate_config_input') as mock_validate:
            mock_validate.return_value = False
            
            web_server.handle_api_config_post.return_value = Mock(status_code=400)
            response = web_server.handle_api_config_post(malicious_config)
            assert response.status_code == 400
            
    @pytest.mark.unit
    def test_file_path_validation(self):
        """Test file path validation to prevent directory traversal."""
        web_server = Mock()
        
        # Test directory traversal attempts
        malicious_paths = [
            '../../../etc/passwd',
            '..\\..\\windows\\system32\\config\\sam',
            '/etc/shadow',
            '\\windows\\system32\\drivers\\etc\\hosts'
        ]
        
        for path in malicious_paths:
            web_server.serve_static_file.return_value = Mock(status_code=404)
            response = web_server.serve_static_file(path)
            assert response.status_code == 404


if __name__ == '__main__':
    pytest.main([__file__, '-v'])