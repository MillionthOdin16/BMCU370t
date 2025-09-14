"""
Visual Regression Testing using Playwright.

Tests ESP32-S3 web interface for visual changes that might negatively impact
user experience, including layout shifts, missing elements, or broken styling.
"""

import pytest
import asyncio
import hashlib
from pathlib import Path
from typing import Dict, List, Any, Optional
from playwright.async_api import async_playwright, Page, Browser, BrowserContext


class TestVisualRegression:
    """Test visual consistency to catch UI regressions before users see them."""
    
    @pytest.fixture
    def visual_config(self):
        """Configuration for visual regression testing."""
        return {
            "threshold": 0.05,  # 5% visual difference threshold
            "animation_handling": "disabled",  # Disable animations for consistent screenshots
            "full_page": True,  # Capture full page, not just viewport
            "mask_dynamic_content": True  # Mask timestamps and dynamic data
        }
    
    @pytest.fixture
    def esp32_base_url(self):
        """Base URL for ESP32 web interface."""
        return "http://localhost:8080"
    
    @pytest.fixture
    def baseline_dir(self):
        """Directory to store baseline images."""
        baseline_path = Path("test-results/visual-regression/baselines")
        baseline_path.mkdir(parents=True, exist_ok=True)
        return baseline_path
    
    @pytest.fixture
    def comparison_dir(self):
        """Directory to store comparison results."""
        comparison_path = Path("test-results/visual-regression/comparisons")
        comparison_path.mkdir(parents=True, exist_ok=True)
        return comparison_path
    
    @pytest.mark.user_interaction
    @pytest.mark.visual_regression
    @pytest.mark.asyncio
    async def test_main_dashboard_visual_consistency(self, esp32_base_url, visual_config, baseline_dir):
        """Test that main dashboard maintains visual consistency."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                viewport={"width": 1920, "height": 1080},
                reduced_motion="reduce"  # Disable animations
            )
            page = await context.new_page()
            
            try:
                await page.goto(esp32_base_url)
                await page.wait_for_load_state("networkidle")
                
                # Mask dynamic content that changes between test runs
                await self._mask_dynamic_content(page)
                
                # Wait for any loading states to complete
                await page.wait_for_timeout(1000)
                
                # Take screenshot
                screenshot_path = baseline_dir / "dashboard_main.png"
                await page.screenshot(
                    path=str(screenshot_path),
                    full_page=visual_config["full_page"],
                    animations=visual_config["animation_handling"]
                )
                
                # Verify critical elements are visible
                await self._verify_critical_elements_visible(page, "dashboard")
                
                # Test responsive breakpoints
                breakpoints = [
                    {"width": 1920, "height": 1080, "name": "desktop_large"},
                    {"width": 1440, "height": 900, "name": "desktop_medium"},
                    {"width": 1024, "height": 768, "name": "tablet_landscape"},
                    {"width": 768, "height": 1024, "name": "tablet_portrait"},
                    {"width": 375, "height": 667, "name": "mobile"}
                ]
                
                for breakpoint in breakpoints:
                    await page.set_viewport_size(breakpoint["width"], breakpoint["height"])
                    await page.wait_for_timeout(500)  # Allow layout adjustment
                    
                    await self._mask_dynamic_content(page)
                    
                    screenshot_path = baseline_dir / f"dashboard_{breakpoint['name']}.png"
                    await page.screenshot(
                        path=str(screenshot_path),
                        full_page=visual_config["full_page"]
                    )
                    
                    # Verify layout doesn't break at this breakpoint
                    await self._verify_responsive_layout(page, breakpoint)
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.visual_regression
    @pytest.mark.asyncio
    async def test_data_visualization_consistency(self, esp32_base_url, visual_config, baseline_dir):
        """Test that data visualization maintains visual consistency."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                viewport={"width": 1920, "height": 1080},
                reduced_motion="reduce"
            )
            page = await context.new_page()
            
            try:
                await page.goto(f"{esp32_base_url}/data")
                await page.wait_for_load_state("networkidle")
                
                # Wait for charts to render
                await page.wait_for_timeout(2000)
                
                # Mask dynamic data values but keep chart structure
                await self._mask_chart_data_values(page)
                
                # Test different chart states
                chart_states = [
                    {"name": "initial_load", "action": None},
                    {"name": "after_interaction", "action": "hover_chart"},
                    {"name": "zoomed_view", "action": "zoom_chart"}
                ]
                
                for state in chart_states:
                    if state["action"]:
                        await self._perform_chart_action(page, state["action"])
                        await page.wait_for_timeout(500)
                    
                    screenshot_path = baseline_dir / f"data_charts_{state['name']}.png"
                    await page.screenshot(
                        path=str(screenshot_path),
                        full_page=visual_config["full_page"]
                    )
                    
                    # Verify chart elements are rendered
                    await self._verify_chart_elements(page)
                
                # Test data loading states
                await self._simulate_data_loading_states(page, baseline_dir, visual_config)
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.visual_regression
    @pytest.mark.asyncio
    async def test_form_interface_consistency(self, esp32_base_url, visual_config, baseline_dir):
        """Test that form interfaces maintain visual consistency."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                viewport={"width": 1920, "height": 1080}
            )
            page = await context.new_page()
            
            try:
                # Test WiFi configuration form
                await page.goto(f"{esp32_base_url}/wifi")
                await page.wait_for_load_state("networkidle")
                
                # Test various form states
                form_states = [
                    {"name": "empty_form", "setup": None},
                    {"name": "filled_form", "setup": "fill_form"},
                    {"name": "validation_errors", "setup": "trigger_errors"},
                    {"name": "success_state", "setup": "success_feedback"}
                ]
                
                for state in form_states:
                    if state["setup"]:
                        await self._setup_form_state(page, state["setup"])
                        await page.wait_for_timeout(500)
                    
                    screenshot_path = baseline_dir / f"wifi_form_{state['name']}.png"
                    await page.screenshot(
                        path=str(screenshot_path),
                        full_page=visual_config["full_page"]
                    )
                
                # Test other forms if they exist
                forms_to_test = ["/config", "/upload"]
                
                for form_url in forms_to_test:
                    await page.goto(f"{esp32_base_url}{form_url}")
                    await page.wait_for_load_state("networkidle")
                    
                    form_name = form_url.strip("/")
                    screenshot_path = baseline_dir / f"{form_name}_form.png"
                    await page.screenshot(
                        path=str(screenshot_path),
                        full_page=visual_config["full_page"]
                    )
                    
                    # Verify form elements are properly styled
                    await self._verify_form_styling(page)
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.visual_regression
    @pytest.mark.asyncio
    async def test_error_state_visuals(self, esp32_base_url, visual_config, baseline_dir):
        """Test visual consistency of error states."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                viewport={"width": 1920, "height": 1080}
            )
            page = await context.new_page()
            
            try:
                # Test 404 error page
                await page.goto(f"{esp32_base_url}/nonexistent-page")
                await page.wait_for_load_state("networkidle")
                
                screenshot_path = baseline_dir / "error_404.png"
                await page.screenshot(
                    path=str(screenshot_path),
                    full_page=visual_config["full_page"]
                )
                
                # Test network error simulation
                await self._simulate_network_error(page)
                
                await page.goto(esp32_base_url)
                await page.wait_for_timeout(2000)  # Wait for error state to show
                
                screenshot_path = baseline_dir / "error_network.png"
                await page.screenshot(
                    path=str(screenshot_path),
                    full_page=visual_config["full_page"]
                )
                
                # Test form validation errors
                await self._restore_network(page)
                await page.goto(f"{esp32_base_url}/wifi")
                await page.wait_for_load_state("networkidle")
                
                # Trigger validation errors
                submit_btn = page.locator('button[type="submit"], input[type="submit"]')
                if await submit_btn.count() > 0:
                    await submit_btn.first.click()
                    await page.wait_for_timeout(1000)
                    
                    screenshot_path = baseline_dir / "error_form_validation.png"
                    await page.screenshot(
                        path=str(screenshot_path),
                        full_page=visual_config["full_page"]
                    )
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.visual_regression
    @pytest.mark.asyncio
    async def test_loading_state_visuals(self, esp32_base_url, visual_config, baseline_dir):
        """Test visual consistency of loading states."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                viewport={"width": 1920, "height": 1080}
            )
            page = await context.new_page()
            
            try:
                # Slow down network to capture loading states
                await page.route("**/*", lambda route: (
                    asyncio.create_task(self._slow_response(route, 1000))
                ))
                
                # Navigate and capture loading state
                navigation_promise = page.goto(esp32_base_url)
                await page.wait_for_timeout(500)  # Capture mid-loading
                
                screenshot_path = baseline_dir / "loading_initial.png"
                await page.screenshot(
                    path=str(screenshot_path),
                    full_page=visual_config["full_page"]
                )
                
                # Wait for full load
                await navigation_promise
                await page.wait_for_load_state("networkidle")
                
                # Test data loading states
                await page.goto(f"{esp32_base_url}/data")
                await page.wait_for_timeout(500)  # Capture data loading
                
                screenshot_path = baseline_dir / "loading_data.png"
                await page.screenshot(
                    path=str(screenshot_path),
                    full_page=visual_config["full_page"]
                )
                
                # Test file upload loading
                await page.goto(f"{esp32_base_url}/upload")
                await page.wait_for_load_state("networkidle")
                
                # Simulate file upload loading
                file_input = page.locator('input[type="file"]')
                if await file_input.count() > 0:
                    # Create dummy file for upload test
                    import tempfile
                    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as temp_file:
                        temp_file.write(b"test content" * 1000)
                        temp_file_path = temp_file.name
                    
                    try:
                        await file_input.set_input_files(temp_file_path)
                        
                        submit_btn = page.locator('button[type="submit"], .upload-btn')
                        if await submit_btn.count() > 0:
                            await submit_btn.first.click()
                            await page.wait_for_timeout(500)  # Capture upload loading
                            
                            screenshot_path = baseline_dir / "loading_upload.png"
                            await page.screenshot(
                                path=str(screenshot_path),
                                full_page=visual_config["full_page"]
                            )
                    finally:
                        import os
                        if os.path.exists(temp_file_path):
                            os.unlink(temp_file_path)
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.visual_regression
    @pytest.mark.asyncio
    async def test_cross_browser_visual_consistency(self, esp32_base_url, visual_config, baseline_dir):
        """Test visual consistency across different browsers."""
        browsers_to_test = [
            {"name": "chromium", "engine": "chromium"},
            {"name": "firefox", "engine": "firefox"},
            {"name": "webkit", "engine": "webkit"}
        ]
        
        async with async_playwright() as p:
            for browser_config in browsers_to_test:
                try:
                    if browser_config["engine"] == "chromium":
                        browser = await p.chromium.launch(headless=True)
                    elif browser_config["engine"] == "firefox":
                        browser = await p.firefox.launch(headless=True)
                    elif browser_config["engine"] == "webkit":
                        browser = await p.webkit.launch(headless=True)
                    else:
                        continue
                    
                    context = await browser.new_context(
                        viewport={"width": 1920, "height": 1080}
                    )
                    page = await context.new_page()
                    
                    # Test main pages across browsers
                    pages_to_test = ["/", "/data", "/wifi", "/config"]
                    
                    for page_url in pages_to_test:
                        await page.goto(f"{esp32_base_url}{page_url}")
                        await page.wait_for_load_state("networkidle")
                        
                        await self._mask_dynamic_content(page)
                        await page.wait_for_timeout(1000)
                        
                        page_name = page_url.strip("/") or "home"
                        screenshot_path = baseline_dir / f"{page_name}_{browser_config['name']}.png"
                        
                        await page.screenshot(
                            path=str(screenshot_path),
                            full_page=visual_config["full_page"]
                        )
                        
                        # Verify critical functionality works in this browser
                        await self._verify_browser_compatibility(page, browser_config["name"])
                    
                    await context.close()
                    await browser.close()
                    
                except Exception as e:
                    print(f"Browser {browser_config['name']} not available: {e}")
                    continue
    
    async def _mask_dynamic_content(self, page: Page):
        """Mask dynamic content like timestamps and changing data."""
        # Mask timestamp elements
        await page.add_style_tag(content="""
            [data-timestamp],
            .timestamp,
            .last-updated,
            .data-time {
                color: transparent !important;
                background-color: #cccccc !important;
            }
            
            /* Mask sensor values that change */
            .sensor-value,
            [data-sensor-value],
            .data-value {
                color: transparent !important;
                background-color: #cccccc !important;
            }
            
            /* Hide loading spinners for consistent screenshots */
            .loading,
            .spinner,
            .loading-indicator {
                display: none !important;
            }
        """)
    
    async def _mask_chart_data_values(self, page: Page):
        """Mask dynamic chart data while keeping chart structure."""
        # Inject script to mask chart tooltips and dynamic labels
        await page.evaluate("""
            () => {
                // Hide chart tooltips
                const tooltips = document.querySelectorAll('.tooltip, [data-tooltip], .chart-tooltip');
                tooltips.forEach(tooltip => tooltip.style.display = 'none');
                
                // Mask dynamic chart labels
                const chartLabels = document.querySelectorAll('.chart-label, .axis-label');
                chartLabels.forEach(label => {
                    if (/\\d/.test(label.textContent)) {
                        label.style.color = 'transparent';
                        label.style.backgroundColor = '#cccccc';
                    }
                });
            }
        """)
    
    async def _verify_critical_elements_visible(self, page: Page, page_type: str):
        """Verify that critical elements are visible on the page."""
        if page_type == "dashboard":
            critical_selectors = [
                'nav, [role="navigation"]',  # Navigation
                '.main-content, main, #content',  # Main content area
                '.sensor-data, [data-sensor]'  # Sensor data display
            ]
        else:
            critical_selectors = [
                'nav, [role="navigation"]',
                '.main-content, main, #content'
            ]
        
        for selector in critical_selectors:
            element = page.locator(selector)
            if await element.count() > 0:
                is_visible = await element.first.is_visible()
                assert is_visible, f"Critical element should be visible: {selector}"
    
    async def _verify_responsive_layout(self, page: Page, breakpoint: Dict):
        """Verify layout works correctly at different breakpoints."""
        viewport_width = breakpoint["width"]
        
        # Check for horizontal overflow
        content_width = await page.evaluate("document.documentElement.scrollWidth")
        assert content_width <= viewport_width + 50, \
            f"Content overflows at {breakpoint['name']}: {content_width} > {viewport_width}"
        
        # Check navigation adapts to mobile
        if viewport_width < 768:  # Mobile breakpoint
            hamburger_menu = page.locator('.hamburger, .menu-toggle, .mobile-menu-btn')
            regular_nav = page.locator('nav ul, .nav-links')
            
            # On mobile, either have hamburger menu or nav should be adapted
            has_mobile_nav = await hamburger_menu.count() > 0
            nav_hidden = await regular_nav.is_hidden() if await regular_nav.count() > 0 else True
            
            assert has_mobile_nav or nav_hidden, \
                f"Navigation should adapt for mobile at {breakpoint['name']}"
    
    async def _verify_chart_elements(self, page: Page):
        """Verify chart elements are rendered correctly."""
        chart_elements = page.locator('canvas, svg, .chart')
        chart_count = await chart_elements.count()
        
        for i in range(chart_count):
            chart = chart_elements.nth(i)
            is_visible = await chart.is_visible()
            assert is_visible, f"Chart {i} should be visible"
            
            # Check chart has reasonable dimensions
            bounding_box = await chart.bounding_box()
            if bounding_box:
                assert bounding_box["width"] > 100, f"Chart {i} width too small: {bounding_box['width']}"
                assert bounding_box["height"] > 100, f"Chart {i} height too small: {bounding_box['height']}"
    
    async def _perform_chart_action(self, page: Page, action: str):
        """Perform specific chart interaction actions."""
        chart = page.locator('canvas, .chart').first
        
        if await chart.count() == 0:
            return
        
        chart_box = await chart.bounding_box()
        if not chart_box:
            return
        
        center_x = chart_box["x"] + chart_box["width"] / 2
        center_y = chart_box["y"] + chart_box["height"] / 2
        
        if action == "hover_chart":
            await page.mouse.move(center_x, center_y)
        elif action == "zoom_chart":
            # Simulate zoom with mouse wheel
            await page.mouse.move(center_x, center_y)
            await page.mouse.wheel(0, -100)  # Zoom in
    
    async def _simulate_data_loading_states(self, page: Page, baseline_dir: Path, visual_config: Dict):
        """Simulate different data loading states."""
        # Add loading indicators via JavaScript
        await page.evaluate("""
            () => {
                const loadingStates = [
                    { selector: '.sensor-data', state: 'loading' },
                    { selector: '.chart', state: 'loading' },
                    { selector: '.data-table', state: 'no-data' }
                ];
                
                loadingStates.forEach(state => {
                    const elements = document.querySelectorAll(state.selector);
                    elements.forEach(el => {
                        if (state.state === 'loading') {
                            el.innerHTML = '<div class="loading-spinner">Loading...</div>';
                        } else if (state.state === 'no-data') {
                            el.innerHTML = '<div class="no-data">No data available</div>';
                        }
                    });
                });
            }
        """)
        
        await page.wait_for_timeout(500)
        
        screenshot_path = baseline_dir / "data_loading_states.png"
        await page.screenshot(
            path=str(screenshot_path),
            full_page=visual_config["full_page"]
        )
    
    async def _setup_form_state(self, page: Page, state_type: str):
        """Setup specific form states for visual testing."""
        if state_type == "fill_form":
            # Fill form with test data
            inputs = await page.locator('input[type="text"], input[type="password"]').all()
            for i, input_field in enumerate(inputs[:3]):  # Fill first 3 inputs
                await input_field.fill(f"TestValue{i+1}")
        
        elif state_type == "trigger_errors":
            # Submit empty form to trigger validation errors
            submit_btn = page.locator('button[type="submit"], input[type="submit"]')
            if await submit_btn.count() > 0:
                await submit_btn.first.click()
        
        elif state_type == "success_feedback":
            # Simulate success state
            await page.evaluate("""
                () => {
                    const form = document.querySelector('form');
                    if (form) {
                        const successMsg = document.createElement('div');
                        successMsg.className = 'success-message';
                        successMsg.textContent = 'Configuration saved successfully!';
                        form.parentNode.insertBefore(successMsg, form.nextSibling);
                    }
                }
            """)
    
    async def _verify_form_styling(self, page: Page):
        """Verify form elements are properly styled."""
        form_elements = await page.locator('input, select, textarea, button').all()
        
        for element in form_elements:
            # Check element is visible and has reasonable size
            is_visible = await element.is_visible()
            if is_visible:
                bounding_box = await element.bounding_box()
                if bounding_box:
                    assert bounding_box["height"] >= 20, "Form elements should have minimum height"
                    assert bounding_box["width"] >= 50, "Form elements should have minimum width"
    
    async def _simulate_network_error(self, page: Page):
        """Simulate network error conditions."""
        await page.route("**/*", lambda route: route.abort())
    
    async def _restore_network(self, page: Page):
        """Restore normal network conditions."""
        await page.unroute("**/*")
    
    async def _slow_response(self, route, delay_ms: int):
        """Slow down network responses for loading state testing."""
        await asyncio.sleep(delay_ms / 1000)
        await route.continue_()
    
    async def _verify_browser_compatibility(self, page: Page, browser_name: str):
        """Verify basic functionality works in the browser."""
        # Check JavaScript is working
        js_works = await page.evaluate("() => typeof document !== 'undefined'")
        assert js_works, f"JavaScript should work in {browser_name}"
        
        # Check CSS is applied
        body_style = await page.evaluate("() => getComputedStyle(document.body).display")
        assert body_style != "", f"CSS should be applied in {browser_name}"
        
        # Check interactive elements are clickable
        buttons = page.locator('button, [role="button"]')
        if await buttons.count() > 0:
            first_button = buttons.first
            is_enabled = await first_button.is_enabled()
            assert is_enabled, f"Buttons should be enabled in {browser_name}"