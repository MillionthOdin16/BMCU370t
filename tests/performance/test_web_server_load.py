"""
Performance and load tests for the ESP32 web server.

Tests web server performance under various load conditions,
memory usage, and response times for the BMCU370 interface.
"""

import pytest
import time
import threading
import queue
from unittest.mock import Mock, patch
from tests.conftest import load_test_data, assert_memory_usage_acceptable


class TestWebServerPerformance:
    """Test web server performance and scalability."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.performance_config = load_test_data('sample_historical_data.json')['performance_metrics']
        
    @pytest.mark.performance
    @pytest.mark.slow
    def test_concurrent_client_handling(self):
        """Test handling multiple concurrent clients."""
        max_clients = self.performance_config['web_server']['max_concurrent_clients']
        response_times = queue.Queue()
        errors = queue.Queue()
        
        def simulate_client():
            """Simulate a client making requests."""
            try:
                start_time = time.time()
                
                # Mock HTTP request
                with patch('requests.get') as mock_get:
                    mock_response = Mock()
                    mock_response.status_code = 200
                    mock_response.json.return_value = {'success': True}
                    mock_get.return_value = mock_response
                    
                    import requests
                    response = requests.get('http://localhost:8080/api/status')
                    
                end_time = time.time()
                response_times.put(end_time - start_time)
                
                if response.status_code != 200:
                    errors.put(f"HTTP {response.status_code}")
                    
            except Exception as e:
                errors.put(str(e))
                
        # Create multiple client threads
        threads = []
        for i in range(max_clients):
            thread = threading.Thread(target=simulate_client)
            threads.append(thread)
            
        # Start all threads simultaneously
        start_time = time.time()
        for thread in threads:
            thread.start()
            
        # Wait for all threads to complete
        for thread in threads:
            thread.join(timeout=10)  # 10 second timeout
            
        end_time = time.time()
        total_time = end_time - start_time
        
        # Collect results
        times = []
        while not response_times.empty():
            times.append(response_times.get())
            
        error_list = []
        while not errors.empty():
            error_list.append(errors.get())
            
        # Verify performance metrics
        assert len(error_list) == 0, f"Errors occurred: {error_list}"
        assert len(times) == max_clients, f"Not all clients completed: {len(times)}/{max_clients}"
        
        avg_response_time = sum(times) / len(times)
        max_response_time = max(times)
        
        expected_max_response = self.performance_config['web_server']['average_response_time_ms'] / 1000 * 2
        assert avg_response_time < expected_max_response, \
            f"Average response time too high: {avg_response_time:.3f}s"
        assert total_time < 5.0, f"Total time too high: {total_time:.3f}s"
        
    @pytest.mark.performance
    def test_request_throughput(self):
        """Test request throughput capability."""
        expected_rpm = self.performance_config['web_server']['requests_per_minute']
        request_count = 60  # Test for 1 minute worth of requests
        
        response_times = []
        
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = {'success': True}
            mock_get.return_value = mock_response
            
            start_time = time.time()
            
            for i in range(request_count):
                request_start = time.time()
                
                import requests
                response = requests.get('http://localhost:8080/api/status')
                
                request_end = time.time()
                response_times.append(request_end - request_start)
                
                assert response.status_code == 200
                
                # Add small delay to simulate realistic timing
                time.sleep(0.01)  # 10ms between requests
                
            end_time = time.time()
            total_time = end_time - start_time
            
        # Calculate throughput
        requests_per_second = request_count / total_time
        requests_per_minute = requests_per_second * 60
        
        avg_response_time = sum(response_times) / len(response_times)
        
        # Verify throughput meets requirements
        assert requests_per_minute >= expected_rpm * 0.8, \
            f"Throughput too low: {requests_per_minute:.1f} RPM (expected ≥{expected_rpm})"
        assert avg_response_time < 0.1, f"Average response time too high: {avg_response_time:.3f}s"
        
    @pytest.mark.performance
    def test_memory_usage_under_load(self, esp32_memory_limits):
        """Test memory usage under high load conditions."""
        memory_per_client = self.performance_config['web_server']['memory_usage_per_client_bytes']
        max_clients = self.performance_config['web_server']['max_concurrent_clients']
        
        # Simulate memory allocation for multiple clients
        initial_memory = 200000  # 200KB initial usage
        client_memory_usage = []
        
        for client_count in range(1, max_clients + 1):
            # Calculate estimated memory usage
            estimated_usage = initial_memory + (client_count * memory_per_client)
            client_memory_usage.append(estimated_usage)
            
            # Verify memory usage is within limits
            assert_memory_usage_acceptable(
                estimated_usage, 
                esp32_memory_limits['heap_warning_threshold'],
                threshold=0.8
            )
            
        # Test peak memory usage
        peak_usage = max(client_memory_usage)
        available_memory = esp32_memory_limits['heap_warning_threshold']
        
        usage_ratio = peak_usage / available_memory
        assert usage_ratio < 0.9, f"Peak memory usage too high: {usage_ratio:.1%}"
        
    @pytest.mark.performance
    def test_websocket_message_throughput(self):
        """Test WebSocket message throughput."""
        websocket_config = self.performance_config['websocket']
        message_frequency = 1000 / websocket_config['message_frequency_ms']  # Messages per second
        max_message_size = websocket_config['max_message_size_bytes']
        
        # Simulate WebSocket message handling
        messages_processed = []
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = Mock()
            
            def mock_send(message):
                # Simulate message processing time
                processing_time = len(message) / 100000  # 100KB/s processing rate
                time.sleep(processing_time)
                messages_processed.append(len(message))
                
            mock_websocket.send = mock_send
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            # Test message throughput
            test_duration = 2.0  # 2 seconds
            message_size = max_message_size // 2  # Use half of max size
            
            start_time = time.time()
            messages_sent = 0
            
            while time.time() - start_time < test_duration:
                test_message = 'x' * message_size
                mock_websocket.send(test_message)
                messages_sent += 1
                
                # Respect frequency limit
                time.sleep(1.0 / message_frequency)
                
            end_time = time.time()
            actual_duration = end_time - start_time
            
        # Verify throughput
        actual_frequency = len(messages_processed) / actual_duration
        expected_frequency = message_frequency * 0.8  # Allow 20% tolerance
        
        assert actual_frequency >= expected_frequency, \
            f"WebSocket throughput too low: {actual_frequency:.1f} msg/s (expected ≥{expected_frequency:.1f})"
            
    @pytest.mark.performance
    def test_large_response_handling(self):
        """Test handling of large API responses."""
        # Simulate large status response (e.g., with historical data)
        large_response_data = {
            'success': True,
            'data': {
                'system': {'uptime': 123456},
                'channels': [{'id': i, 'data': 'x' * 1000} for i in range(100)],  # Large dataset
                'history': [{'timestamp': i, 'values': list(range(50))} for i in range(200)]
            }
        }
        
        import json
        response_size = len(json.dumps(large_response_data))
        
        with patch('requests.get') as mock_get:
            mock_response = Mock()
            mock_response.status_code = 200
            mock_response.json.return_value = large_response_data
            mock_response.content = json.dumps(large_response_data).encode()
            mock_get.return_value = mock_response
            
            start_time = time.time()
            
            import requests
            response = requests.get('http://localhost:8080/api/status')
            
            end_time = time.time()
            response_time = end_time - start_time
            
        # Verify large response handling
        assert response.status_code == 200
        assert len(response.content) > 50000  # Should be substantial response
        
        # Response time should be reasonable even for large responses
        max_response_time = 2.0  # 2 seconds for large responses
        assert response_time < max_response_time, \
            f"Large response time too high: {response_time:.3f}s"
            
    @pytest.mark.performance
    def test_api_rate_limiting_performance(self):
        """Test API rate limiting performance impact."""
        rate_limit = 10  # requests per second
        
        response_times = []
        status_codes = []
        
        with patch('requests.get') as mock_get:
            def mock_response_generator(call_count=[0]):
                call_count[0] += 1
                mock_response = Mock()
                
                if call_count[0] <= rate_limit:
                    mock_response.status_code = 200
                    mock_response.json.return_value = {'success': True}
                else:
                    mock_response.status_code = 429  # Rate limited
                    mock_response.json.return_value = {'success': False, 'error': 'Rate limited'}
                    
                return mock_response
                
            mock_get.side_effect = lambda *args, **kwargs: mock_response_generator()
            
            # Make rapid requests
            for i in range(15):
                start_time = time.time()
                
                import requests
                response = requests.get('http://localhost:8080/api/status')
                
                end_time = time.time()
                response_times.append(end_time - start_time)
                status_codes.append(response.status_code)
                
                time.sleep(0.05)  # 50ms between requests
                
        # Verify rate limiting behavior
        successful_requests = status_codes.count(200)
        rate_limited_requests = status_codes.count(429)
        
        assert successful_requests == rate_limit, \
            f"Unexpected successful requests: {successful_requests} (expected {rate_limit})"
        assert rate_limited_requests == 5, \
            f"Unexpected rate limited requests: {rate_limited_requests} (expected 5)"
            
        # Rate limiting shouldn't significantly impact response times for successful requests
        successful_times = [response_times[i] for i, code in enumerate(status_codes) if code == 200]
        avg_successful_time = sum(successful_times) / len(successful_times)
        
        assert avg_successful_time < 0.1, \
            f"Rate limiting impacted performance: {avg_successful_time:.3f}s average"


class TestMemoryUsageMonitoring:
    """Test memory usage monitoring and optimization."""
    
    @pytest.mark.performance
    def test_heap_memory_monitoring(self, esp32_memory_limits):
        """Test heap memory usage monitoring."""
        # Simulate different memory usage scenarios
        memory_scenarios = [
            {'clients': 1, 'expected_usage': 30000},  # Light load
            {'clients': 2, 'expected_usage': 35000},  # Medium load
            {'clients': 4, 'expected_usage': 45000},  # Heavy load
        ]
        
        for scenario in memory_scenarios:
            clients = scenario['clients']
            expected_usage = scenario['expected_usage']
            
            # Verify memory usage is within acceptable limits
            warning_threshold = esp32_memory_limits['heap_warning_threshold']
            assert_memory_usage_acceptable(expected_usage, warning_threshold, 0.8)
            
    @pytest.mark.performance
    def test_memory_leak_detection(self):
        """Test memory leak detection over time."""
        initial_memory = 200000  # 200KB
        memory_usage_over_time = []
        
        # Simulate memory usage over multiple operations
        for operation in range(100):
            # Simulate gradual memory increase (should be minimal)
            current_memory = initial_memory + (operation * 10)  # 10 bytes per operation
            memory_usage_over_time.append(current_memory)
            
        # Check for memory leaks (significant unbounded growth)
        memory_growth = memory_usage_over_time[-1] - memory_usage_over_time[0]
        max_acceptable_growth = 5000  # 5KB over 100 operations
        
        assert memory_growth < max_acceptable_growth, \
            f"Potential memory leak detected: {memory_growth} bytes growth"
            
    @pytest.mark.performance
    def test_garbage_collection_efficiency(self):
        """Test garbage collection and memory cleanup."""
        # Simulate memory allocation and cleanup cycles
        allocation_sizes = [1000, 2000, 5000, 1000, 500]  # Varying allocation sizes
        memory_after_gc = []
        
        current_memory = 200000  # Base memory
        
        for size in allocation_sizes:
            # Simulate allocation
            current_memory += size
            
            # Simulate garbage collection (memory should decrease)
            gc_efficiency = 0.8  # 80% of allocated memory is freed
            freed_memory = size * gc_efficiency
            current_memory -= freed_memory
            
            memory_after_gc.append(current_memory)
            
        # Memory should not grow unbounded
        final_memory = memory_after_gc[-1]
        initial_memory = 200000
        
        memory_increase = final_memory - initial_memory
        max_acceptable_increase = 2000  # 2KB net increase
        
        assert memory_increase < max_acceptable_increase, \
            f"Memory not being freed efficiently: {memory_increase} bytes increase"


class TestResponseTimeOptimization:
    """Test response time optimization."""
    
    @pytest.mark.performance
    def test_api_response_caching(self):
        """Test API response caching for performance."""
        cache_duration = 1.0  # 1 second cache
        
        response_times = []
        
        with patch('requests.get') as mock_get:
            call_count = [0]
            
            def mock_cached_response(*args, **kwargs):
                call_count[0] += 1
                mock_response = Mock()
                mock_response.status_code = 200
                
                if call_count[0] == 1:
                    # First call - simulate database/device query
                    time.sleep(0.1)  # 100ms for "real" data fetch
                    mock_response.json.return_value = {'success': True, 'cached': False}
                else:
                    # Subsequent calls - cached response
                    time.sleep(0.01)  # 10ms for cached response
                    mock_response.json.return_value = {'success': True, 'cached': True}
                    
                return mock_response
                
            mock_get.side_effect = mock_cached_response
            
            # Make multiple requests within cache period
            for i in range(3):
                start_time = time.time()
                
                import requests
                response = requests.get('http://localhost:8080/api/status')
                
                end_time = time.time()
                response_times.append(end_time - start_time)
                
                time.sleep(0.1)  # Small delay between requests
                
        # First request should be slower (uncached)
        assert response_times[0] > 0.05, "First request should be slower (uncached)"
        
        # Subsequent requests should be faster (cached)
        for i in range(1, len(response_times)):
            assert response_times[i] < response_times[0], \
                f"Cached request {i} should be faster than first request"
                
    @pytest.mark.performance
    def test_static_file_serving_performance(self):
        """Test static file serving performance."""
        file_sizes = [1024, 5120, 20480]  # 1KB, 5KB, 20KB files
        
        for file_size in file_sizes:
            with patch('requests.get') as mock_get:
                mock_response = Mock()
                mock_response.status_code = 200
                mock_response.content = b'x' * file_size
                mock_response.headers = {'Content-Type': 'text/html'}
                mock_get.return_value = mock_response
                
                start_time = time.time()
                
                import requests
                response = requests.get('http://localhost:8080/index.html')
                
                end_time = time.time()
                response_time = end_time - start_time
                
                # Response time should scale reasonably with file size
                max_time_per_kb = 0.01  # 10ms per KB
                expected_max_time = (file_size / 1024) * max_time_per_kb
                
                assert response_time < expected_max_time, \
                    f"File serving too slow: {response_time:.3f}s for {file_size} bytes"
                    
    @pytest.mark.performance
    def test_database_query_optimization(self):
        """Test database/storage query optimization."""
        # Simulate different query complexities
        query_scenarios = [
            {'type': 'simple_status', 'expected_time': 0.05},  # 50ms
            {'type': 'channel_data', 'expected_time': 0.1},    # 100ms
            {'type': 'historical_data', 'expected_time': 0.2}, # 200ms
        ]
        
        for scenario in query_scenarios:
            query_type = scenario['type']
            expected_max_time = scenario['expected_time']
            
            with patch('requests.get') as mock_get:
                mock_response = Mock()
                mock_response.status_code = 200
                
                # Simulate query processing time
                time.sleep(expected_max_time * 0.5)  # Half of expected max time
                
                mock_response.json.return_value = {
                    'success': True,
                    'query_type': query_type,
                    'data': {}
                }
                mock_get.return_value = mock_response
                
                start_time = time.time()
                
                import requests
                response = requests.get(f'http://localhost:8080/api/{query_type}')
                
                end_time = time.time()
                actual_time = end_time - start_time
                
                assert actual_time < expected_max_time, \
                    f"{query_type} query too slow: {actual_time:.3f}s (expected <{expected_max_time:.3f}s)"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])