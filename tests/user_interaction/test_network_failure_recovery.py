"""
Network Failure and Recovery Testing using Playwright.

Tests how users experience network issues and recovery scenarios with the
ESP32-S3 web interface, ensuring graceful degradation and helpful feedback.
"""

import pytest
import asyncio
import time
from typing import Dict, List, Any
from playwright.async_api import async_playwright, Page, Browser, BrowserContext, Route


class TestNetworkFailureRecovery:
    """Test user experience during network failures and recovery."""
    
    @pytest.fixture
    def network_scenarios(self):
        """Different network failure scenarios to test."""
        return {
            "complete_offline": {
                "description": "Complete network disconnection",
                "handler": "abort_all_requests"
            },
            "slow_connection": {
                "description": "Very slow network (like poor WiFi)",
                "handler": "delay_requests",
                "delay_ms": 5000
            },
            "intermittent_failures": {
                "description": "Random request failures",
                "handler": "random_failures",
                "failure_rate": 0.3
            },
            "api_only_failure": {
                "description": "API endpoints fail but static content works",
                "handler": "api_failures"
            },
            "timeout_errors": {
                "description": "Requests timeout",
                "handler": "timeout_requests"
            }
        }
    
    @pytest.fixture
    def esp32_url(self):
        """Base URL for ESP32 web interface."""
        return "http://localhost:8080"
    
    @pytest.mark.user_interaction
    @pytest.mark.network_failure
    @pytest.mark.asyncio
    async def test_user_experience_during_complete_offline(self, esp32_url, network_scenarios):
        """Test user experience when completely offline."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                # User starts with working connection
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Verify page loads normally
                page_title = await page.title()
                assert "BMCU370" in page_title or len(page_title) > 0, "Page should load initially"
                
                # User loses internet connection
                await page.route("**/*", lambda route: route.abort())
                
                # User tries to navigate to another page
                data_link = page.locator('a[href*="/data"], [data-nav="data"]')
                if await data_link.count() > 0:
                    await data_link.first.click()
                    await page.wait_for_timeout(2000)  # Wait for error to show
                    
                    # User should see helpful offline message
                    offline_indicators = [
                        page.locator('.offline-message, .connection-error, .network-error'),
                        page.locator(':has-text("offline"), :has-text("connection"), :has-text("network")'),
                        page.locator('[data-status="offline"], [data-error="network"]')
                    ]
                    
                    offline_message_found = False
                    for indicator in offline_indicators:
                        if await indicator.count() > 0:
                            message_text = await indicator.first.text_content()
                            if message_text and len(message_text.strip()) > 0:
                                offline_message_found = True
                                # Message should be helpful to user
                                helpful_words = ['connection', 'offline', 'network', 'internet', 'retry', 'check']
                                assert any(word in message_text.lower() for word in helpful_words), \
                                    f"Offline message should be helpful: {message_text}"
                                break
                    
                    # If no custom offline message, browser's default error is acceptable
                    if not offline_message_found:
                        # Check if browser shows connection error
                        page_content = await page.content()
                        assert any(word in page_content.lower() for word in 
                                 ['connection', 'network', 'offline', 'failed', 'error']), \
                               "Should indicate connection problem to user"
                
                # User tries to refresh page
                await page.reload()
                await page.wait_for_timeout(2000)
                
                # Should show appropriate offline state
                current_url = page.url
                # Page might show browser error or stay on cached version
                assert "error" in current_url.lower() or "about:blank" in current_url or \
                       current_url == esp32_url, "Page should handle refresh gracefully"
                
                # User connection comes back
                await page.unroute("**/*")
                
                # User tries to navigate again
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Should work normally again
                recovered_title = await page.title()
                assert len(recovered_title) > 0, "Page should work after network recovery"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.network_failure
    @pytest.mark.asyncio
    async def test_user_experience_with_slow_connection(self, esp32_url):
        """Test user experience with very slow network connection."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                # Simulate slow connection
                await page.route("**/*", lambda route: self._slow_request_handler(route, 3000))
                
                # User tries to load page with slow connection
                start_time = time.time()
                await page.goto(esp32_url, wait_until="domcontentloaded", timeout=10000)
                load_time = (time.time() - start_time) * 1000
                
                # User should see loading indicators
                loading_indicators = [
                    page.locator('.loading, .spinner, .loading-indicator'),
                    page.locator('[data-loading], [data-status="loading"]'),
                    page.locator(':has-text("loading"), :has-text("Loading")')
                ]
                
                loading_shown = False
                for indicator in loading_indicators:
                    if await indicator.count() > 0 and await indicator.first.is_visible():
                        loading_shown = True
                        break
                
                # If page takes a while to load, user should get feedback
                if load_time > 2000:
                    # Either loading indicator or quick initial content
                    has_initial_content = await page.locator('body').text_content()
                    assert loading_shown or len(has_initial_content.strip()) > 50, \
                        "User should see loading feedback or initial content on slow connections"
                
                # User tries to interact before page fully loads
                await page.wait_for_timeout(1000)  # Partial load state
                
                # Interactive elements should either work or show they're not ready
                buttons = page.locator('button, [role="button"]')
                if await buttons.count() > 0:
                    first_button = buttons.first
                    
                    try:
                        await first_button.click(timeout=1000)
                        # If click works, that's fine
                    except:
                        # If click fails, button should indicate it's not ready
                        is_disabled = await first_button.is_disabled()
                        has_loading_state = await first_button.get_attribute('aria-busy') == 'true'
                        
                        assert is_disabled or has_loading_state, \
                            "Buttons should indicate when not ready during slow loading"
                
                # Wait for full load and test functionality
                await page.wait_for_load_state("networkidle", timeout=15000)
                
                # Page should eventually work normally
                final_title = await page.title()
                assert len(final_title) > 0, "Page should eventually load completely"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.network_failure
    @pytest.mark.asyncio
    async def test_user_experience_with_intermittent_failures(self, esp32_url):
        """Test user experience with intermittent network failures."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                # Setup intermittent failures (30% failure rate)
                self.failure_count = 0
                await page.route("**/*", lambda route: self._intermittent_failure_handler(route, 0.3))
                
                # User navigates to main page
                await page.goto(esp32_url)
                await page.wait_for_load_state("domcontentloaded")
                
                # User tries to navigate to data page multiple times
                for attempt in range(3):
                    data_link = page.locator('a[href*="/data"], [data-nav="data"]')
                    if await data_link.count() > 0:
                        await data_link.first.click()
                        await page.wait_for_timeout(2000)
                        
                        # Check if navigation succeeded or failed gracefully
                        current_url = page.url
                        
                        if "/data" in current_url:
                            # Success - verify page works
                            await self._verify_data_page_functionality(page)
                            break
                        else:
                            # Failure - user should see retry option
                            retry_elements = [
                                page.locator('.retry-btn, [data-action="retry"]'),
                                page.locator(':has-text("retry"), :has-text("Retry")'),
                                page.locator('button:has-text("Try Again")')
                            ]
                            
                            retry_found = False
                            for retry_element in retry_elements:
                                if await retry_element.count() > 0:
                                    retry_found = True
                                    await retry_element.first.click()
                                    await page.wait_for_timeout(1000)
                                    break
                            
                            if not retry_found:
                                # User manually retries by clicking link again
                                continue
                
                # Test form submission with intermittent failures
                await page.goto(f"{esp32_url}/wifi")
                await page.wait_for_load_state("domcontentloaded")
                
                # User fills form
                ssid_input = page.locator('input[name="ssid"], input[id="ssid"]')
                if await ssid_input.count() > 0:
                    await ssid_input.fill("TestNetwork")
                    
                    # User submits form
                    submit_btn = page.locator('button[type="submit"], input[type="submit"]')
                    if await submit_btn.count() > 0:
                        await submit_btn.first.click()
                        await page.wait_for_timeout(3000)
                        
                        # Check for success or helpful error message
                        success_msg = page.locator('.success, .success-message, [data-status="success"]')
                        error_msg = page.locator('.error, .error-message, [data-status="error"]')
                        
                        if await success_msg.count() > 0:
                            success_text = await success_msg.first.text_content()
                            assert len(success_text.strip()) > 0, "Success message should be meaningful"
                        elif await error_msg.count() > 0:
                            error_text = await error_msg.first.text_content()
                            assert "try again" in error_text.lower() or "retry" in error_text.lower(), \
                                "Error message should suggest retry for intermittent failures"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.network_failure
    @pytest.mark.asyncio
    async def test_api_failure_graceful_degradation(self, esp32_url):
        """Test graceful degradation when API endpoints fail but static content works."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                # Allow static content but fail API calls
                await page.route("**/api/**", lambda route: route.abort())
                await page.route("**/*.json", lambda route: route.abort())
                
                # User loads main page (static content should work)
                await page.goto(esp32_url)
                await page.wait_for_load_state("domcontentloaded")
                
                # Page structure should load even without API data
                page_title = await page.title()
                assert len(page_title) > 0, "Static page content should load"
                
                # User navigates to data page
                await page.goto(f"{esp32_url}/data")
                await page.wait_for_load_state("domcontentloaded")
                
                # Page should show but indicate data unavailable
                await page.wait_for_timeout(2000)  # Wait for API calls to fail
                
                # Look for indicators that data is unavailable
                data_unavailable_indicators = [
                    page.locator('.no-data, .data-unavailable, .api-error'),
                    page.locator(':has-text("unavailable"), :has-text("No data"), :has-text("error")'),
                    page.locator('[data-status="error"], [data-error="api"]')
                ]
                
                data_error_shown = False
                for indicator in data_unavailable_indicators:
                    if await indicator.count() > 0:
                        indicator_text = await indicator.first.text_content()
                        if indicator_text and len(indicator_text.strip()) > 0:
                            data_error_shown = True
                            # Message should be helpful to user
                            helpful_words = ['unavailable', 'error', 'problem', 'retry', 'refresh']
                            assert any(word in indicator_text.lower() for word in helpful_words), \
                                f"Data error message should be helpful: {indicator_text}"
                            break
                
                # Even if no specific error message, page should not show stale/wrong data
                sensor_displays = page.locator('.sensor-reading, [data-sensor], .data-value')
                if await sensor_displays.count() > 0:
                    for i in range(min(3, await sensor_displays.count())):
                        display = sensor_displays.nth(i)
                        display_text = await display.text_content()
                        
                        # Should not show outdated timestamps or confusing data
                        if display_text:
                            assert not any(word in display_text.lower() for word in 
                                         ['undefined', 'null', 'nan', 'error']), \
                                   f"Should not show technical errors to user: {display_text}"
                
                # User tries to refresh data
                refresh_button = page.locator('.refresh-btn, [data-action="refresh"]')
                if await refresh_button.count() > 0:
                    await refresh_button.first.click()
                    await page.wait_for_timeout(1000)
                    
                    # Should indicate refresh attempt failed
                    assert data_error_shown or await page.locator('.error, .failed').count() > 0, \
                        "Should indicate when refresh fails"
                
                # Test form submission still works for static forms
                await page.goto(f"{esp32_url}/config")
                await page.wait_for_load_state("domcontentloaded")
                
                # Non-API forms should still be usable
                config_inputs = page.locator('input[type="text"], input[type="number"]')
                if await config_inputs.count() > 0:
                    # Form structure should be available
                    first_input = config_inputs.first
                    await first_input.fill("test value")
                    
                    input_value = await first_input.input_value()
                    assert input_value == "test value", "Form inputs should work without API"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.network_failure
    @pytest.mark.asyncio
    async def test_websocket_connection_failure_recovery(self, esp32_url):
        """Test WebSocket connection failure and recovery from user perspective."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                # User navigates to real-time data page
                await page.goto(f"{esp32_url}/data")
                await page.wait_for_load_state("networkidle")
                
                # Wait for potential WebSocket connection
                await page.wait_for_timeout(2000)
                
                # Check initial connection status
                connection_status = await self._check_websocket_status(page)
                
                # Simulate WebSocket failure
                await page.evaluate("""
                    () => {
                        // Close any existing WebSocket connections
                        if (window.websocket) {
                            window.websocket.close();
                        }
                        
                        // Override WebSocket to simulate connection failures
                        const OriginalWebSocket = window.WebSocket;
                        window.WebSocket = function(...args) {
                            const ws = new OriginalWebSocket(...args);
                            setTimeout(() => ws.close(), 100);  // Force disconnect
                            return ws;
                        };
                    }
                """)
                
                # User should see connection status change
                await page.wait_for_timeout(1000)
                
                # Look for connection status indicators
                connection_indicators = [
                    page.locator('.connection-status, .ws-status, [data-connection]'),
                    page.locator(':has-text("connected"), :has-text("disconnected")'),
                    page.locator('.status-indicator, [data-status]')
                ]
                
                connection_status_shown = False
                for indicator in connection_indicators:
                    if await indicator.count() > 0:
                        status_text = await indicator.first.text_content()
                        if status_text and len(status_text.strip()) > 0:
                            connection_status_shown = True
                            # Status should be clear to user
                            status_words = ['connected', 'disconnected', 'online', 'offline', 'live', 'stopped']
                            assert any(word in status_text.lower() for word in status_words), \
                                f"Connection status should be clear: {status_text}"
                            break
                
                # User should be informed about real-time data status
                if connection_status_shown:
                    # Look for reconnection attempts or manual refresh options
                    reconnect_options = [
                        page.locator('.reconnect-btn, [data-action="reconnect"]'),
                        page.locator(':has-text("reconnect"), :has-text("refresh")'),
                        page.locator('button:has-text("Connect")')
                    ]
                    
                    for option in reconnect_options:
                        if await option.count() > 0:
                            # User tries to reconnect
                            await option.first.click()
                            await page.wait_for_timeout(1000)
                            break
                
                # Test automatic reconnection behavior
                await page.evaluate("""
                    () => {
                        // Restore normal WebSocket behavior
                        delete window.WebSocket;
                    }
                """)
                
                # Wait for potential automatic reconnection
                await page.wait_for_timeout(3000)
                
                # Check if connection recovers
                recovered_status = await self._check_websocket_status(page)
                
                # User should see recovery or clear indication of status
                final_indicators = page.locator('.connection-status, .ws-status, [data-connection]')
                if await final_indicators.count() > 0:
                    final_status_text = await final_indicators.first.text_content()
                    # Should show current status clearly
                    assert len(final_status_text.strip()) > 0, "Connection status should be visible"
                
            finally:
                await context.close()
                await browser.close()
    
    async def _slow_request_handler(self, route: Route, delay_ms: int):
        """Handle requests with artificial delay."""
        await asyncio.sleep(delay_ms / 1000)
        await route.continue_()
    
    async def _intermittent_failure_handler(self, route: Route, failure_rate: float):
        """Handle requests with random failures."""
        import random
        
        if random.random() < failure_rate:
            self.failure_count += 1
            await route.abort()
        else:
            await route.continue_()
    
    async def _verify_data_page_functionality(self, page: Page):
        """Verify data page works correctly."""
        # Check for data displays
        data_elements = page.locator('.sensor-data, [data-sensor], .data-display')
        if await data_elements.count() > 0:
            # Should have some data content
            first_element = data_elements.first
            element_text = await first_element.text_content()
            assert len(element_text.strip()) > 0, "Data page should show content"
    
    async def _check_websocket_status(self, page: Page) -> Dict[str, Any]:
        """Check WebSocket connection status."""
        status = await page.evaluate("""
            () => {
                // Check for WebSocket instance
                if (window.websocket) {
                    return {
                        exists: true,
                        ready_state: window.websocket.readyState,
                        url: window.websocket.url
                    };
                }
                
                // Check for connection status in DOM
                const statusElement = document.querySelector('.connection-status, .ws-status, [data-connection]');
                if (statusElement) {
                    return {
                        exists: false,
                        dom_status: statusElement.textContent.trim()
                    };
                }
                
                return { exists: false };
            }
        """)
        
        return status