#!/usr/bin/env python3
"""
Mock BMCU370 Simulator for ESP32 Testing

This script simulates the BMCU370 (CH32V203) USB CDC responses to enable
comprehensive testing of the ESP32 firmware without requiring physical hardware.

Simulates:
- USB CDC-ACM serial communication
- JSON status responses
- Command protocol (GET_STATUS, SET_PARAM, DFU)
- BambuBus protocol responses
- Error conditions and edge cases
"""

import json
import time
import random
import logging
import threading
from dataclasses import dataclass, asdict
from typing import Dict, Any, Optional
from datetime import datetime

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@dataclass
class BMCUSystemStatus:
    """Data structure matching BMCU370 status response"""
    timestamp: int
    system_status: int
    channel_count: int
    channels: list
    temperatures: Dict[str, float]
    voltages: Dict[str, float]
    current_consumption: float
    error_flags: int
    led_status: Dict[str, bool]
    motor_positions: Dict[str, int]
    sensors: Dict[str, Any]
    firmware_version: str
    hardware_revision: str
    
    
class MockBMCU370:
    """Mock BMCU370 device simulator"""
    
    def __init__(self):
        self.device_connected = True
        self.dfu_mode = False
        self.system_status = self._generate_initial_status()
        self.command_history = []
        self.error_simulation = False
        self._lock = threading.Lock()
        
    def _generate_initial_status(self) -> BMCUSystemStatus:
        """Generate realistic initial system status"""
        return BMCUSystemStatus(
            timestamp=int(time.time()),
            system_status=1,  # Normal operation
            channel_count=16,
            channels=[
                {
                    "id": i,
                    "online": random.choice([True, False]),
                    "material_type": random.choice(["PLA", "PETG", "ABS", "TPU", None]),
                    "temperature": round(random.uniform(20.0, 25.0), 1),
                    "humidity": round(random.uniform(40.0, 60.0), 1),
                    "load_status": random.choice([0, 1, 2]),  # 0=empty, 1=loaded, 2=loading
                    "error_code": 0
                }
                for i in range(16)
            ],
            temperatures={
                "mcu": round(random.uniform(35.0, 45.0), 1),
                "ambient": round(random.uniform(20.0, 25.0), 1),
                "board": round(random.uniform(30.0, 40.0), 1)
            },
            voltages={
                "vcc_3v3": round(random.uniform(3.25, 3.35), 2),
                "vcc_5v": round(random.uniform(4.95, 5.05), 2),
                "vcc_12v": round(random.uniform(11.8, 12.2), 2)
            },
            current_consumption=round(random.uniform(150.0, 300.0), 1),
            error_flags=0,
            led_status={
                "status": True,
                "error": False,
                "activity": random.choice([True, False])
            },
            motor_positions={
                f"motor_{i}": random.randint(0, 360) for i in range(4)
            },
            sensors={
                "hall_sensors": [random.randint(0, 4095) for _ in range(8)],
                "adc_values": [random.randint(0, 4095) for _ in range(12)],
                "encoder_positions": [random.randint(0, 1000) for _ in range(4)]
            },
            firmware_version="0.1.0020",
            hardware_revision="BMCU-C 370 Hall V0.1"
        )
    
    def process_command(self, command: str) -> str:
        """Process incoming command and return appropriate response"""
        with self._lock:
            self.command_history.append({
                "timestamp": time.time(),
                "command": command.strip(),
                "dfu_mode": self.dfu_mode
            })
            
            command = command.strip()
            logger.info(f"Processing command: {command}")
            
            # Simulate communication delays
            time.sleep(random.uniform(0.01, 0.05))
            
            if self.error_simulation:
                return self._simulate_error_response(command)
            
            if command == "GET_STATUS":
                return self._handle_get_status()
            elif command.startswith("SET_PARAM"):
                return self._handle_set_param(command)
            elif command == "ENTER_DFU":
                return self._handle_enter_dfu()
            elif command == "EXIT_DFU":
                return self._handle_exit_dfu()
            elif command == "RESET":
                return self._handle_reset()
            elif command == "GET_VERSION":
                return self._handle_get_version()
            elif command.startswith("CONTROL_LED"):
                return self._handle_control_led(command)
            elif command.startswith("CONTROL_MOTOR"):
                return self._handle_control_motor(command)
            else:
                return self._handle_unknown_command(command)
    
    def _handle_get_status(self) -> str:
        """Handle GET_STATUS command"""
        if self.dfu_mode:
            return "ERROR: Device in DFU mode\n"
        
        # Update dynamic values
        self.system_status.timestamp = int(time.time())
        
        # Simulate some data variations
        for channel in self.system_status.channels:
            if channel["online"]:
                channel["temperature"] += random.uniform(-0.1, 0.1)
                channel["humidity"] += random.uniform(-0.5, 0.5)
        
        self.system_status.temperatures["mcu"] += random.uniform(-0.5, 0.5)
        self.system_status.current_consumption += random.uniform(-5.0, 5.0)
        
        # Return JSON status
        status_json = json.dumps(asdict(self.system_status), indent=None, separators=(',', ':'))
        return f"STATUS:{status_json}\n"
    
    def _handle_set_param(self, command: str) -> str:
        """Handle SET_PARAM commands"""
        if self.dfu_mode:
            return "ERROR: Device in DFU mode\n"
        
        parts = command.split(" ", 2)
        if len(parts) < 3:
            return "ERROR: Invalid SET_PARAM format\n"
        
        param_name = parts[1]
        param_value = parts[2]
        
        logger.info(f"Setting parameter {param_name} to {param_value}")
        
        # Simulate parameter setting
        if param_name == "led_brightness":
            return "OK: LED brightness set\n"
        elif param_name == "motor_speed":
            return "OK: Motor speed set\n"
        elif param_name == "channel_enable":
            return "OK: Channel configuration updated\n"
        else:
            return f"ERROR: Unknown parameter {param_name}\n"
    
    def _handle_enter_dfu(self) -> str:
        """Handle DFU mode entry"""
        self.dfu_mode = True
        logger.info("Entering DFU mode")
        return "OK: Entering DFU mode\n"
    
    def _handle_exit_dfu(self) -> str:
        """Handle DFU mode exit"""
        self.dfu_mode = False
        logger.info("Exiting DFU mode")
        return "OK: Exiting DFU mode\n"
    
    def _handle_reset(self) -> str:
        """Handle system reset"""
        self.dfu_mode = False
        self.system_status = self._generate_initial_status()
        logger.info("System reset")
        return "OK: System reset\n"
    
    def _handle_get_version(self) -> str:
        """Handle version request"""
        version_info = {
            "firmware_version": self.system_status.firmware_version,
            "hardware_revision": self.system_status.hardware_revision,
            "build_date": "2024-01-15",
            "git_commit": "abc123def456"
        }
        return f"VERSION:{json.dumps(version_info)}\n"
    
    def _handle_control_led(self, command: str) -> str:
        """Handle LED control commands"""
        parts = command.split(" ")
        if len(parts) < 3:
            return "ERROR: Invalid LED command format\n"
        
        led_id = parts[1]
        action = parts[2]
        
        if led_id in ["status", "error", "activity"]:
            self.system_status.led_status[led_id] = (action.lower() == "on")
            return f"OK: LED {led_id} {action}\n"
        else:
            return f"ERROR: Unknown LED {led_id}\n"
    
    def _handle_control_motor(self, command: str) -> str:
        """Handle motor control commands"""
        parts = command.split(" ")
        if len(parts) < 4:
            return "ERROR: Invalid motor command format\n"
        
        motor_id = parts[1]
        direction = parts[2]
        steps = parts[3]
        
        try:
            steps = int(steps)
            motor_key = f"motor_{motor_id}"
            if motor_key in self.system_status.motor_positions:
                if direction == "cw":
                    self.system_status.motor_positions[motor_key] += steps
                elif direction == "ccw":
                    self.system_status.motor_positions[motor_key] -= steps
                
                # Keep position in valid range
                self.system_status.motor_positions[motor_key] %= 360
                return f"OK: Motor {motor_id} moved {steps} steps {direction}\n"
            else:
                return f"ERROR: Unknown motor {motor_id}\n"
        except ValueError:
            return "ERROR: Invalid step count\n"
    
    def _handle_unknown_command(self, command: str) -> str:
        """Handle unknown commands"""
        logger.warning(f"Unknown command: {command}")
        return f"ERROR: Unknown command '{command}'\n"
    
    def _simulate_error_response(self, command: str) -> str:
        """Simulate various error conditions"""
        error_types = [
            "ERROR: Communication timeout\n",
            "ERROR: Checksum failure\n", 
            "ERROR: Device busy\n",
            "ERROR: Invalid state\n",
            "",  # No response (timeout)
            "CORRUPTED_DATA_123!@#\n"  # Corrupted response
        ]
        return random.choice(error_types)
    
    def simulate_error_conditions(self, enable: bool = True):
        """Enable/disable error simulation for testing"""
        self.error_simulation = enable
        logger.info(f"Error simulation {'enabled' if enable else 'disabled'}")
    
    def disconnect(self):
        """Simulate device disconnection"""
        self.device_connected = False
        logger.info("Device disconnected")
    
    def reconnect(self):
        """Simulate device reconnection"""
        self.device_connected = True
        self.dfu_mode = False
        logger.info("Device reconnected")
    
    def get_statistics(self) -> Dict[str, Any]:
        """Get simulator statistics"""
        return {
            "connected": self.device_connected,
            "dfu_mode": self.dfu_mode,
            "command_count": len(self.command_history),
            "error_simulation": self.error_simulation,
            "uptime": time.time() - (self.command_history[0]["timestamp"] if self.command_history else time.time())
        }


if __name__ == "__main__":
    # Example usage and testing
    mock_device = MockBMCU370()
    
    print("Mock BMCU370 Simulator")
    print("Available commands: GET_STATUS, SET_PARAM, ENTER_DFU, EXIT_DFU, RESET, GET_VERSION")
    print("Special commands: ERROR_SIM (toggle error simulation), QUIT")
    
    while True:
        try:
            cmd = input("\n> ").strip()
            if cmd.upper() == "QUIT":
                break
            elif cmd.upper() == "ERROR_SIM":
                mock_device.simulate_error_conditions(not mock_device.error_simulation)
                print(f"Error simulation: {'ON' if mock_device.error_simulation else 'OFF'}")
                continue
            elif cmd.upper() == "STATS":
                stats = mock_device.get_statistics()
                print(f"Statistics: {json.dumps(stats, indent=2)}")
                continue
            
            response = mock_device.process_command(cmd)
            print(f"Response: {response.rstrip()}")
            
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"Error: {e}")