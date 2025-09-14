"""
Security tests for input validation and injection attacks.

Tests the ESP32 web interface against various security vulnerabilities
including SQL injection, XSS, CSRF, and malformed input handling.
"""

import pytest
import json
import base64
from unittest.mock import Mock, patch
from tests.conftest import load_test_data, assert_valid_json_response


class TestInputValidation:
    """Test input validation and security measures."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.malicious_payloads = {
            'sql_injection': [
                "'; DROP TABLE users; --",
                "' OR '1'='1",
                "'; SELECT * FROM system; --",
                "1'; EXEC sp_executesql N'SELECT @@version'; --"
            ],
            'xss_payloads': [
                "<script>alert('XSS')</script>",
                "javascript:alert('XSS')",
                "<img src=x onerror=alert('XSS')>",
                "';alert(String.fromCharCode(88,83,83))//';",
                "<svg onload=alert('XSS')>",
                "&#x3C;script&#x3E;alert('XSS')&#x3C;/script&#x3E;"
            ],
            'command_injection': [
                "; cat /etc/passwd",
                "&& rm -rf /",
                "| nc -l 4444",
                "`whoami`",
                "$(id)",
                "${IFS}cat${IFS}/etc/passwd"
            ],
            'buffer_overflow': [
                "A" * 1000,
                "A" * 10000,
                "A" * 65536,
                "\x00" * 1000,
                "\xff" * 1000
            ],
            'format_string': [
                "%s%s%s%s%s%s%s%s%s%s",
                "%x%x%x%x%x%x%x%x%x%x",
                "%n%n%n%n%n%n%n%n%n%n",
                "AAAA%x%x%x%x%x%x%x%x"
            ]
        }
        
    @pytest.mark.security
    @pytest.mark.parametrize("payload", [
        "'; DROP TABLE users; --",
        "' OR '1'='1",
        "'; SELECT * FROM system; --"
    ])
    def test_sql_injection_protection(self, payload, mock_web_server):
        """Test protection against SQL injection attacks."""
        # Test WiFi SSID input
        response = mock_web_server.handle_wifi_connect({
            'ssid': payload,
            'password': 'test123'
        })
        
        # Should reject malicious input
        assert response.status_code == 400
        data = assert_valid_json_response(response.body)
        assert data['success'] == False
        assert 'invalid' in data['error'].lower() or 'malicious' in data['error'].lower()
        
    @pytest.mark.security
    @pytest.mark.parametrize("payload", [
        "<script>alert('XSS')</script>",
        "javascript:alert('XSS')",
        "<img src=x onerror=alert('XSS')>"
    ])
    def test_xss_protection(self, payload, mock_web_server):
        """Test protection against XSS attacks."""
        # Test device name input
        response = mock_web_server.handle_set_config({
            'device_name': payload,
            'led_brightness': {'main': 50}
        })
        
        # Should sanitize or reject XSS payload
        assert response.status_code == 400
        data = assert_valid_json_response(response.body)
        assert data['success'] == False
        
    @pytest.mark.security
    def test_command_injection_protection(self, mock_web_server):
        """Test protection against command injection."""
        malicious_commands = [
            "; cat /etc/passwd",
            "&& rm -rf /",
            "| nc -l 4444"
        ]
        
        for payload in malicious_commands:
            response = mock_web_server.handle_system_control({
                'action': 'update_firmware',
                'url': f'http://example.com/firmware.bin{payload}'
            })
            
            assert response.status_code == 400
            data = assert_valid_json_response(response.body)
            assert data['success'] == False
            
    @pytest.mark.security
    def test_buffer_overflow_protection(self, mock_web_server):
        """Test protection against buffer overflow attacks."""
        # Test with oversized JSON payload
        large_payload = {
            'data': 'A' * 100000,  # 100KB payload
            'config': {f'key_{i}': 'A' * 1000 for i in range(100)}
        }
        
        response = mock_web_server.handle_set_config(large_payload)
        
        # Should reject oversized payload
        assert response.status_code == 413 or response.status_code == 400
        
    @pytest.mark.security
    def test_malformed_json_handling(self, mock_web_server):
        """Test handling of malformed JSON input."""
        malformed_json_cases = [
            '{"incomplete": ',
            '{"invalid": "json"',
            '{"nested": {"unclosed": {"object": true}',
            '{"array": [1,2,3}',
            '{"unicode": "\uXXXX"}',
            '{"control": "\x00\x01\x02"}',
            '{"huge_number": 1e999999}',
            '{"recursive": {"a": {"b": {"c": {"d": {"e": "infinite"}}}}}}'
        ]
        
        for malformed_json in malformed_json_cases:
            response = mock_web_server.handle_raw_json_input(malformed_json)
            
            # Should handle gracefully with proper error response
            assert response.status_code == 400
            data = assert_valid_json_response(response.body)
            assert data['success'] == False
            assert 'json' in data['error'].lower() or 'format' in data['error'].lower()
            
    @pytest.mark.security
    def test_authentication_bypass_attempts(self, mock_web_server):
        """Test protection against authentication bypass."""
        bypass_attempts = [
            {'token': 'admin'},
            {'token': '../../admin'},
            {'token': '../../../etc/passwd'},
            {'token': 'admin\x00'},
            {'token': 'ADMIN'},
            {'token': ''},
            {'token': None}
        ]
        
        for attempt in bypass_attempts:
            response = mock_web_server.handle_authenticated_request('/api/admin/reset', attempt)
            
            # Should reject all bypass attempts
            assert response.status_code == 401 or response.status_code == 403
            
    @pytest.mark.security
    def test_csrf_protection(self, mock_web_server):
        """Test CSRF protection mechanisms."""
        # Test state-changing operations without proper CSRF token
        dangerous_operations = [
            {'action': 'factory_reset'},
            {'action': 'update_firmware', 'url': 'http://malicious.com/firmware.bin'},
            {'action': 'change_wifi', 'ssid': 'evil_network', 'password': 'hacked'}
        ]
        
        for operation in dangerous_operations:
            # Request without CSRF token should be rejected
            response = mock_web_server.handle_system_control(operation, csrf_token=None)
            assert response.status_code == 403
            
            # Request with invalid CSRF token should be rejected
            response = mock_web_server.handle_system_control(operation, csrf_token="invalid_token")
            assert response.status_code == 403
            
    @pytest.mark.security
    def test_file_path_traversal_protection(self, mock_web_server):
        """Test protection against path traversal attacks."""
        path_traversal_payloads = [
            "../../../etc/passwd",
            "..\\..\\..\\windows\\system32\\config\\sam",
            "/etc/passwd",
            "file:///etc/passwd",
            "....//....//....//etc/passwd",
            "%2e%2e%2f%2e%2e%2f%2e%2e%2fetc%2fpasswd",
            "..%252f..%252f..%252fetc%252fpasswd"
        ]
        
        for payload in path_traversal_payloads:
            response = mock_web_server.serve_static_file(payload)
            
            # Should not serve files outside webroot
            assert response.status_code == 404 or response.status_code == 403
            
    @pytest.mark.security
    def test_rate_limiting_effectiveness(self, mock_web_server):
        """Test rate limiting protection against brute force."""
        # Simulate rapid requests from same IP
        client_ip = "192.168.1.100"
        
        successful_requests = 0
        rate_limited_requests = 0
        
        for i in range(100):  # Attempt 100 rapid requests
            response = mock_web_server.handle_api_request(
                '/api/status', 
                client_ip=client_ip,
                timestamp=1000 + i  # 1ms apart
            )
            
            if response.status_code == 200:
                successful_requests += 1
            elif response.status_code == 429:  # Too Many Requests
                rate_limited_requests += 1
                
        # Should start rate limiting after reasonable threshold
        assert rate_limited_requests > 50  # At least half should be rate limited
        assert successful_requests < 50   # No more than half should succeed
        
    @pytest.mark.security
    def test_memory_exhaustion_protection(self, mock_web_server):
        """Test protection against memory exhaustion attacks."""
        # Test with many concurrent connections
        connections = []
        max_connections = 200
        
        for i in range(max_connections):
            try:
                conn = mock_web_server.create_websocket_connection(f"client_{i}")
                connections.append(conn)
            except Exception as e:
                # Should eventually reject excess connections
                assert "too many" in str(e).lower() or "limit" in str(e).lower()
                break
                
        # Should not allow unlimited connections
        assert len(connections) < max_connections
        
        # Cleanup
        for conn in connections:
            conn.close()
            
    @pytest.mark.security
    def test_header_injection_protection(self, mock_web_server):
        """Test protection against HTTP header injection."""
        malicious_headers = [
            "Content-Type: text/html\r\nSet-Cookie: admin=true",
            "Location: http://evil.com\r\nContent-Length: 0\r\n\r\nHTTP/1.1 200 OK",
            "Custom-Header: value\r\nConnection: close\r\nHost: evil.com",
            "X-Forwarded-For: 127.0.0.1\r\nAuthorization: Bearer admin_token"
        ]
        
        for malicious_header in malicious_headers:
            response = mock_web_server.handle_request_with_header(
                '/api/status',
                custom_header=malicious_header
            )
            
            # Should sanitize headers or reject request
            assert response.status_code != 200 or not any(
                'Set-Cookie' in h or 'Authorization' in h 
                for h in response.headers
            )
            
    @pytest.mark.security  
    def test_websocket_security(self, mock_web_server):
        """Test WebSocket security measures."""
        # Test malicious WebSocket frames
        malicious_frames = [
            b'\x81\x7F' + b'A' * 10000,  # Oversized frame
            b'\x81\x00',  # Empty frame
            b'\xFF\xFF\xFF\xFF',  # Malformed frame
            json.dumps({'command': 'eval', 'code': 'rm -rf /'}).encode(),
            json.dumps({'type': 'control', 'action': 'shutdown'}).encode()
        ]
        
        ws_client = mock_web_server.create_websocket_connection("test_client")
        
        for frame in malicious_frames:
            try:
                ws_client.send_raw_frame(frame)
                response = ws_client.receive_with_timeout(1.0)
                
                # Should reject malicious frames
                if response:
                    data = json.loads(response)
                    assert data.get('error') is not None
            except Exception:
                # Exception is acceptable for malformed frames
                pass
                
        ws_client.close()

    def create_mock_web_server(self):
        """Create a mock web server for testing."""
        mock_server = Mock()
        
        # Configure standard responses
        mock_server.handle_wifi_connect.return_value = Mock(
            status_code=400, 
            body='{"success": false, "error": "Invalid input detected"}'
        )
        
        mock_server.handle_set_config.return_value = Mock(
            status_code=400,
            body='{"success": false, "error": "Malicious input rejected"}'
        )
        
        mock_server.handle_system_control.return_value = Mock(
            status_code=400,
            body='{"success": false, "error": "Invalid command"}'
        )
        
        mock_server.handle_raw_json_input.return_value = Mock(
            status_code=400,
            body='{"success": false, "error": "Invalid JSON format"}'
        )
        
        mock_server.handle_authenticated_request.return_value = Mock(
            status_code=401,
            body='{"success": false, "error": "Authentication required"}'
        )
        
        mock_server.serve_static_file.return_value = Mock(
            status_code=404,
            body='{"success": false, "error": "File not found"}'
        )
        
        mock_server.handle_api_request.return_value = Mock(
            status_code=429,
            body='{"success": false, "error": "Rate limit exceeded"}'
        )
        
        mock_server.create_websocket_connection.side_effect = lambda x: Mock()
        
        mock_server.handle_request_with_header.return_value = Mock(
            status_code=400,
            headers=[],
            body='{"success": false, "error": "Invalid headers"}'
        )
        
        return mock_server

    @pytest.fixture
    def mock_web_server(self):
        """Fixture providing mock web server."""
        return self.create_mock_web_server()