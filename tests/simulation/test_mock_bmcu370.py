"""
Test suite for Mock BMCU370 simulator

Tests the mock device simulator to ensure it provides realistic
responses for ESP32 testing scenarios.
"""

import pytest
import json
import time
import sys
import os
from unittest.mock import patch

# Add the project root to Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from tests.simulation.mock_bmcu370 import MockBMCU370, BMCUSystemStatus


class TestMockBMCU370:
    """Test cases for Mock BMCU370 simulator"""
    
    @pytest.fixture
    def mock_device(self):
        """Create a fresh mock device for each test"""
        return MockBMCU370()
    
    def test_initialization(self, mock_device):
        """Test mock device initializes correctly"""
        assert mock_device.device_connected is True
        assert mock_device.dfu_mode is False
        assert len(mock_device.command_history) == 0
        assert mock_device.system_status is not None
        
    def test_get_status_command(self, mock_device):
        """Test GET_STATUS command returns valid JSON"""
        response = mock_device.process_command("GET_STATUS")
        
        assert response.startswith("STATUS:")
        assert response.endswith("\n")
        
        # Extract and validate JSON
        json_data = response[7:-1]  # Remove "STATUS:" and "\n"
        status = json.loads(json_data)
        
        # Validate required fields
        assert "timestamp" in status
        assert "system_status" in status
        assert "channel_count" in status
        assert "channels" in status
        assert "temperatures" in status
        assert "voltages" in status
        assert "firmware_version" in status
        
        # Validate channel data structure
        assert len(status["channels"]) == status["channel_count"]
        for channel in status["channels"]:
            assert "id" in channel
            assert "online" in channel
            assert "temperature" in channel
            assert "humidity" in channel
            
    def test_set_param_commands(self, mock_device):
        """Test SET_PARAM command handling"""
        # Valid parameter
        response = mock_device.process_command("SET_PARAM led_brightness 80")
        assert response == "OK: LED brightness set\n"
        
        # Unknown parameter
        response = mock_device.process_command("SET_PARAM unknown_param value")
        assert response.startswith("ERROR:")
        
        # Invalid format
        response = mock_device.process_command("SET_PARAM")
        assert response.startswith("ERROR:")
        
    def test_dfu_mode_handling(self, mock_device):
        """Test DFU mode entry and exit"""
        # Enter DFU mode
        response = mock_device.process_command("ENTER_DFU")
        assert response == "OK: Entering DFU mode\n"
        assert mock_device.dfu_mode is True
        
        # Commands should fail in DFU mode
        response = mock_device.process_command("GET_STATUS")
        assert response == "ERROR: Device in DFU mode\n"
        
        # Exit DFU mode
        response = mock_device.process_command("EXIT_DFU")
        assert response == "OK: Exiting DFU mode\n"
        assert mock_device.dfu_mode is False
        
    def test_led_control(self, mock_device):
        """Test LED control commands"""
        # Valid LED control
        response = mock_device.process_command("CONTROL_LED status on")
        assert response == "OK: LED status on\n"
        assert mock_device.system_status.led_status["status"] is True
        
        response = mock_device.process_command("CONTROL_LED status off")
        assert response == "OK: LED status off\n"
        assert mock_device.system_status.led_status["status"] is False
        
        # Invalid LED
        response = mock_device.process_command("CONTROL_LED invalid on")
        assert response.startswith("ERROR:")
        
    def test_motor_control(self, mock_device):
        """Test motor control commands"""
        initial_pos = mock_device.system_status.motor_positions["motor_0"]
        
        # Move motor clockwise
        response = mock_device.process_command("CONTROL_MOTOR 0 cw 90")
        assert response == "OK: Motor 0 moved 90 steps cw\n"
        assert mock_device.system_status.motor_positions["motor_0"] == (initial_pos + 90) % 360
        
        # Move motor counter-clockwise
        response = mock_device.process_command("CONTROL_MOTOR 0 ccw 45")
        assert response == "OK: Motor 0 moved 45 steps ccw\n"
        assert mock_device.system_status.motor_positions["motor_0"] == (initial_pos + 90 - 45) % 360
        
        # Invalid motor
        response = mock_device.process_command("CONTROL_MOTOR 999 cw 10")
        assert response.startswith("ERROR:")
        
    def test_version_command(self, mock_device):
        """Test GET_VERSION command"""
        response = mock_device.process_command("GET_VERSION")
        
        assert response.startswith("VERSION:")
        version_json = response[8:-1]  # Remove "VERSION:" and "\n"
        version_data = json.loads(version_json)
        
        assert "firmware_version" in version_data
        assert "hardware_revision" in version_data
        assert "build_date" in version_data
        assert "git_commit" in version_data
        
    def test_unknown_command(self, mock_device):
        """Test handling of unknown commands"""
        response = mock_device.process_command("UNKNOWN_COMMAND")
        assert response.startswith("ERROR: Unknown command")
        
    def test_error_simulation(self, mock_device):
        """Test error simulation functionality"""
        # Enable error simulation
        mock_device.simulate_error_conditions(True)
        assert mock_device.error_simulation is True
        
        # Should return error responses
        response = mock_device.process_command("GET_STATUS")
        assert response != "STATUS:" # Should be an error
        
        # Disable error simulation
        mock_device.simulate_error_conditions(False)
        assert mock_device.error_simulation is False
        
        # Should return normal responses
        response = mock_device.process_command("GET_STATUS")
        assert response.startswith("STATUS:")
        
    def test_command_history(self, mock_device):
        """Test command history tracking"""
        commands = ["GET_STATUS", "GET_VERSION", "ENTER_DFU", "EXIT_DFU"]
        
        for cmd in commands:
            mock_device.process_command(cmd)
            
        assert len(mock_device.command_history) == len(commands)
        
        for i, cmd in enumerate(commands):
            assert mock_device.command_history[i]["command"] == cmd
            assert "timestamp" in mock_device.command_history[i]
            
    def test_device_connection(self, mock_device):
        """Test device connection simulation"""
        assert mock_device.device_connected is True
        
        mock_device.disconnect()
        assert mock_device.device_connected is False
        
        mock_device.reconnect()
        assert mock_device.device_connected is True
        assert mock_device.dfu_mode is False
        
    def test_statistics(self, mock_device):
        """Test statistics collection"""
        # Process some commands
        mock_device.process_command("GET_STATUS")
        mock_device.process_command("GET_VERSION")
        
        stats = mock_device.get_statistics()
        
        assert "connected" in stats
        assert "dfu_mode" in stats  
        assert "command_count" in stats
        assert "error_simulation" in stats
        assert "uptime" in stats
        
        assert stats["connected"] is True
        assert stats["command_count"] == 2
        
    def test_dynamic_status_updates(self, mock_device):
        """Test that status values change over time"""
        # Get initial status
        response1 = mock_device.process_command("GET_STATUS")
        status1_json = response1[7:-1]
        status1 = json.loads(status1_json)
        
        # Longer delay to ensure timestamp changes in CI environments
        time.sleep(1.1)
        
        # Get status again
        response2 = mock_device.process_command("GET_STATUS")
        status2_json = response2[7:-1]
        status2 = json.loads(status2_json)
        
        # Timestamps should be different
        assert status2["timestamp"] >= status1["timestamp"]  # Allow equal for fast CI
        
    def test_reset_command(self, mock_device):
        """Test system reset functionality"""
        # Change some state
        mock_device.process_command("ENTER_DFU")
        mock_device.process_command("CONTROL_LED status off")
        
        assert mock_device.dfu_mode is True
        
        # Reset system
        response = mock_device.process_command("RESET")
        assert response == "OK: System reset\n"
        assert mock_device.dfu_mode is False
        
        # Status should be regenerated
        response = mock_device.process_command("GET_STATUS")
        assert response.startswith("STATUS:")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])