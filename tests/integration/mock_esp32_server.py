"""
Mock ESP32 HTTP server for integration testing

This script creates a simple HTTP server that mimics the ESP32 web interface
for integration testing purposes.
"""

import json
import time
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class MockESP32Handler(BaseHTTPRequestHandler):
    """HTTP request handler mimicking ESP32 web server responses"""
    
    def do_GET(self):
        """Handle GET requests"""
        parsed_path = urlparse(self.path)
        path = parsed_path.path
        query = parse_qs(parsed_path.query)
        
        logger.info(f"GET {path}")
        
        if path == "/":
            self._serve_index_page()
        elif path == "/api/status":
            self._serve_status_api()
        elif path == "/api/config":
            self._serve_config_api()
        elif path == "/api/wifi":
            self._serve_wifi_api()
        elif path.startswith("/static/"):
            self._serve_static_file(path)
        else:
            self._send_404()
            
    def do_POST(self):
        """Handle POST requests"""
        path = urlparse(self.path).path
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length)
        
        logger.info(f"POST {path}")
        
        if path == "/api/config":
            self._handle_config_update(post_data)
        elif path == "/api/wifi":
            self._handle_wifi_config(post_data)
        elif path == "/api/control":
            self._handle_device_control(post_data)
        else:
            self._send_404()
    
    def _serve_index_page(self):
        """Serve main web interface page"""
        html_content = """
        <!DOCTYPE html>
        <html>
        <head>
            <title>BMCU370 Interface</title>
            <meta charset="utf-8">
            <style>
                body { font-family: Arial, sans-serif; margin: 20px; }
                .status { background: #f0f0f0; padding: 10px; margin: 10px 0; }
                .error { color: red; }
                .success { color: green; }
                button { padding: 10px; margin: 5px; }
            </style>
        </head>
        <body>
            <h1>BMCU370 USB-ESP Interface</h1>
            <div id="status" class="status">Loading...</div>
            <div id="controls">
                <button onclick="fetchStatus()">Refresh Status</button>
                <button onclick="toggleLED()">Toggle LED</button>
            </div>
            <script>
                async function fetchStatus() {
                    try {
                        const response = await fetch('/api/status');
                        const data = await response.json();
                        document.getElementById('status').innerHTML = 
                            '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                    } catch (error) {
                        document.getElementById('status').innerHTML = 
                            '<div class="error">Error: ' + error.message + '</div>';
                    }
                }
                
                async function toggleLED() {
                    try {
                        const response = await fetch('/api/control', {
                            method: 'POST',
                            headers: {'Content-Type': 'application/json'},
                            body: JSON.stringify({action: 'toggle_led', led: 'status'})
                        });
                        const result = await response.json();
                        if (result.success) {
                            fetchStatus();
                        }
                    } catch (error) {
                        console.error('Control error:', error);
                    }
                }
                
                // Auto-refresh status
                setInterval(fetchStatus, 5000);
                fetchStatus();
            </script>
        </body>
        </html>
        """
        
        self._send_response(200, html_content, 'text/html')
    
    def _serve_status_api(self):
        """Serve status API endpoint"""
        status_data = {
            "timestamp": int(time.time()),
            "device_connected": True,
            "bmcu370_status": {
                "firmware_version": "0.1.0020",
                "system_status": 1,
                "channel_count": 16,
                "temperatures": {
                    "mcu": 42.5,
                    "ambient": 23.1
                },
                "voltages": {
                    "vcc_3v3": 3.31,
                    "vcc_5v": 5.02
                },
                "current_consumption": 234.5
            },
            "esp32_status": {
                "heap_free": 234567,
                "wifi_connected": True,
                "wifi_ssid": "TestNetwork",
                "ip_address": "192.168.1.100"
            },
            "system_info": {
                "uptime": 3600,
                "last_update": int(time.time() - 10)
            }
        }
        
        self._send_json_response(status_data)
    
    def _serve_config_api(self):
        """Serve configuration API endpoint"""
        config_data = {
            "wifi": {
                "ssid": "TestNetwork",
                "connected": True
            },
            "device": {
                "name": "BMCU370-Test",
                "location": "Test Lab"
            },
            "settings": {
                "auto_refresh": True,
                "refresh_interval": 5000,
                "debug_mode": False
            }
        }
        
        self._send_json_response(config_data)
    
    def _serve_wifi_api(self):
        """Serve WiFi API endpoint"""
        wifi_data = {
            "current": {
                "ssid": "TestNetwork",
                "connected": True,
                "signal_strength": -45,
                "ip_address": "192.168.1.100"
            },
            "available_networks": [
                {"ssid": "TestNetwork", "signal": -45, "secured": True},
                {"ssid": "OpenNetwork", "signal": -67, "secured": False},
                {"ssid": "WeakNetwork", "signal": -78, "secured": True}
            ]
        }
        
        self._send_json_response(wifi_data)
    
    def _handle_config_update(self, post_data):
        """Handle configuration update POST"""
        try:
            config = json.loads(post_data.decode('utf-8'))
            logger.info(f"Config update: {config}")
            
            response = {
                "success": True,
                "message": "Configuration updated successfully",
                "updated_fields": list(config.keys())
            }
            
            self._send_json_response(response)
        except Exception as e:
            self._send_error_response(400, f"Invalid configuration: {str(e)}")
    
    def _handle_wifi_config(self, post_data):
        """Handle WiFi configuration POST"""
        try:
            wifi_config = json.loads(post_data.decode('utf-8'))
            logger.info(f"WiFi config: {wifi_config}")
            
            response = {
                "success": True,
                "message": "WiFi configuration updated",
                "connecting": True
            }
            
            self._send_json_response(response)
        except Exception as e:
            self._send_error_response(400, f"Invalid WiFi configuration: {str(e)}")
    
    def _handle_device_control(self, post_data):
        """Handle device control POST"""
        try:
            control_data = json.loads(post_data.decode('utf-8'))
            logger.info(f"Device control: {control_data}")
            
            response = {
                "success": True,
                "message": f"Control action '{control_data.get('action')}' executed",
                "action": control_data.get('action'),
                "result": "OK"
            }
            
            self._send_json_response(response)
        except Exception as e:
            self._send_error_response(400, f"Invalid control command: {str(e)}")
    
    def _serve_static_file(self, path):
        """Serve static files (CSS, JS, etc.)"""
        # For testing, just return empty content
        self._send_response(200, "", 'text/plain')
    
    def _send_response(self, status_code, content, content_type):
        """Send HTTP response"""
        self.send_response(status_code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(content)))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(content.encode('utf-8'))
    
    def _send_json_response(self, data):
        """Send JSON response"""
        json_content = json.dumps(data, indent=2)
        self._send_response(200, json_content, 'application/json')
    
    def _send_error_response(self, status_code, message):
        """Send error response"""
        error_data = {
            "error": True,
            "message": message,
            "status_code": status_code
        }
        json_content = json.dumps(error_data, indent=2)
        self.send_response(status_code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(json_content)))
        self.end_headers()
        self.wfile.write(json_content.encode('utf-8'))
    
    def _send_404(self):
        """Send 404 Not Found response"""
        self._send_error_response(404, "Not found")
    
    def log_message(self, format, *args):
        """Override to use our logger"""
        logger.info(format % args)


def run_mock_server(port=8080):
    """Run the mock ESP32 server"""
    server_address = ('', port)
    httpd = HTTPServer(server_address, MockESP32Handler)
    
    logger.info(f"Mock ESP32 server starting on port {port}")
    logger.info(f"Access at: http://localhost:{port}")
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        logger.info("Server stopping...")
        httpd.shutdown()


if __name__ == '__main__':
    run_mock_server()