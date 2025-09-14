"""
Protocol validation tests for BMCU370-ESP32 communication

Tests the communication protocol between ESP32 and BMCU370 using
the mock simulator to validate command/response patterns.
"""

import pytest
import json
import time
import asyncio
import sys
import os
from unittest.mock import Mock, patch

# Add the project root to Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

from tests.simulation.mock_bmcu370 import MockBMCU370


class TestProtocolValidation:
    """Test suite for USB CDC protocol validation"""
    
    @pytest.fixture
    def mock_device(self):
        """Create a fresh mock device for each test"""
        return MockBMCU370()
    
    def test_status_request_cycle(self, mock_device):
        """Test complete status request/response cycle"""
        # Simulate ESP32 requesting status
        response = mock_device.process_command("GET_STATUS")
        
        # Validate response format
        assert response.startswith("STATUS:")
        assert response.endswith("\n")
        
        # Parse status data
        status_json = response[7:-1]
        status_data = json.loads(status_json)
        
        # Validate required ESP32 fields
        required_fields = [
            "timestamp", "system_status", "channel_count", "channels",
            "temperatures", "voltages", "current_consumption", "error_flags"
        ]
        
        for field in required_fields:
            assert field in status_data, f"Missing required field: {field}"
            
        # Validate channel data structure expected by ESP32
        channels = status_data["channels"]
        assert len(channels) == status_data["channel_count"]
        
        for i, channel in enumerate(channels):
            assert channel["id"] == i
            assert "online" in channel
            assert "material_type" in channel
            assert "temperature" in channel
            assert "humidity" in channel
            assert "load_status" in channel
            
    def test_parameter_update_cycle(self, mock_device):
        """Test parameter update request/response cycle"""
        test_params = [
            ("led_brightness", "75"),
            ("motor_speed", "100"),
            ("channel_enable", "1,2,3,4")
        ]
        
        for param_name, param_value in test_params:
            command = f"SET_PARAM {param_name} {param_value}"
            response = mock_device.process_command(command)
            
            # ESP32 expects "OK:" for successful parameter sets
            assert response.startswith("OK:"), f"Failed parameter set: {param_name}"
            
    def test_dfu_workflow(self, mock_device):
        """Test DFU mode workflow as ESP32 would use it"""
        # Normal operation
        response = mock_device.process_command("GET_STATUS")
        assert response.startswith("STATUS:")
        
        # Enter DFU mode (for firmware updates)
        response = mock_device.process_command("ENTER_DFU")
        assert response == "OK: Entering DFU mode\n"
        
        # Status should fail in DFU mode
        response = mock_device.process_command("GET_STATUS")
        assert response.startswith("ERROR:")
        
        # Exit DFU mode (after update complete)
        response = mock_device.process_command("EXIT_DFU")
        assert response == "OK: Exiting DFU mode\n"
        
        # Status should work again
        response = mock_device.process_command("GET_STATUS")
        assert response.startswith("STATUS:")
        
    def test_error_handling_protocol(self, mock_device):
        """Test error condition handling in protocol"""
        # Enable error simulation
        mock_device.simulate_error_conditions(True)
        
        error_responses = []
        for _ in range(10):
            response = mock_device.process_command("GET_STATUS")
            error_responses.append(response)
            
        # Should get various error conditions
        error_types = set(error_responses)
        assert len(error_types) > 1, "Should simulate multiple error types"
        
        # ESP32 should handle these error patterns
        for response in error_responses:
            assert (response.startswith("ERROR:") or 
                   response == "" or 
                   "CORRUPTED" in response), "Unexpected error response format"
                   
    def test_command_timing_requirements(self, mock_device):
        """Test command timing and response delays"""
        # Measure response times
        response_times = []
        
        for _ in range(5):
            start_time = time.time()
            response = mock_device.process_command("GET_STATUS")
            end_time = time.time()
            
            response_time = end_time - start_time
            response_times.append(response_time)
            
            # Should respond within reasonable time for ESP32
            assert response_time < 1.0, "Response too slow for real-time operation"
            assert response_time > 0.01, "Response should include realistic delays"
            
        # Average response time should be reasonable
        avg_response_time = sum(response_times) / len(response_times)
        assert 0.01 < avg_response_time < 0.1, "Average response time out of range"
        
    def test_json_schema_validation(self, mock_device):
        """Test that JSON responses match expected schema"""
        response = mock_device.process_command("GET_STATUS")
        status_json = response[7:-1]
        status_data = json.loads(status_json)
        
        # Validate data types ESP32 expects
        assert isinstance(status_data["timestamp"], int)
        assert isinstance(status_data["system_status"], int)
        assert isinstance(status_data["channel_count"], int)
        assert isinstance(status_data["channels"], list)
        assert isinstance(status_data["temperatures"], dict)
        assert isinstance(status_data["voltages"], dict)
        assert isinstance(status_data["current_consumption"], (int, float))
        assert isinstance(status_data["error_flags"], int)
        
        # Validate voltage ranges ESP32 monitors
        voltages = status_data["voltages"]
        assert 3.0 < voltages["vcc_3v3"] < 3.6, "3.3V rail out of range"
        assert 4.5 < voltages["vcc_5v"] < 5.5, "5V rail out of range"
        assert 11.0 < voltages["vcc_12v"] < 13.0, "12V rail out of range"
        
        # Validate temperature ranges ESP32 monitors
        temps = status_data["temperatures"]
        for temp_name, temp_value in temps.items():
            assert -40 < temp_value < 125, f"Temperature {temp_name} out of valid range"
            
    def test_channel_state_transitions(self, mock_device):
        """Test channel state transitions ESP32 needs to handle"""
        # Get initial status
        response = mock_device.process_command("GET_STATUS")
        status1 = json.loads(response[7:-1])
        
        # Wait for state changes
        time.sleep(0.1)
        
        # Get updated status
        response = mock_device.process_command("GET_STATUS")
        status2 = json.loads(response[7:-1])
        
        # Verify ESP32 can track state changes
        channels1 = {ch["id"]: ch for ch in status1["channels"]}
        channels2 = {ch["id"]: ch for ch in status2["channels"]}
        
        for ch_id in channels1:
            ch1 = channels1[ch_id]
            ch2 = channels2[ch_id]
            
            # Online status should be consistent or change reasonably
            assert isinstance(ch1["online"], bool)
            assert isinstance(ch2["online"], bool)
            
            # Load status should be valid
            assert ch1["load_status"] in [0, 1, 2]
            assert ch2["load_status"] in [0, 1, 2]
            
    def test_hardware_control_protocol(self, mock_device):
        """Test hardware control commands ESP32 uses"""
        # LED control commands
        led_commands = [
            ("status", "on"),
            ("status", "off"),
            ("error", "on"),
            ("activity", "off")
        ]
        
        for led_id, action in led_commands:
            command = f"CONTROL_LED {led_id} {action}"
            response = mock_device.process_command(command)
            assert response == f"OK: LED {led_id} {action}\n"
            
        # Motor control commands
        motor_commands = [
            ("0", "cw", "90"),
            ("1", "ccw", "180"),
            ("2", "cw", "45")
        ]
        
        for motor_id, direction, steps in motor_commands:
            command = f"CONTROL_MOTOR {motor_id} {direction} {steps}"
            response = mock_device.process_command(command)
            assert response == f"OK: Motor {motor_id} moved {steps} steps {direction}\n"
            
    def test_version_information_protocol(self, mock_device):
        """Test version information exchange"""
        response = mock_device.process_command("GET_VERSION")
        
        assert response.startswith("VERSION:")
        version_json = response[8:-1]
        version_data = json.loads(version_json)
        
        # ESP32 needs these fields for display and validation
        required_version_fields = [
            "firmware_version",
            "hardware_revision", 
            "build_date",
            "git_commit"
        ]
        
        for field in required_version_fields:
            assert field in version_data
            assert isinstance(version_data[field], str)
            assert len(version_data[field]) > 0
            
    def test_reset_recovery_protocol(self, mock_device):
        """Test reset and recovery procedures"""
        # Get initial state
        initial_response = mock_device.process_command("GET_STATUS")
        initial_status = json.loads(initial_response[7:-1])
        
        # Enter DFU mode
        mock_device.process_command("ENTER_DFU")
        assert mock_device.dfu_mode is True
        
        # Reset should clear DFU mode
        response = mock_device.process_command("RESET")
        assert response == "OK: System reset\n"
        assert mock_device.dfu_mode is False
        
        # Status should be available after reset
        reset_response = mock_device.process_command("GET_STATUS")
        reset_status = json.loads(reset_response[7:-1])
        
        # Should have valid status structure after reset
        assert reset_status["system_status"] == 1  # Normal operation
        assert reset_status["channel_count"] == initial_status["channel_count"]
        assert len(reset_status["channels"]) == len(initial_status["channels"])
        
    def test_concurrent_command_handling(self, mock_device):
        """Test handling of rapid command sequences"""
        commands = [
            "GET_STATUS",
            "GET_VERSION", 
            "CONTROL_LED status on",
            "GET_STATUS",
            "CONTROL_LED status off",
            "GET_STATUS"
        ]
        
        responses = []
        start_time = time.time()
        
        # Send commands rapidly
        for cmd in commands:
            response = mock_device.process_command(cmd)
            responses.append(response)
            
        end_time = time.time()
        total_time = end_time - start_time
        
        # All commands should succeed
        assert len(responses) == len(commands)
        
        # Should handle commands in reasonable time
        assert total_time < 2.0, "Command sequence took too long"
        
        # Verify response patterns
        assert responses[0].startswith("STATUS:")
        assert responses[1].startswith("VERSION:")
        assert responses[2] == "OK: LED status on\n"
        assert responses[3].startswith("STATUS:")
        assert responses[4] == "OK: LED status off\n"
        assert responses[5].startswith("STATUS:")


class TestProtocolEdgeCases:
    """Test edge cases and error conditions in protocol"""
    
    @pytest.fixture
    def mock_device(self):
        return MockBMCU370()
    
    def test_malformed_commands(self, mock_device):
        """Test handling of malformed commands"""
        malformed_commands = [
            "",
            "   ",
            "INVALID",
            "SET_PARAM",
            "SET_PARAM only_one_param",
            "CONTROL_LED",
            "CONTROL_MOTOR 0",
            "CONTROL_MOTOR 0 cw"
        ]
        
        for cmd in malformed_commands:
            response = mock_device.process_command(cmd)
            assert response.startswith("ERROR:"), f"Should error on: '{cmd}'"
            
    def test_boundary_conditions(self, mock_device):
        """Test boundary conditions in parameters"""
        # Test motor step limits
        large_steps = "99999"
        response = mock_device.process_command(f"CONTROL_MOTOR 0 cw {large_steps}")
        assert response.startswith("OK:"), "Should handle large step counts"
        
        # Test invalid motor IDs  
        response = mock_device.process_command("CONTROL_MOTOR 999 cw 10")
        assert response.startswith("ERROR:"), "Should reject invalid motor ID"
        
        # Test invalid LED IDs
        response = mock_device.process_command("CONTROL_LED invalid_led on")
        assert response.startswith("ERROR:"), "Should reject invalid LED ID"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])