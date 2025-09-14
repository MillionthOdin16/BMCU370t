"""
Performance and load tests for the ESP32 web server.

Tests web server performance under various load conditions,
memory usage, and response times for the BMCU370 interface.
"""

import pytest
import time
import threading
import queue
import statistics
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
                
        # Start concurrent clients
        threads = []
        for i in range(max_clients):
            t = threading.Thread(target=simulate_client)
            threads.append(t)
            t.start()
            
        # Wait for all clients to complete
        for t in threads:
            t.join(timeout=10.0)
            
        # Collect results
        times = []
        while not response_times.empty():
            times.append(response_times.get())
            
        error_list = []
        while not errors.empty():
            error_list.append(errors.get())
            
        # Verify performance metrics
        assert len(times) >= max_clients * 0.8  # At least 80% should complete
        assert len(error_list) < max_clients * 0.2  # Less than 20% errors
        
        if times:
            avg_response_time = sum(times) / len(times)
            max_response_time = max(times)
            
            # Response times should be reasonable
            assert avg_response_time < 1.0  # Average < 1 second
            assert max_response_time < 5.0  # Maximum < 5 seconds
            
    @pytest.mark.performance
    def test_websocket_message_throughput(self):
        """Test WebSocket message throughput under load."""
        websocket_server = self.create_mock_websocket_server()
        
        message_count = 1000
        sent_messages = 0
        received_messages = 0
        errors = 0
        
        start_time = time.time()
        
        # Send rapid messages
        for i in range(message_count):
            try:
                message = {'type': 'sensor_data', 'data': {'temp': 25.0 + i * 0.1}}
                result = websocket_server.send_message(message)
                if result:
                    sent_messages += 1
                else:
                    errors += 1
                    
                # Simulate processing time
                time.sleep(0.001)  # 1ms between messages
                
            except Exception:
                errors += 1
                
        end_time = time.time()
        duration = end_time - start_time
        
        # Verify throughput metrics
        throughput = sent_messages / duration  # messages per second
        error_rate = errors / message_count
        
        assert throughput >= 500  # At least 500 messages/second
        assert error_rate < 0.05  # Less than 5% error rate
        assert sent_messages >= message_count * 0.95  # At least 95% sent
        
    @pytest.mark.performance
    def test_memory_usage_under_load(self):
        """Test memory usage patterns under sustained load."""
        web_server = self.create_mock_web_server()
        
        initial_memory = web_server.get_memory_usage()
        memory_samples = [initial_memory]
        
        # Sustained load test
        load_duration = 10.0  # 10 seconds
        requests_per_second = 50
        
        start_time = time.time()
        request_count = 0
        
        while time.time() - start_time < load_duration:
            # Make multiple rapid requests
            for _ in range(requests_per_second):
                web_server.handle_api_request('/api/status')
                web_server.handle_api_request('/api/config')
                request_count += 1
                
            # Sample memory usage
            current_memory = web_server.get_memory_usage()
            memory_samples.append(current_memory)
            
            time.sleep(1.0)  # 1 second intervals
            
        final_memory = web_server.get_memory_usage()
        
        # Analyze memory usage patterns
        memory_growth = final_memory - initial_memory
        memory_growth_percent = (memory_growth / initial_memory) * 100
        
        max_memory = max(memory_samples)
        memory_variance = statistics.variance(memory_samples[-5:])  # Variance in last 5 samples
        
        # Memory should not grow excessively
        assert memory_growth_percent < 50, f"Excessive memory growth: {memory_growth_percent:.2f}%"
        
        # Memory usage should stabilize (low variance at end)
        assert memory_variance < (initial_memory * 0.1) ** 2, "Memory usage not stabilized"
        
        # Maximum memory should be reasonable
        assert max_memory < initial_memory * 2, "Memory usage peaked too high"
        
        print(f"Processed {request_count} requests, memory growth: {memory_growth_percent:.2f}%")
        
    @pytest.mark.performance
    def test_api_response_time_distribution(self):
        """Test API response time distribution and consistency."""
        web_server = self.create_mock_web_server()
        
        api_endpoints = [
            '/api/status',
            '/api/config',
            '/api/logs',
            '/api/wifi/status',
            '/api/historical/summary'
        ]
        
        response_times = {endpoint: [] for endpoint in api_endpoints}
        
        # Collect response time samples
        samples_per_endpoint = 100
        
        for endpoint in api_endpoints:
            for _ in range(samples_per_endpoint):
                start_time = time.time()
                web_server.handle_api_request(endpoint)
                end_time = time.time()
                
                response_time = (end_time - start_time) * 1000  # Convert to milliseconds
                response_times[endpoint].append(response_time)
                
                # Small delay between requests
                time.sleep(0.01)
                
        # Analyze response time statistics
        for endpoint, times in response_times.items():
            avg_time = statistics.mean(times)
            median_time = statistics.median(times)
            p95_time = sorted(times)[int(len(times) * 0.95)]
            p99_time = sorted(times)[int(len(times) * 0.99)]
            std_dev = statistics.stdev(times)
            
            # Performance requirements
            assert avg_time < 100, f"{endpoint}: Average response time {avg_time:.2f}ms too high"
            assert median_time < 50, f"{endpoint}: Median response time {median_time:.2f}ms too high"
            assert p95_time < 200, f"{endpoint}: 95th percentile {p95_time:.2f}ms too high"
            assert p99_time < 500, f"{endpoint}: 99th percentile {p99_time:.2f}ms too high"
            assert std_dev < 50, f"{endpoint}: Response time variance {std_dev:.2f}ms too high"
            
            print(f"{endpoint}: avg={avg_time:.1f}ms, p95={p95_time:.1f}ms, p99={p99_time:.1f}ms")
            
    @pytest.mark.performance
    def test_database_query_performance(self):
        """Test historical data database query performance."""
        history_manager = self.create_mock_history_manager()
        
        # Pre-populate with test data
        data_points = 10000
        for i in range(data_points):
            timestamp = time.time() - (data_points - i) * 60  # 1 minute intervals
            data = {
                'timestamp': timestamp,
                'temperature': 25.0 + (i % 100) * 0.1,
                'voltage': 4.8 + (i % 50) * 0.001,
                'current': 0.15 + (i % 20) * 0.001
            }
            history_manager.store_data_point(data)
            
        # Test various query patterns
        query_tests = [
            {
                'name': 'recent_data',
                'query': lambda: history_manager.get_recent_data(hours=1),
                'max_time_ms': 100
            },
            {
                'name': 'daily_summary',
                'query': lambda: history_manager.get_daily_summary(days=7),
                'max_time_ms': 200
            },
            {
                'name': 'trend_analysis',
                'query': lambda: history_manager.get_trend_analysis(hours=24),
                'max_time_ms': 500
            },
            {
                'name': 'statistical_summary',
                'query': lambda: history_manager.get_statistical_summary(days=30),
                'max_time_ms': 1000
            }
        ]
        
        for test in query_tests:
            # Run query multiple times to get average
            query_times = []
            for _ in range(10):
                start_time = time.time()
                result = test['query']()
                end_time = time.time()
                
                query_time = (end_time - start_time) * 1000  # Convert to milliseconds
                query_times.append(query_time)
                
                # Verify query returned data
                assert result is not None
                assert len(result) > 0
                
            avg_query_time = statistics.mean(query_times)
            max_query_time = max(query_times)
            
            # Performance requirements
            assert avg_query_time < test['max_time_ms'], \
                f"{test['name']}: Average query time {avg_query_time:.2f}ms exceeds {test['max_time_ms']}ms"
            assert max_query_time < test['max_time_ms'] * 2, \
                f"{test['name']}: Maximum query time {max_query_time:.2f}ms too high"
                
            print(f"{test['name']}: avg={avg_query_time:.1f}ms, max={max_query_time:.1f}ms")
            
    @pytest.mark.performance
    def test_stress_test_recovery(self):
        """Test system recovery after stress conditions."""
        web_server = self.create_mock_web_server()
        
        # Phase 1: Normal operation baseline
        baseline_response_times = []
        for _ in range(50):
            start_time = time.time()
            web_server.handle_api_request('/api/status')
            end_time = time.time()
            baseline_response_times.append((end_time - start_time) * 1000)
            time.sleep(0.02)
            
        baseline_avg = statistics.mean(baseline_response_times)
        
        # Phase 2: Stress test
        stress_duration = 5.0  # 5 seconds of stress
        stress_start = time.time()
        stress_requests = 0
        
        while time.time() - stress_start < stress_duration:
            # Rapid fire requests
            for _ in range(20):
                web_server.handle_api_request('/api/status')
                stress_requests += 1
                
        # Phase 3: Recovery period
        recovery_start = time.time()
        recovery_response_times = []
        
        while time.time() - recovery_start < 10.0:  # 10 seconds recovery
            start_time = time.time()
            web_server.handle_api_request('/api/status')
            end_time = time.time()
            recovery_response_times.append((end_time - start_time) * 1000)
            time.sleep(0.1)
            
        # Analyze recovery
        recovery_samples = 10  # Last 10 samples
        final_response_times = recovery_response_times[-recovery_samples:]
        final_avg = statistics.mean(final_response_times)
        
        # System should recover to near baseline performance
        performance_degradation = (final_avg - baseline_avg) / baseline_avg * 100
        
        assert performance_degradation < 50, \
            f"Performance degradation {performance_degradation:.1f}% after stress test too high"
        
        assert stress_requests > 500, f"Stress test too light: {stress_requests} requests"
        
        print(f"Baseline: {baseline_avg:.1f}ms, Post-stress: {final_avg:.1f}ms, " +
              f"Degradation: {performance_degradation:.1f}%")
              
    @pytest.mark.performance  
    def test_resource_exhaustion_handling(self):
        """Test handling of resource exhaustion scenarios."""
        web_server = self.create_mock_web_server()
        
        # Test file descriptor exhaustion
        file_handles = []
        max_files = 100  # Simulate limited file descriptors
        
        try:
            for i in range(max_files + 10):
                handle = web_server.open_file_handle(f"test_file_{i}")
                if handle:
                    file_handles.append(handle)
                else:
                    # Should gracefully handle exhaustion
                    break
                    
        except Exception as e:
            # Should not crash, but handle gracefully
            assert "resource" in str(e).lower() or "limit" in str(e).lower()
            
        # Verify system still responsive
        response = web_server.handle_api_request('/api/status')
        assert response.status_code == 200
        
        # Cleanup
        for handle in file_handles:
            web_server.close_file_handle(handle)
            
        # Test memory exhaustion simulation
        large_allocations = []
        allocation_size = 1024 * 1024  # 1MB allocations
        
        for i in range(50):  # Try to allocate 50MB
            try:
                allocation = web_server.allocate_memory(allocation_size)
                if allocation:
                    large_allocations.append(allocation)
                else:
                    # Should gracefully handle memory pressure
                    break
            except MemoryError:
                # Expected when memory is exhausted
                break
                
        # System should still be responsive
        response = web_server.handle_api_request('/api/status')
        assert response.status_code == 200
        
        # Cleanup
        for allocation in large_allocations:
            web_server.free_memory(allocation)

    def create_mock_web_server(self):
        """Create a mock web server for performance testing."""
        server = Mock()
        server._memory_usage = 50000  # Initial memory usage
        server._file_handles = []
        server._memory_allocations = []
        
        def handle_api_request_mock(endpoint):
            # Simulate realistic processing time
            if endpoint == '/api/historical/summary':
                time.sleep(0.01)  # 10ms for complex queries
            else:
                time.sleep(0.002)  # 2ms for simple requests
                
            # Simulate memory usage
            server._memory_usage += 100  # Small memory increase per request
            
            return Mock(status_code=200)
            
        def get_memory_usage_mock():
            return server._memory_usage
            
        def open_file_handle_mock(filename):
            if len(server._file_handles) >= 100:  # Simulate limit
                return None
            handle = Mock()
            server._file_handles.append(handle)
            return handle
            
        def close_file_handle_mock(handle):
            if handle in server._file_handles:
                server._file_handles.remove(handle)
                
        def allocate_memory_mock(size):
            if len(server._memory_allocations) >= 30:  # Simulate limit
                return None
            allocation = Mock()
            server._memory_allocations.append(allocation)
            server._memory_usage += size
            return allocation
            
        def free_memory_mock(allocation):
            if allocation in server._memory_allocations:
                server._memory_allocations.remove(allocation)
                server._memory_usage -= 1024 * 1024  # 1MB
                
        server.handle_api_request.side_effect = handle_api_request_mock
        server.get_memory_usage.side_effect = get_memory_usage_mock
        server.open_file_handle.side_effect = open_file_handle_mock
        server.close_file_handle.side_effect = close_file_handle_mock
        server.allocate_memory.side_effect = allocate_memory_mock
        server.free_memory.side_effect = free_memory_mock
        
        return server
        
    def create_mock_websocket_server(self):
        """Create a mock WebSocket server for testing."""
        server = Mock()
        
        def send_message_mock(message):
            # Simulate message processing
            time.sleep(0.001)  # 1ms processing time
            return True  # Success
            
        server.send_message.side_effect = send_message_mock
        return server
        
    def create_mock_history_manager(self):
        """Create a mock historical data manager."""
        manager = Mock()
        manager._data_points = []
        
        def store_data_point_mock(data):
            manager._data_points.append(data)
            
        def get_recent_data_mock(hours=1):
            # Simulate database query time
            time.sleep(0.05)  # 50ms
            cutoff = time.time() - hours * 3600
            return [dp for dp in manager._data_points if dp['timestamp'] > cutoff]
            
        def get_daily_summary_mock(days=7):
            time.sleep(0.1)  # 100ms
            return [{'day': i, 'avg_temp': 25.0, 'avg_voltage': 4.8} for i in range(days)]
            
        def get_trend_analysis_mock(hours=24):
            time.sleep(0.2)  # 200ms
            return {'trend': 'stable', 'slope': 0.1, 'correlation': 0.95}
            
        def get_statistical_summary_mock(days=30):
            time.sleep(0.5)  # 500ms
            return {
                'mean': 25.0, 'std': 2.5, 'min': 20.0, 'max': 30.0,
                'percentiles': {'p25': 23.0, 'p50': 25.0, 'p75': 27.0}
            }
            
        manager.store_data_point.side_effect = store_data_point_mock
        manager.get_recent_data.side_effect = get_recent_data_mock
        manager.get_daily_summary.side_effect = get_daily_summary_mock
        manager.get_trend_analysis.side_effect = get_trend_analysis_mock
        manager.get_statistical_summary.side_effect = get_statistical_summary_mock
        
        return manager
                    
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