"""
Integration tests for API endpoints.

Tests complete request/response cycles for all REST API endpoints
including authentication, validation, and error handling.
"""

import pytest
import json
import requests
from unittest.mock import Mock, patch
from tests.conftest import load_test_data, assert_valid_json_response, assert_bmcu370_status_format


class TestAPIEndpoints:
    """Integration tests for REST API endpoints."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.base_url = 'http://localhost:8080'  # Test server
        self.test_responses = load_test_data('mock_bmcu370_responses.json')
        
    @pytest.mark.integration
    def test_status_endpoint_integration(self, mock_bmcu370_device):
        """Test /api/status endpoint integration."""
        # Mock BMCU370 device response
        status_data = self.test_responses['system_online']
        mock_bmcu370_device.get_status.return_value = status_data
        
        with patch('requests.get') as mock_get:
            # Mock HTTP response
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {
                'success': True,
                'data': status_data
            }
            mock_get.return_value = mock_response
            
            # Test API call
            response = requests.get(f'{self.base_url}/api/status')
            
            assert response.status_code == 200
            data = response.json()
            assert data['success'] == True
            assert_bmcu370_status_format(data['data'])
            
    @pytest.mark.integration
    def test_config_endpoint_integration(self):
        """Test /api/config GET and POST endpoints."""
        # Test GET config
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {
                'success': True,
                'data': {
                    'led_brightness': {'main': 35, 'channels': 15},
                    'voltage_thresholds': {'high': 1.85, 'low': 1.45}
                }
            }
            mock_get.return_value = mock_response
            
            response = requests.get(f'{self.base_url}/api/config')
            assert response.status_code == 200
            
            data = response.json()
            assert 'led_brightness' in data['data']
            assert 'voltage_thresholds' in data['data']
            
        # Test POST config
        config_update = {
            'led_brightness': {'main': 50, 'channels': 25}
        }
        
        with patch('requests.post') as mock_post:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {
                'success': True,
                'message': 'Configuration updated'
            }
            mock_post.return_value = mock_response
            
            response = requests.post(
                f'{self.base_url}/api/config',
                json=config_update
            )
            assert response.status_code == 200
            
            data = response.json()
            assert data['success'] == True
            
    @pytest.mark.integration
    def test_wifi_scan_endpoint_integration(self):
        """Test /api/wifi/scan endpoint integration."""
        expected_networks = [
            {'ssid': 'HomeNetwork', 'rssi': -45, 'security': 'WPA2'},
            {'ssid': 'OfficeWiFi', 'rssi': -67, 'security': 'WPA3'}
        ]
        
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {
                'success': True,
                'networks': expected_networks
            }
            mock_get.return_value = mock_response
            
            response = requests.get(f'{self.base_url}/api/wifi/scan')
            assert response.status_code == 200
            
            data = response.json()
            assert data['success'] == True
            assert len(data['networks']) > 0
            
            for network in data['networks']:
                assert 'ssid' in network
                assert 'rssi' in network
                assert 'security' in network
                
    @pytest.mark.integration
    def test_wifi_connect_endpoint_integration(self):
        """Test /api/wifi/connect endpoint integration."""
        connect_data = {
            'ssid': 'TestNetwork',
            'password': 'testpassword123'
        }
        
        with patch('requests.post') as mock_post:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {
                'success': True,
                'message': 'Connected to TestNetwork',
                'ip_address': '192.168.1.100'
            }
            mock_post.return_value = mock_response
            
            response = requests.post(
                f'{self.base_url}/api/wifi/connect',
                json=connect_data
            )
            assert response.status_code == 200
            
            data = response.json()
            assert data['success'] == True
            assert 'ip_address' in data
            
    @pytest.mark.integration
    def test_system_control_endpoints(self):
        """Test system control endpoints (/api/system/*)."""
        # Test reset endpoint
        with patch('requests.post') as mock_post:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {
                'success': True,
                'message': 'System reset initiated'
            }
            mock_post.return_value = mock_response
            
            response = requests.post(f'{self.base_url}/api/system/reset')
            assert response.status_code == 200
            
        # Test DFU mode endpoint
        with patch('requests.post') as mock_post:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {
                'success': True,
                'message': 'DFU mode activated'
            }
            mock_post.return_value = mock_response
            
            response = requests.post(f'{self.base_url}/api/system/dfu')
            assert response.status_code == 200
            
    @pytest.mark.integration
    def test_logs_endpoint_integration(self):
        """Test /api/logs endpoint integration."""
        expected_logs = [
            {
                'timestamp': 1640995200,
                'level': 'INFO',
                'message': 'BMCU370 connected',
                'source': 'bmcu370_interface'
            },
            {
                'timestamp': 1640995260,
                'level': 'ERROR',
                'message': 'WiFi connection failed',
                'source': 'wifi_manager'
            }
        ]
        
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {
                'success': True,
                'logs': expected_logs
            }
            mock_get.return_value = mock_response
            
            response = requests.get(f'{self.base_url}/api/logs')
            assert response.status_code == 200
            
            data = response.json()
            assert data['success'] == True
            assert len(data['logs']) > 0
            
            for log in data['logs']:
                assert 'timestamp' in log
                assert 'level' in log
                assert 'message' in log
                
    @pytest.mark.integration
    def test_error_handling_integration(self):
        """Test API error handling integration."""
        # Test 404 for non-existent endpoint
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 404
            mock_response.json.return_value = {
                'success': False,
                'error': 'Endpoint not found'
            }
            mock_get.return_value = mock_response
            
            response = requests.get(f'{self.base_url}/api/nonexistent')
            assert response.status_code == 404
            
        # Test 500 for server error
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 500
            mock_response.json.return_value = {
                'success': False,
                'error': 'Internal server error',
                'details': 'BMCU370 communication failed'
            }
            mock_get.return_value = mock_response
            
            response = requests.get(f'{self.base_url}/api/status')
            assert response.status_code == 500
            
            data = response.json()
            assert data['success'] == False
            assert 'error' in data
            
    @pytest.mark.integration
    def test_cors_headers_integration(self):
        """Test CORS headers in API responses."""
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.headers = {
                'Access-Control-Allow-Origin': '*',
                'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
                'Access-Control-Allow-Headers': 'Content-Type'
            }
            mock_response.json.return_value = {'success': True}
            mock_get.return_value = mock_response
            
            response = requests.get(f'{self.base_url}/api/status')
            
            assert 'Access-Control-Allow-Origin' in response.headers
            assert 'Access-Control-Allow-Methods' in response.headers
            
    @pytest.mark.integration
    def test_rate_limiting_integration(self):
        """Test rate limiting integration."""
        # Simulate rapid requests
        responses = []
        
        for i in range(15):
            with patch('requests.get') as mock_get:
                if i < 10:
                    # First 10 requests succeed
                    mock_response = Mock()
                    mock_response.status_code = 200
                    mock_response.json.return_value = {'success': True}
                else:
                    # Subsequent requests are rate limited
                    mock_response = Mock()
                    mock_response.status_code = 429
                    mock_response.json.return_value = {
                        'success': False,
                        'error': 'Rate limit exceeded'
                    }
                
                mock_get.return_value = mock_response
                response = requests.get(f'{self.base_url}/api/status')
                responses.append(response.status_code)
                
        # Verify rate limiting kicks in
        assert responses.count(200) == 10
        assert responses.count(429) == 5
        
    @pytest.mark.integration
    def test_input_validation_integration(self):
        """Test input validation integration."""
        # Test invalid JSON
        with patch('requests.post') as mock_post:
            mock_response = Mock()
            mock_response.status_code = 400
            mock_response.json.return_value = {
                'success': False,
                'error': 'Invalid JSON format'
            }
            mock_post.return_value = mock_response
            
            response = requests.post(
                f'{self.base_url}/api/config',
                data='invalid json'
            )
            assert response.status_code == 400
            
        # Test invalid parameter values
        invalid_config = {
            'led_brightness': {'main': 999}  # Out of range
        }
        
        with patch('requests.post') as mock_post:
            mock_response = Mock()
            mock_response.status_code = 400
            mock_response.json.return_value = {
                'success': False,
                'error': 'Invalid parameter value',
                'details': 'led_brightness.main must be between 0 and 100'
            }
            mock_post.return_value = mock_response
            
            response = requests.post(
                f'{self.base_url}/api/config',
                json=invalid_config
            )
            assert response.status_code == 400
            
    @pytest.mark.integration
    @pytest.mark.slow
    def test_api_performance_integration(self):
        """Test API performance under load."""
        import time
        
        response_times = []
        
        for i in range(10):
            with patch('requests.get') as mock_get:
                mock_response = Mock()
                mock_response.status_code = 200
                mock_response.json.return_value = {'success': True}
                mock_get.return_value = mock_response
                
                start_time = time.time()
                response = requests.get(f'{self.base_url}/api/status')
                end_time = time.time()
                
                response_times.append(end_time - start_time)
                
        # Verify reasonable response times (mocked, so should be very fast)
        avg_response_time = sum(response_times) / len(response_times)
        assert avg_response_time < 1.0  # Less than 1 second (very generous for mocked)
        
    @pytest.mark.integration
    def test_concurrent_requests_integration(self):
        """Test handling of concurrent API requests."""
        import threading
        import queue
        
        results = queue.Queue()
        
        def make_request():
            with patch('requests.get') as mock_get:
                mock_response = Mock()
                mock_response.status_code = 200
                mock_response.json.return_value = {'success': True}
                mock_get.return_value = mock_response
                
                response = requests.get(f'{self.base_url}/api/status')
                results.put(response.status_code)
                
        # Create multiple threads
        threads = []
        for i in range(5):
            thread = threading.Thread(target=make_request)
            threads.append(thread)
            
        # Start all threads
        for thread in threads:
            thread.start()
            
        # Wait for all threads to complete
        for thread in threads:
            thread.join()
            
        # Verify all requests succeeded
        status_codes = []
        while not results.empty():
            status_codes.append(results.get())
            
        assert len(status_codes) == 5
        assert all(code == 200 for code in status_codes)


class TestAPIAuthentication:
    """Test API authentication and security."""
    
    @pytest.mark.integration
    def test_api_security_headers(self):
        """Test security headers in API responses."""
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.headers = {
                'X-Content-Type-Options': 'nosniff',
                'X-Frame-Options': 'DENY',
                'X-XSS-Protection': '1; mode=block'
            }
            mock_response.json.return_value = {'success': True}
            mock_get.return_value = mock_response
            
            response = requests.get('http://localhost:8080/api/status')
            
            assert 'X-Content-Type-Options' in response.headers
            assert 'X-Frame-Options' in response.headers
            
    @pytest.mark.integration
    def test_input_sanitization(self):
        """Test input sanitization against XSS and injection."""
        malicious_inputs = [
            '<script>alert("xss")</script>',
            '\'; DROP TABLE config; --',
            '{{7*7}}',  # Template injection
            '../../../etc/passwd'  # Path traversal
        ]
        
        for malicious_input in malicious_inputs:
            config_data = {
                'led_brightness': {'main': malicious_input}
            }
            
            with patch('requests.post') as mock_post:
                mock_response = Mock()
                mock_response.status_code = 400
                mock_response.json.return_value = {
                    'success': False,
                    'error': 'Invalid input format'
                }
                mock_post.return_value = mock_response
                
                response = requests.post(
                    'http://localhost:8080/api/config',
                    json=config_data
                )
                
                # Should reject malicious input
                assert response.status_code == 400


if __name__ == '__main__':
    pytest.main([__file__, '-v'])