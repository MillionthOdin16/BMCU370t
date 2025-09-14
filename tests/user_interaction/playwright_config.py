"""
Playwright configuration for ESP32 user interaction tests.

Configuration for Playwright browser automation to test ESP32 web interface
with real user interaction patterns.
"""

import os
from pathlib import Path


# Browser configuration for testing
BROWSER_CONFIG = {
    "headless": True,  # Set to False for debugging
    "slow_mo": 100,   # Slow down actions to be more human-like
    "timeout": 30000,  # 30 second timeout for actions
    "viewport": {"width": 1920, "height": 1080},
    "ignore_https_errors": True,  # ESP32 often uses self-signed certificates
    "record_video_dir": "test-results/videos",
    "record_har_path": "test-results/network.har"
}

# Mobile device configurations
MOBILE_DEVICES = {
    "iPhone 13": {
        "viewport": {"width": 390, "height": 844},
        "user_agent": "Mozilla/5.0 (iPhone; CPU iPhone OS 15_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.0 Mobile/15E148 Safari/604.1",
        "device_scale_factor": 3,
        "is_mobile": True,
        "has_touch": True
    },
    "Pixel 6": {
        "viewport": {"width": 412, "height": 915},
        "user_agent": "Mozilla/5.0 (Linux; Android 12; Pixel 6) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/96.0.4664.45 Mobile Safari/537.36",
        "device_scale_factor": 2.625,
        "is_mobile": True,
        "has_touch": True
    },
    "iPad Air": {
        "viewport": {"width": 820, "height": 1180},
        "user_agent": "Mozilla/5.0 (iPad; CPU OS 15_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.0 Mobile/15E148 Safari/604.1",
        "device_scale_factor": 2,
        "is_mobile": True,
        "has_touch": True
    }
}

# ESP32 test server configurations
ESP32_TEST_CONFIGS = {
    "default": {
        "url": "http://192.168.4.1",  # Default ESP32 AP mode
        "endpoints": {
            "/": "main_dashboard",
            "/data": "sensor_data",
            "/config": "device_config", 
            "/wifi": "wifi_setup",
            "/upload": "firmware_upload",
            "/api/status": "status_api",
            "/api/data": "data_api"
        }
    },
    "mock": {
        "url": "http://localhost:8080",  # Mock server for testing
        "endpoints": {
            "/": "main_dashboard",
            "/data": "sensor_data",
            "/config": "device_config",
            "/wifi": "wifi_setup", 
            "/upload": "firmware_upload",
            "/api/status": "status_api",
            "/api/data": "data_api"
        }
    }
}

# Test data directories
TEST_RESULTS_DIR = Path("test-results")
SCREENSHOTS_DIR = TEST_RESULTS_DIR / "screenshots"
VIDEOS_DIR = TEST_RESULTS_DIR / "videos"
VISUAL_BASELINE_DIR = TEST_RESULTS_DIR / "visual-regression" / "baselines"
VISUAL_COMPARISON_DIR = TEST_RESULTS_DIR / "visual-regression" / "comparisons"

# Create directories if they don't exist
for directory in [TEST_RESULTS_DIR, SCREENSHOTS_DIR, VIDEOS_DIR, VISUAL_BASELINE_DIR, VISUAL_COMPARISON_DIR]:
    directory.mkdir(parents=True, exist_ok=True)

# Performance thresholds for user experience
PERFORMANCE_THRESHOLDS = {
    "page_load": {
        "excellent": 1000,    # < 1s feels instant
        "good": 2500,        # < 2.5s feels fast  
        "acceptable": 5000,   # < 5s is tolerable
        "poor": 10000        # > 10s feels broken
    },
    "interaction": {
        "instant": 100,       # < 100ms feels instant
        "fast": 300,         # < 300ms feels responsive
        "noticeable": 1000,  # < 1s is acceptable
        "sluggish": 3000     # > 3s feels broken
    },
    "visual_feedback": {
        "immediate": 16,      # One frame at 60fps
        "responsive": 100,    # User notices but acceptable
        "delayed": 300       # User definitely notices delay
    }
}

# Accessibility testing configuration
ACCESSIBILITY_CONFIG = {
    "color_contrast_ratio": 4.5,  # WCAG AA standard
    "minimum_touch_target": 44,   # 44x44 CSS pixels (iOS HIG)
    "maximum_heading_skip": 1,    # Don't skip heading levels
    "required_landmarks": ["main", "navigation"],
    "keyboard_navigation_timeout": 5000
}

# Visual regression testing configuration
VISUAL_REGRESSION_CONFIG = {
    "threshold": 0.05,           # 5% visual difference threshold
    "full_page": True,           # Capture full page
    "mask_dynamic_content": True, # Mask timestamps and changing data
    "animation_handling": "disabled",  # Disable animations for consistency
    "browsers": ["chromium", "firefox", "webkit"]
}

# Network failure simulation patterns
NETWORK_FAILURE_PATTERNS = {
    "complete_offline": {
        "handler": "abort_all_requests"
    },
    "slow_connection": {
        "handler": "delay_requests",
        "delay_ms": 5000
    },
    "intermittent_failures": {
        "handler": "random_failures", 
        "failure_rate": 0.3
    },
    "api_only_failure": {
        "handler": "fail_api_only"
    },
    "timeout_errors": {
        "handler": "timeout_requests"
    }
}

# Test user scenarios
USER_SCENARIOS = {
    "first_time_user": {
        "description": "User discovering the interface for the first time",
        "actions": ["explore_navigation", "read_help_text", "try_basic_features"],
        "expectations": ["clear_navigation", "helpful_onboarding", "obvious_next_steps"]
    },
    "regular_user": {
        "description": "User performing routine monitoring tasks",
        "actions": ["check_sensor_data", "review_alerts", "adjust_settings"],
        "expectations": ["fast_access", "current_data", "efficient_workflow"]
    },
    "mobile_user": {
        "description": "User accessing interface on mobile device",
        "actions": ["touch_navigation", "zoom_content", "rotate_device"],
        "expectations": ["touch_friendly", "readable_text", "responsive_layout"]
    },
    "accessibility_user": {
        "description": "User with assistive technology",
        "actions": ["keyboard_navigation", "screen_reader_use", "high_contrast"],
        "expectations": ["keyboard_accessible", "screen_reader_friendly", "clear_focus"]
    }
}

def get_test_config():
    """Get test configuration based on environment."""
    config = {
        "browser": BROWSER_CONFIG.copy(),
        "mobile_devices": MOBILE_DEVICES,
        "esp32": ESP32_TEST_CONFIGS["mock"],  # Use mock by default
        "performance": PERFORMANCE_THRESHOLDS,
        "accessibility": ACCESSIBILITY_CONFIG,
        "visual_regression": VISUAL_REGRESSION_CONFIG,
        "network_patterns": NETWORK_FAILURE_PATTERNS,
        "user_scenarios": USER_SCENARIOS
    }
    
    # Override with environment variables if set
    if os.getenv("ESP32_TEST_URL"):
        config["esp32"]["url"] = os.getenv("ESP32_TEST_URL")
    
    if os.getenv("PLAYWRIGHT_HEADLESS") == "false":
        config["browser"]["headless"] = False
    
    if os.getenv("PLAYWRIGHT_SLOW_MO"):
        config["browser"]["slow_mo"] = int(os.getenv("PLAYWRIGHT_SLOW_MO"))
    
    return config