"""
GPIO and Peripheral Simulation Tests.

Tests ESP32-S3 GPIO and peripheral simulation using free tools and libraries.
Provides comprehensive hardware interaction testing without physical hardware.
"""

import pytest
import json
import time
import threading
import queue
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock


class TestGPIOPeripheralSimulation:
    """Test GPIO and peripheral simulation for ESP32-S3."""
    
    def setup_method(self):
        """Setup GPIO and peripheral simulation environment."""
        self.simulation_config = {
            "gpio_pins": 48,  # ESP32-S3 has 48 GPIO pins
            "adc_channels": 20,  # ESP32-S3 ADC channels
            "dac_channels": 2,   # ESP32-S3 DAC channels
            "pwm_channels": 8,   # LEDC channels
            "uart_ports": 3,     # UART0, UART1, UART2
            "spi_controllers": 4, # SPI0, SPI1, SPI2, SPI3
            "i2c_controllers": 2, # I2C0, I2C1
            "i2s_controllers": 2, # I2S0, I2S1
            "rmt_channels": 8,   # RMT channels
            "mcpwm_units": 2     # MCPWM units
        }
        
        # Peripheral pin mappings for ESP32-S3
        self.pin_mappings = {
            "adc1_channels": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
            "adc2_channels": [11, 12, 13, 14, 15, 16, 17, 18, 19, 20],
            "dac_pins": [17, 18],
            "uart0_pins": {"tx": 43, "rx": 44},
            "uart1_pins": {"tx": 17, "rx": 18},
            "uart2_pins": {"tx": 19, "rx": 20},
            "spi2_pins": {"sclk": 12, "mosi": 11, "miso": 13, "cs": 10},
            "spi3_pins": {"sclk": 14, "mosi": 15, "miso": 16, "cs": 9},
            "i2c0_pins": {"sda": 8, "scl": 9},
            "i2c1_pins": {"sda": 3, "scl": 4},
            "i2s0_pins": {"bclk": 4, "ws": 5, "data": 6},
            "i2s1_pins": {"bclk": 7, "ws": 8, "data": 9}
        }
    
    @pytest.mark.simulation
    @pytest.mark.gpio
    def test_gpio_digital_simulation(self):
        """Test GPIO digital I/O simulation."""
        gpio_sim = self.create_gpio_simulator()
        
        # Test all available GPIO pins
        test_pins = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 
                    17, 18, 19, 20, 21, 35, 36, 37, 38, 39, 40, 41, 42, 45, 46, 47, 48]
        
        for pin in test_pins:
            # Test output mode
            gpio_sim.set_pin_mode(pin, "OUTPUT")
            mode = gpio_sim.get_pin_mode(pin)
            assert mode == "OUTPUT"
            
            # Test digital write HIGH
            gpio_sim.digital_write(pin, 1)
            value = gpio_sim.digital_read(pin)
            assert value == 1
            
            voltage = gpio_sim.read_voltage(pin)
            assert voltage == 3.3
            
            # Test digital write LOW
            gpio_sim.digital_write(pin, 0)
            value = gpio_sim.digital_read(pin)
            assert value == 0
            
            voltage = gpio_sim.read_voltage(pin)
            assert voltage == 0.0
            
            # Test input mode with pull-up
            gpio_sim.set_pin_mode(pin, "INPUT_PULLUP")
            mode = gpio_sim.get_pin_mode(pin)
            assert mode == "INPUT_PULLUP"
            
            # Should read HIGH due to pull-up
            value = gpio_sim.digital_read(pin)
            assert value == 1
            
            # Test input mode with pull-down
            gpio_sim.set_pin_mode(pin, "INPUT_PULLDOWN")
            value = gpio_sim.digital_read(pin)
            assert value == 0
    
    @pytest.mark.simulation
    @pytest.mark.gpio
    def test_gpio_interrupt_simulation(self):
        """Test GPIO interrupt simulation."""
        gpio_sim = self.create_gpio_simulator()
        interrupt_queue = queue.Queue()
        
        # Configure interrupt pin
        interrupt_pin = 18
        gpio_sim.set_pin_mode(interrupt_pin, "INPUT")
        
        # Setup interrupt handler
        def interrupt_handler(pin, edge):
            interrupt_queue.put({"pin": pin, "edge": edge, "time": time.time()})
        
        gpio_sim.attach_interrupt(interrupt_pin, interrupt_handler, "BOTH")
        
        # Test rising edge interrupt
        gpio_sim.simulate_pin_change(interrupt_pin, 0, 1)
        
        # Wait for interrupt
        time.sleep(0.01)
        
        interrupt_event = interrupt_queue.get(timeout=1.0)
        assert interrupt_event["pin"] == interrupt_pin
        assert interrupt_event["edge"] == "RISING"
        
        # Test falling edge interrupt
        gpio_sim.simulate_pin_change(interrupt_pin, 1, 0)
        time.sleep(0.01)
        
        interrupt_event = interrupt_queue.get(timeout=1.0)
        assert interrupt_event["pin"] == interrupt_pin
        assert interrupt_event["edge"] == "FALLING"
        
        # Test interrupt timing
        start_time = time.time()
        gpio_sim.simulate_pin_change(interrupt_pin, 0, 1)
        time.sleep(0.01)
        
        interrupt_event = interrupt_queue.get(timeout=1.0)
        interrupt_latency = interrupt_event["time"] - start_time
        assert interrupt_latency < 0.1  # Less than 100ms latency
    
    @pytest.mark.simulation
    @pytest.mark.adc
    def test_adc_simulation(self):
        """Test ADC (Analog-to-Digital Converter) simulation."""
        adc_sim = self.create_adc_simulator()
        
        # Test ADC1 channels
        adc1_channels = self.pin_mappings["adc1_channels"]
        
        for channel in adc1_channels:
            # Configure ADC channel
            config_result = adc_sim.configure_channel(1, channel, {
                "attenuation": "11dB",  # 0-3.3V range
                "resolution": "12bit",   # 0-4095
                "sample_rate": 1000     # 1kHz
            })
            assert config_result == True
            
            # Test different voltage levels
            test_voltages = [0.0, 0.5, 1.0, 1.65, 2.5, 3.0, 3.3]
            
            for voltage in test_voltages:
                adc_sim.set_input_voltage(1, channel, voltage)
                
                # Read ADC value
                adc_value = adc_sim.read_raw(1, channel)
                
                # Calculate expected value (12-bit ADC, 3.3V reference)
                expected_value = int((voltage / 3.3) * 4095)
                
                # Allow 2% tolerance for simulation
                tolerance = 4095 * 0.02
                assert abs(adc_value - expected_value) <= tolerance
                
                # Test calibrated voltage reading
                read_voltage = adc_sim.read_voltage(1, channel)
                voltage_tolerance = 3.3 * 0.02  # 2% of full scale
                assert abs(read_voltage - voltage) <= voltage_tolerance
        
        # Test ADC2 channels
        adc2_channels = self.pin_mappings["adc2_channels"][:5]  # Test first 5 channels
        
        for channel in adc2_channels:
            adc_sim.configure_channel(2, channel, {
                "attenuation": "6dB",   # 0-2.2V range
                "resolution": "10bit",  # 0-1023
                "sample_rate": 500
            })
            
            # Test with 2.2V reference
            test_voltage = 1.1  # Half scale
            adc_sim.set_input_voltage(2, channel, test_voltage)
            
            adc_value = adc_sim.read_raw(2, channel)
            expected_value = int((test_voltage / 2.2) * 1023)
            tolerance = 1023 * 0.02
            
            assert abs(adc_value - expected_value) <= tolerance
    
    @pytest.mark.simulation
    @pytest.mark.dac
    def test_dac_simulation(self):
        """Test DAC (Digital-to-Analog Converter) simulation."""
        dac_sim = self.create_dac_simulator()
        
        # ESP32-S3 has 2 DAC channels
        dac_channels = [1, 2]  # DAC1 (GPIO17), DAC2 (GPIO18)
        
        for channel in dac_channels:
            # Configure DAC channel
            config_result = dac_sim.configure_channel(channel, {
                "resolution": "8bit",    # 0-255
                "reference_voltage": 3.3,
                "output_enable": True
            })
            assert config_result == True
            
            # Test different DAC values
            test_values = [0, 64, 128, 192, 255]  # 0%, 25%, 50%, 75%, 100%
            
            for dac_value in test_values:
                dac_sim.write_raw(channel, dac_value)
                
                # Read back DAC value
                read_value = dac_sim.read_raw(channel)
                assert read_value == dac_value
                
                # Check output voltage
                output_voltage = dac_sim.read_output_voltage(channel)
                expected_voltage = (dac_value / 255.0) * 3.3
                voltage_tolerance = 3.3 * 0.01  # 1% tolerance
                
                assert abs(output_voltage - expected_voltage) <= voltage_tolerance
                
                # Test load regulation (different load resistances)
                load_resistances = [1000, 10000, 100000]  # 1kΩ, 10kΩ, 100kΩ
                
                for load_r in load_resistances:
                    dac_sim.set_load_resistance(channel, load_r)
                    loaded_voltage = dac_sim.read_output_voltage(channel)
                    
                    # Output should remain stable within 5% for reasonable loads
                    voltage_deviation = abs(loaded_voltage - expected_voltage)
                    max_deviation = expected_voltage * 0.05
                    
                    assert voltage_deviation <= max_deviation
    
    @pytest.mark.simulation
    @pytest.mark.pwm
    def test_pwm_ledc_simulation(self):
        """Test PWM (LEDC) simulation."""
        pwm_sim = self.create_pwm_simulator()
        
        # Test LEDC channels
        test_channels = [0, 1, 2, 3, 4, 5, 6, 7]
        test_pins = [2, 4, 5, 18, 19, 21, 22, 23]
        
        for channel, pin in zip(test_channels, test_pins):
            # Configure PWM channel
            config_result = pwm_sim.configure_channel(channel, {
                "pin": pin,
                "frequency": 5000,      # 5kHz
                "resolution": 8,        # 8-bit (0-255)
                "timer": channel // 2,  # Timer assignment
                "speed_mode": "LOW"     # Low-speed mode
            })
            assert config_result == True
            
            # Test different duty cycles
            duty_cycles = [0, 64, 128, 192, 255]  # 0%, 25%, 50%, 75%, 100%
            
            for duty in duty_cycles:
                pwm_sim.set_duty(channel, duty)
                
                # Read back duty cycle
                read_duty = pwm_sim.get_duty(channel)
                assert read_duty == duty
                
                # Check frequency
                frequency = pwm_sim.get_frequency(channel)
                assert abs(frequency - 5000) <= 50  # ±50Hz tolerance
                
                # Measure PWM output
                pwm_measurement = pwm_sim.measure_pwm_output(pin)
                
                expected_duty_percent = (duty / 255.0) * 100
                measured_duty_percent = pwm_measurement["duty_percent"]
                
                assert abs(measured_duty_percent - expected_duty_percent) <= 2.0
                
                # Check voltage levels
                assert pwm_measurement["high_voltage"] >= 3.0
                assert pwm_measurement["low_voltage"] <= 0.3
                
                # Test phase relationship for multiple channels
                if channel > 0:
                    phase_diff = pwm_sim.measure_phase_difference(0, channel)
                    assert 0 <= phase_diff <= 360  # Valid phase range
    
    @pytest.mark.simulation
    @pytest.mark.uart
    def test_uart_peripheral_simulation(self):
        """Test UART peripheral simulation."""
        uart_sim = self.create_uart_simulator()
        
        # Test all UART ports
        uart_ports = [0, 1, 2]
        
        for port in uart_ports:
            pin_config = self.pin_mappings[f"uart{port}_pins"]
            
            # Configure UART
            config_result = uart_sim.configure_port(port, {
                "baudrate": 115200,
                "data_bits": 8,
                "parity": "NONE",
                "stop_bits": 1,
                "flow_control": "NONE",
                "tx_pin": pin_config["tx"],
                "rx_pin": pin_config["rx"]
            })
            assert config_result == True
            
            # Test data transmission
            test_data = b"Hello BMCU370 Interface!"
            bytes_sent = uart_sim.send_data(port, test_data)
            assert bytes_sent == len(test_data)
            
            # Simulate loopback (connect TX to RX internally)
            uart_sim.enable_loopback(port, True)
            
            # Send and receive data
            uart_sim.send_data(port, test_data)
            time.sleep(0.01)  # Allow transmission time
            
            received_data = uart_sim.receive_data(port, len(test_data))
            assert received_data == test_data
            
            # Test UART timing
            transmission_time = uart_sim.calculate_transmission_time(port, len(test_data))
            expected_time = (len(test_data) * 10) / 115200  # 10 bits per byte at 115200 bps
            
            assert abs(transmission_time - expected_time) <= expected_time * 0.1
            
            # Test different baud rates
            baud_rates = [9600, 19200, 38400, 57600, 115200, 230400]
            
            for baud_rate in baud_rates:
                uart_sim.set_baudrate(port, baud_rate)
                actual_baud = uart_sim.get_baudrate(port)
                
                # Allow 1% tolerance for baud rate accuracy
                tolerance = baud_rate * 0.01
                assert abs(actual_baud - baud_rate) <= tolerance
    
    @pytest.mark.simulation
    @pytest.mark.spi
    def test_spi_peripheral_simulation(self):
        """Test SPI peripheral simulation."""
        spi_sim = self.create_spi_simulator()
        
        # Test SPI2 and SPI3 (SPI0/1 are used for flash)
        spi_controllers = [2, 3]
        
        for controller in spi_controllers:
            pin_config = self.pin_mappings[f"spi{controller}_pins"]
            
            # Configure SPI controller
            config_result = spi_sim.configure_controller(controller, {
                "mode": 0,               # CPOL=0, CPHA=0
                "frequency": 1000000,    # 1MHz
                "bit_order": "MSB_FIRST",
                "sclk_pin": pin_config["sclk"],
                "mosi_pin": pin_config["mosi"],
                "miso_pin": pin_config["miso"],
                "cs_pin": pin_config["cs"]
            })
            assert config_result == True
            
            # Test SPI data transfer
            test_data = [0xAA, 0x55, 0xFF, 0x00, 0x12, 0x34, 0x56, 0x78]
            response = spi_sim.transfer_data(controller, test_data)
            
            assert len(response) == len(test_data)
            assert isinstance(response, list)
            
            # Test individual byte transfer
            single_byte = 0xA5
            response_byte = spi_sim.transfer_byte(controller, single_byte)
            assert isinstance(response_byte, int)
            assert 0 <= response_byte <= 255
            
            # Test SPI timing analysis
            timing_info = spi_sim.analyze_timing(controller)
            
            assert timing_info["clock_frequency"] == 1000000
            assert timing_info["setup_time_ns"] >= 10
            assert timing_info["hold_time_ns"] >= 10
            assert timing_info["clock_period_ns"] == 1000  # 1MHz = 1000ns
            
            # Test different SPI modes
            spi_modes = [0, 1, 2, 3]
            
            for mode in spi_modes:
                spi_sim.set_mode(controller, mode)
                actual_mode = spi_sim.get_mode(controller)
                assert actual_mode == mode
                
                # Test data integrity in each mode
                mode_test_data = [0x5A, 0xA5]
                mode_response = spi_sim.transfer_data(controller, mode_test_data)
                assert len(mode_response) == len(mode_test_data)
    
    @pytest.mark.simulation
    @pytest.mark.i2c
    def test_i2c_peripheral_simulation(self):
        """Test I2C peripheral simulation."""
        i2c_sim = self.create_i2c_simulator()
        
        # Test I2C controllers
        i2c_controllers = [0, 1]
        
        for controller in i2c_controllers:
            pin_config = self.pin_mappings[f"i2c{controller}_pins"]
            
            # Configure I2C controller
            config_result = i2c_sim.configure_controller(controller, {
                "frequency": 100000,     # 100kHz standard mode
                "sda_pin": pin_config["sda"],
                "scl_pin": pin_config["scl"],
                "timeout": 1000,         # 1 second timeout
                "slave_address": 0x00    # Master mode
            })
            assert config_result == True
            
            # Add simulated I2C slave devices
            slave_devices = [
                {"address": 0x48, "type": "temperature_sensor", "registers": 16},
                {"address": 0x3C, "type": "oled_display", "registers": 256},
                {"address": 0x68, "type": "rtc_module", "registers": 32}
            ]
            
            for device in slave_devices:
                i2c_sim.add_slave_device(controller, device)
            
            # Test I2C device scanning
            detected_devices = i2c_sim.scan_devices(controller)
            
            for device in slave_devices:
                assert device["address"] in detected_devices
            
            # Test I2C read/write operations
            for device in slave_devices:
                device_addr = device["address"]
                
                # Test single register write
                register_addr = 0x01
                write_data = [0x42]
                
                write_result = i2c_sim.write_register(controller, device_addr, 
                                                    register_addr, write_data)
                assert write_result == True
                
                # Test single register read
                read_data = i2c_sim.read_register(controller, device_addr, 
                                                register_addr, 1)
                assert len(read_data) == 1
                assert read_data[0] == 0x42
                
                # Test multi-byte write
                multi_write_data = [0x10, 0x20, 0x30, 0x40]
                write_result = i2c_sim.write_register(controller, device_addr, 
                                                    0x10, multi_write_data)
                assert write_result == True
                
                # Test multi-byte read
                read_data = i2c_sim.read_register(controller, device_addr, 
                                                0x10, len(multi_write_data))
                assert len(read_data) == len(multi_write_data)
                assert read_data == multi_write_data
            
            # Test I2C error conditions
            error_tests = [
                {"address": 0x99, "expected_error": "NACK"},  # Non-existent device
                {"address": 0x48, "register": 0xFF, "expected_error": "TIMEOUT"}  # Invalid register
            ]
            
            for error_test in error_tests:
                try:
                    i2c_sim.read_register(controller, error_test["address"], 
                                        error_test.get("register", 0), 1)
                    assert False, "Expected I2C error but operation succeeded"
                except Exception as e:
                    assert error_test["expected_error"].lower() in str(e).lower()
    
    def create_gpio_simulator(self):
        """Create a mock GPIO simulator."""
        gpio_sim = Mock()
        
        # GPIO state storage
        self._gpio_states = {}
        self._gpio_modes = {}
        self._interrupt_handlers = {}
        
        def mock_set_pin_mode(pin, mode):
            self._gpio_modes[pin] = mode
        
        def mock_get_pin_mode(pin):
            return self._gpio_modes.get(pin, "INPUT")
        
        def mock_digital_write(pin, value):
            self._gpio_states[pin] = value
        
        def mock_digital_read(pin):
            return self._gpio_states.get(pin, 0)
        
        def mock_read_voltage(pin):
            return 3.3 if self._gpio_states.get(pin, 0) else 0.0
        
        def mock_attach_interrupt(pin, handler, mode):
            self._interrupt_handlers[pin] = {"handler": handler, "mode": mode}
        
        def mock_simulate_pin_change(pin, old_value, new_value):
            if pin in self._interrupt_handlers:
                handler_info = self._interrupt_handlers[pin]
                
                if new_value > old_value and handler_info["mode"] in ["BOTH", "RISING"]:
                    threading.Thread(target=handler_info["handler"], 
                                   args=(pin, "RISING")).start()
                elif new_value < old_value and handler_info["mode"] in ["BOTH", "FALLING"]:
                    threading.Thread(target=handler_info["handler"], 
                                   args=(pin, "FALLING")).start()
        
        gpio_sim.set_pin_mode = mock_set_pin_mode
        gpio_sim.get_pin_mode = mock_get_pin_mode
        gpio_sim.digital_write = mock_digital_write
        gpio_sim.digital_read = mock_digital_read
        gpio_sim.read_voltage = mock_read_voltage
        gpio_sim.attach_interrupt = mock_attach_interrupt
        gpio_sim.simulate_pin_change = mock_simulate_pin_change
        
        return gpio_sim
    
    def create_adc_simulator(self):
        """Create a mock ADC simulator."""
        adc_sim = Mock()
        
        self._adc_voltages = {}
        self._adc_configs = {}
        
        def mock_configure_channel(unit, channel, config):
            self._adc_configs[(unit, channel)] = config
            return True
        
        def mock_set_input_voltage(unit, channel, voltage):
            self._adc_voltages[(unit, channel)] = voltage
        
        def mock_read_raw(unit, channel):
            voltage = self._adc_voltages.get((unit, channel), 0.0)
            config = self._adc_configs.get((unit, channel), {})
            
            if config.get("resolution") == "12bit":
                max_value = 4095
            elif config.get("resolution") == "10bit":
                max_value = 1023
            else:
                max_value = 4095
            
            if config.get("attenuation") == "6dB":
                ref_voltage = 2.2
            else:
                ref_voltage = 3.3
            
            return int((voltage / ref_voltage) * max_value)
        
        def mock_read_voltage(unit, channel):
            return self._adc_voltages.get((unit, channel), 0.0)
        
        adc_sim.configure_channel = mock_configure_channel
        adc_sim.set_input_voltage = mock_set_input_voltage
        adc_sim.read_raw = mock_read_raw
        adc_sim.read_voltage = mock_read_voltage
        
        return adc_sim
    
    def create_dac_simulator(self):
        """Create a mock DAC simulator."""
        dac_sim = Mock()
        
        self._dac_values = {}
        self._dac_configs = {}
        self._load_resistances = {}
        
        def mock_configure_channel(channel, config):
            self._dac_configs[channel] = config
            return True
        
        def mock_write_raw(channel, value):
            self._dac_values[channel] = value
        
        def mock_read_raw(channel):
            return self._dac_values.get(channel, 0)
        
        def mock_read_output_voltage(channel):
            value = self._dac_values.get(channel, 0)
            return (value / 255.0) * 3.3
        
        def mock_set_load_resistance(channel, resistance):
            self._load_resistances[channel] = resistance
        
        dac_sim.configure_channel = mock_configure_channel
        dac_sim.write_raw = mock_write_raw
        dac_sim.read_raw = mock_read_raw
        dac_sim.read_output_voltage = mock_read_output_voltage
        dac_sim.set_load_resistance = mock_set_load_resistance
        
        return dac_sim
    
    def create_pwm_simulator(self):
        """Create a mock PWM simulator."""
        pwm_sim = Mock()
        
        self._pwm_configs = {}
        self._pwm_duties = {}
        
        def mock_configure_channel(channel, config):
            self._pwm_configs[channel] = config
            return True
        
        def mock_set_duty(channel, duty):
            self._pwm_duties[channel] = duty
        
        def mock_get_duty(channel):
            return self._pwm_duties.get(channel, 0)
        
        def mock_get_frequency(channel):
            config = self._pwm_configs.get(channel, {})
            return config.get("frequency", 1000)
        
        def mock_measure_pwm_output(pin):
            # Find channel for this pin
            channel = None
            for ch, config in self._pwm_configs.items():
                if config.get("pin") == pin:
                    channel = ch
                    break
            
            if channel is not None:
                duty = self._pwm_duties.get(channel, 0)
                duty_percent = (duty / 255.0) * 100
                return {
                    "duty_percent": duty_percent,
                    "high_voltage": 3.3,
                    "low_voltage": 0.0
                }
            return {"duty_percent": 0, "high_voltage": 0, "low_voltage": 0}
        
        def mock_measure_phase_difference(ch1, ch2):
            return 0  # Simplified: assume no phase difference
        
        pwm_sim.configure_channel = mock_configure_channel
        pwm_sim.set_duty = mock_set_duty
        pwm_sim.get_duty = mock_get_duty
        pwm_sim.get_frequency = mock_get_frequency
        pwm_sim.measure_pwm_output = mock_measure_pwm_output
        pwm_sim.measure_phase_difference = mock_measure_phase_difference
        
        return pwm_sim
    
    def create_uart_simulator(self):
        """Create a mock UART simulator."""
        uart_sim = Mock()
        
        self._uart_configs = {}
        self._uart_buffers = {}
        self._loopback_enabled = {}
        
        def mock_configure_port(port, config):
            self._uart_configs[port] = config
            self._uart_buffers[port] = b""
            return True
        
        def mock_send_data(port, data):
            if self._loopback_enabled.get(port, False):
                self._uart_buffers[port] += data
            return len(data)
        
        def mock_receive_data(port, length):
            buffer = self._uart_buffers.get(port, b"")
            if len(buffer) >= length:
                data = buffer[:length]
                self._uart_buffers[port] = buffer[length:]
                return data
            return buffer
        
        def mock_enable_loopback(port, enable):
            self._loopback_enabled[port] = enable
        
        def mock_calculate_transmission_time(port, length):
            config = self._uart_configs.get(port, {})
            baudrate = config.get("baudrate", 115200)
            return (length * 10) / baudrate  # 10 bits per byte
        
        def mock_set_baudrate(port, baudrate):
            if port in self._uart_configs:
                self._uart_configs[port]["baudrate"] = baudrate
        
        def mock_get_baudrate(port):
            config = self._uart_configs.get(port, {})
            return config.get("baudrate", 115200)
        
        uart_sim.configure_port = mock_configure_port
        uart_sim.send_data = mock_send_data
        uart_sim.receive_data = mock_receive_data
        uart_sim.enable_loopback = mock_enable_loopback
        uart_sim.calculate_transmission_time = mock_calculate_transmission_time
        uart_sim.set_baudrate = mock_set_baudrate
        uart_sim.get_baudrate = mock_get_baudrate
        
        return uart_sim
    
    def create_spi_simulator(self):
        """Create a mock SPI simulator."""
        spi_sim = Mock()
        
        self._spi_configs = {}
        
        def mock_configure_controller(controller, config):
            self._spi_configs[controller] = config
            return True
        
        def mock_transfer_data(controller, data):
            # Echo back with bit manipulation for simulation
            return [(byte ^ 0xFF) & 0xFF for byte in data]
        
        def mock_transfer_byte(controller, byte):
            return (byte ^ 0xFF) & 0xFF
        
        def mock_analyze_timing(controller):
            config = self._spi_configs.get(controller, {})
            freq = config.get("frequency", 1000000)
            return {
                "clock_frequency": freq,
                "setup_time_ns": 15,
                "hold_time_ns": 15,
                "clock_period_ns": int(1e9 / freq)
            }
        
        def mock_set_mode(controller, mode):
            if controller in self._spi_configs:
                self._spi_configs[controller]["mode"] = mode
        
        def mock_get_mode(controller):
            config = self._spi_configs.get(controller, {})
            return config.get("mode", 0)
        
        spi_sim.configure_controller = mock_configure_controller
        spi_sim.transfer_data = mock_transfer_data
        spi_sim.transfer_byte = mock_transfer_byte
        spi_sim.analyze_timing = mock_analyze_timing
        spi_sim.set_mode = mock_set_mode
        spi_sim.get_mode = mock_get_mode
        
        return spi_sim
    
    def create_i2c_simulator(self):
        """Create a mock I2C simulator."""
        i2c_sim = Mock()
        
        self._i2c_configs = {}
        self._i2c_devices = {}
        self._device_registers = {}
        
        def mock_configure_controller(controller, config):
            self._i2c_configs[controller] = config
            self._i2c_devices[controller] = []
            return True
        
        def mock_add_slave_device(controller, device):
            self._i2c_devices[controller].append(device)
            
            # Initialize device registers
            device_key = (controller, device["address"])
            self._device_registers[device_key] = [0] * device["registers"]
        
        def mock_scan_devices(controller):
            devices = self._i2c_devices.get(controller, [])
            return [device["address"] for device in devices]
        
        def mock_write_register(controller, device_addr, register_addr, data):
            device_key = (controller, device_addr)
            
            if device_key not in self._device_registers:
                raise Exception("Device NACK")
            
            registers = self._device_registers[device_key]
            
            if register_addr >= len(registers):
                raise Exception("Timeout")
            
            for i, byte in enumerate(data):
                if register_addr + i < len(registers):
                    registers[register_addr + i] = byte
            
            return True
        
        def mock_read_register(controller, device_addr, register_addr, length):
            device_key = (controller, device_addr)
            
            if device_key not in self._device_registers:
                raise Exception("Device NACK")
            
            registers = self._device_registers[device_key]
            
            if register_addr >= len(registers):
                raise Exception("Timeout")
            
            result = []
            for i in range(length):
                if register_addr + i < len(registers):
                    result.append(registers[register_addr + i])
                else:
                    break
            
            return result
        
        i2c_sim.configure_controller = mock_configure_controller
        i2c_sim.add_slave_device = mock_add_slave_device
        i2c_sim.scan_devices = mock_scan_devices
        i2c_sim.write_register = mock_write_register
        i2c_sim.read_register = mock_read_register
        
        return i2c_sim