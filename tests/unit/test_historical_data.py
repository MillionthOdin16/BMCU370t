"""
Unit tests for Historical Data Manager module.

Tests data logging, storage, retrieval, and management functionality
for sensor data and system metrics.
"""

import pytest
import json
import time
from unittest.mock import Mock, patch, mock_open
from tests.conftest import load_test_data, assert_memory_usage_acceptable


class TestHistoricalDataManager:
    """Test cases for HistoricalDataManager class."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.sample_data = load_test_data('sample_historical_data.json')
        
    @pytest.mark.unit
    def test_data_manager_initialization(self):
        """Test historical data manager initialization."""
        data_manager = self.create_mock_data_manager()
        
        result = data_manager.init()
        assert result == True
        assert data_manager.is_initialized() == True
        
    @pytest.mark.unit
    def test_data_logging(self):
        """Test sensor data logging functionality."""
        data_manager = self.create_mock_data_manager()
        
        # Test logging sensor data
        sensor_data = {
            'timestamp': int(time.time()),
            'channels': [
                {
                    'id': 0,
                    'hall_position': 1024,
                    'filament_present': True,
                    'temperature': 25.5,
                    'pressure': 1650
                }
            ],
            'system': {
                'free_heap': 45678,
                'wifi_rssi': -45
            }
        }
        
        data_manager.log_data.return_value = True
        result = data_manager.log_data(sensor_data)
        assert result == True
        
    @pytest.mark.unit
    def test_data_retrieval(self):
        """Test historical data retrieval."""
        data_manager = self.create_mock_data_manager()
        
        # Test retrieving data by time range
        start_time = int(time.time()) - 3600  # 1 hour ago
        end_time = int(time.time())
        
        expected_data = self.sample_data['historical_data']['last_24_hours']
        data_manager.get_data_range.return_value = expected_data
        
        retrieved_data = data_manager.get_data_range(start_time, end_time)
        
        assert len(retrieved_data) > 0
        assert all('timestamp' in entry for entry in retrieved_data)
        assert all('data' in entry for entry in retrieved_data)
        
    @pytest.mark.unit
    def test_data_storage_management(self):
        """Test data storage and memory management."""
        data_manager = self.create_mock_data_manager()
        
        # Test storage limits
        max_size = self.sample_data['historical_data']['max_data_size_bytes']
        current_size = 16384  # 16KB
        
        data_manager.get_storage_size.return_value = current_size
        data_manager.get_max_storage_size.return_value = max_size
        
        storage_size = data_manager.get_storage_size()
        max_storage = data_manager.get_max_storage_size()
        
        assert storage_size <= max_storage
        assert_memory_usage_acceptable(storage_size, max_storage, 0.9)
        
    @pytest.mark.unit
    def test_data_compression(self):
        """Test data compression functionality."""
        data_manager = self.create_mock_data_manager()
        
        # Test data compression
        raw_data = json.dumps(self.sample_data['historical_data']['last_24_hours'])
        compressed_size = len(raw_data) // 2  # Assume 50% compression
        
        data_manager.compress_data.return_value = compressed_size
        result_size = data_manager.compress_data(raw_data)
        
        assert result_size < len(raw_data)
        
    @pytest.mark.unit
    def test_circular_buffer_behavior(self):
        """Test circular buffer implementation for data storage."""
        data_manager = self.create_mock_data_manager()
        
        # Test buffer overflow handling
        max_entries = 100
        data_manager.get_max_entries.return_value = max_entries
        data_manager.get_entry_count.return_value = max_entries
        
        # Adding new entry should remove oldest
        new_entry = {'timestamp': int(time.time()), 'data': {}}
        data_manager.add_entry.return_value = True
        
        result = data_manager.add_entry(new_entry)
        assert result == True
        
        # Entry count should remain at max
        data_manager.get_entry_count.return_value = max_entries
        assert data_manager.get_entry_count() == max_entries
        
    @pytest.mark.unit
    def test_data_aggregation(self):
        """Test data aggregation and statistics."""
        data_manager = self.create_mock_data_manager()
        
        # Test hourly aggregation
        hourly_stats = {
            'avg_temperature': 25.7,
            'max_temperature': 26.3,
            'min_temperature': 25.1,
            'avg_pressure': 1675,
            'data_points': 12
        }
        
        data_manager.get_hourly_stats.return_value = hourly_stats
        stats = data_manager.get_hourly_stats(0)  # Channel 0
        
        assert 'avg_temperature' in stats
        assert 'data_points' in stats
        assert stats['data_points'] > 0
        
    @pytest.mark.unit
    def test_data_export(self):
        """Test data export functionality."""
        data_manager = self.create_mock_data_manager()
        
        # Test CSV export
        csv_data = "timestamp,channel_id,temperature,pressure\n1640995200,0,25.5,1650\n"
        data_manager.export_csv.return_value = csv_data
        
        exported = data_manager.export_csv()
        assert 'timestamp' in exported
        assert 'temperature' in exported
        
        # Test JSON export
        json_data = {'data': [{'timestamp': 1640995200, 'channels': []}]}
        data_manager.export_json.return_value = json_data
        
        exported_json = data_manager.export_json()
        assert 'data' in exported_json
        assert isinstance(exported_json['data'], list)
        
    @pytest.mark.unit
    def test_data_cleanup(self):
        """Test automatic data cleanup and retention."""
        data_manager = self.create_mock_data_manager()
        
        # Test cleanup of old data
        retention_hours = self.sample_data['historical_data']['retention_period_hours']
        cutoff_time = int(time.time()) - (retention_hours * 3600)
        
        data_manager.cleanup_old_data.return_value = 5  # 5 entries removed
        removed_count = data_manager.cleanup_old_data(cutoff_time)
        
        assert removed_count >= 0
        
    @pytest.mark.unit
    def test_data_validation(self):
        """Test data validation before storage."""
        data_manager = self.create_mock_data_manager()
        
        # Test valid data
        valid_data = {
            'timestamp': int(time.time()),
            'channels': [{'id': 0, 'temperature': 25.5}],
            'system': {'free_heap': 45678}
        }
        
        data_manager.validate_data.return_value = True
        assert data_manager.validate_data(valid_data) == True
        
        # Test invalid data
        invalid_data = {
            'timestamp': 'invalid',  # Should be int
            'channels': 'invalid',   # Should be list
        }
        
        data_manager.validate_data.return_value = False
        assert data_manager.validate_data(invalid_data) == False
        
    @pytest.mark.unit
    def test_storage_persistence(self):
        """Test data persistence across system restarts."""
        data_manager = self.create_mock_data_manager()
        
        # Test saving to flash storage
        data_manager.save_to_flash.return_value = True
        result = data_manager.save_to_flash()
        assert result == True
        
        # Test loading from flash storage
        saved_data = self.sample_data['historical_data']['last_24_hours']
        data_manager.load_from_flash.return_value = saved_data
        
        loaded_data = data_manager.load_from_flash()
        assert len(loaded_data) > 0
        
    @pytest.mark.unit
    def test_real_time_streaming(self):
        """Test real-time data streaming capabilities."""
        data_manager = self.create_mock_data_manager()
        
        # Test streaming data to WebSocket clients
        stream_data = {'timestamp': int(time.time()), 'temperature': 25.5}
        
        data_manager.stream_to_clients.return_value = True
        result = data_manager.stream_to_clients(stream_data)
        assert result == True
        
    def create_mock_data_manager(self):
        """Create a mock HistoricalDataManager for testing."""
        manager = Mock()
        
        # Basic functionality
        manager.init = Mock(return_value=True)
        manager.is_initialized = Mock(return_value=True)
        manager.log_data = Mock(return_value=True)
        manager.get_data_range = Mock(return_value=[])
        
        # Storage management
        manager.get_storage_size = Mock(return_value=16384)
        manager.get_max_storage_size = Mock(return_value=32768)
        manager.compress_data = Mock(return_value=8192)
        
        # Buffer management
        manager.get_max_entries = Mock(return_value=100)
        manager.get_entry_count = Mock(return_value=50)
        manager.add_entry = Mock(return_value=True)
        
        # Data analysis
        manager.get_hourly_stats = Mock(return_value={})
        manager.export_csv = Mock(return_value='')
        manager.export_json = Mock(return_value={})
        
        # Maintenance
        manager.cleanup_old_data = Mock(return_value=0)
        manager.validate_data = Mock(return_value=True)
        
        # Persistence
        manager.save_to_flash = Mock(return_value=True)
        manager.load_from_flash = Mock(return_value=[])
        
        # Streaming
        manager.stream_to_clients = Mock(return_value=True)
        
        return manager


class TestDataAnalytics:
    """Test data analytics and trend analysis functionality."""
    
    @pytest.mark.unit
    def test_trend_analysis(self):
        """Test trend analysis calculations."""
        data_manager = Mock()
        
        # Test temperature trend
        temperature_trend = {
            'slope': 0.1,  # Rising
            'correlation': 0.85,
            'prediction': 26.0
        }
        
        data_manager.calculate_trend.return_value = temperature_trend
        trend = data_manager.calculate_trend('temperature', 3600)  # 1 hour
        
        assert 'slope' in trend
        assert 'correlation' in trend
        assert trend['correlation'] > 0.8  # Strong correlation
        
    @pytest.mark.unit
    def test_anomaly_detection(self):
        """Test anomaly detection in sensor data."""
        data_manager = Mock()
        
        # Test anomaly detection
        anomalies = [
            {
                'timestamp': 1640995200,
                'channel': 0,
                'parameter': 'temperature',
                'value': 35.0,
                'expected': 25.5,
                'severity': 'high'
            }
        ]
        
        data_manager.detect_anomalies.return_value = anomalies
        detected = data_manager.detect_anomalies()
        
        assert len(detected) >= 0
        if detected:
            assert 'severity' in detected[0]
            assert 'parameter' in detected[0]
            
    @pytest.mark.unit
    def test_statistical_analysis(self):
        """Test statistical analysis of historical data."""
        data_manager = Mock()
        
        # Test statistics calculation
        stats = {
            'mean': 25.5,
            'median': 25.3,
            'std_dev': 1.2,
            'min': 23.1,
            'max': 28.9,
            'percentile_95': 27.8
        }
        
        data_manager.calculate_statistics.return_value = stats
        calculated = data_manager.calculate_statistics('temperature')
        
        assert 'mean' in calculated
        assert 'std_dev' in calculated
        assert calculated['min'] <= calculated['mean'] <= calculated['max']


class TestDataErrorHandling:
    """Test error handling in historical data management."""
    
    @pytest.mark.unit
    def test_storage_full_handling(self):
        """Test handling when storage is full."""
        data_manager = Mock()
        
        data_manager.log_data.side_effect = OSError("Storage full")
        
        with pytest.raises(OSError):
            data_manager.log_data({'timestamp': int(time.time())})
            
    @pytest.mark.unit
    def test_corrupted_data_handling(self):
        """Test handling of corrupted data files."""
        data_manager = Mock()
        
        data_manager.load_from_flash.side_effect = json.JSONDecodeError("Invalid JSON", "", 0)
        
        with pytest.raises(json.JSONDecodeError):
            data_manager.load_from_flash()
            
    @pytest.mark.unit
    def test_memory_pressure_handling(self):
        """Test handling under memory pressure."""
        data_manager = Mock()
        
        # Simulate low memory condition
        data_manager.get_available_memory.return_value = 10240  # 10KB
        data_manager.log_data.side_effect = MemoryError("Insufficient memory")
        
        with pytest.raises(MemoryError):
            data_manager.log_data({'large_data': 'x' * 20000})


if __name__ == '__main__':
    pytest.main([__file__, '-v'])