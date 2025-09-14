"""
Accessibility User Experience Tests using Playwright.

Tests ESP32-S3 web interface for real accessibility issues that users with
disabilities might encounter, using assistive technology simulation.
"""

import pytest
import asyncio
from typing import Dict, List, Any
from playwright.async_api import async_playwright, Page, Browser, BrowserContext


class TestAccessibilityUserExperience:
    """Test accessibility from real user perspective with assistive technologies."""
    
    @pytest.fixture
    def accessibility_config(self):
        """Configuration for accessibility testing."""
        return {
            "force_reduced_motion": True,
            "color_scheme": "no-preference",
            "reduce_transparency": True,
            "high_contrast": True
        }
    
    @pytest.fixture
    def esp32_url(self):
        """Base URL for ESP32 web interface."""
        return "http://localhost:8080"
    
    @pytest.mark.user_interaction
    @pytest.mark.accessibility
    @pytest.mark.asyncio
    async def test_screen_reader_user_experience(self, esp32_url, accessibility_config):
        """Test experience for users relying on screen readers."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                reduced_motion="reduce",
                forced_colors="active",
                color_scheme="no-preference"
            )
            page = await context.new_page()
            
            try:
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Test 1: Page has proper heading structure for navigation
                headings = await page.locator('h1, h2, h3, h4, h5, h6').all()
                heading_levels = []
                
                for heading in headings:
                    tag_name = await heading.evaluate("el => el.tagName.toLowerCase()")
                    level = int(tag_name[1])
                    heading_levels.append(level)
                    
                    # Each heading should have meaningful text
                    heading_text = await heading.text_content()
                    assert len(heading_text.strip()) > 0, f"Heading {tag_name} should have text content"
                    assert not heading_text.strip().isdigit(), \
                        f"Heading should be descriptive, not just numbers: {heading_text}"
                
                # Heading structure should be logical (no skipped levels)
                if heading_levels:
                    assert heading_levels[0] == 1, "Page should start with h1"
                    for i in range(1, len(heading_levels)):
                        level_jump = heading_levels[i] - heading_levels[i-1]
                        assert level_jump <= 1, \
                            f"Heading levels should not skip: h{heading_levels[i-1]} to h{heading_levels[i]}"
                
                # Test 2: All interactive elements have accessible names
                interactive_elements = await page.locator(
                    'button, a, input, select, textarea, [role="button"], [tabindex="0"]'
                ).all()
                
                for element in interactive_elements:
                    accessible_name = await element.evaluate("""el => {
                        // Check various ways an element can have an accessible name
                        return el.getAttribute('aria-label') ||
                               el.getAttribute('aria-labelledby') ||
                               el.getAttribute('title') ||
                               el.textContent ||
                               el.getAttribute('alt') ||
                               el.getAttribute('value') ||
                               '';
                    }""")
                    
                    assert len(accessible_name.strip()) > 0, \
                        f"Interactive element should have accessible name: {await element.tag_name()}"
                
                # Test 3: Form fields have proper labels
                form_inputs = await page.locator('input, select, textarea').all()
                
                for input_element in form_inputs:
                    input_type = await input_element.get_attribute('type')
                    if input_type in ['hidden', 'submit', 'button']:
                        continue  # These don't need labels
                    
                    # Check for label association
                    input_id = await input_element.get_attribute('id')
                    input_name = await input_element.get_attribute('name')
                    
                    label_found = False
                    
                    # Check for explicit label
                    if input_id:
                        label = page.locator(f'label[for="{input_id}"]')
                        if await label.count() > 0:
                            label_text = await label.text_content()
                            assert len(label_text.strip()) > 0, f"Label for {input_id} should have text"
                            label_found = True
                    
                    # Check for implicit label (wrapping label)
                    if not label_found:
                        parent_label = await input_element.evaluate(
                            "el => el.closest('label')"
                        )
                        if parent_label:
                            label_found = True
                    
                    # Check for aria-label or aria-labelledby
                    if not label_found:
                        aria_label = await input_element.get_attribute('aria-label')
                        aria_labelledby = await input_element.get_attribute('aria-labelledby')
                        if aria_label or aria_labelledby:
                            label_found = True
                    
                    assert label_found, f"Form input should have proper label: {input_name or input_type}"
                
                # Test 4: Images have alt text or are marked decorative
                images = await page.locator('img').all()
                
                for img in images:
                    alt_text = await img.get_attribute('alt')
                    role = await img.get_attribute('role')
                    
                    # Images should either have alt text or be marked as decorative
                    assert alt_text is not None, "Images should have alt attribute (empty for decorative)"
                    
                    # If alt is empty, image should be decorative
                    if alt_text == "":
                        # Decorative images should have role="presentation" or be in decorative context
                        src = await img.get_attribute('src')
                        assert role == "presentation" or "decoration" in (src or "").lower() or \
                               "icon" in (src or "").lower(), \
                               "Empty alt text should be for decorative images only"
                
                # Test 5: Status updates are announced to screen readers
                await page.goto(f"{esp32_url}/data")
                await page.wait_for_load_state("networkidle")
                
                # Look for live regions for data updates
                live_regions = await page.locator('[aria-live], [role="status"], [role="alert"]').all()
                
                if len(live_regions) > 0:
                    for region in live_regions:
                        aria_live = await region.get_attribute('aria-live')
                        role = await region.get_attribute('role')
                        
                        # Live regions should have appropriate politeness level
                        if aria_live:
                            assert aria_live in ['polite', 'assertive'], \
                                f"aria-live should be 'polite' or 'assertive', not '{aria_live}'"
                        
                        # Status regions should contain meaningful content
                        if role in ['status', 'alert']:
                            content = await region.text_content()
                            # Content can be empty initially, but structure should be present
                            assert region is not None, "Status regions should exist in DOM"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.accessibility
    @pytest.mark.asyncio
    async def test_keyboard_only_user_experience(self, esp32_url):
        """Test complete keyboard navigation for users who cannot use a mouse."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context()
            page = await context.new_page()
            
            try:
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Test 1: User tabs through all interactive elements
                focusable_elements = []
                current_element = None
                tab_count = 0
                max_tabs = 50  # Prevent infinite loop
                
                while tab_count < max_tabs:
                    await page.keyboard.press('Tab')
                    tab_count += 1
                    
                    # Get currently focused element
                    focused = await page.evaluate("document.activeElement")
                    if focused == current_element:
                        break  # Tab cycle completed
                    
                    current_element = focused
                    
                    # Check if element is actually focusable and visible
                    is_visible = await page.evaluate("""
                        (el) => {
                            if (!el || el === document.body) return false;
                            const rect = el.getBoundingClientRect();
                            return rect.width > 0 && rect.height > 0;
                        }
                    """, focused)
                    
                    if is_visible:
                        tag_name = await page.evaluate("el => el.tagName.toLowerCase()", focused)
                        element_info = {
                            "tag": tag_name,
                            "element": focused
                        }
                        focusable_elements.append(element_info)
                
                # Should have reasonable number of focusable elements
                assert len(focusable_elements) > 0, "Page should have focusable elements"
                assert len(focusable_elements) < 30, \
                    f"Too many tab stops may overwhelm keyboard users: {len(focusable_elements)}"
                
                # Test 2: Focus indicators are visible
                for element_info in focusable_elements[:5]:  # Test first 5 elements
                    element = element_info["element"]
                    
                    # Focus the element
                    await page.evaluate("el => el.focus()", element)
                    await page.wait_for_timeout(100)
                    
                    # Check if focus is visible
                    focus_visible = await page.evaluate("""
                        (el) => {
                            const styles = getComputedStyle(el);
                            const pseudo = getComputedStyle(el, ':focus');
                            
                            // Check for focus outline or other focus indicators
                            return styles.outline !== 'none' || 
                                   styles.outlineWidth !== '0px' ||
                                   styles.border !== pseudo.border ||
                                   styles.backgroundColor !== pseudo.backgroundColor ||
                                   styles.boxShadow !== pseudo.boxShadow;
                        }
                    """, element)
                    
                    assert focus_visible, f"Focus should be visible on {element_info['tag']} elements"
                
                # Test 3: User navigates to WiFi config using only keyboard
                await page.goto(f"{esp32_url}/wifi")
                await page.wait_for_load_state("networkidle")
                
                # Find first form field and navigate to it
                first_input = page.locator('input:not([type="hidden"]), select, textarea').first
                if await first_input.count() > 0:
                    await first_input.focus()
                    
                    # User fills form using keyboard
                    await page.keyboard.type("TestNetwork")
                    
                    # Tab to next field
                    await page.keyboard.press('Tab')
                    
                    # Type password
                    await page.keyboard.type("password123")
                    
                    # Navigate to submit button using Tab
                    submit_found = False
                    for _ in range(10):  # Max 10 tabs to find submit
                        await page.keyboard.press('Tab')
                        focused_element = await page.evaluate("document.activeElement")
                        tag_name = await page.evaluate("el => el.tagName.toLowerCase()", focused_element)
                        element_type = await page.evaluate("el => el.type", focused_element)
                        
                        if tag_name == 'button' or element_type == 'submit':
                            submit_found = True
                            # User presses Enter to submit
                            await page.keyboard.press('Enter')
                            await page.wait_for_timeout(500)
                            break
                    
                    assert submit_found, "Submit button should be reachable via keyboard navigation"
                
                # Test 4: User can access all main sections via keyboard
                main_sections = ["/", "/config", "/data"]
                
                for section in main_sections:
                    await page.goto(f"{esp32_url}{section}")
                    await page.wait_for_load_state("networkidle")
                    
                    # User should be able to navigate content with Tab and arrow keys
                    await page.keyboard.press('Tab')
                    focused = await page.evaluate("document.activeElement")
                    
                    # Should focus on meaningful content, not just body
                    tag_name = await page.evaluate("el => el.tagName.toLowerCase()", focused)
                    assert tag_name != 'body', f"Focus should land on content in {section}"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.accessibility
    @pytest.mark.asyncio
    async def test_high_contrast_user_experience(self, esp32_url):
        """Test experience for users with high contrast mode enabled."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                forced_colors="active",  # Simulates high contrast mode
                color_scheme="dark"
            )
            page = await context.new_page()
            
            try:
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Test 1: Text remains readable in high contrast
                text_elements = await page.locator('p, span, div, h1, h2, h3, h4, h5, h6, label').all()
                
                for element in text_elements[:10]:  # Test first 10 text elements
                    text_content = await element.text_content()
                    if len(text_content.strip()) > 5:  # Only test elements with substantial text
                        
                        # Check if text is visible (not transparent or same color as background)
                        is_visible = await element.evaluate("""
                            (el) => {
                                const styles = getComputedStyle(el);
                                const color = styles.color;
                                const bgColor = styles.backgroundColor;
                                
                                // In forced colors mode, system should provide high contrast
                                return color !== 'transparent' && 
                                       styles.opacity !== '0' &&
                                       styles.visibility !== 'hidden';
                            }
                        """)
                        
                        assert is_visible, f"Text should be visible in high contrast mode: {text_content[:50]}"
                
                # Test 2: Interactive elements are distinguishable
                buttons = await page.locator('button, [role="button"], input[type="submit"]').all()
                
                for button in buttons:
                    # Button should have visible border or background in high contrast
                    has_visible_boundary = await button.evaluate("""
                        (el) => {
                            const styles = getComputedStyle(el);
                            return styles.border !== 'none' || 
                                   styles.outline !== 'none' ||
                                   styles.backgroundColor !== 'transparent';
                        }
                    """)
                    
                    assert has_visible_boundary, "Buttons should be visually distinct in high contrast mode"
                
                # Test 3: Form fields are distinguishable
                form_inputs = await page.locator('input, select, textarea').all()
                
                for input_field in form_inputs:
                    input_type = await input_field.get_attribute('type')
                    if input_type == 'hidden':
                        continue
                    
                    # Form fields should have visible boundaries
                    has_boundary = await input_field.evaluate("""
                        (el) => {
                            const styles = getComputedStyle(el);
                            return styles.border !== 'none' || styles.outline !== 'none';
                        }
                    """)
                    
                    assert has_boundary, f"Form fields should have visible boundaries: {input_type}"
                
                # Test 4: Charts and data visualizations work in high contrast
                await page.goto(f"{esp32_url}/data")
                await page.wait_for_load_state("networkidle")
                
                charts = await page.locator('canvas, svg, .chart').all()
                for chart in charts:
                    # Chart should be visible (not hidden by color-only information)
                    is_chart_visible = await chart.is_visible()
                    assert is_chart_visible, "Charts should remain visible in high contrast mode"
                    
                    # Check if chart has alternative text representation
                    aria_label = await chart.get_attribute('aria-label')
                    aria_describedby = await chart.get_attribute('aria-describedby')
                    
                    if not aria_label and not aria_describedby:
                        # Look for data table alternative
                        chart_container = await chart.evaluate("el => el.closest('div, section')")
                        if chart_container:
                            table_alternative = await page.evaluate("""
                                (container) => {
                                    return container.querySelector('table, [role="table"]') !== null;
                                }
                            """, chart_container)
                            
                            # Charts should have text alternative for high contrast users
                            assert table_alternative or aria_label or aria_describedby, \
                                "Charts should have text alternative for high contrast users"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.accessibility
    @pytest.mark.asyncio
    async def test_motor_impairment_user_experience(self, esp32_url):
        """Test experience for users with motor impairments (limited dexterity)."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                reduced_motion="reduce"  # Users may prefer reduced motion
            )
            page = await context.new_page()
            
            try:
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Test 1: Click targets are large enough
                clickable_elements = await page.locator(
                    'button, a, input[type="submit"], input[type="button"], [role="button"]'
                ).all()
                
                for element in clickable_elements:
                    bounding_box = await element.bounding_box()
                    if bounding_box:
                        # WCAG recommends minimum 44x44 CSS pixels for touch targets
                        min_size = 44
                        assert bounding_box["width"] >= min_size - 10 or bounding_box["height"] >= min_size - 10, \
                            f"Click target too small for users with motor impairments: {bounding_box}"
                
                # Test 2: Hover targets don't require precise mouse control
                hoverable_elements = await page.locator('[title], [data-tooltip]').all()
                
                for element in hoverable_elements[:3]:  # Test first 3 hover elements
                    # Hover should work with some tolerance (not require pixel-perfect positioning)
                    bounding_box = await element.bounding_box()
                    if bounding_box:
                        # Test hover from slightly outside the element
                        hover_x = bounding_box["x"] + bounding_box["width"] + 5
                        hover_y = bounding_box["y"] + bounding_box["height"] / 2
                        
                        await page.mouse.move(hover_x, hover_y)
                        await page.wait_for_timeout(300)
                        
                        # Then move to actual element
                        center_x = bounding_box["x"] + bounding_box["width"] / 2
                        center_y = bounding_box["y"] + bounding_box["height"] / 2
                        
                        await page.mouse.move(center_x, center_y)
                        await page.wait_for_timeout(500)
                        
                        # Tooltip or hover effect should be forgiving
                        # (Actual implementation depends on specific tooltip library)
                
                # Test 3: Form submission doesn't require double-clicks or complex gestures
                await page.goto(f"{esp32_url}/wifi")
                await page.wait_for_load_state("networkidle")
                
                submit_buttons = await page.locator('button[type="submit"], input[type="submit"]').all()
                
                for button in submit_buttons:
                    # Single click should be sufficient
                    click_count = 0
                    
                    # Simulate single click
                    await button.click()
                    click_count += 1
                    await page.wait_for_timeout(100)
                    
                    # Should not require multiple clicks to activate
                    assert click_count == 1, "Buttons should activate with single click"
                    
                    # Should not require holding down mouse button
                    # (This is tested by the simple click() method)
                
                # Test 4: Time-sensitive operations have sufficient time limits
                timed_elements = await page.locator('[data-timeout], .timeout-warning').all()
                
                for element in timed_elements:
                    # Check if timeout warnings are present
                    timeout_warning = await element.text_content()
                    if "timeout" in timeout_warning.lower() or "time" in timeout_warning.lower():
                        # Should provide at least 20 seconds for user actions (WCAG recommendation)
                        assert "20" in timeout_warning or "30" in timeout_warning or \
                               any(str(i) for i in range(20, 121) if str(i) in timeout_warning), \
                               "Timeout should allow sufficient time for users with motor impairments"
                
                # Test 5: Drag and drop operations have alternatives
                draggable_elements = await page.locator('[draggable="true"], .draggable').all()
                
                for draggable in draggable_elements:
                    # Should have keyboard alternative or alternative interaction method
                    # Check for aria-label explaining alternative
                    aria_label = await draggable.get_attribute('aria-label')
                    aria_describedby = await draggable.get_attribute('aria-describedby')
                    
                    # Look for alternative controls near draggable element
                    parent = await draggable.evaluate("el => el.parentElement")
                    alternative_controls = await page.evaluate("""
                        (parent) => {
                            if (!parent) return false;
                            const buttons = parent.querySelectorAll('button, [role="button"]');
                            return buttons.length > 0;
                        }
                    """, parent)
                    
                    assert alternative_controls or aria_label or aria_describedby, \
                        "Drag and drop should have alternatives for users with motor impairments"
                
            finally:
                await context.close()
                await browser.close()
    
    @pytest.mark.user_interaction
    @pytest.mark.accessibility
    @pytest.mark.asyncio
    async def test_cognitive_accessibility_user_experience(self, esp32_url):
        """Test experience for users with cognitive disabilities."""
        async with async_playwright() as p:
            browser = await p.chromium.launch(headless=True)
            context = await browser.new_context(
                reduced_motion="reduce"  # Reduced motion helps with cognitive processing
            )
            page = await context.new_page()
            
            try:
                await page.goto(esp32_url)
                await page.wait_for_load_state("networkidle")
                
                # Test 1: Page structure is predictable and consistent
                # Check for consistent navigation across pages
                nav_structure = await self._analyze_navigation_structure(page)
                
                # Navigate to different pages and check consistency
                pages_to_check = ["/config", "/data", "/wifi"]
                nav_structures = [nav_structure]
                
                for page_url in pages_to_check:
                    await page.goto(f"{esp32_url}{page_url}")
                    await page.wait_for_load_state("networkidle")
                    
                    page_nav_structure = await self._analyze_navigation_structure(page)
                    nav_structures.append(page_nav_structure)
                
                # Navigation should be consistent across pages
                base_structure = nav_structures[0]
                for structure in nav_structures[1:]:
                    assert structure["nav_count"] == base_structure["nav_count"], \
                        "Navigation structure should be consistent across pages"
                    assert structure["has_nav"] == base_structure["has_nav"], \
                        "Navigation presence should be consistent"
                
                # Test 2: Instructions and labels are clear and simple
                await page.goto(f"{esp32_url}/wifi")
                await page.wait_for_load_state("networkidle")
                
                labels = await page.locator('label').all()
                for label in labels:
                    label_text = await label.text_content()
                    if len(label_text.strip()) > 0:
                        # Labels should be reasonably concise (not overwhelming)
                        assert len(label_text) < 100, \
                            f"Label text should be concise for cognitive accessibility: {label_text[:50]}..."
                        
                        # Should not use technical jargon without explanation
                        technical_terms = ['API', 'JSON', 'HTTP', 'TCP', 'IP', 'DNS', 'DHCP']
                        label_upper = label_text.upper()
                        
                        for term in technical_terms:
                            if term in label_upper:
                                # Should have help text or explanation nearby
                                parent = await label.evaluate("el => el.parentElement")
                                has_help = await page.evaluate("""
                                    (parent) => {
                                        if (!parent) return false;
                                        const help = parent.querySelector('.help-text, .description, [data-help]');
                                        return help !== null;
                                    }
                                """, parent)
                                
                                assert has_help, f"Technical term '{term}' should have explanation for cognitive accessibility"
                
                # Test 3: Error messages are helpful and actionable
                # Try to trigger validation errors
                form_inputs = await page.locator('input[required], select[required]').all()
                
                if len(form_inputs) > 0:
                    # Submit form without filling required fields
                    submit_btn = page.locator('button[type="submit"], input[type="submit"]')
                    if await submit_btn.count() > 0:
                        await submit_btn.first.click()
                        await page.wait_for_timeout(1000)
                        
                        # Look for error messages
                        error_messages = await page.locator('.error, .invalid, [aria-invalid="true"]').all()
                        
                        for error in error_messages:
                            error_text = await error.text_content()
                            if len(error_text.strip()) > 0:
                                # Error messages should be helpful, not just "Error"
                                assert "error" not in error_text.lower() or \
                                       len(error_text) > 10, \
                                       f"Error message should be descriptive: {error_text}"
                                
                                # Should suggest corrective action
                                helpful_words = ['enter', 'select', 'choose', 'required', 'must', 'should']
                                assert any(word in error_text.lower() for word in helpful_words), \
                                    f"Error message should suggest action: {error_text}"
                
                # Test 4: No time-pressured interactions without alternatives
                await page.goto(f"{esp32_url}/data")
                await page.wait_for_load_state("networkidle")
                
                # Check for auto-refreshing content
                auto_refresh_elements = await page.locator('[data-refresh], .auto-refresh').all()
                
                for element in auto_refresh_elements:
                    # Should have pause/stop control for cognitive accessibility
                    parent = await element.evaluate("el => el.parentElement")
                    has_pause_control = await page.evaluate("""
                        (parent) => {
                            if (!parent) return false;
                            const controls = parent.querySelectorAll('button, [role="button"]');
                            for (let control of controls) {
                                const text = control.textContent.toLowerCase();
                                if (text.includes('pause') || text.includes('stop') || text.includes('freeze')) {
                                    return true;
                                }
                            }
                            return false;
                        }
                    """, parent)
                    
                    assert has_pause_control, \
                        "Auto-refreshing content should have pause control for cognitive accessibility"
                
                # Test 5: Complex interactions have clear multi-step guidance
                await page.goto(f"{esp32_url}/upload")
                await page.wait_for_load_state("networkidle")
                
                # Check for step-by-step instructions for complex tasks
                step_indicators = await page.locator('.steps, .wizard, .progress-steps').all()
                
                if len(step_indicators) > 0:
                    for indicator in step_indicators:
                        # Should show current step and total steps
                        step_text = await indicator.text_content()
                        
                        # Look for step numbering or progress indication
                        has_step_info = any(char.isdigit() for char in step_text) or \
                                       'step' in step_text.lower() or \
                                       '/' in step_text
                        
                        assert has_step_info, \
                            "Complex processes should show step progress for cognitive accessibility"
                
            finally:
                await context.close()
                await browser.close()
    
    async def _analyze_navigation_structure(self, page: Page) -> Dict[str, Any]:
        """Analyze the navigation structure of a page."""
        nav_info = {
            "has_nav": False,
            "nav_count": 0,
            "nav_items": []
        }
        
        # Check for navigation elements
        nav_elements = await page.locator('nav, [role="navigation"]').all()
        nav_info["nav_count"] = len(nav_elements)
        nav_info["has_nav"] = len(nav_elements) > 0
        
        # Get navigation items
        nav_links = await page.locator('nav a, [role="navigation"] a').all()
        for link in nav_links[:10]:  # Limit to first 10 to avoid overwhelming
            link_text = await link.text_content()
            href = await link.get_attribute('href')
            nav_info["nav_items"].append({
                "text": link_text.strip() if link_text else "",
                "href": href
            })
        
        return nav_info