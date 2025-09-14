"""
User-Focused Performance Testing using Playwright.

Tests ESP32-S3 web interface performance from the user's perspective,
measuring real user experience metrics like load times, responsiveness,
and perceived performance rather than just technical metrics.
"""

import pytest
import asyncio
import time
import statistics
from typing import Dict, List, Any, Optional
from playwright.async_api import async_playwright, Page, Browser, BrowserContext


class TestUserPerformanceExperience:
    """Test performance from real user experience perspective."""
    
    @pytest.fixture
    def performance_thresholds(self):
        """Performance thresholds based on user experience research."""
        return {
            "page_load": {
                "excellent": 1000,      # < 1s feels instant
                "good": 2500,          # < 2.5s feels fast
                "acceptable": 5000,    # < 5s is tolerable
                "poor": 10000         # > 10s feels broken
            },
            "interaction": {
                "instant": 100,        # < 100ms feels instant
                "fast": 300,          # < 300ms feels responsive
                "noticeable": 1000,   # < 1s is acceptable
                "sluggish": 3000      # > 3s feels broken
            },
            "visual_feedback": {
                "immediate": 16,       # One frame at 60fps
                "responsive": 100,     # User notices but acceptable
                "delayed": 300        # User definitely notices delay
            }
        }
    
    @pytest.fixture
    def esp32_url(self):
        """Base URL for ESP32 web interface."""
        return "http://localhost:8080"
    
    @pytest.mark.user_interaction
    @pytest.mark.performance
    @pytest.mark.asyncio
    async def test_user_perceived_page_load_speed(self, esp32_url, performance_thresholds):
        """Test page load speed from user's perspective."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                # Test multiple page loads to get consistent results
                load_times = []
                first_paint_times = []
                content_load_times = []
                
                pages_to_test = [
                    {"url": esp32_url, "name": "dashboard"},
                    {"url": f"{esp32_url}/data", "name": "data_page"},
                    {"url": f"{esp32_url}/wifi", "name": "wifi_config"},
                    {"url": f"{esp32_url}/config", "name": "device_config"}
                ]
                
                for page_info in pages_to_test:
                    # Measure multiple loads for consistency
                    page_load_times = []
                    
                    for run in range(3):  # 3 runs per page
                        # Clear cache between runs to simulate new user
                        if run > 0:
                            await context.clear_cookies()
                            await page.evaluate("() => { localStorage.clear(); sessionStorage.clear(); }")
                        
                        # Measure user-perceived load time
                        start_time = time.time()
                        
                        await page.goto(page_info["url"])
                        
                        # Wait for First Contentful Paint (when user sees something)
                        await page.wait_for_function("""
                            () => {
                                return performance.getEntriesByType('paint')
                                    .some(entry => entry.name === 'first-contentful-paint');
                            }
                        """, timeout=10000)
                        
                        fcp_time = time.time()
                        first_paint_duration = (fcp_time - start_time) * 1000
                        
                        # Wait for meaningful content (user can start reading/interacting)
                        await page.wait_for_load_state("domcontentloaded")
                        
                        content_time = time.time()
                        content_duration = (content_time - start_time) * 1000
                        
                        # Wait for full interactivity
                        await page.wait_for_load_state("networkidle")
                        
                        full_load_time = time.time()
                        total_duration = (full_load_time - start_time) * 1000
                        
                        page_load_times.append({
                            "first_paint": first_paint_duration,
                            "content_ready": content_duration,
                            "fully_loaded": total_duration
                        })
                        
                        # Verify user can actually interact with the page
                        await self._verify_page_interactivity(page, page_info["name"])
                    
                    # Analyze results for this page
                    avg_first_paint = statistics.mean([t["first_paint"] for t in page_load_times])
                    avg_content = statistics.mean([t["content_ready"] for t in page_load_times])
                    avg_full_load = statistics.mean([t["fully_loaded"] for t in page_load_times])
                    
                    # Assert performance meets user expectations
                    thresholds = performance_thresholds["page_load"]
                    
                    # First paint should be fast (user sees something quickly)
                    assert avg_first_paint < thresholds["good"], \
                        f"{page_info['name']} first paint too slow: {avg_first_paint:.0f}ms"
                    
                    # Content should be readable quickly
                    assert avg_content < thresholds["acceptable"], \
                        f"{page_info['name']} content load too slow: {avg_content:.0f}ms"
                    
                    # Full interactivity should be reasonable
                    assert avg_full_load < thresholds["poor"], \
                        f"{page_info['name']} full load too slow: {avg_full_load:.0f}ms"
                    
                    print(f"{page_info['name']}: FCP={avg_first_paint:.0f}ms, Content={avg_content:.0f}ms, Full={avg_full_load:.0f}ms")
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.performance
    @pytest.mark.asyncio
    async def test_user_interaction_responsiveness(self, esp32_url, performance_thresholds):
        """Test how responsive the interface feels to user interactions."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Test button click responsiveness
                await self._test_button_responsiveness(page, performance_thresholds)
                
                # Test form input responsiveness
                await self._test_form_input_responsiveness(page, performance_thresholds)
                
                # Test navigation responsiveness
                await self._test_navigation_responsiveness(page, performance_thresholds)
                
                # Test scroll responsiveness
                await self._test_scroll_responsiveness(page, performance_thresholds)
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.performance
    @pytest.mark.asyncio
    async def test_real_time_data_performance(self, esp32_url, performance_thresholds):
        """Test real-time data updates from user experience perspective."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                await page.goto(f"{esp32_url}/data")
                await page.wait_for_load_state("networkidle")
                
                # Monitor data update performance
                update_times = []
                frame_drops = 0
                
                # Observe data updates for 10 seconds
                observation_start = time.time()
                last_update_time = observation_start
                
                while (time.time() - observation_start) < 10:
                    # Check for data updates
                    current_data = await self._capture_current_data(page)
                    
                    # Measure time between updates
                    current_time = time.time()
                    time_since_last = (current_time - last_update_time) * 1000
                    
                    # Check if visual updates are smooth
                    frame_rate = await page.evaluate("""
                        () => {
                            return new Promise(resolve => {
                                let frames = 0;
                                const start = performance.now();
                                
                                function countFrames() {
                                    frames++;
                                    if (performance.now() - start < 100) {  // Count for 100ms
                                        requestAnimationFrame(countFrames);
                                    } else {
                                        resolve(frames * 10);  // Convert to FPS
                                    }
                                }
                                
                                requestAnimationFrame(countFrames);
                            });
                        }
                    """)
                    
                    # User notices if frame rate drops below 30fps
                    if frame_rate < 30:
                        frame_drops += 1
                    
                    update_times.append(time_since_last)
                    last_update_time = current_time
                    
                    await page.wait_for_timeout(200)  # Check every 200ms
                
                # Analyze real-time performance
                if update_times:
                    avg_update_interval = statistics.mean(update_times)
                    max_update_delay = max(update_times)
                    
                    # Updates should feel real-time to user
                    assert avg_update_interval < 2000, \
                        f"Data updates too slow for real-time feel: {avg_update_interval:.0f}ms"
                    
                    # No single update should cause noticeable lag
                    assert max_update_delay < 5000, \
                        f"Maximum update delay too long: {max_update_delay:.0f}ms"
                
                # Visual performance should be smooth
                frame_drop_rate = frame_drops / (10 / 0.2)  # 10 seconds / 200ms intervals
                assert frame_drop_rate < 0.1, \
                    f"Too many frame drops for smooth user experience: {frame_drop_rate:.1%}"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.performance
    @pytest.mark.asyncio
    async def test_memory_performance_impact_on_user(self, esp32_url):
        """Test how memory usage affects user experience over time."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Measure initial memory usage
                initial_memory = await self._get_memory_usage(page)
                
                # Simulate extended user session (30 minutes of activity)
                session_duration = 30  # seconds (representing 30 minutes)
                activities_per_second = 2
                
                for second in range(session_duration):
                    # Simulate typical user activities
                    await self._simulate_user_activity(page)
                    
                    # Check memory usage periodically
                    if second % 5 == 0:  # Every 5 seconds
                        current_memory = await self._get_memory_usage(page)
                        
                        # Check if memory growth affects performance
                        performance_score = await self._measure_ui_responsiveness(page)
                        
                        # User experience should not degrade over time
                        assert performance_score > 0.7, \
                            f"UI responsiveness degraded over time: {performance_score:.2f}"
                        
                        # Memory should not grow excessively
                        memory_growth = current_memory - initial_memory
                        if memory_growth > 50:  # 50MB growth
                            # Check if it's actually affecting user experience
                            interaction_delay = await self._measure_click_response_time(page)
                            assert interaction_delay < 500, \
                                f"Memory growth affecting user interaction: {interaction_delay:.0f}ms"
                    
                    await page.wait_for_timeout(1000 // activities_per_second)
                
                # Final memory check
                final_memory = await self._get_memory_usage(page)
                total_growth = final_memory - initial_memory
                
                # Memory growth should be reasonable for extended use
                assert total_growth < 100, \
                    f"Excessive memory growth during session: {total_growth:.1f}MB"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.performance
    @pytest.mark.asyncio
    async def test_slow_device_performance(self, esp32_url, performance_thresholds):
        """Test performance on slower devices (low-end mobile/tablet)."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            
            # Simulate slower device with limited CPU
            context = await browser.new_context(
                viewport={"width": 375, "height": 667},  # Mobile viewport
                device_scale_factor=2,
                is_mobile=True
            )
            page = await context.new_page()
            
            # Throttle CPU to simulate slower device
            await page.evaluate("""
                () => {
                    // Simulate slower CPU by adding delays to heavy operations
                    const originalSetTimeout = window.setTimeout;
                    window.setTimeout = function(callback, delay) {
                        return originalSetTimeout(() => {
                            // Add artificial delay to simulate slower processing
                            const start = performance.now();
                            while (performance.now() - start < 10) {
                                // Busy wait for 10ms to simulate slow CPU
                            }
                            callback();
                        }, delay);
                    };
                }
            """)
            
            try:
                # Test page load on slow device
                start_time = time.time()
                await page.goto(esp32_url)
                await page.wait_for_load_state("domcontentloaded")
                load_time = (time.time() - start_time) * 1000
                
                # Should still be usable on slow devices
                thresholds = performance_thresholds["page_load"]
                assert load_time < thresholds["poor"], \
                    f"Page unusable on slow device: {load_time:.0f}ms"
                
                # Test interaction responsiveness on slow device
                buttons = page.locator('button, [role="button"]')
                if await buttons.count() > 0:
                    click_start = time.time()
                    await buttons.first.click()
                    click_end = time.time()
                    click_response_time = (click_end - click_start) * 1000
                    
                    # Should remain responsive even on slow devices
                    interaction_thresholds = performance_thresholds["interaction"]
                    assert click_response_time < interaction_thresholds["sluggish"], \
                        f"Interactions too slow on slow device: {click_response_time:.0f}ms"
                
                # Test form filling on slow device
                await page.goto(f"{esp32_url}/wifi")
                await page.wait_for_load_state("domcontentloaded")
                
                text_inputs = page.locator('input[type="text"], input[type="password"]')
                if await text_inputs.count() > 0:
                    input_field = text_inputs.first
                    
                    # Test typing responsiveness
                    typing_start = time.time()
                    await input_field.type("TestInput", delay=50)  # Human-like typing
                    typing_end = time.time()
                    
                    # Verify input appears correctly
                    input_value = await input_field.input_value()
                    assert input_value == "TestInput", "Input should work correctly on slow devices"
                    
                    # Typing should not feel laggy
                    typing_duration = (typing_end - typing_start) * 1000
                    expected_duration = len("TestInput") * 50 + 200  # Typing delay + processing time
                    assert typing_duration < expected_duration * 2, \
                        f"Typing too laggy on slow device: {typing_duration:.0f}ms"
                
            finally:
                await context.close()
                await browser.close()
    
    async def _verify_page_interactivity(self, page: Page, page_name: str):
        """Verify page is actually interactive for users."""
        # Check if interactive elements are functional
        interactive_elements = page.locator('button, a, input, select')
        element_count = await interactive_elements.count()
        
        if element_count > 0:
            # Test first interactive element
            first_element = interactive_elements.first
            
            # Element should be visible and enabled
            is_visible = await first_element.is_visible()
            is_enabled = await first_element.is_enabled()
            
            assert is_visible, f"Interactive elements should be visible on {page_name}"
            assert is_enabled, f"Interactive elements should be enabled on {page_name}"
    
    async def _test_button_responsiveness(self, page: Page, thresholds: Dict):
        """Test button click responsiveness."""
        buttons = page.locator('button, [role="button"]')
        button_count = await buttons.count()
        
        if button_count > 0:
            # Test multiple buttons for consistency
            for i in range(min(3, button_count)):
                button = buttons.nth(i)
                
                if await button.is_visible() and await button.is_enabled():
                    # Measure click response time
                    start_time = time.time()
                    await button.click()
                    
                    # Wait for visual feedback (animation, state change, etc.)
                    await page.wait_for_timeout(50)
                    
                    response_time = (time.time() - start_time) * 1000
                    
                    # Should feel responsive to user
                    assert response_time < thresholds["interaction"]["noticeable"], \
                        f"Button {i} response too slow: {response_time:.0f}ms"
    
    async def _test_form_input_responsiveness(self, page: Page, thresholds: Dict):
        """Test form input responsiveness."""
        # Navigate to a page with forms
        await page.goto(f"{page.url.split('/')[0]}//{page.url.split('//')[1].split('/')[0]}/wifi")
        await page.wait_for_load_state("domcontentloaded")
        
        text_inputs = page.locator('input[type="text"], input[type="password"]')
        input_count = await text_inputs.count()
        
        if input_count > 0:
            input_field = text_inputs.first
            
            # Test typing responsiveness
            await input_field.focus()
            
            typing_delays = []
            test_text = "Quick"
            
            for char in test_text:
                char_start = time.time()
                await input_field.type(char)
                char_end = time.time()
                
                char_delay = (char_end - char_start) * 1000
                typing_delays.append(char_delay)
            
            # Average character input should be fast
            avg_delay = statistics.mean(typing_delays)
            assert avg_delay < thresholds["interaction"]["fast"], \
                f"Text input too slow: {avg_delay:.0f}ms per character"
    
    async def _test_navigation_responsiveness(self, page: Page, thresholds: Dict):
        """Test navigation responsiveness."""
        nav_links = page.locator('nav a, [role="navigation"] a')
        link_count = await nav_links.count()
        
        if link_count > 0:
            # Test navigation link click
            nav_link = nav_links.first
            
            if await nav_link.is_visible():
                start_time = time.time()
                await nav_link.click()
                
                # Wait for navigation to start
                await page.wait_for_load_state("domcontentloaded", timeout=5000)
                
                nav_time = (time.time() - start_time) * 1000
                
                # Navigation should feel responsive
                assert nav_time < thresholds["page_load"]["acceptable"], \
                    f"Navigation too slow: {nav_time:.0f}ms"
    
    async def _test_scroll_responsiveness(self, page: Page, thresholds: Dict):
        """Test scroll responsiveness and smoothness."""
        # Measure scroll performance
        scroll_start = time.time()
        
        # Perform scroll action
        await page.mouse.wheel(0, 500)
        
        # Wait for scroll to complete
        await page.wait_for_timeout(100)
        
        scroll_time = (time.time() - scroll_start) * 1000
        
        # Scrolling should feel smooth
        assert scroll_time < thresholds["visual_feedback"]["delayed"], \
            f"Scroll response too slow: {scroll_time:.0f}ms"
    
    async def _capture_current_data(self, page: Page) -> Dict:
        """Capture current data values from the page."""
        data_elements = page.locator('.sensor-data, [data-sensor], .data-value')
        data_count = await data_elements.count()
        
        current_data = {}
        for i in range(min(5, data_count)):  # Capture first 5 data points
            element = data_elements.nth(i)
            element_text = await element.text_content()
            current_data[f"sensor_{i}"] = element_text.strip() if element_text else ""
        
        return current_data
    
    async def _get_memory_usage(self, page: Page) -> float:
        """Get current memory usage in MB."""
        memory_info = await page.evaluate("""
            () => {
                if (performance.memory) {
                    return performance.memory.usedJSHeapSize / (1024 * 1024);
                }
                return 0;
            }
        """)
        return memory_info
    
    async def _simulate_user_activity(self, page: Page):
        """Simulate typical user activity patterns."""
        activities = [
            lambda: page.mouse.move(100, 100),
            lambda: page.mouse.move(200, 200),
            lambda: page.evaluate("window.scrollBy(0, 50)"),
            lambda: page.evaluate("window.scrollBy(0, -50)"),
        ]
        
        # Randomly perform activities
        import random
        activity = random.choice(activities)
        await activity()
    
    async def _measure_ui_responsiveness(self, page: Page) -> float:
        """Measure overall UI responsiveness (0-1 score)."""
        # Measure frame rate as indicator of UI smoothness
        frame_rate = await page.evaluate("""
            () => {
                return new Promise(resolve => {
                    let frames = 0;
                    const start = performance.now();
                    
                    function countFrames() {
                        frames++;
                        if (performance.now() - start < 1000) {  // Count for 1 second
                            requestAnimationFrame(countFrames);
                        } else {
                            resolve(frames);  // Return FPS
                        }
                    }
                    
                    requestAnimationFrame(countFrames);
                });
            }
        """)
        
        # Convert frame rate to responsiveness score (60fps = 1.0, 30fps = 0.5, etc.)
        return min(1.0, frame_rate / 60.0)
    
    async def _measure_click_response_time(self, page: Page) -> float:
        """Measure click response time in milliseconds."""
        buttons = page.locator('button, [role="button"]')
        
        if await buttons.count() > 0:
            button = buttons.first
            
            if await button.is_visible() and await button.is_enabled():
                start_time = time.time()
                await button.click()
                end_time = time.time()
                
                return (end_time - start_time) * 1000
        
        return 0