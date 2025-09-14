# User Interaction Tests for ESP32 Web Interface

This directory contains comprehensive user interaction tests using Playwright to simulate real user behavior and catch issues before actual users encounter them.

## Overview

The user interaction tests focus on testing the ESP32 web interface exactly as real users would interact with it, rather than just testing individual components in isolation. This approach helps identify usability issues, accessibility problems, and user experience regressions that traditional unit tests might miss.

## Test Categories

### 1. Real User Interactions (`test_real_user_interactions.py`)
Tests core user workflows and navigation patterns:
- **Dashboard Navigation**: Complete user flow through interface sections
- **WiFi Configuration**: Step-by-step WiFi setup workflow  
- **Data Monitoring**: Real-time sensor data viewing experience
- **Firmware Updates**: File upload and progress monitoring
- **Error Recovery**: How users experience and recover from errors

### 2. Mobile User Experience (`test_mobile_user_interactions.py`)
Tests mobile-specific user interactions:
- **Touch Navigation**: Hamburger menus, tap targets, swipe gestures
- **Responsive Design**: Layout adaptation across device sizes
- **Form Interactions**: Mobile keyboard, input focus, validation
- **Orientation Changes**: Portrait/landscape layout transitions
- **Performance**: Touch responsiveness, scroll smoothness, memory usage

### 3. Accessibility Testing (`test_accessibility_user_experience.py`) 
Tests for users with disabilities and assistive technologies:
- **Screen Reader Support**: Proper headings, labels, live regions
- **Keyboard Navigation**: Tab order, focus indicators, keyboard shortcuts
- **High Contrast Mode**: Visual accessibility for low vision users
- **Motor Impairments**: Large touch targets, timeout considerations
- **Cognitive Accessibility**: Clear instructions, consistent navigation

### 4. Visual Regression Testing (`test_visual_regression.py`)
Catches visual changes that might impact user experience:
- **Layout Consistency**: Page structure across different views
- **Responsive Breakpoints**: Visual layout at different screen sizes
- **Form States**: Empty, filled, error, and success states
- **Loading States**: Progress indicators and loading animations
- **Cross-Browser**: Visual consistency across Chrome, Firefox, Safari

### 5. Network Failure Recovery (`test_network_failure_recovery.py`)
Tests user experience during network issues:
- **Complete Offline**: How interface behaves when fully disconnected
- **Slow Connections**: Loading states and user feedback on slow networks
- **Intermittent Failures**: Random network errors and retry mechanisms
- **API Failures**: Graceful degradation when data APIs are unavailable
- **WebSocket Issues**: Real-time data connection failures and recovery

### 6. User Performance Experience (`test_user_performance_experience.py`)
Performance testing from user perspective:
- **Perceived Load Speed**: First paint, content ready, interactive timing
- **Interaction Responsiveness**: Button clicks, form input, navigation speed
- **Real-time Updates**: Data refresh rates and visual smoothness
- **Memory Impact**: How memory usage affects user experience over time
- **Slow Device Performance**: Testing on simulated low-end devices

## Key Features

### Real User Simulation
- **Human-like Timing**: Actions include realistic delays and pauses
- **Actual Browser Behavior**: Tests run in real browsers, not headless simulations
- **Touch and Mouse Events**: Proper event simulation for different input methods
- **User Journey Focus**: Tests complete workflows, not isolated functions

### Comprehensive Device Coverage
- **Desktop**: Multiple screen resolutions and browsers
- **Mobile Phones**: iPhone, Android with actual device characteristics
- **Tablets**: iPad and Android tablets with touch interfaces
- **Accessibility**: Screen readers, keyboard-only, high contrast modes

### User-Centric Metrics
- **Perceived Performance**: How fast the interface feels to users
- **Usability Metrics**: Error rates, task completion times, user satisfaction
- **Accessibility Compliance**: WCAG 2.1 AA standards
- **Visual Consistency**: Detecting unintended UI changes

## Running the Tests

### Install Dependencies
```bash
# Install Playwright and browsers
pip install playwright pytest-playwright
playwright install

# Install additional dependencies
pip install -r requirements-test.txt
```

### Run All User Interaction Tests
```bash
python run_tests.py --all-user-tests
```

### Run Specific Test Categories
```bash
# Real user interactions
python run_tests.py --user-interaction

# Mobile experience
python run_tests.py --mobile

# Accessibility testing
python run_tests.py --accessibility

# Visual regression
python run_tests.py --visual-regression

# Network failure scenarios
python run_tests.py --network-failure

# User performance experience
python run_tests.py --user-performance
```

### Run with Visual Output (for debugging)
```bash
# Run with visible browser windows
PLAYWRIGHT_HEADLESS=false python run_tests.py --user-interaction --verbose

# Slow down actions to observe
PLAYWRIGHT_SLOW_MO=1000 python run_tests.py --mobile --verbose
```

## Configuration

### Test URLs
Tests can run against different ESP32 configurations:
```bash
# Test against real ESP32 device
ESP32_TEST_URL=http://192.168.4.1 python run_tests.py --user-interaction

# Test against development mock server
ESP32_TEST_URL=http://localhost:8080 python run_tests.py --user-interaction
```

### Browser Selection
```bash
# Test on specific browser
pytest user_interaction/ --browser chromium
pytest user_interaction/ --browser firefox
pytest user_interaction/ --browser webkit
```

## Test Results

### Screenshots and Videos
- **Screenshots**: Captured automatically on test failures
- **Videos**: Full test execution recordings for debugging
- **HAR Files**: Network traffic logs for performance analysis

### Visual Regression Baselines
- **Baseline Images**: Reference screenshots for visual comparison
- **Difference Images**: Highlighted changes between test runs
- **Cross-Browser**: Consistent visuals across different browsers

### Performance Metrics
- **Load Times**: Page load performance from user perspective
- **Interaction Times**: Response times for user actions
- **Memory Usage**: Memory consumption during user sessions
- **Frame Rates**: Visual smoothness metrics

## Test Data and Fixtures

### Mock ESP32 Server
For testing without physical hardware, the tests include a mock ESP32 server that simulates:
- **API Endpoints**: Realistic sensor data and configuration responses
- **WebSocket Connections**: Real-time data streaming simulation
- **File Upload**: Firmware update progress simulation
- **Error Conditions**: Network failures and timeout scenarios

### Test User Personas
Tests simulate different types of users:
- **First-time User**: Discovering the interface, needs clear guidance
- **Regular User**: Routine monitoring tasks, expects efficiency
- **Mobile User**: Touch-based interaction, responsive design needs
- **Accessibility User**: Assistive technology, keyboard navigation

## Troubleshooting

### Common Issues

#### Browser Installation
```bash
# If browser installation fails
playwright install --force

# Install system dependencies (Linux)
playwright install-deps
```

#### Timeout Issues
```bash
# Increase timeout for slow connections
pytest user_interaction/ --timeout=60
```

#### Visual Regression Failures
```bash
# Update baselines after intentional changes
pytest user_interaction/ --visual-regression --update-baselines
```

### Debugging Tips

1. **Run with Visible Browser**: Set `PLAYWRIGHT_HEADLESS=false`
2. **Slow Down Actions**: Set `PLAYWRIGHT_SLOW_MO=1000`
3. **Check Screenshots**: Review failure screenshots in `test-results/`
4. **Network Logs**: Examine HAR files for network issues
5. **Console Logs**: Check browser console for JavaScript errors

## Integration with CI/CD

### GitHub Actions
The tests integrate with GitHub Actions for automated testing:
- **Parallel Execution**: Tests run across multiple browsers simultaneously
- **Artifact Storage**: Screenshots and videos saved for failed tests
- **Performance Monitoring**: Track performance metrics over time
- **Visual Regression**: Detect unintended UI changes

### Test Reports
Comprehensive HTML reports include:
- **Test Results**: Pass/fail status with detailed error information
- **Performance Metrics**: Load times, interaction responsiveness
- **Accessibility Report**: WCAG compliance status
- **Visual Differences**: Before/after comparison images
- **Network Analysis**: API performance and error rates

## Best Practices

### Writing New Tests

1. **Think Like a User**: Focus on complete workflows, not individual functions
2. **Use Realistic Data**: Test with data that mirrors real usage patterns
3. **Handle Variability**: Account for timing differences and async operations
4. **Test Error Paths**: Ensure users get helpful feedback when things go wrong
5. **Consider All Users**: Include accessibility and mobile considerations

### Test Maintenance

1. **Update Baselines**: Refresh visual regression baselines after UI changes
2. **Review Timeouts**: Adjust timeouts for different network conditions
3. **Monitor Performance**: Track performance trends over time
4. **Cross-Browser Testing**: Ensure compatibility across all supported browsers

### Performance Considerations

1. **Parallel Execution**: Run tests in parallel to reduce total time
2. **Smart Caching**: Cache browser installations and test results
3. **Selective Testing**: Run full suite on main branch, subset on PRs
4. **Resource Limits**: Monitor memory and CPU usage during test execution

## Contributing

When adding new user interaction tests:

1. **Focus on User Value**: Test features that directly impact user experience
2. **Document User Scenarios**: Clearly describe the user workflow being tested
3. **Include Error Cases**: Test how users experience and recover from errors
4. **Consider Edge Cases**: Test with slow networks, small screens, etc.
5. **Maintain Test Independence**: Each test should be able to run in isolation

## Related Documentation

- **[Main Test Framework](../README.md)**: Overview of entire test suite
- **[Playwright Configuration](playwright_config.py)**: Test configuration options
- **[Test Results Analysis](../VALIDATION_RESULTS.md)**: Interpreting test results
- **[Performance Benchmarks](../TESTING_OPTIMIZATION_GUIDE.md)**: Performance optimization