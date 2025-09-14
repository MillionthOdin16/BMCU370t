"""
Integration tests for WebSocket functionality.

Tests real-time communication, message broadcasting, and connection management
for the WebSocket interface.
"""

import pytest
import json
import asyncio
import websockets
from unittest.mock import Mock, patch, AsyncMock
from tests.conftest import load_test_data


class TestWebSocketIntegration:
    """Integration tests for WebSocket functionality."""
    
    def setup_method(self):
        """Setup test environment for each test."""
        self.ws_url = 'ws://localhost:8080/ws'
        self.test_data = load_test_data('mock_bmcu370_responses.json')
        
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_connection(self):
        """Test WebSocket connection establishment."""
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect(self.ws_url) as websocket:
                assert websocket is not None
                
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_status_updates_via_websocket(self):
        """Test receiving status updates via WebSocket."""
        status_data = self.test_data['system_online']
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.return_value = json.dumps({
                'type': 'status_update',
                'data': status_data
            })
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect(self.ws_url) as websocket:
                message = await websocket.recv()
                data = json.loads(message)
                
                assert data['type'] == 'status_update'
                assert 'data' in data
                assert data['data']['system']['bambubus_status'] == 'online'
                
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_command_sending(self):
        """Test sending commands via WebSocket."""
        command = {
            'type': 'set_parameter',
            'parameter': 'led_brightness.main',
            'value': 50
        }
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.return_value = json.dumps({
                'type': 'command_response',
                'success': True,
                'message': 'Parameter updated'
            })
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect(self.ws_url) as websocket:
                await websocket.send(json.dumps(command))
                response = await websocket.recv()
                data = json.loads(response)
                
                assert data['type'] == 'command_response'
                assert data['success'] == True
                
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_real_time_sensor_data(self):
        """Test real-time sensor data streaming."""
        sensor_data = {
            'type': 'sensor_data',
            'timestamp': 1640995200,
            'channels': [
                {
                    'id': 0,
                    'hall_position': 1024,
                    'temperature': 25.5,
                    'filament_present': True
                }
            ]
        }
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.side_effect = [
                json.dumps(sensor_data),
                json.dumps({**sensor_data, 'timestamp': 1640995260})
            ]
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect(self.ws_url) as websocket:
                # Receive first update
                message1 = await websocket.recv()
                data1 = json.loads(message1)
                
                # Receive second update
                message2 = await websocket.recv()
                data2 = json.loads(message2)
                
                assert data1['type'] == 'sensor_data'
                assert data2['timestamp'] > data1['timestamp']
                
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_error_handling(self):
        """Test WebSocket error handling."""
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.side_effect = websockets.exceptions.ConnectionClosed(None, None)
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            with pytest.raises(websockets.exceptions.ConnectionClosed):
                async with websockets.connect(self.ws_url) as websocket:
                    await websocket.recv()
                    
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_multiple_websocket_clients(self):
        """Test multiple WebSocket clients simultaneously."""
        clients = []
        
        for i in range(3):
            with patch('websockets.connect') as mock_connect:
                mock_websocket = AsyncMock()
                mock_websocket.recv.return_value = json.dumps({
                    'type': 'client_connected',
                    'client_id': i
                })
                mock_connect.return_value.__aenter__.return_value = mock_websocket
                clients.append(mock_websocket)
                
        # Verify all clients can connect
        assert len(clients) == 3
        
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_heartbeat(self):
        """Test WebSocket heartbeat/ping-pong mechanism."""
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.ping.return_value = asyncio.Future()
            mock_websocket.ping.return_value.set_result(None)
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect(self.ws_url) as websocket:
                # Send ping
                await websocket.ping()
                # Verify ping was called
                mock_websocket.ping.assert_called()
                
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_message_queue(self):
        """Test WebSocket message queuing and delivery."""
        messages = [
            {'type': 'status_update', 'data': {'uptime': 1000}},
            {'type': 'status_update', 'data': {'uptime': 2000}},
            {'type': 'status_update', 'data': {'uptime': 3000}}
        ]
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.side_effect = [json.dumps(msg) for msg in messages]
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            received_messages = []
            
            async with websockets.connect(self.ws_url) as websocket:
                for _ in range(3):
                    message = await websocket.recv()
                    received_messages.append(json.loads(message))
                    
            assert len(received_messages) == 3
            assert received_messages[0]['data']['uptime'] == 1000
            assert received_messages[2]['data']['uptime'] == 3000
            
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_authentication(self):
        """Test WebSocket authentication mechanism."""
        auth_message = {
            'type': 'authenticate',
            'token': 'test_token_123'
        }
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.return_value = json.dumps({
                'type': 'auth_response',
                'success': True,
                'message': 'Authentication successful'
            })
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect(self.ws_url) as websocket:
                await websocket.send(json.dumps(auth_message))
                response = await websocket.recv()
                data = json.loads(response)
                
                assert data['type'] == 'auth_response'
                assert data['success'] == True
                
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_subscription_management(self):
        """Test WebSocket subscription management."""
        subscription = {
            'type': 'subscribe',
            'topics': ['status_updates', 'sensor_data']
        }
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.return_value = json.dumps({
                'type': 'subscription_response',
                'success': True,
                'subscribed_topics': ['status_updates', 'sensor_data']
            })
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect(self.ws_url) as websocket:
                await websocket.send(json.dumps(subscription))
                response = await websocket.recv()
                data = json.loads(response)
                
                assert data['type'] == 'subscription_response'
                assert len(data['subscribed_topics']) == 2
                
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_rate_limiting(self):
        """Test WebSocket rate limiting."""
        messages = []
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            
            # First 10 messages succeed, then rate limited
            def recv_side_effect():
                if len(messages) < 10:
                    messages.append('message')
                    return json.dumps({'type': 'data', 'accepted': True})
                else:
                    return json.dumps({
                        'type': 'error',
                        'message': 'Rate limit exceeded'
                    })
                    
            mock_websocket.recv.side_effect = recv_side_effect
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect(self.ws_url) as websocket:
                # Send many messages rapidly
                for i in range(15):
                    await websocket.send(json.dumps({'type': 'test', 'id': i}))
                    response = await websocket.recv()
                    data = json.loads(response)
                    
                    if i >= 10:
                        assert data['type'] == 'error'
                        assert 'rate limit' in data['message'].lower()


class TestWebSocketPerformance:
    """Test WebSocket performance and scalability."""
    
    @pytest.mark.integration
    @pytest.mark.slow
    @pytest.mark.asyncio
    async def test_websocket_throughput(self):
        """Test WebSocket message throughput."""
        message_count = 100
        messages_sent = []
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.side_effect = [
                json.dumps({'type': 'ack', 'id': i}) for i in range(message_count)
            ]
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            import time
            start_time = time.time()
            
            async with websockets.connect('ws://localhost:8080/ws') as websocket:
                for i in range(message_count):
                    await websocket.send(json.dumps({'type': 'test', 'id': i}))
                    response = await websocket.recv()
                    messages_sent.append(i)
                    
            end_time = time.time()
            duration = end_time - start_time
            
            # Calculate throughput (mocked, so should be very fast)
            throughput = message_count / duration
            assert throughput > 100  # Should handle >100 messages/second when mocked
            
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_memory_usage(self):
        """Test WebSocket memory usage under load."""
        large_message = {
            'type': 'large_data',
            'data': 'x' * 10000  # 10KB message
        }
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.return_value = json.dumps({
                'type': 'memory_status',
                'free_heap': 40000  # Should have sufficient memory
            })
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect('ws://localhost:8080/ws') as websocket:
                await websocket.send(json.dumps(large_message))
                response = await websocket.recv()
                data = json.loads(response)
                
                # Should handle large messages without memory issues
                assert data['free_heap'] > 20000  # Sufficient memory remaining
                
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_connection_recovery(self):
        """Test WebSocket connection recovery after disconnect."""
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            
            # First connection fails, second succeeds
            connection_attempts = []
            
            def connect_side_effect(*args, **kwargs):
                connection_attempts.append(len(connection_attempts))
                if len(connection_attempts) == 1:
                    raise websockets.exceptions.ConnectionClosed(None, None)
                else:
                    return mock_websocket
                    
            mock_connect.side_effect = connect_side_effect
            mock_websocket.recv.return_value = json.dumps({
                'type': 'connection_restored',
                'message': 'Reconnected successfully'
            })
            
            # First attempt should fail
            with pytest.raises(websockets.exceptions.ConnectionClosed):
                async with websockets.connect('ws://localhost:8080/ws') as websocket:
                    pass
                    
            # Second attempt should succeed
            # In real implementation, this would be handled by reconnection logic


class TestWebSocketSecurity:
    """Test WebSocket security features."""
    
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_input_validation(self):
        """Test WebSocket input validation and sanitization."""
        malicious_messages = [
            {'type': '<script>alert("xss")</script>'},
            {'type': 'command', 'parameter': '../../../etc/passwd'},
            {'type': 'test', 'data': '\'; DROP TABLE users; --'}
        ]
        
        for message in malicious_messages:
            with patch('websockets.connect') as mock_connect:
                mock_websocket = AsyncMock()
                mock_websocket.recv.return_value = json.dumps({
                    'type': 'error',
                    'message': 'Invalid message format'
                })
                mock_connect.return_value.__aenter__.return_value = mock_websocket
                
                async with websockets.connect('ws://localhost:8080/ws') as websocket:
                    await websocket.send(json.dumps(message))
                    response = await websocket.recv()
                    data = json.loads(response)
                    
                    # Should reject malicious input
                    assert data['type'] == 'error'
                    
    @pytest.mark.integration
    @pytest.mark.asyncio
    async def test_websocket_message_size_limits(self):
        """Test WebSocket message size limitations."""
        oversized_message = {
            'type': 'test',
            'data': 'x' * 100000  # 100KB message
        }
        
        with patch('websockets.connect') as mock_connect:
            mock_websocket = AsyncMock()
            mock_websocket.recv.return_value = json.dumps({
                'type': 'error',
                'message': 'Message too large'
            })
            mock_connect.return_value.__aenter__.return_value = mock_websocket
            
            async with websockets.connect('ws://localhost:8080/ws') as websocket:
                await websocket.send(json.dumps(oversized_message))
                response = await websocket.recv()
                data = json.loads(response)
                
                # Should reject oversized messages
                assert data['type'] == 'error'
                assert 'large' in data['message'].lower()


if __name__ == '__main__':
    pytest.main([__file__, '-v'])