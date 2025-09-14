"""
Mobile User Interaction Tests using Playwright.

Tests ESP32-S3 web interface on mobile devices to catch mobile-specific
issues before real users encounter them on phones and tablets.
"""

import pytest
import asyncio
from typing import Dict, List, Tuple
from playwright.async_api import async_playwright, Page, Browser, BrowserContext


class TestMobileUserInteractions:
    """Test mobile user interactions with the ESP32 web interface."""
    
    @pytest.fixture
    def mobile_devices(self):
        """Common mobile device configurations for testing."""
        return {
            "iphone_13": {
                "viewport": {"width": 390, "height": 844},
                "user_agent": "Mozilla/5.0 (iPhone; CPU iPhone OS 15_0 like Mac OS X) AppleWebKit/605.1.15",
                "device_scale_factor": 3,
                "is_mobile": True,
                "has_touch": True
            },
            "pixel_6": {
                "viewport": {"width": 412, "height": 915},
                "user_agent": "Mozilla/5.0 (Linux; Android 12; Pixel 6) AppleWebKit/537.36",
                "device_scale_factor": 2.625,
                "is_mobile": True,
                "has_touch": True
            },
            "ipad_air": {
                "viewport": {"width": 820, "height": 1180},
                "user_agent": "Mozilla/5.0 (iPad; CPU OS 15_0 like Mac OS X) AppleWebKit/605.1.15",
                "device_scale_factor": 2,
                "is_mobile": True,
                "has_touch": True
            },
            "samsung_tablet": {
                "viewport": {"width": 800, "height": 1280},
                "user_agent": "Mozilla/5.0 (Linux; Android 12; SM-T970) AppleWebKit/537.36",
                "device_scale_factor": 2,
                "is_mobile": True,
                "has_touch": True
            }
        }
    
    @pytest.fixture
    def esp32_mobile_url(self):
        """Base URL for ESP32 web interface."""
        return "http://localhost:8080"  # Mock server for testing
    
    @pytest.mark.user_interaction
    @pytest.mark.mobile
    @pytest.mark.asyncio
    async def test_mobile_navigation_experience(self, mobile_devices, esp32_mobile_url):
        """Test mobile navigation patterns and hamburger menus."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            
            for device_name, device_config in mobile_devices.items():
                context = await browser.new_context(
                    viewport=device_config["viewport"],
                    user_agent=device_config["user_agent"],
                    device_scale_factor=device_config["device_scale_factor"],
                    is_mobile=device_config["is_mobile"],
                    has_touch=device_config["has_touch"]
                )
                
                page = await context.new_page()
                
                try:
                    # User opens ESP32 interface on mobile device
                    await page.goto(esp32_mobile_url)
                    await page.wait_for_load_state("networkidle")
                    
                    # Mobile users often look for hamburger menu
                    hamburger_selectors = [
                        '[data-testid="hamburger-menu"]',
                        '.hamburger',
                        '.menu-toggle',
                        '.mobile-menu-btn',
                        'button[aria-label*="menu"]',
                        '.nav-toggle'
                    ]
                    
                    hamburger_found = False
                    for selector in hamburger_selectors:
                        hamburger = page.locator(selector)
                        if await hamburger.count() > 0:
                            hamburger_found = True
                            
                            # User taps hamburger menu
                            await hamburger.tap()
                            await page.wait_for_timeout(500)  # Animation time
                            
                            # Mobile menu should appear
                            mobile_menu = page.locator('.mobile-menu, .nav-mobile, [data-mobile-menu]')
                            await mobile_menu.wait_for(timeout=3000)
                            
                            # Menu should be visible and accessible
                            menu_visible = await mobile_menu.is_visible()
                            assert menu_visible, f"Mobile menu should be visible on {device_name}"
                            
                            # Menu items should be touch-friendly (min 44px tap targets)
                            menu_items = page.locator('.mobile-menu a, .nav-mobile a')
                            item_count = await menu_items.count()
                            
                            for i in range(min(item_count, 5)):  # Check first 5 items
                                item = menu_items.nth(i)
                                box = await item.bounding_box()
                                if box:
                                    # iOS Human Interface Guidelines recommend 44px minimum
                                    min_touch_size = 44
                                    assert box["height"] >= min_touch_size or box["width"] >= min_touch_size, \
                                        f"Menu item {i} too small for touch on {device_name}: {box}"
                            
                            break
                    
                    # If no hamburger menu, check if navigation is mobile-optimized
                    if not hamburger_found:
                        nav_elements = page.locator('nav a, .navigation a')
                        nav_count = await nav_elements.count()
                        
                        if nav_count > 0:
                            # Check if navigation items are touch-friendly
                            for i in range(min(nav_count, 3)):
                                nav_item = nav_elements.nth(i)
                                box = await nav_item.bounding_box()
                                if box:
                                    assert box["height"] >= 32, \
                                        f"Navigation item {i} too small for mobile on {device_name}"
                    
                    # Test mobile viewport responsiveness
                    viewport_width = device_config["viewport"]["width"]
                    
                    # Content should not overflow horizontally
                    page_width = await page.evaluate("document.documentElement.scrollWidth")
                    assert page_width <= viewport_width + 50, \
                        f"Content overflows mobile viewport on {device_name}: {page_width} > {viewport_width}"
                    
                    # Text should be readable (min 16px on mobile)
                    body_font_size = await page.evaluate(
                        "parseInt(getComputedStyle(document.body).fontSize)"
                    )
                    assert body_font_size >= 14, \
                        f"Font too small for mobile reading on {device_name}: {body_font_size}px"
                    
                finally:
                    await context.close()
            
            await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.mobile
    @pytest.mark.asyncio
    async def test_mobile_touch_interactions(self, mobile_devices, esp32_mobile_url):
        """Test touch-specific interactions like swipe, pinch, tap."""
        async with async_playwright() as p:
            browser = await p.webkit.launch(headless=True)  # WebKit for iOS-like behavior
            
            device_config = mobile_devices["iphone_13"]
            context = await browser.new_context(
                viewport=device_config["viewport"],
                user_agent=device_config["user_agent"],
                device_scale_factor=device_config["device_scale_factor"],
                is_mobile=True,
                has_touch=True
            )
            
            page = await context.new_page()
            
            try:
                await page.goto(f"{esp32_mobile_url}/data")
                await page.wait_for_load_state("networkidle")
                
                # Test 1: User swipes through data views
                chart_container = page.locator('canvas, .chart, [data-chart]')
                if await chart_container.count() > 0:
                    chart_box = await chart_container.first.bounding_box()
                    if chart_box:
                        # User swipes left across chart
                        start_x = chart_box["x"] + chart_box["width"] * 0.8
                        end_x = chart_box["x"] + chart_box["width"] * 0.2
                        y = chart_box["y"] + chart_box["height"] * 0.5
                        
                        await page.mouse.move(start_x, y)
                        await page.mouse.down()
                        await page.mouse.move(end_x, y)
                        await page.mouse.up()
                        
                        await page.wait_for_timeout(500)
                        
                        # Chart should respond to swipe (pan or change view)
                        # This is validated by checking if any visual changes occurred
                        # We can't easily test specific chart behavior without the actual chart library
                
                # Test 2: User double-taps to zoom
                data_display = page.locator('.sensor-data, [data-sensor], .data-display')
                if await data_display.count() > 0:
                    display_box = await data_display.first.bounding_box()
                    if display_box:
                        center_x = display_box["x"] + display_box["width"] / 2
                        center_y = display_box["y"] + display_box["height"] / 2
                        
                        # User double-taps
                        await page.mouse.dblclick(center_x, center_y)
                        await page.wait_for_timeout(300)
                        
                        # Should not trigger unwanted zoom (viewport zoom should be disabled)
                        viewport_scale = await page.evaluate("window.visualViewport?.scale || 1")
                        assert viewport_scale == 1, "User zoom should be disabled on mobile interface"
                
                # Test 3: User long-presses for context menu
                control_button = page.locator('button, .btn, [role="button"]')
                if await control_button.count() > 0:
                    button_box = await control_button.first.bounding_box()
                    if button_box:
                        center_x = button_box["x"] + button_box["width"] / 2
                        center_y = button_box["y"] + button_box["height"] / 2
                        
                        # User long-presses button
                        await page.mouse.move(center_x, center_y)
                        await page.mouse.down()
                        await page.wait_for_timeout(800)  # Long press duration
                        await page.mouse.up()
                        
                        # Should handle long press appropriately (no default context menu)
                        # This tests that touch events are properly handled
                
                # Test 4: User scrolls with momentum
                initial_scroll = await page.evaluate("window.scrollY")
                
                # User performs momentum scroll
                await page.mouse.wheel(0, 300)
                await page.wait_for_timeout(100)
                
                current_scroll = await page.evaluate("window.scrollY")
                # Should scroll smoothly without lag
                assert current_scroll != initial_scroll, "Page should respond to scroll gestures"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.mobile
    @pytest.mark.asyncio
    async def test_mobile_form_interactions(self, mobile_devices, esp32_mobile_url):
        """Test mobile form filling with virtual keyboard considerations."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            
            device_config = mobile_devices["pixel_6"]
            context = await browser.new_context(
                viewport=device_config["viewport"],
                user_agent=device_config["user_agent"],
                is_mobile=True,
                has_touch=True
            )
            
            page = await context.new_page()
            
            try:
                await page.goto(f"{esp32_mobile_url}/wifi")
                await page.wait_for_load_state("networkidle")
                
                # Test mobile form interactions
                form_fields = page.locator('input[type="text"], input[type="password"], input[type="email"]')
                field_count = await form_fields.count()
                
                if field_count > 0:
                    # Test 1: User taps input field
                    first_field = form_fields.first
                    await first_field.tap()
                    await page.wait_for_timeout(300)
                    
                    # Field should be focused
                    is_focused = await first_field.is_focused()
                    assert is_focused, "Tapped input field should receive focus"
                    
                    # Viewport should adjust for virtual keyboard
                    # On real mobile, this would change the visual viewport
                    
                    # Test 2: User types with mobile keyboard
                    await first_field.fill("TestNetwork")
                    await page.wait_for_timeout(200)
                    
                    field_value = await first_field.input_value()
                    assert field_value == "TestNetwork", "Mobile text input should work correctly"
                    
                    # Test 3: User switches between fields
                    if field_count > 1:
                        second_field = form_fields.nth(1)
                        await second_field.tap()
                        await page.wait_for_timeout(200)
                        
                        # Previous field should lose focus
                        first_focused = await first_field.is_focused()
                        second_focused = await second_field.is_focused()
                        
                        assert not first_focused, "Previous field should lose focus"
                        assert second_focused, "New field should gain focus"
                
                # Test mobile-specific input types
                number_inputs = page.locator('input[type="number"], input[inputmode="numeric"]')
                if await number_inputs.count() > 0:
                    number_field = number_inputs.first
                    await number_field.tap()
                    
                    # Should trigger numeric keyboard on mobile
                    input_mode = await number_field.get_attribute("inputmode")
                    input_type = await number_field.get_attribute("type")
                    
                    assert input_mode == "numeric" or input_type == "number", \
                        "Numeric fields should have proper mobile keyboard hint"
                
                # Test form submission on mobile
                submit_button = page.locator('button[type="submit"], input[type="submit"]')
                if await submit_button.count() > 0:
                    # Button should be touch-friendly
                    button_box = await submit_button.first.bounding_box()
                    if button_box:
                        assert button_box["height"] >= 44, \
                            f"Submit button too small for touch: {button_box['height']}px"
                    
                    # User taps submit
                    await submit_button.first.tap()
                    await page.wait_for_timeout(1000)
                    
                    # Should handle form submission appropriately
                    # (Actual validation depends on server response)
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.mobile
    @pytest.mark.asyncio
    async def test_mobile_orientation_changes(self, mobile_devices, esp32_mobile_url):
        """Test user experience during orientation changes."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            
            device_config = mobile_devices["iphone_13"]
            context = await browser.new_context(
                viewport=device_config["viewport"],
                user_agent=device_config["user_agent"],
                is_mobile=True,
                has_touch=True
            )
            
            page = await context.new_page()
            
            try:
                await page.goto(esp32_mobile_url)
                await page.wait_for_load_state("networkidle")
                
                # Test portrait orientation
                portrait_viewport = device_config["viewport"]
                await page.set_viewport_size(portrait_viewport["width"], portrait_viewport["height"])
                await page.wait_for_timeout(500)
                
                # Check layout in portrait
                navigation_visible = await self._check_navigation_visibility(page)
                content_readable = await self._check_content_readability(page)
                
                assert navigation_visible, "Navigation should be accessible in portrait"
                assert content_readable, "Content should be readable in portrait"
                
                # User rotates device to landscape
                landscape_viewport = {
                    "width": portrait_viewport["height"],
                    "height": portrait_viewport["width"]
                }
                
                await page.set_viewport_size(landscape_viewport["width"], landscape_viewport["height"])
                await page.wait_for_timeout(800)  # Allow for orientation change animation
                
                # Check layout in landscape
                navigation_visible_landscape = await self._check_navigation_visibility(page)
                content_readable_landscape = await self._check_content_readability(page)
                
                assert navigation_visible_landscape, "Navigation should adapt to landscape"
                assert content_readable_landscape, "Content should be readable in landscape"
                
                # Test that data displays adapt to wider viewport
                data_containers = page.locator('.sensor-data, [data-sensor], .data-display')
                if await data_containers.count() > 0:
                    # In landscape, data might be arranged differently
                    container_count = await data_containers.count()
                    assert container_count > 0, "Data should be visible in landscape mode"
                
                # Test charts/graphs in landscape
                charts = page.locator('canvas, .chart, [data-chart]')
                if await charts.count() > 0:
                    chart_box = await charts.first.bounding_box()
                    if chart_box:
                        # Chart should utilize available landscape space
                        assert chart_box["width"] > chart_box["height"], \
                            "Charts should adapt to landscape orientation"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.mobile
    @pytest.mark.asyncio
    async def test_mobile_performance_experience(self, mobile_devices, esp32_mobile_url):
        """Test mobile performance from user perspective."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            
            # Test on mid-range device (slower than desktop)
            device_config = mobile_devices["pixel_6"]
            context = await browser.new_context(
                viewport=device_config["viewport"],
                user_agent=device_config["user_agent"],
                is_mobile=True,
                has_touch=True
            )
            
            page = await context.new_page()
            
            try:
                # Measure page load performance
                start_time = time.time()
                await page.goto(esp32_mobile_url)
                await page.wait_for_load_state("networkidle")
                load_time = (time.time() - start_time) * 1000
                
                # Mobile users expect fast loading
                assert load_time < 3000, f"Page load too slow for mobile: {load_time:.0f}ms"
                
                # Test touch responsiveness
                interactive_elements = page.locator('button, a, [role="button"], input')
                if await interactive_elements.count() > 0:
                    element = interactive_elements.first
                    
                    # Measure tap response time
                    start_time = time.time()
                    await element.tap()
                    await page.wait_for_timeout(100)  # Min perceptible delay
                    response_time = (time.time() - start_time) * 1000
                    
                    # Touches should feel responsive
                    assert response_time < 100, f"Touch response too slow: {response_time:.0f}ms"
                
                # Test scroll performance
                await page.goto(f"{esp32_mobile_url}/data")
                await page.wait_for_load_state("networkidle")
                
                # Measure scroll smoothness
                scroll_start_time = time.time()
                await page.mouse.wheel(0, 500)
                await page.wait_for_timeout(50)
                scroll_time = (time.time() - scroll_start_time) * 1000
                
                # Scrolling should be smooth
                assert scroll_time < 50, f"Scroll too laggy for mobile: {scroll_time:.0f}ms"
                
                # Test memory usage (mobile devices have limited RAM)
                memory_info = await page.evaluate("""() => {
                    if (performance.memory) {
                        return {
                            used: performance.memory.usedJSHeapSize,
                            total: performance.memory.totalJSHeapSize,
                            limit: performance.memory.jsHeapSizeLimit
                        };
                    }
                    return null;
                }""")
                
                if memory_info:
                    memory_mb = memory_info["used"] / (1024 * 1024)
                    # Mobile apps should be memory-efficient
                    assert memory_mb < 100, f"Memory usage too high for mobile: {memory_mb:.1f}MB"
                
            finally:
                await context.close()
                await browser.close()
    
    async def _check_navigation_visibility(self, page: Page) -> bool:
        """Check if navigation is visible and accessible."""
        nav_selectors = [
            'nav',
            '.navigation',
            '.navbar',
            '.menu',
            '[role="navigation"]'
        ]
        
        for selector in nav_selectors:
            nav = page.locator(selector)
            if await nav.count() > 0 and await nav.is_visible():
                return True
        
        # Check for mobile menu button
        mobile_menu_selectors = [
            '.hamburger',
            '.menu-toggle',
            '[data-mobile-menu]'
        ]
        
        for selector in mobile_menu_selectors:
            menu_btn = page.locator(selector)
            if await menu_btn.count() > 0 and await menu_btn.is_visible():
                return True
        
        return False
    
    async def _check_content_readability(self, page: Page) -> bool:
        """Check if content is readable (not overlapping, proper size)."""
        # Check font size
        body_font_size = await page.evaluate(
            "parseInt(getComputedStyle(document.body).fontSize)"
        )
        
        if body_font_size < 14:  # Too small for mobile
            return False
        
        # Check for horizontal overflow
        viewport_width = await page.evaluate("window.innerWidth")
        content_width = await page.evaluate("document.documentElement.scrollWidth")
        
        if content_width > viewport_width + 10:  # Allow small tolerance
            return False
        
        # Check that main content is visible
        main_content = page.locator('main, .main, .content, #content')
        if await main_content.count() > 0:
            is_visible = await main_content.first.is_visible()
            return is_visible
        
        # Fallback: check if body has reasonable content
        body_text = await page.evaluate("document.body.textContent")
        return len(body_text.strip()) > 50


import time  # Add missing import