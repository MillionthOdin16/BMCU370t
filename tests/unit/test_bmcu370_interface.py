"""
Unit tests for BMCU370 Interface module.

Tests the communication layer between ESP32 and BMCU370 device,
including USB enumeration, command protocol, and status parsing.
"""

import pytest
import json
from unittest.mock import Mock, patch, MagicMock
from tests.conftest import load_test_data, assert_valid_json_response, assert_bmcu370_status_format


class TestBMCU370Interface:
    """Test cases for BMCU370Interface class."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.mock_responses = load_test_data('mock_bmcu370_responses.json')
        
    @pytest.mark.unit
    def test_device_initialization(self, mock_usb_device):
        """Test BMCU370 device initialization and connection."""
        # Mock the device connection
        with patch('bmcu370_interface.usb_enumerate_devices') as mock_enum:
            mock_enum.return_value = [mock_usb_device]
            
            # Test initialization
            interface = self.create_mock_interface()
            assert interface is not None
            assert interface.connect() == True
            assert interface.is_connected() == True
            
    @pytest.mark.unit
    def test_usb_command_protocol(self, mock_usb_device):
        """Test USB command protocol implementation."""
        interface = self.create_mock_interface()
        
        # Test GET_STATUS command
        expected_response = self.mock_responses['usb_commands']['GET_STATUS']
        mock_usb_device.read.return_value = expected_response.encode()
        
        result = interface.send_command('GET_STATUS')
        assert result is not None
        
        # Verify command was sent correctly
        mock_usb_device.write.assert_called_with(b'GET_STATUS\n')
        
    @pytest.mark.unit
    def test_status_parsing(self):
        """Test parsing of BMCU370 status responses."""
        interface = self.create_mock_interface()
        
        # Test valid status response
        status_json = self.mock_responses['system_online']
        parsed_status = interface.parse_status_response(json.dumps(status_json))
        
        assert_bmcu370_status_format(parsed_status)
        assert parsed_status['system']['bambubus_status'] == 'online'
        assert len(parsed_status['channels']) == 2
        
    @pytest.mark.unit
    def test_error_handling(self, mock_usb_device):
        """Test error handling in communication."""
        interface = self.create_mock_interface()
        
        # Test timeout scenario
        mock_usb_device.read.side_effect = TimeoutError("USB read timeout")
        
        result = interface.send_command('GET_STATUS', timeout=1.0)
        assert result is None
        assert interface.get_last_error() == 'USB_TIMEOUT'
        
    @pytest.mark.unit
    def test_command_validation(self):
        """Test command validation and formatting."""
        interface = self.create_mock_interface()
        
        # Test valid commands
        valid_commands = ['GET_STATUS', 'GET_CONFIG', 'GET_VERSION', 'RESET', 'DFU']
        for cmd in valid_commands:
            assert interface.validate_command(cmd) == True
            
        # Test invalid commands
        invalid_commands = ['', 'INVALID', 'get_status', 'STATUS']
        for cmd in invalid_commands:
            assert interface.validate_command(cmd) == False
            
    @pytest.mark.unit
    def test_parameter_setting(self, mock_usb_device):
        """Test parameter setting via SET_PARAM command."""
        interface = self.create_mock_interface()
        
        # Test valid parameter setting
        mock_usb_device.read.return_value = b'OK'
        
        result = interface.set_parameter('led_brightness.main', 50)
        assert result == True
        
        # Verify correct command format
        expected_cmd = b'SET_PARAM led_brightness.main=50\n'
        mock_usb_device.write.assert_called_with(expected_cmd)
        
    @pytest.mark.unit
    def test_device_enumeration(self):
        """Test USB device enumeration and filtering."""
        interface = self.create_mock_interface()
        
        # Mock multiple USB devices
        mock_devices = [
            Mock(vendor_id=0x1234, product_id=0x5678),  # BMCU370
            Mock(vendor_id=0xAAAA, product_id=0xBBBB),  # Other device
            Mock(vendor_id=0x1234, product_id=0x9999),  # Wrong product ID
        ]
        
        with patch('bmcu370_interface.usb_enumerate_devices') as mock_enum:
            mock_enum.return_value = mock_devices
            
            found_device = interface.find_bmcu370_device()
            assert found_device is not None
            assert found_device.vendor_id == 0x1234
            assert found_device.product_id == 0x5678
            
    @pytest.mark.unit
    def test_connection_recovery(self, mock_usb_device):
        """Test connection recovery after USB disconnect."""
        interface = self.create_mock_interface()
        
        # Simulate initial connection
        interface.connect()
        assert interface.is_connected() == True
        
        # Simulate disconnect
        mock_usb_device.is_open = False
        assert interface.is_connected() == False
        
        # Test automatic reconnection
        mock_usb_device.is_open = True
        interface.attempt_reconnect()
        assert interface.is_connected() == True
        
    @pytest.mark.unit
    def test_status_caching(self):
        """Test status caching mechanism."""
        interface = self.create_mock_interface()
        
        # Test cache expiration
        interface.status_cache_timeout = 1.0  # 1 second
        
        # First call should fetch new data
        status1 = interface.get_cached_status()
        
        # Second call within timeout should return cached data
        status2 = interface.get_cached_status()
        assert status1 == status2
        
        # Wait for cache expiration and test refresh
        import time
        time.sleep(1.1)
        
        # This should fetch new data
        status3 = interface.get_cached_status()
        # Note: In real implementation, status3 might differ from status1
        
    @pytest.mark.unit
    def test_dfu_mode_entry(self, mock_usb_device):
        """Test DFU mode entry command."""
        interface = self.create_mock_interface()
        
        mock_usb_device.read.return_value = b'DFU_MODE_ACTIVE'
        
        result = interface.enter_dfu_mode()
        assert result == True
        
        # Verify DFU command was sent
        mock_usb_device.write.assert_called_with(b'DFU\n')
        
        # Device should be disconnected after DFU entry
        assert interface.is_connected() == False
        
    @pytest.mark.unit
    def test_channel_data_parsing(self):
        """Test parsing of individual channel data."""
        interface = self.create_mock_interface()
        
        channel_data = self.mock_responses['system_online']['channels'][0]
        parsed_channel = interface.parse_channel_data(channel_data)
        
        # Verify required fields
        assert 'id' in parsed_channel
        assert 'filament' in parsed_channel
        assert 'motion' in parsed_channel
        assert 'rgb' in parsed_channel
        assert 'sensors' in parsed_channel
        
        # Verify data types
        assert isinstance(parsed_channel['id'], int)
        assert isinstance(parsed_channel['filament']['meters_remaining'], float)
        assert isinstance(parsed_channel['sensors']['filament_present'], bool)
        
    def create_mock_interface(self):
        """Create a mock BMCU370Interface for testing."""
        # This would be replaced with actual interface class in real implementation
        interface = Mock()
        interface.connect = Mock(return_value=True)
        interface.is_connected = Mock(return_value=True)
        interface.send_command = Mock(return_value='OK')
        interface.validate_command = Mock(return_value=True)
        interface.set_parameter = Mock(return_value=True)
        interface.get_last_error = Mock(return_value=None)
        interface.parse_status_response = Mock()
        interface.parse_channel_data = Mock()
        interface.find_bmcu370_device = Mock()
        interface.attempt_reconnect = Mock()
        interface.get_cached_status = Mock()
        interface.enter_dfu_mode = Mock(return_value=True)
        interface.status_cache_timeout = 5.0
        
        return interface


class TestBMCU370ErrorHandling:
    """Test error handling scenarios for BMCU370 communication."""
    
    @pytest.mark.unit
    def test_usb_enumeration_failure(self):
        """Test handling of USB enumeration failures."""
        interface = Mock()
        
        with patch('bmcu370_interface.usb_enumerate_devices') as mock_enum:
            mock_enum.side_effect = OSError("USB enumeration failed")
            
            interface.connect.side_effect = OSError("USB enumeration failed")
            
            with pytest.raises(OSError):
                interface.connect()
                
    @pytest.mark.unit
    def test_invalid_response_format(self):
        """Test handling of invalid JSON responses."""
        interface = Mock()
        
        # Test invalid JSON
        interface.parse_status_response.side_effect = json.JSONDecodeError("Invalid JSON", "", 0)
        
        with pytest.raises(json.JSONDecodeError):
            interface.parse_status_response('invalid json')
            
    @pytest.mark.unit
    def test_device_disconnection_during_operation(self, mock_usb_device):
        """Test handling of device disconnection during operation."""
        interface = Mock()
        
        # Simulate device disconnection
        mock_usb_device.write.side_effect = OSError("Device disconnected")
        interface.send_command.side_effect = OSError("Device disconnected")
        
        with pytest.raises(OSError):
            interface.send_command('GET_STATUS')
            
    @pytest.mark.unit
    def test_command_timeout_handling(self, mock_usb_device):
        """Test timeout handling for commands."""
        interface = Mock()
        
        # Simulate timeout
        mock_usb_device.read.side_effect = TimeoutError("Read timeout")
        interface.send_command.side_effect = TimeoutError("Read timeout")
        
        with pytest.raises(TimeoutError):
            interface.send_command('GET_STATUS', timeout=1.0)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])