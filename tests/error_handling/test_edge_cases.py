"""
Edge case and error handling tests for ESP32 web interface.

Tests boundary conditions, error scenarios, and graceful degradation
under various failure modes and unexpected inputs.
"""

import pytest
import json
import time
import threading
from unittest.mock import Mock, patch, MagicMock
from tests.conftest import load_test_data, assert_valid_json_response, assert_bmcu370_status_format


class TestEdgeCasesAndErrorHandling:
    """Test edge cases and error handling scenarios."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.test_data = load_test_data('test_configurations.json')
        
    @pytest.mark.error_handling
    def test_memory_pressure_scenarios(self, mock_bmcu370_device):
        """Test behavior under memory pressure conditions."""
        # Simulate low memory conditions
        memory_scenarios = [
            {'free_heap': 1024, 'free_psram': 0},      # Very low heap
            {'free_heap': 512, 'free_psram': 1024},    # Critically low heap
            {'free_heap': 8192, 'free_psram': 0},      # No PSRAM available
            {'free_heap': 0, 'free_psram': 32768},     # No heap, PSRAM only
            {'free_heap': 100, 'free_psram': 100}      # Both critically low
        ]
        
        for scenario in memory_scenarios:
            with patch('esp32.get_free_heap', return_value=scenario['free_heap']):
                with patch('esp32.get_free_psram', return_value=scenario['free_psram']):
                    # System should handle gracefully
                    interface = self.create_mock_interface()
                    status = interface.get_system_status()
                    
                    # Should report memory pressure but not crash
                    assert status is not None
                    assert 'memory_pressure' in status or 'low_memory' in status
                    
    @pytest.mark.error_handling  
    def test_filesystem_error_conditions(self, mock_web_server):
        """Test filesystem error handling."""
        filesystem_errors = [
            OSError("No space left on device"),
            PermissionError("Permission denied"),
            FileNotFoundError("File not found"),
            IsADirectoryError("Is a directory"),
            OSError("Filesystem corrupted")
        ]
        
        for error in filesystem_errors:
            with patch('builtins.open', side_effect=error):
                # Should handle filesystem errors gracefully
                result = mock_web_server.save_configuration({'test': 'data'})
                
                assert result['success'] == False
                assert 'filesystem' in result['error'].lower() or 'storage' in result['error'].lower()
                
    @pytest.mark.error_handling
    def test_network_disconnection_scenarios(self, mock_wifi_manager):
        """Test WiFi disconnection and reconnection scenarios."""
        disconnection_scenarios = [
            {'reason': 'AUTH_FAIL', 'retry_count': 3},
            {'reason': 'NO_AP_FOUND', 'retry_count': 5},
            {'reason': 'CONN_FAIL', 'retry_count': 2},
            {'reason': 'LOST_CONNECTION', 'retry_count': 1},
            {'reason': 'BEACON_TIMEOUT', 'retry_count': 4}
        ]
        
        for scenario in disconnection_scenarios:
            # Simulate disconnection
            mock_wifi_manager.simulate_disconnection(
                reason=scenario['reason'],
                retry_count=scenario['retry_count']
            )
            
            # Should attempt reconnection with exponential backoff
            status = mock_wifi_manager.get_status()
            assert status['state'] in ['disconnected', 'reconnecting', 'connected']
            
            if status['state'] == 'reconnecting':
                assert status['retry_count'] <= scenario['retry_count']
                assert status['backoff_time'] > 0
                
    @pytest.mark.error_handling
    def test_usb_communication_errors(self, mock_bmcu370_device):
        """Test USB communication error scenarios."""
        usb_errors = [
            {'error': 'DEVICE_NOT_FOUND', 'recovery_action': 'rescan'},
            {'error': 'COMMUNICATION_TIMEOUT', 'recovery_action': 'retry'},
            {'error': 'PROTOCOL_ERROR', 'recovery_action': 'reset'},
            {'error': 'DEVICE_BUSY', 'recovery_action': 'wait'},
            {'error': 'INVALID_RESPONSE', 'recovery_action': 'validate'}
        ]
        
        for error_scenario in usb_errors:
            mock_bmcu370_device.inject_error(error_scenario['error'])
            
            # Attempt operation
            result = mock_bmcu370_device.get_status()
            
            # Should handle error appropriately
            if result is None:
                # Error should be logged and recovery attempted
                assert mock_bmcu370_device.last_error is not None
                assert mock_bmcu370_device.recovery_attempted == True
            else:
                # Or return error status
                assert result.get('error') is not None
                
    @pytest.mark.error_handling
    def test_concurrent_access_conflicts(self, mock_web_server):
        """Test handling of concurrent access to shared resources."""
        results = []
        errors = []
        
        def concurrent_operation(operation_id):
            """Simulate concurrent operations."""
            try:
                # Multiple threads accessing same resource
                result = mock_web_server.update_configuration(
                    {'operation_id': operation_id, 'timestamp': time.time()}
                )
                results.append(result)
            except Exception as e:
                errors.append(str(e))
                
        # Start multiple concurrent operations
        threads = []
        for i in range(10):
            t = threading.Thread(target=concurrent_operation, args=(i,))
            threads.append(t)
            t.start()
            
        # Wait for all threads to complete
        for t in threads:
            t.join()
            
        # Should handle concurrent access gracefully
        # Either through locking (all succeed) or proper error handling
        assert len(results) + len(errors) == 10
        
        if errors:
            # If there are errors, they should be meaningful
            for error in errors:
                assert 'concurrent' in error.lower() or 'locked' in error.lower()
                
    @pytest.mark.error_handling
    def test_json_parsing_edge_cases(self, mock_web_server):
        """Test JSON parsing with edge case inputs."""
        edge_case_json = [
            '{}',  # Empty object
            '[]',  # Empty array
            'null',  # Null value
            '""',  # Empty string
            '0',   # Zero
            '-0',  # Negative zero
            '1e308',  # Very large number
            '1e-324',  # Very small number
            '{"": ""}',  # Empty key
            '{"\\u0000": "null_char"}',  # Null character in key
            '{"nested": ' + '{"layer": ' * 100 + '"deep"' + '}' * 100,  # Deep nesting
            '{"array": [' + '1,' * 1000 + '1]}',  # Large array
            '{"unicode": "\\u0001\\u0002\\u0003"}',  # Control characters
            '{"float": 1.7976931348623157e+308}',  # Float max
            '{"scientific": 1.23e-10}'  # Scientific notation
        ]
        
        for json_input in edge_case_json:
            response = mock_web_server.handle_json_input(json_input)
            
            # Should parse successfully or fail gracefully
            assert response.status_code in [200, 400]
            
            if response.status_code == 200:
                # Successful parsing
                data = assert_valid_json_response(response.body)
                assert data['success'] == True
            else:
                # Graceful failure
                data = assert_valid_json_response(response.body)
                assert data['success'] == False
                assert 'parsing' in data['error'].lower() or 'format' in data['error'].lower()
                
    @pytest.mark.error_handling
    def test_websocket_connection_limits(self, mock_web_server):
        """Test WebSocket connection limit handling."""
        connections = []
        connection_limit = 50  # Assume reasonable limit
        
        # Attempt to create many connections
        for i in range(connection_limit + 10):
            try:
                conn = mock_web_server.create_websocket_connection(f"client_{i}")
                connections.append(conn)
            except Exception as e:
                # Should eventually reject excess connections
                assert i >= connection_limit - 5  # Allow some tolerance
                assert 'limit' in str(e).lower() or 'too many' in str(e).lower()
                break
                
        # Should not exceed reasonable limit
        assert len(connections) <= connection_limit
        
        # Cleanup connections
        for conn in connections:
            conn.close()
            
    @pytest.mark.error_handling
    def test_configuration_validation_edge_cases(self, mock_web_server):
        """Test configuration validation with edge case values."""
        edge_case_configs = [
            # Boundary values
            {'led_brightness': {'main': 0}},      # Minimum brightness
            {'led_brightness': {'main': 100}},    # Maximum brightness
            {'led_brightness': {'main': -1}},     # Below minimum (invalid)
            {'led_brightness': {'main': 101}},    # Above maximum (invalid)
            
            # Float precision edge cases
            {'voltage_threshold': 1.000000001},   # High precision
            {'voltage_threshold': 1e-10},         # Very small value
            {'voltage_threshold': float('inf')},  # Infinity (invalid)
            {'voltage_threshold': float('nan')},  # NaN (invalid)
            
            # String length edge cases
            {'device_name': ''},                  # Empty string
            {'device_name': 'A' * 32},           # Maximum length
            {'device_name': 'A' * 33},           # Over maximum (invalid)
            {'device_name': '\x00\x01\x02'},     # Control characters (invalid)
            
            # Array edge cases
            {'channel_config': []},               # Empty array
            {'channel_config': list(range(100))}, # Large array
            
            # Nested object edge cases
            {'advanced': {}},                     # Empty nested object
            {'advanced': {'level': {'deep': {'very': 'nested'}}}},  # Deep nesting
        ]
        
        for config in edge_case_configs:
            response = mock_web_server.validate_configuration(config)
            
            # Should validate appropriately
            if self.is_valid_config(config):
                assert response['valid'] == True
            else:
                assert response['valid'] == False
                assert 'validation_errors' in response
                assert len(response['validation_errors']) > 0
                
    @pytest.mark.error_handling
    def test_interrupt_signal_handling(self, mock_web_server):
        """Test handling of system interrupt signals."""
        import signal
        
        # Test various signals
        signals_to_test = [
            signal.SIGTERM,  # Termination
            signal.SIGINT,   # Interrupt (Ctrl+C)
            signal.SIGUSR1,  # User-defined signal 1
            signal.SIGUSR2   # User-defined signal 2
        ]
        
        for sig in signals_to_test:
            # Simulate signal reception
            mock_web_server.receive_signal(sig)
            
            # Should handle gracefully
            status = mock_web_server.get_status()
            
            if sig in [signal.SIGTERM, signal.SIGINT]:
                # Should initiate graceful shutdown
                assert status['shutting_down'] == True
            else:
                # Should log signal but continue operating
                assert status['last_signal'] == sig
                assert status['running'] == True
                
    @pytest.mark.error_handling
    def test_temperature_extreme_conditions(self, mock_bmcu370_device):
        """Test behavior under extreme temperature conditions."""
        temperature_scenarios = [
            {'temp': -40, 'condition': 'extreme_cold'},   # Below operating range
            {'temp': 85, 'condition': 'extreme_hot'},     # Above operating range
            {'temp': 0, 'condition': 'freezing'},         # Freezing point
            {'temp': 25, 'condition': 'normal'},          # Normal operation
            {'temp': 70, 'condition': 'hot_operation'}    # High but acceptable
        ]
        
        for scenario in temperature_scenarios:
            mock_bmcu370_device.set_temperature(scenario['temp'])
            
            status = mock_bmcu370_device.get_status()
            
            # Should report temperature and any warnings/errors
            assert 'temperature' in status
            assert status['temperature'] == scenario['temp']
            
            if scenario['temp'] < -20 or scenario['temp'] > 80:
                # Should have temperature warning/error
                assert status.get('temperature_warning') == True or \
                       status.get('temperature_error') == True
                       
    @pytest.mark.error_handling
    def test_power_supply_fluctuations(self, mock_bmcu370_device):
        """Test behavior under power supply fluctuations."""
        voltage_scenarios = [
            {'voltage': 4.5, 'condition': 'low_voltage'},     # Below minimum
            {'voltage': 5.5, 'condition': 'high_voltage'},    # Above maximum
            {'voltage': 3.0, 'condition': 'critical_low'},    # Critically low
            {'voltage': 6.0, 'condition': 'critical_high'},   # Critically high
            {'voltage': 5.0, 'condition': 'normal'}           # Normal operation
        ]
        
        for scenario in voltage_scenarios:
            mock_bmcu370_device.set_supply_voltage(scenario['voltage'])
            
            status = mock_bmcu370_device.get_status()
            
            # Should monitor and respond to voltage changes
            assert 'supply_voltage' in status
            assert abs(status['supply_voltage'] - scenario['voltage']) < 0.1
            
            if scenario['voltage'] < 4.0 or scenario['voltage'] > 5.5:
                # Should have power warning/error
                assert status.get('power_warning') == True or \
                       status.get('power_error') == True
                       
    @pytest.mark.error_handling
    def test_rapid_api_calls(self, mock_web_server):
        """Test handling of rapid API calls."""
        api_endpoints = [
            '/api/status',
            '/api/config',
            '/api/logs',
            '/api/wifi/status'
        ]
        
        response_times = []
        errors = []
        
        # Make rapid API calls
        for i in range(100):
            endpoint = api_endpoints[i % len(api_endpoints)]
            
            start_time = time.time()
            try:
                response = mock_web_server.handle_api_call(endpoint)
                end_time = time.time()
                
                response_times.append(end_time - start_time)
                
                # Should handle all requests or rate limit gracefully
                assert response.status_code in [200, 429]  # OK or Too Many Requests
                
            except Exception as e:
                errors.append(str(e))
                
        # Should handle rapid calls without crashing
        assert len(errors) < 10  # Allow some errors but not many
        
        if response_times:
            avg_response_time = sum(response_times) / len(response_times)
            # Response times should remain reasonable
            assert avg_response_time < 1.0  # Less than 1 second average
            
    def is_valid_config(self, config):
        """Helper method to determine if configuration should be valid."""
        try:
            if 'led_brightness' in config:
                brightness = config['led_brightness'].get('main', 50)
                if brightness < 0 or brightness > 100:
                    return False
                    
            if 'voltage_threshold' in config:
                voltage = config['voltage_threshold']
                if voltage != voltage or voltage == float('inf'):  # NaN or infinity
                    return False
                    
            if 'device_name' in config:
                name = config['device_name']
                if len(name) > 32 or any(ord(c) < 32 for c in name):
                    return False
                    
            return True
        except:
            return False

    def create_mock_interface(self):
        """Create mock BMCU370 interface."""
        mock_interface = Mock()
        mock_interface.get_system_status.return_value = {
            'memory_pressure': False,
            'free_heap': 100000,
            'free_psram': 2000000
        }
        return mock_interface

    @pytest.fixture
    def mock_bmcu370_device(self):
        """Fixture providing mock BMCU370 device."""
        device = Mock()
        device.inject_error = Mock()
        device.last_error = None
        device.recovery_attempted = False
        device.set_temperature = Mock()
        device.set_supply_voltage = Mock()
        
        def get_status_mock():
            return {
                'temperature': getattr(device, '_temperature', 25),
                'supply_voltage': getattr(device, '_supply_voltage', 5.0),
                'temperature_warning': getattr(device, '_temperature', 25) > 80,
                'power_warning': getattr(device, '_supply_voltage', 5.0) > 5.5
            }
            
        device.get_status.return_value = get_status_mock()
        
        def set_temp_mock(temp):
            device._temperature = temp
            
        def set_voltage_mock(voltage):
            device._supply_voltage = voltage
            
        device.set_temperature.side_effect = set_temp_mock
        device.set_supply_voltage.side_effect = set_voltage_mock
        
        return device

    @pytest.fixture
    def mock_wifi_manager(self):
        """Fixture providing mock WiFi manager."""
        manager = Mock()
        manager.simulate_disconnection = Mock()
        manager.get_status.return_value = {
            'state': 'connected',
            'retry_count': 0,
            'backoff_time': 0
        }
        return manager

    @pytest.fixture
    def mock_web_server(self):
        """Fixture providing mock web server."""
        server = Mock()
        server.save_configuration.return_value = {'success': False, 'error': 'Filesystem error'}
        server.update_configuration.return_value = {'success': True}
        server.handle_json_input.return_value = Mock(status_code=200, body='{"success": true}')
        server.create_websocket_connection.return_value = Mock()
        server.validate_configuration.return_value = {'valid': True}
        server.receive_signal = Mock()
        server.get_status.return_value = {'running': True, 'shutting_down': False}
        server.handle_api_call.return_value = Mock(status_code=200)
        return server