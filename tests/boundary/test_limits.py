"""
Boundary and limit testing for ESP32 web interface.

Tests system behavior at operational limits, boundary conditions,
and edge cases for all configurable parameters and inputs.
"""

import pytest
import json
import sys
from unittest.mock import Mock, patch
from tests.conftest import load_test_data, assert_valid_json_response


class TestBoundaryConditions:
    """Test boundary conditions and operational limits."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.boundary_configs = load_test_data('test_configurations.json')['boundary_tests']
        
    @pytest.mark.boundary
    def test_numeric_parameter_boundaries(self, mock_web_server):
        """Test numeric parameters at their boundaries."""
        numeric_boundaries = [
            # LED Brightness boundaries
            {'param': 'led_brightness.main', 'min': 0, 'max': 100, 'invalid_low': -1, 'invalid_high': 101},
            {'param': 'led_brightness.channels', 'min': 0, 'max': 100, 'invalid_low': -5, 'invalid_high': 255},
            
            # Voltage thresholds
            {'param': 'voltage_thresholds.high', 'min': 1.0, 'max': 5.5, 'invalid_low': 0.5, 'invalid_high': 6.0},
            {'param': 'voltage_thresholds.low', 'min': 0.8, 'max': 4.0, 'invalid_low': 0.1, 'invalid_high': 5.0},
            
            # Communication settings
            {'param': 'communication.baud_rate', 'min': 9600, 'max': 921600, 'invalid_low': 1200, 'invalid_high': 2000000},
            {'param': 'communication.timeout_ms', 'min': 100, 'max': 30000, 'invalid_low': 50, 'invalid_high': 60000},
            
            # Sensor calibration
            {'param': 'sensor_calibration.temperature_offset', 'min': -10.0, 'max': 10.0, 'invalid_low': -20.0, 'invalid_high': 20.0},
            {'param': 'sensor_calibration.voltage_offset', 'min': -0.5, 'max': 0.5, 'invalid_low': -1.0, 'invalid_high': 1.0},
            
            # Network settings
            {'param': 'network.connection_timeout', 'min': 5, 'max': 300, 'invalid_low': 1, 'invalid_high': 600},
            {'param': 'network.retry_attempts', 'min': 1, 'max': 10, 'invalid_low': 0, 'invalid_high': 50}
        ]
        
        for boundary in numeric_boundaries:
            param_path = boundary['param'].split('.')
            
            # Test minimum valid value
            config = self.create_nested_config(param_path, boundary['min'])
            response = mock_web_server.validate_configuration(config)
            assert response['valid'] == True, f"Minimum value {boundary['min']} should be valid for {boundary['param']}"
            
            # Test maximum valid value
            config = self.create_nested_config(param_path, boundary['max'])
            response = mock_web_server.validate_configuration(config)
            assert response['valid'] == True, f"Maximum value {boundary['max']} should be valid for {boundary['param']}"
            
            # Test invalid low value
            config = self.create_nested_config(param_path, boundary['invalid_low'])
            response = mock_web_server.validate_configuration(config)
            assert response['valid'] == False, f"Invalid low value {boundary['invalid_low']} should be rejected for {boundary['param']}"
            
            # Test invalid high value
            config = self.create_nested_config(param_path, boundary['invalid_high'])
            response = mock_web_server.validate_configuration(config)
            assert response['valid'] == False, f"Invalid high value {boundary['invalid_high']} should be rejected for {boundary['param']}"
            
    @pytest.mark.boundary
    def test_string_length_boundaries(self, mock_web_server):
        """Test string parameters at length boundaries."""
        string_boundaries = [
            {'param': 'device_name', 'min_len': 1, 'max_len': 32},
            {'param': 'wifi.ssid', 'min_len': 1, 'max_len': 32},
            {'param': 'wifi.password', 'min_len': 8, 'max_len': 63},
            {'param': 'network.hostname', 'min_len': 1, 'max_len': 63},
            {'param': 'system.description', 'min_len': 0, 'max_len': 255},
            {'param': 'api.access_token', 'min_len': 16, 'max_len': 128}
        ]
        
        for boundary in string_boundaries:
            param_path = boundary['param'].split('.')
            
            # Test minimum length (if > 0)
            if boundary['min_len'] > 0:
                min_string = 'A' * boundary['min_len']
                config = self.create_nested_config(param_path, min_string)
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == True, f"Minimum length string should be valid for {boundary['param']}"
                
            # Test maximum length
            max_string = 'A' * boundary['max_len']
            config = self.create_nested_config(param_path, max_string)
            response = mock_web_server.validate_configuration(config)
            assert response['valid'] == True, f"Maximum length string should be valid for {boundary['param']}"
            
            # Test below minimum length (if min > 0)
            if boundary['min_len'] > 0:
                short_string = 'A' * (boundary['min_len'] - 1) if boundary['min_len'] > 1 else ''
                config = self.create_nested_config(param_path, short_string)
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == False, f"Below minimum length string should be rejected for {boundary['param']}"
                
            # Test above maximum length
            long_string = 'A' * (boundary['max_len'] + 1)
            config = self.create_nested_config(param_path, long_string)
            response = mock_web_server.validate_configuration(config)
            assert response['valid'] == False, f"Above maximum length string should be rejected for {boundary['param']}"
            
    @pytest.mark.boundary
    def test_array_size_boundaries(self, mock_web_server):
        """Test array parameters at size boundaries."""
        array_boundaries = [
            {'param': 'channel_config', 'min_size': 1, 'max_size': 16},
            {'param': 'sensor_channels', 'min_size': 0, 'max_size': 8},
            {'param': 'calibration_points', 'min_size': 2, 'max_size': 10},
            {'param': 'network.dns_servers', 'min_size': 1, 'max_size': 4},
            {'param': 'api.allowed_origins', 'min_size': 0, 'max_size': 20}
        ]
        
        for boundary in array_boundaries:
            param_path = boundary['param'].split('.')
            
            # Test minimum size
            if boundary['min_size'] > 0:
                min_array = list(range(boundary['min_size']))
                config = self.create_nested_config(param_path, min_array)
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == True, f"Minimum size array should be valid for {boundary['param']}"
                
            # Test maximum size
            max_array = list(range(boundary['max_size']))
            config = self.create_nested_config(param_path, max_array)
            response = mock_web_server.validate_configuration(config)
            assert response['valid'] == True, f"Maximum size array should be valid for {boundary['param']}"
            
            # Test below minimum size (if min > 0)
            if boundary['min_size'] > 0:
                small_array = list(range(boundary['min_size'] - 1))
                config = self.create_nested_config(param_path, small_array)
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == False, f"Below minimum size array should be rejected for {boundary['param']}"
                
            # Test above maximum size
            large_array = list(range(boundary['max_size'] + 1))
            config = self.create_nested_config(param_path, large_array)
            response = mock_web_server.validate_configuration(config)
            assert response['valid'] == False, f"Above maximum size array should be rejected for {boundary['param']}"
            
    @pytest.mark.boundary
    def test_floating_point_precision_boundaries(self, mock_web_server):
        """Test floating point precision and special values."""
        float_test_cases = [
            # Normal precision values
            {'value': 1.0, 'should_pass': True},
            {'value': 1.5, 'should_pass': True},
            {'value': 1.23456789, 'should_pass': True},
            
            # High precision values
            {'value': 1.123456789012345, 'should_pass': True},
            {'value': 1e-10, 'should_pass': True},
            {'value': 1e10, 'should_pass': False},  # Too large
            
            # Special values
            {'value': float('inf'), 'should_pass': False},  # Infinity
            {'value': float('-inf'), 'should_pass': False},  # Negative infinity
            {'value': float('nan'), 'should_pass': False},  # NaN
            
            # Boundary precision
            {'value': sys.float_info.epsilon, 'should_pass': True},  # Smallest positive
            {'value': sys.float_info.max, 'should_pass': False},     # Largest finite
            {'value': sys.float_info.min, 'should_pass': True},      # Smallest positive normalized
            
            # Scientific notation
            {'value': 1.23e-5, 'should_pass': True},
            {'value': 4.56e2, 'should_pass': True},
            {'value': 7.89e20, 'should_pass': False}  # Too large
        ]
        
        for test_case in float_test_cases:
            config = {'voltage_threshold': test_case['value']}
            response = mock_web_server.validate_configuration(config)
            
            expected_result = test_case['should_pass']
            actual_result = response['valid']
            
            assert actual_result == expected_result, \
                f"Float value {test_case['value']} validation result {actual_result} != expected {expected_result}"
                
    @pytest.mark.boundary
    def test_json_nesting_depth_boundaries(self, mock_web_server):
        """Test JSON nesting depth limits."""
        max_nesting_depth = 10  # Reasonable limit for ESP32
        
        # Test acceptable nesting depth
        nested_config = {}
        current_level = nested_config
        for i in range(max_nesting_depth - 1):
            current_level[f'level_{i}'] = {}
            current_level = current_level[f'level_{i}']
        current_level['value'] = 'deep_value'
        
        response = mock_web_server.validate_configuration(nested_config)
        assert response['valid'] == True, f"Nesting depth {max_nesting_depth - 1} should be acceptable"
        
        # Test excessive nesting depth
        deeply_nested_config = {}
        current_level = deeply_nested_config
        for i in range(max_nesting_depth + 5):  # Exceed limit
            current_level[f'level_{i}'] = {}
            current_level = current_level[f'level_{i}']
        current_level['value'] = 'too_deep'
        
        response = mock_web_server.validate_configuration(deeply_nested_config)
        assert response['valid'] == False, f"Excessive nesting depth should be rejected"
        
    @pytest.mark.boundary
    def test_concurrent_connection_limits(self, mock_web_server):
        """Test concurrent connection handling at limits."""
        max_connections = 50  # ESP32 typical limit
        connections = []
        
        # Create connections up to limit
        for i in range(max_connections):
            try:
                conn = mock_web_server.create_connection(f"client_{i}")
                if conn:
                    connections.append(conn)
                else:
                    break
            except Exception:
                break
                
        # Verify we can create connections up to limit
        assert len(connections) >= max_connections * 0.8, "Should accept connections near limit"
        
        # Try to exceed limit
        excess_connections = []
        for i in range(10):  # Try 10 more
            try:
                conn = mock_web_server.create_connection(f"excess_{i}")
                if conn:
                    excess_connections.append(conn)
            except Exception as e:
                # Should reject excess connections
                assert 'limit' in str(e).lower() or 'too many' in str(e).lower()
                
        # Should not accept unlimited connections
        total_connections = len(connections) + len(excess_connections)
        assert total_connections <= max_connections + 5, "Should not accept unlimited connections"
        
        # Cleanup
        for conn in connections + excess_connections:
            mock_web_server.close_connection(conn)
            
    @pytest.mark.boundary
    def test_memory_allocation_boundaries(self, mock_web_server):
        """Test memory allocation at boundaries."""
        # Test configuration sizes
        memory_test_cases = [
            {
                'name': 'small_config',
                'size_bytes': 1024,  # 1KB
                'should_pass': True
            },
            {
                'name': 'medium_config',
                'size_bytes': 16384,  # 16KB
                'should_pass': True
            },
            {
                'name': 'large_config',
                'size_bytes': 65536,  # 64KB
                'should_pass': True
            },
            {
                'name': 'excessive_config',
                'size_bytes': 1048576,  # 1MB
                'should_pass': False
            }
        ]
        
        for test_case in memory_test_cases:
            # Create configuration of specified size
            large_string = 'A' * (test_case['size_bytes'] // 2)  # Approximate size
            config = {
                'large_data': large_string,
                'metadata': {
                    'size': test_case['size_bytes'],
                    'test_name': test_case['name']
                }
            }
            
            try:
                response = mock_web_server.validate_configuration(config)
                actual_result = response['valid']
            except MemoryError:
                actual_result = False
                
            expected_result = test_case['should_pass']
            assert actual_result == expected_result, \
                f"Memory test {test_case['name']} ({test_case['size_bytes']} bytes): " + \
                f"expected {expected_result}, got {actual_result}"
                
    @pytest.mark.boundary
    def test_time_based_boundaries(self, mock_web_server):
        """Test time-based parameter boundaries."""
        import time
        
        time_boundaries = [
            # Timestamps
            {'param': 'last_update', 'min_time': 0, 'max_time': 2147483647},  # Unix timestamp range
            {'param': 'session_timeout', 'min_seconds': 60, 'max_seconds': 86400},  # 1 min to 24 hours
            {'param': 'data_retention_days', 'min_days': 1, 'max_days': 365},
            {'param': 'heartbeat_interval', 'min_ms': 100, 'max_ms': 60000}  # 100ms to 1 minute
        ]
        
        current_time = int(time.time())
        
        for boundary in time_boundaries:
            param = boundary['param']
            
            if 'min_time' in boundary:
                # Test timestamp boundaries
                config = {param: boundary['min_time']}
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == True, f"Minimum timestamp should be valid for {param}"
                
                config = {param: boundary['max_time']}
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == True, f"Maximum timestamp should be valid for {param}"
                
                # Test invalid timestamps
                config = {param: -1}
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == False, f"Negative timestamp should be invalid for {param}"
                
                config = {param: boundary['max_time'] + 1}
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == False, f"Timestamp overflow should be invalid for {param}"
                
            elif 'min_seconds' in boundary:
                # Test duration boundaries
                config = {param: boundary['min_seconds']}
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == True, f"Minimum duration should be valid for {param}"
                
                config = {param: boundary['max_seconds']}
                response = mock_web_server.validate_configuration(config)
                assert response['valid'] == True, f"Maximum duration should be valid for {param}"
                
    def create_nested_config(self, param_path, value):
        """Create nested configuration from parameter path."""
        config = {}
        current = config
        
        for key in param_path[:-1]:
            current[key] = {}
            current = current[key]
            
        current[param_path[-1]] = value
        return config

    @pytest.fixture
    def mock_web_server(self):
        """Fixture providing mock web server with boundary validation."""
        server = Mock()
        
        def validate_configuration_mock(config):
            """Mock configuration validation with boundary checking."""
            try:
                # Check for obvious invalid values
                config_str = json.dumps(config)
                if len(config_str) > 100000:  # 100KB limit
                    return {'valid': False, 'error': 'Configuration too large'}
                    
                # Check for invalid float values
                for key, value in self.flatten_dict(config).items():
                    if isinstance(value, float):
                        if value != value:  # NaN check
                            return {'valid': False, 'error': f'NaN value in {key}'}
                        if value == float('inf') or value == float('-inf'):
                            return {'valid': False, 'error': f'Infinite value in {key}'}
                            
                    # Check string lengths
                    if isinstance(value, str):
                        if 'password' in key.lower() and len(value) < 8:
                            return {'valid': False, 'error': f'Password too short: {key}'}
                        if len(value) > 255:
                            return {'valid': False, 'error': f'String too long: {key}'}
                            
                    # Check numeric ranges
                    if isinstance(value, (int, float)):
                        if 'brightness' in key.lower() and (value < 0 or value > 100):
                            return {'valid': False, 'error': f'Brightness out of range: {key}'}
                        if 'voltage' in key.lower() and (value < 0.5 or value > 6.0):
                            return {'valid': False, 'error': f'Voltage out of range: {key}'}
                            
                # Check nesting depth
                if self.get_nesting_depth(config) > 10:
                    return {'valid': False, 'error': 'Configuration too deeply nested'}
                    
                return {'valid': True}
                
            except Exception as e:
                return {'valid': False, 'error': str(e)}
                
        def create_connection_mock(client_id):
            """Mock connection creation with limits."""
            if hasattr(server, '_connection_count'):
                server._connection_count += 1
            else:
                server._connection_count = 1
                
            if server._connection_count > 50:  # Simulate connection limit
                raise Exception("Too many connections")
                
            return Mock()
            
        def close_connection_mock(conn):
            """Mock connection closure."""
            if hasattr(server, '_connection_count') and server._connection_count > 0:
                server._connection_count -= 1
                
        server.validate_configuration.side_effect = validate_configuration_mock
        server.create_connection.side_effect = create_connection_mock
        server.close_connection.side_effect = close_connection_mock
        
        return server
        
    def flatten_dict(self, d, parent_key='', sep='.'):
        """Flatten nested dictionary for easier validation."""
        items = []
        for k, v in d.items():
            new_key = f"{parent_key}{sep}{k}" if parent_key else k
            if isinstance(v, dict):
                items.extend(self.flatten_dict(v, new_key, sep).items())
            else:
                items.append((new_key, v))
        return dict(items)
        
    def get_nesting_depth(self, obj, depth=0):
        """Calculate maximum nesting depth of object."""
        if not isinstance(obj, dict):
            return depth
            
        if not obj:
            return depth
            
        return max(self.get_nesting_depth(value, depth + 1) for value in obj.values())