"""
Real User Interaction Tests using Playwright.

Tests ESP32-S3 web interface with real browser automation that closely mimics
how actual users would interact with the system. Designed to catch issues
before users encounter them.
"""

import pytest
import asyncio
import time
import json
from pathlib import Path
from typing import Dict, List, Any
from playwright.async_api import async_playwright, Page, Browser, BrowserContext
from playwright.sync_api import sync_playwright, Page as SyncPage, Browser as SyncBrowser


class TestRealUserInteractions:
    """Test real user interactions with the ESP32 web interface."""
    
    @pytest.fixture(scope="session")
    def browser_config(self):
        """Browser configuration for testing."""
        return {
            "headless": True,  # Set to False for debugging
            "viewport": {"width": 1920, "height": 1080},
            "slow_mo": 100,  # Slow down actions to be more human-like
            "record_video_dir": "test-results/videos",
            "record_har_path": "test-results/network.har"
        }
    
    @pytest.fixture
    def esp32_base_url(self):
        """Base URL for ESP32 web interface."""
        return "http://192.168.4.1"  # Default ESP32 AP mode IP
    
    @pytest.fixture
    def mock_esp32_server(self):
        """Mock ESP32 server for testing without physical hardware."""
        # This would be replaced with actual ESP32 server setup
        # For now, we'll use a mock server that mimics ESP32 behavior
        return {
            "url": "http://localhost:8080",
            "endpoints": {
                "/": "main_dashboard",
                "/config": "device_config",
                "/data": "data_viewer",
                "/wifi": "wifi_setup",
                "/api/status": "api_status",
                "/api/data": "api_data",
                "/upload": "firmware_upload"
            }
        }
    
    @pytest.mark.user_interaction
    @pytest.mark.asyncio
    async def test_user_dashboard_navigation_flow(self, mock_esp32_server, browser_config):
        """Test complete user navigation flow through dashboard."""
        async with async_playwright() as p:
            # Launch browser with realistic settings
            browser = await p.chromium.launch(
                headless=browser_config["headless"],
                slow_mo=browser_config["slow_mo"]
            )
            
            context = await browser.new_context(
                viewport=browser_config["viewport"],
                record_video_dir=browser_config.get("record_video_dir"),
                record_har_path=browser_config.get("record_har_path")
            )
            
            page = await context.new_page()
            
            try:
                # Simulate user opening the ESP32 interface
                await page.goto(mock_esp32_server["url"])
                
                # Wait for page to load like a real user would
                await page.wait_for_load_state("networkidle")
                
                # Verify page title appears as user would see it
                title = await page.title()
                assert "BMCU370" in title, f"Expected BMCU370 in title, got: {title}"
                
                # Test user reading and understanding the interface
                # Check that key elements are visible and accessible
                await page.wait_for_selector('[data-testid="main-dashboard"]', timeout=5000)
                
                # Test user looking for navigation - check menu visibility
                nav_menu = page.locator('[role="navigation"], nav, .navigation')
                await nav_menu.wait_for(timeout=3000)
                
                # Simulate user clicking through different sections
                navigation_items = [
                    {"selector": 'a[href="/config"], [data-nav="config"]', "expected_url": "/config"},
                    {"selector": 'a[href="/data"], [data-nav="data"]', "expected_url": "/data"},
                    {"selector": 'a[href="/wifi"], [data-nav="wifi"]', "expected_url": "/wifi"},
                    {"selector": 'a[href="/"], [data-nav="home"]', "expected_url": "/"}
                ]
                
                for nav_item in navigation_items:
                    # User clicks navigation item
                    nav_element = page.locator(nav_item["selector"]).first
                    if await nav_element.count() > 0:
                        await nav_element.click()
                        
                        # User waits for page to load
                        await page.wait_for_load_state("networkidle")
                        
                        # User checks they're on the right page
                        current_url = page.url
                        assert nav_item["expected_url"] in current_url, \
                            f"Expected {nav_item['expected_url']} in URL, got: {current_url}"
                        
                        # User briefly reviews the page content
                        await page.wait_for_timeout(500)  # Human pause
                
                # Test user interacting with real-time data
                await page.goto(f"{mock_esp32_server['url']}/data")
                
                # User waits for data to appear
                await page.wait_for_selector('[data-testid="sensor-data"], .sensor-reading, .data-display')
                
                # User observes data updates (simulate real-time monitoring)
                initial_readings = await self._capture_sensor_readings(page)
                
                # User waits to see if data updates
                await page.wait_for_timeout(2000)
                updated_readings = await self._capture_sensor_readings(page)
                
                # Verify data is live (either different values or timestamps)
                assert initial_readings != updated_readings or \
                       self._timestamps_differ(initial_readings, updated_readings), \
                       "Real-time data should update over time"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.asyncio
    async def test_user_wifi_configuration_workflow(self, mock_esp32_server, browser_config):
        """Test complete WiFi configuration workflow as a user would do it."""
        async with async_playwright() as p:
            browser = await p.firefox.launch(headless=browser_config["headless"])
            context = await browser.new_context(viewport=browser_config["viewport"])
            page = await context.new_page()
            
            try:
                # User navigates to WiFi setup
                await page.goto(f"{mock_esp32_server['url']}/wifi")
                await page.wait_for_load_state("networkidle")
                
                # User looks for WiFi configuration form
                wifi_form = page.locator('form[data-form="wifi"], #wifi-form, .wifi-config-form')
                await wifi_form.wait_for(timeout=5000)
                
                # User fills out WiFi settings step by step
                
                # 1. User enters network name
                ssid_field = page.locator('input[name="ssid"], input[id="ssid"], input[placeholder*="network"]')
                await ssid_field.fill("TestNetwork_2.4GHz")
                await page.wait_for_timeout(300)  # User pause after typing
                
                # 2. User enters password
                password_field = page.locator('input[name="password"], input[type="password"]')
                await password_field.fill("SecurePassword123!")
                await page.wait_for_timeout(300)
                
                # 3. User selects security type
                security_select = page.locator('select[name="security"], select[id="security"]')
                if await security_select.count() > 0:
                    await security_select.select_option("WPA2")
                    await page.wait_for_timeout(200)
                
                # 4. User might configure advanced settings
                advanced_toggle = page.locator('[data-toggle="advanced"], .advanced-toggle')
                if await advanced_toggle.count() > 0:
                    await advanced_toggle.click()
                    await page.wait_for_timeout(500)
                    
                    # User sets static IP if available
                    static_ip_checkbox = page.locator('input[name="static_ip"], input[id="staticIp"]')
                    if await static_ip_checkbox.count() > 0:
                        await static_ip_checkbox.check()
                        
                        ip_field = page.locator('input[name="ip_address"], input[id="ipAddress"]')
                        await ip_field.fill("192.168.1.100")
                        await page.wait_for_timeout(200)
                        
                        gateway_field = page.locator('input[name="gateway"], input[id="gateway"]')
                        await gateway_field.fill("192.168.1.1")
                        await page.wait_for_timeout(200)
                
                # 5. User submits the form
                submit_button = page.locator('button[type="submit"], input[type="submit"], .submit-btn')
                await submit_button.click()
                
                # 6. User waits for response and checks for feedback
                # Look for success message or loading indicator
                success_indicator = page.locator('.success-message, .alert-success, [data-status="success"]')
                loading_indicator = page.locator('.loading, .spinner, [data-status="loading"]')
                error_indicator = page.locator('.error-message, .alert-error, [data-status="error"]')
                
                # Wait for some kind of response
                await page.wait_for_function(
                    """() => {
                        return document.querySelector('.success-message, .alert-success, [data-status="success"], .error-message, .alert-error, [data-status="error"]') !== null;
                    }""",
                    timeout=10000
                )
                
                # User checks the result
                if await success_indicator.count() > 0:
                    success_text = await success_indicator.text_content()
                    assert len(success_text.strip()) > 0, "Success message should have content"
                elif await error_indicator.count() > 0:
                    # User sees error and might retry
                    error_text = await error_indicator.text_content()
                    # Verify error is descriptive for user
                    assert len(error_text.strip()) > 0, "Error message should be descriptive"
                    assert any(word in error_text.lower() for word in ["password", "network", "connection", "failed"]), \
                        "Error message should be user-friendly"
                
                # 7. User verifies connection status
                status_display = page.locator('.connection-status, [data-display="status"]')
                if await status_display.count() > 0:
                    status_text = await status_display.text_content()
                    # Status should be meaningful to user
                    assert any(word in status_text.lower() for word in ["connected", "connecting", "failed", "disconnected"]), \
                        f"Connection status should be clear: {status_text}"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.asyncio
    async def test_user_data_monitoring_experience(self, mock_esp32_server, browser_config):
        """Test user experience when monitoring sensor data."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=browser_config["headless"])
            context = await browser.new_context(viewport=browser_config["viewport"])
            page = await context.new_page()
            
            try:
                # User navigates to data monitoring page
                await page.goto(f"{mock_esp32_server['url']}/data")
                await page.wait_for_load_state("networkidle")
                
                # User looks for current sensor readings
                sensor_displays = page.locator('.sensor-reading, [data-sensor], .data-value')
                await sensor_displays.first.wait_for(timeout=5000)
                
                # User checks each sensor type they expect
                expected_sensors = ["temperature", "humidity", "pressure"]
                
                for sensor_type in expected_sensors:
                    sensor_element = page.locator(f'[data-sensor="{sensor_type}"], [class*="{sensor_type}"]')
                    
                    if await sensor_element.count() > 0:
                        # User reads the current value
                        sensor_value = await sensor_element.text_content()
                        
                        # Verify value is meaningful to user
                        assert len(sensor_value.strip()) > 0, f"{sensor_type} should show a value"
                        
                        # Check for reasonable units/formatting
                        if sensor_type == "temperature":
                            assert any(unit in sensor_value for unit in ["°C", "°F", "C", "F"]), \
                                f"Temperature should have units: {sensor_value}"
                        elif sensor_type == "humidity":
                            assert "%" in sensor_value or "rh" in sensor_value.lower(), \
                                f"Humidity should have units: {sensor_value}"
                        elif sensor_type == "pressure":
                            assert any(unit in sensor_value.lower() for unit in ["pa", "hpa", "bar", "psi"]), \
                                f"Pressure should have units: {sensor_value}"
                
                # User looks for historical data or charts
                chart_container = page.locator('canvas, .chart, [data-chart], .graph')
                if await chart_container.count() > 0:
                    # User waits for chart to render
                    await page.wait_for_timeout(1000)
                    
                    # User might interact with chart (zoom, hover)
                    chart_element = chart_container.first
                    chart_box = await chart_element.bounding_box()
                    
                    if chart_box:
                        # User hovers over chart to see tooltip
                        await page.mouse.move(
                            chart_box["x"] + chart_box["width"] * 0.7,
                            chart_box["y"] + chart_box["height"] * 0.5
                        )
                        await page.wait_for_timeout(500)
                        
                        # Check if tooltip appears
                        tooltip = page.locator('.tooltip, [data-tooltip], .chart-tooltip')
                        if await tooltip.count() > 0:
                            tooltip_text = await tooltip.text_content()
                            assert len(tooltip_text.strip()) > 0, "Chart tooltip should show data"
                
                # User tests real-time updates
                initial_timestamp = await self._get_data_timestamp(page)
                
                # User waits and expects to see updates
                for attempt in range(5):
                    await page.wait_for_timeout(2000)
                    current_timestamp = await self._get_data_timestamp(page)
                    
                    if current_timestamp != initial_timestamp:
                        break  # Data updated as expected
                else:
                    # Check if there's a connection status indicator
                    connection_status = page.locator('.connection-status, [data-connection], .ws-status')
                    if await connection_status.count() > 0:
                        status_text = await connection_status.text_content()
                        # If disconnected, that's a valid state to test
                        assert "disconnected" in status_text.lower() or "connected" in status_text.lower(), \
                            "Connection status should be clear to user"
                
                # User tries refresh if data seems stale
                await page.reload()
                await page.wait_for_load_state("networkidle")
                
                # User verifies data loads after refresh
                await sensor_displays.first.wait_for(timeout=5000)
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.asyncio
    async def test_user_firmware_update_process(self, mock_esp32_server, browser_config):
        """Test user firmware update workflow with file upload."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=browser_config["headless"])
            context = await browser.new_context(viewport=browser_config["viewport"])
            page = await context.new_page()
            
            try:
                # User navigates to firmware update page
                await page.goto(f"{mock_esp32_server['url']}/upload")
                await page.wait_for_load_state("networkidle")
                
                # User looks for upload form
                upload_form = page.locator('form[enctype="multipart/form-data"], .upload-form, [data-form="upload"]')
                await upload_form.wait_for(timeout=5000)
                
                # User reads instructions/warnings
                warning_text = page.locator('.warning, .alert-warning, .important-notice')
                if await warning_text.count() > 0:
                    warning_content = await warning_text.text_content()
                    # Warning should be clear and helpful
                    assert any(word in warning_content.lower() for word in ["backup", "power", "interrupt", "brick"]), \
                        "Firmware update warnings should mention important precautions"
                
                # User creates a test firmware file (simulate user selecting file)
                test_firmware_content = b"TEST_FIRMWARE_BINARY_DATA" + b"\x00" * 100000  # 100KB test file
                
                # Create temporary file for upload test
                import tempfile
                with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as temp_file:
                    temp_file.write(test_firmware_content)
                    temp_file_path = temp_file.name
                
                try:
                    # User selects firmware file
                    file_input = page.locator('input[type="file"]')
                    await file_input.set_input_files(temp_file_path)
                    
                    # User waits for file validation
                    await page.wait_for_timeout(1000)
                    
                    # Check for file validation feedback
                    file_info = page.locator('.file-info, [data-file-info], .selected-file')
                    if await file_info.count() > 0:
                        info_text = await file_info.text_content()
                        assert "bin" in info_text or "firmware" in info_text.lower(), \
                            "File info should show firmware file details"
                    
                    # User confirms they want to proceed
                    confirm_checkbox = page.locator('input[type="checkbox"][name*="confirm"], .confirm-update')
                    if await confirm_checkbox.count() > 0:
                        await confirm_checkbox.check()
                        await page.wait_for_timeout(300)
                    
                    # User starts upload
                    upload_button = page.locator('button[type="submit"], .upload-btn, [data-action="upload"]')
                    await upload_button.click()
                    
                    # User waits and monitors progress
                    progress_bar = page.locator('.progress-bar, progress, [data-progress]')
                    progress_text = page.locator('.progress-text, [data-progress-text]')
                    
                    # Wait for upload to start
                    await page.wait_for_timeout(500)
                    
                    # Monitor progress for reasonable time
                    upload_timeout = 30000  # 30 seconds max
                    start_time = time.time()
                    
                    while (time.time() - start_time) * 1000 < upload_timeout:
                        # Check for completion indicators
                        success_msg = page.locator('.success, .upload-success, [data-status="complete"]')
                        error_msg = page.locator('.error, .upload-error, [data-status="error"]')
                        
                        if await success_msg.count() > 0:
                            success_text = await success_msg.text_content()
                            assert "success" in success_text.lower() or "complete" in success_text.lower(), \
                                "Success message should be clear"
                            break
                        elif await error_msg.count() > 0:
                            error_text = await error_msg.text_content()
                            assert len(error_text.strip()) > 0, "Error message should be descriptive"
                            break
                        
                        # Check progress updates
                        if await progress_text.count() > 0:
                            progress_content = await progress_text.text_content()
                            # Progress should be informative
                            assert any(char in progress_content for char in ["%", "/"]) or \
                                   any(word in progress_content.lower() for word in ["uploading", "progress", "bytes"]), \
                                f"Progress text should be informative: {progress_content}"
                        
                        await page.wait_for_timeout(1000)
                    
                    # User sees final result
                    final_status = page.locator('.upload-result, [data-upload-result], .final-status')
                    if await final_status.count() > 0:
                        status_text = await final_status.text_content()
                        assert len(status_text.strip()) > 0, "Final status should inform user of result"
                
                finally:
                    # Clean up temporary file
                    import os
                    if os.path.exists(temp_file_path):
                        os.unlink(temp_file_path)
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.asyncio
    async def test_user_error_recovery_scenarios(self, mock_esp32_server, browser_config):
        """Test how users experience and recover from error scenarios."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=browser_config["headless"])
            context = await browser.new_context(viewport=browser_config["viewport"])
            page = await context.new_page()
            
            try:
                # Test 1: User experiences network disconnection
                await page.goto(mock_esp32_server["url"])
                await page.wait_for_load_state("networkidle")
                
                # Simulate network issues by navigating to invalid endpoint
                await page.goto(f"{mock_esp32_server['url']}/nonexistent-page")
                
                # User should see helpful error message
                error_indicators = [
                    page.locator('.error-page, .not-found, .error-message'),
                    page.locator('h1:has-text("404"), h1:has-text("Error"), h1:has-text("Not Found")'),
                    page.locator('[data-error], [data-status="error"]')
                ]
                
                error_found = False
                for indicator in error_indicators:
                    if await indicator.count() > 0:
                        error_text = await indicator.text_content()
                        assert len(error_text.strip()) > 0, "Error message should be visible"
                        error_found = True
                        break
                
                # If no custom error page, browser's default is acceptable
                if not error_found:
                    page_title = await page.title()
                    page_content = await page.content()
                    # Should indicate some kind of error state
                    assert "404" in page_title or "404" in page_content or \
                           "not found" in page_title.lower() or "error" in page_title.lower(), \
                           "Error state should be communicated to user"
                
                # Test 2: User tries to recover by going back
                await page.go_back()
                await page.wait_for_load_state("networkidle")
                
                # Should be back to working page
                current_url = page.url
                assert mock_esp32_server["url"] in current_url, "User should be able to navigate back"
                
                # Test 3: User encounters form validation errors
                await page.goto(f"{mock_esp32_server['url']}/wifi")
                await page.wait_for_load_state("networkidle")
                
                # User submits form with invalid data
                form = page.locator('form')
                if await form.count() > 0:
                    # Try to submit empty form
                    submit_btn = page.locator('button[type="submit"], input[type="submit"]')
                    if await submit_btn.count() > 0:
                        await submit_btn.click()
                        await page.wait_for_timeout(1000)
                        
                        # User should see validation feedback
                        validation_errors = page.locator('.error, .invalid, [aria-invalid="true"]')
                        required_fields = page.locator('input[required], select[required]')
                        
                        if await required_fields.count() > 0:
                            # Should have validation feedback for required fields
                            assert await validation_errors.count() > 0 or \
                                   await page.locator(':invalid').count() > 0, \
                                   "Form validation should provide user feedback"
                
                # Test 4: User experiences slow loading
                # Navigate to data page and check loading states
                await page.goto(f"{mock_esp32_server['url']}/data")
                
                # User should see loading indicators for slow content
                loading_indicators = page.locator('.loading, .spinner, .loading-indicator')
                
                # Even if no loading indicator, page should load eventually
                await page.wait_for_load_state("networkidle", timeout=10000)
                
                # Verify content eventually appears
                content_loaded = await page.wait_for_function(
                    """() => {
                        const bodyText = document.body.textContent;
                        return bodyText.length > 100; // Reasonable amount of content
                    }""",
                    timeout=5000
                )
                assert content_loaded, "Page should eventually load meaningful content"
                
            finally:
                await context.close()
                await browser.close()
    
    async def _capture_sensor_readings(self, page: Page) -> Dict[str, str]:
        """Capture current sensor readings from the page."""
        readings = {}
        
        # Common sensor data selectors
        sensor_selectors = [
            '[data-sensor]',
            '.sensor-reading',
            '.data-value',
            '.measurement'
        ]
        
        for selector in sensor_selectors:
            elements = page.locator(selector)
            count = await elements.count()
            
            for i in range(count):
                element = elements.nth(i)
                sensor_type = await element.get_attribute('data-sensor') or f"sensor_{i}"
                value = await element.text_content()
                readings[sensor_type] = value.strip() if value else ""
        
        return readings
    
    def _timestamps_differ(self, readings1: Dict, readings2: Dict) -> bool:
        """Check if timestamps in readings are different (indicating live data)."""
        # Look for timestamp patterns in the text
        import re
        timestamp_pattern = r'\d{2}:\d{2}:\d{2}|\d+:\d+|\d{4}-\d{2}-\d{2}'
        
        for key in readings1:
            if key in readings2:
                time1_matches = re.findall(timestamp_pattern, readings1[key])
                time2_matches = re.findall(timestamp_pattern, readings2[key])
                
                if time1_matches != time2_matches:
                    return True
        
        return False
    
    async def _get_data_timestamp(self, page: Page) -> str:
        """Get current data timestamp from the page."""
        timestamp_selectors = [
            '[data-timestamp]',
            '.timestamp',
            '.last-updated',
            '.data-time'
        ]
        
        for selector in timestamp_selectors:
            element = page.locator(selector)
            if await element.count() > 0:
                timestamp = await element.text_content()
                return timestamp.strip() if timestamp else ""
        
        # Fallback: get page content hash as timestamp indicator
        content = await page.content()
        import hashlib
        return hashlib.md5(content.encode()).hexdigest()[:8]