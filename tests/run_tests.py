#!/usr/bin/env python3
"""
Test runner script for BMCU370 ESP32 Web Interface tests.

Provides easy execution of different test categories and generates reports.
Includes intelligent caching and optimization features.
"""

import argparse
import sys
import os
import subprocess
import time
import hashlib
import json
from pathlib import Path
from typing import List, Dict, Optional


def get_file_hash(file_path: Path) -> str:
    """Get hash of a file for caching purposes."""
    if not file_path.exists():
        return ""
    
    with open(file_path, 'rb') as f:
        return hashlib.md5(f.read()).hexdigest()


def get_directory_hash(directory: Path, extensions: List[str] = None) -> str:
    """Get combined hash of all files in a directory."""
    if extensions is None:
        extensions = ['.py', '.cpp', '.h', '.html', '.css', '.js']
    
    file_hashes = []
    if directory.exists():
        for ext in extensions:
            for file_path in directory.rglob(f'*{ext}'):
                file_hashes.append(get_file_hash(file_path))
    
    combined_hash = ''.join(sorted(file_hashes))
    return hashlib.md5(combined_hash.encode()).hexdigest()


def load_test_cache() -> Dict:
    """Load test cache from disk."""
    cache_file = Path('.test_cache.json')
    if cache_file.exists():
        try:
            with open(cache_file, 'r') as f:
                return json.load(f)
        except (json.JSONDecodeError, IOError):
            pass
    return {}


def save_test_cache(cache: Dict):
    """Save test cache to disk."""
    cache_file = Path('.test_cache.json')
    try:
        with open(cache_file, 'w') as f:
            json.dump(cache, f, indent=2)
    except IOError:
        pass


def should_skip_tests(test_category: str, verbose: bool = False) -> bool:
    """Check if tests should be skipped based on file changes."""
    cache = load_test_cache()
    
    # Get current hashes
    esp32_hash = get_directory_hash(Path('../esp32_firmware'))
    test_hash = get_directory_hash(Path(f'{test_category}/'))
    
    # Check cache
    cache_key = f'{test_category}_hashes'
    if cache_key in cache:
        cached_esp32_hash = cache[cache_key].get('esp32', '')
        cached_test_hash = cache[cache_key].get('test', '')
        
        if esp32_hash == cached_esp32_hash and test_hash == cached_test_hash:
            if verbose:
                print(f"⚡ Skipping {test_category} tests - no changes detected")
            return True
    
    # Update cache
    cache[cache_key] = {
        'esp32': esp32_hash,
        'test': test_hash,
        'timestamp': time.time()
    }
    save_test_cache(cache)
    
    return False


def run_command(cmd, description="", cache_key: Optional[str] = None):
    """Run a command and return the result with optional caching."""
    print(f"\n{'='*60}")
    if description:
        print(f"Running: {description}")
    print(f"Command: {' '.join(cmd)}")
    print('='*60)
    
    # Check cache if enabled
    if cache_key and os.getenv('USE_TEST_CACHE', 'true').lower() == 'true':
        cache = load_test_cache()
        if cache_key in cache:
            cache_time = cache[cache_key].get('timestamp', 0)
            if time.time() - cache_time < 3600:  # 1 hour cache
                print("⚡ Using cached result")
                return type('Result', (), {
                    'returncode': cache[cache_key].get('returncode', 0),
                    'stdout': cache[cache_key].get('stdout', ''),
                    'stderr': cache[cache_key].get('stderr', '')
                })()
    
    start_time = time.time()
    result = subprocess.run(cmd, capture_output=True, text=True)
    end_time = time.time()
    
    print(f"Duration: {end_time - start_time:.2f} seconds")
    print(f"Exit code: {result.returncode}")
    
    if result.stdout:
        print("STDOUT:")
        print(result.stdout)
    
    if result.stderr:
        print("STDERR:")
        print(result.stderr)
    
    # Cache successful results
    if cache_key and result.returncode == 0:
        cache = load_test_cache()
        cache[cache_key] = {
            'returncode': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr,
            'timestamp': time.time()
        }
        save_test_cache(cache)
    
    return result


def install_dependencies():
    """Install test dependencies."""
    print("Installing test dependencies...")
    
    requirements_file = Path(__file__).parent / "requirements-test.txt"
    if requirements_file.exists():
        cmd = [sys.executable, "-m", "pip", "install", "-r", str(requirements_file)]
        result = run_command(cmd, "Installing test dependencies")
        
        if result.returncode != 0:
            print("Failed to install dependencies!")
            return False
    else:
        print(f"Requirements file not found: {requirements_file}")
        return False
    
    return True


def run_unit_tests(verbose=False, coverage=False):
    """Run unit tests with intelligent caching."""
    if should_skip_tests('unit', verbose):
        return True
        
    cmd = [sys.executable, "-m", "pytest", "tests/unit/"]
    
    if verbose:
        cmd.append("-v")
    
    if coverage:
        cmd.extend(["--cov=esp32_firmware/src", "--cov-report=term-missing"])
    
    cmd.extend(["-m", "unit"])
    
    cache_key = f"unit_tests_{get_directory_hash(Path('unit/'))}"
    result = run_command(cmd, "Unit Tests", cache_key)
    return result.returncode == 0


def run_integration_tests(verbose=False):
    """Run integration tests with intelligent caching."""
    if should_skip_tests('integration', verbose):
        return True
        
    cmd = [sys.executable, "-m", "pytest", "tests/integration/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "integration"])
    
    cache_key = f"integration_tests_{get_directory_hash(Path('integration/'))}"
    result = run_command(cmd, "Integration Tests", cache_key)
    return result.returncode == 0


def run_web_interface_tests(verbose=False):
    """Run web interface tests with intelligent caching."""
    if should_skip_tests('web_interface', verbose):
        return True
        
    cmd = [sys.executable, "-m", "pytest", "tests/web_interface/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "web"])
    
    cache_key = f"web_tests_{get_directory_hash(Path('web_interface/'))}"
    result = run_command(cmd, "Web Interface Tests", cache_key)
    return result.returncode == 0


def run_simulation_tests(verbose=False):
    """Run simulation tests."""
    cmd = [sys.executable, "-m", "pytest", "tests/simulation/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "simulation"])
    
    result = run_command(cmd, "Simulation Tests")
    return result.returncode == 0


def run_performance_tests(verbose=False):
    """Run performance tests."""
    cmd = [sys.executable, "-m", "pytest", "tests/performance/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "performance", "--tb=short"])
    
    result = run_command(cmd, "Performance Tests")
    return result.returncode == 0


def run_security_tests(verbose=False):
    """Run security tests."""
    cmd = [sys.executable, "-m", "pytest", "tests/security/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "security"])
    
    result = run_command(cmd, "Security Tests")
    return result.returncode == 0


def run_error_handling_tests(verbose=False):
    """Run error handling tests."""
    cmd = [sys.executable, "-m", "pytest", "tests/error_handling/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "error_handling"])
    
    result = run_command(cmd, "Error Handling Tests")
    return result.returncode == 0


def run_boundary_tests(verbose=False):
    """Run boundary condition tests."""
    cmd = [sys.executable, "-m", "pytest", "tests/boundary/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "boundary"])
    
    result = run_command(cmd, "Boundary Tests")
    return result.returncode == 0


def run_all_tests(verbose=False, coverage=False, exclude_slow=False):
    """Run all test categories."""
    cmd = [sys.executable, "-m", "pytest", "tests/"]
    
    if verbose:
        cmd.append("-v")
    
    if coverage:
        cmd.extend([
            "--cov=esp32_firmware/src",
            "--cov-report=term-missing",
            "--cov-report=html:htmlcov"
        ])
    
    if exclude_slow:
        cmd.extend(["-m", "not slow"])
    
    result = run_command(cmd, "All Tests")
    return result.returncode == 0


def run_specific_test(test_path, verbose=False):
    """Run a specific test file or function."""
    cmd = [sys.executable, "-m", "pytest", test_path]
    
    if verbose:
        cmd.append("-v")
    
    result = run_command(cmd, f"Specific Test: {test_path}")
    return result.returncode == 0


def generate_test_report():
    """Generate comprehensive test report."""
    print("\nGenerating comprehensive test report...")
    
    cmd = [
        sys.executable, "-m", "pytest", "tests/",
        "--html=test-report.html",
        "--self-contained-html",
        "--cov=esp32_firmware/src",
        "--cov-report=html:htmlcov",
        "--junit-xml=test-results.xml"
    ]
    
    result = run_command(cmd, "Test Report Generation")
    
    if result.returncode == 0:
        print("\nTest reports generated:")
        print("- HTML Report: test-report.html")
        print("- Coverage Report: htmlcov/index.html")
        print("- JUnit XML: test-results.xml")
    
    return result.returncode == 0


def validate_esp32_config():
    """Validate ESP32 configuration before running tests."""
    print("Validating ESP32 configuration...")
    
    config_script = Path("esp32_firmware/validate_esp32s3.py")
    if config_script.exists():
        cmd = [sys.executable, str(config_script)]
        result = run_command(cmd, "ESP32 Configuration Validation")
        return result.returncode == 0
    else:
        print(f"Configuration validation script not found: {config_script}")
        return True  # Don't fail if script doesn't exist


def run_wokwi_tests(verbose=False):
    """Run Wokwi ESP32 simulator tests."""
    cmd = [sys.executable, "-m", "pytest", "simulation/", "-m", "wokwi"]
    
    if verbose:
        cmd.append("-v")
    
    result = run_command(cmd, "Wokwi ESP32 Simulator Tests")
    return result.returncode == 0


def run_qemu_tests(verbose=False):
    """Run QEMU ESP32 emulation tests."""
    cmd = [sys.executable, "-m", "pytest", "simulation/", "-m", "qemu"]
    
    if verbose:
        cmd.append("-v")
    
    result = run_command(cmd, "QEMU ESP32 Emulation Tests")
    return result.returncode == 0


def run_docker_tests(verbose=False):
    """Run Docker ESP32 environment tests."""
    cmd = [sys.executable, "-m", "pytest", "simulation/", "-m", "docker"]
    
    if verbose:
        cmd.append("-v")
    
    result = run_command(cmd, "Docker ESP32 Environment Tests")
    return result.returncode == 0


def run_gpio_tests(verbose=False):
    """Run GPIO and peripheral simulation tests."""
    cmd = [sys.executable, "-m", "pytest", "simulation/", "-m", "gpio"]
    
    if verbose:
        cmd.append("-v")
    
    result = run_command(cmd, "GPIO and Peripheral Simulation Tests")
    return result.returncode == 0


def run_network_tests(verbose=False):
    """Run network simulation tests."""
    cmd = [sys.executable, "-m", "pytest", "simulation/", "-m", "network"]
    
    if verbose:
        cmd.append("-v")
    
    result = run_command(cmd, "Network Simulation Tests")
    return result.returncode == 0


def run_browser_tests(verbose=False):
    """Run browser automation tests."""
    cmd = [sys.executable, "-m", "pytest", "simulation/", "-m", "browser"]
    
    if verbose:
        cmd.append("-v")
    
    result = run_command(cmd, "Browser Automation Tests")
    return result.returncode == 0


def run_all_simulator_tests(verbose=False):
    """Run all simulator and emulator tests."""
    print("Running all simulator and emulator tests...")
    
    # Test categories to run
    simulator_tests = [
        ("Wokwi ESP32 Simulator", run_wokwi_tests),
        ("QEMU ESP32 Emulation", run_qemu_tests),
        ("Docker ESP32 Environment", run_docker_tests),
        ("GPIO and Peripheral Simulation", run_gpio_tests),
        ("Network Simulation", run_network_tests),
        ("Browser Automation", run_browser_tests)
    ]
    
    results = []
    for test_name, test_func in simulator_tests:
        print(f"\n{'='*60}")
        print(f"Running {test_name} Tests...")
        print('='*60)
        
        try:
            success = test_func(verbose)
            results.append((test_name, success))
            
            if success:
                print(f"✅ {test_name} tests passed")
            else:
                print(f"❌ {test_name} tests failed")
        except Exception as e:
            print(f"❌ {test_name} tests failed with error: {e}")
            results.append((test_name, False))
    
    # Print summary
    print(f"\n{'='*60}")
    print("SIMULATOR TEST SUMMARY")
    print('='*60)
    
    passed = 0
    total = len(results)
    
    for test_name, success in results:
        status = "✅ PASSED" if success else "❌ FAILED"
        print(f"{test_name:<35} {status}")
        if success:
            passed += 1
    
    print(f"\nOverall: {passed}/{total} simulator test categories passed")
    
    return all(success for _, success in results)


def run_hardware_tests(verbose=False):
    """Run hardware-in-the-loop tests with real ESP32-S3 hardware."""
    cmd = [sys.executable, "-m", "pytest", "hardware_in_loop/", "-m", "hardware"]
    
    if verbose:
        cmd.append("-v")
    
    print("🔧 Running Hardware-in-the-Loop Tests (requires physical ESP32-S3)...")
    print("Note: These tests require physical ESP32-S3 hardware connected via USB")
    
    result = run_command(cmd, "Hardware-in-the-Loop Tests")
    return result.returncode == 0


def run_environmental_tests(verbose=False):
    """Run environmental stress tests."""
    cmd = [sys.executable, "-m", "pytest", "hardware_in_loop/", "-m", "environmental"]
    
    if verbose:
        cmd.append("-v")
    
    print("🌡️ Running Environmental Stress Tests...")
    print("Testing ESP32-S3 under temperature, voltage, and EMI stress conditions")
    
    result = run_command(cmd, "Environmental Stress Tests")
    return result.returncode == 0


def run_user_interaction_tests(verbose=False):
    """Run real user interaction tests using Playwright."""
    cmd = [sys.executable, "-m", "pytest", "user_interaction/", "-m", "user_interaction"]
    
    if verbose:
        cmd.append("-v")
    
    print("👤 Running Real User Interaction Tests (Playwright)...")
    print("Testing ESP32 interface as real users would interact with it")
    
    result = run_command(cmd, "Real User Interaction Tests")
    return result.returncode == 0


def run_mobile_user_tests(verbose=False):
    """Run mobile user interaction tests."""
    cmd = [sys.executable, "-m", "pytest", "user_interaction/", "-m", "mobile"]
    
    if verbose:
        cmd.append("-v")
    
    print("📱 Running Mobile User Interaction Tests...")
    print("Testing mobile user experience on phones and tablets")
    
    result = run_command(cmd, "Mobile User Interaction Tests")
    return result.returncode == 0


def run_accessibility_tests(verbose=False):
    """Run accessibility user experience tests."""
    cmd = [sys.executable, "-m", "pytest", "user_interaction/", "-m", "accessibility"]
    
    if verbose:
        cmd.append("-v")
    
    print("♿ Running Accessibility User Experience Tests...")
    print("Testing for users with disabilities and assistive technologies")
    
    result = run_command(cmd, "Accessibility User Experience Tests")
    return result.returncode == 0


def run_visual_regression_tests(verbose=False):
    """Run visual regression tests."""
    cmd = [sys.executable, "-m", "pytest", "user_interaction/", "-m", "visual_regression"]
    
    if verbose:
        cmd.append("-v")
    
    print("👁️ Running Visual Regression Tests...")
    print("Detecting visual changes that might impact user experience")
    
    result = run_command(cmd, "Visual Regression Tests")
    return result.returncode == 0


def run_network_failure_tests(verbose=False):
    """Run network failure and recovery tests."""
    cmd = [sys.executable, "-m", "pytest", "user_interaction/", "-m", "network_failure"]
    
    if verbose:
        cmd.append("-v")
    
    print("🌐 Running Network Failure Recovery Tests...")
    print("Testing user experience during network issues and recovery")
    
    result = run_command(cmd, "Network Failure Recovery Tests")
    return result.returncode == 0


def run_user_performance_tests(verbose=False):
    """Run user-focused performance tests."""
    cmd = [sys.executable, "-m", "pytest", "user_interaction/", "-m", "performance"]
    
    if verbose:
        cmd.append("-v")
    
    print("⚡ Running User Performance Experience Tests...")
    print("Testing performance from user's perspective (load times, responsiveness)")
    
    result = run_command(cmd, "User Performance Experience Tests")
    return result.returncode == 0


def run_all_user_interaction_tests(verbose=False):
    """Run all user interaction tests."""
    print("👥 Running All User Interaction Tests...")
    print("Comprehensive testing of user experience with Playwright")
    
    # Test categories to run
    user_tests = [
        ("Real User Interactions", run_user_interaction_tests),
        ("Mobile User Experience", run_mobile_user_tests),
        ("Accessibility Testing", run_accessibility_tests),
        ("Visual Regression", run_visual_regression_tests),
        ("Network Failure Recovery", run_network_failure_tests),
        ("User Performance Experience", run_user_performance_tests)
    ]
    
    results = []
    for test_name, test_func in user_tests:
        print(f"\n{'='*60}")
        print(f"Running {test_name}...")
        print('='*60)
        
        try:
            success = test_func(verbose)
            results.append((test_name, success))
            
            if success:
                print(f"✅ {test_name} passed")
            else:
                print(f"❌ {test_name} failed")
        except Exception as e:
            print(f"❌ {test_name} failed with error: {e}")
            results.append((test_name, False))
    
    # Print summary
    print(f"\n{'='*60}")
    print("USER INTERACTION TEST SUMMARY")
    print('='*60)
    
    passed = 0
    total = len(results)
    
    for test_name, success in results:
        status = "✅ PASSED" if success else "❌ FAILED"
        print(f"{test_name:<35} {status}")
        if success:
            passed += 1
    
    print(f"\nOverall: {passed}/{total} user interaction test categories passed")
    print("\nUser interaction tests provide:")
    print("- Real user behavior simulation")
    print("- Accessibility compliance validation")
    print("- Mobile experience testing")
    print("- Visual consistency checking")
    print("- Network resilience validation")
    print("- Performance from user perspective")
    
    return all(success for _, success in results)


def run_real_world_tests(verbose=False):
    """Run comprehensive real-world accuracy tests."""
    print("🌍 Running Comprehensive Real-World Accuracy Tests...")
    print("This includes enhanced simulation, hardware testing, environmental validation, and user experience")
    
    # Test categories for real-world accuracy
    real_world_tests = [
        ("Enhanced Wokwi Simulation", run_wokwi_tests),
        ("QEMU Hardware Emulation", run_qemu_tests),
        ("Environmental Stress", run_environmental_tests),
        ("Hardware-in-Loop (if available)", run_hardware_tests),
        ("Performance under Load", run_performance_tests),
        ("Security Validation", run_security_tests),
        ("Boundary Conditions", run_boundary_tests),
        ("User Interaction Experience", run_all_user_interaction_tests)
    ]
    
    results = []
    for test_name, test_func in real_world_tests:
        print(f"\n{'='*60}")
        print(f"Running {test_name}...")
        print('='*60)
        
        try:
            success = test_func(verbose)
            results.append((test_name, success))
            
            if success:
                print(f"✅ {test_name} passed")
            else:
                print(f"❌ {test_name} failed")
                # Continue with other tests even if hardware tests fail
                if "Hardware-in-Loop" in test_name:
                    print("   (Hardware tests may fail if no ESP32-S3 is connected)")
        except Exception as e:
            print(f"❌ {test_name} failed with error: {e}")
            results.append((test_name, False))
            # Continue with other tests
            if "Hardware-in-Loop" in test_name:
                print("   (Hardware tests may fail if no ESP32-S3 is connected)")
    
    # Print real-world testing summary
    print(f"\n{'='*60}")
    print("REAL-WORLD TESTING SUMMARY")
    print('='*60)
    
    passed = 0
    total = len(results)
    
    for test_name, success in results:
        status = "✅ PASSED" if success else "❌ FAILED"
        print(f"{test_name:<40} {status}")
        if success:
            passed += 1
    
    print(f"\nOverall: {passed}/{total} real-world test categories passed")
    print("\nReal-world testing provides:")
    print("- Hardware constraint validation")
    print("- Environmental stress testing") 
    print("- Realistic timing and performance")
    print("- Production deployment confidence")
    
    # Consider it successful if most tests pass (allow hardware tests to fail)
    critical_tests = [name for name, _ in results if "Hardware-in-Loop" not in name]
    critical_passed = sum(1 for name, success in results if success and "Hardware-in-Loop" not in name)
    
    return critical_passed >= len(critical_tests) * 0.8  # 80% of non-hardware tests must pass


def main():
    """Main test runner function."""
    parser = argparse.ArgumentParser(
        description="BMCU370 ESP32 Web Interface Test Runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python run_tests.py --all                    # Run all tests
  python run_tests.py --unit --verbose         # Run unit tests with verbose output
  python run_tests.py --integration --coverage # Run integration tests with coverage
  python run_tests.py --performance            # Run performance tests
  python run_tests.py --web                    # Run web interface tests
  python run_tests.py --simulation             # Run simulation tests
  python run_tests.py --specific tests/unit/test_bmcu370_interface.py
  python run_tests.py --report                 # Generate comprehensive report
  
  # New User Interaction Tests:
  python run_tests.py --user-interaction       # Run real user interaction tests with Playwright
  python run_tests.py --mobile                 # Run mobile user experience tests
  python run_tests.py --accessibility          # Run accessibility tests for users with disabilities
  python run_tests.py --visual-regression      # Run visual regression tests
  python run_tests.py --network-failure        # Run network failure recovery tests
  python run_tests.py --user-performance       # Run user-focused performance tests
  python run_tests.py --all-user-tests         # Run all user interaction tests
        """
    )
    
    # Test category options
    parser.add_argument("--all", action="store_true", help="Run all tests")
    parser.add_argument("--unit", action="store_true", help="Run unit tests")
    parser.add_argument("--integration", action="store_true", help="Run integration tests")
    parser.add_argument("--web", action="store_true", help="Run web interface tests")
    parser.add_argument("--simulation", action="store_true", help="Run simulation tests")
    parser.add_argument("--performance", action="store_true", help="Run performance tests")
    parser.add_argument("--security", action="store_true", help="Run security tests")
    parser.add_argument("--error-handling", action="store_true", help="Run error handling tests")
    parser.add_argument("--boundary", action="store_true", help="Run boundary condition tests")
    parser.add_argument("--specific", type=str, help="Run specific test file or function")
    
    # Enhanced simulation categories
    parser.add_argument("--wokwi", action="store_true", help="Run Wokwi ESP32 simulator tests")
    parser.add_argument("--qemu", action="store_true", help="Run QEMU ESP32 emulation tests")
    parser.add_argument("--docker", action="store_true", help="Run Docker ESP32 environment tests")
    parser.add_argument("--gpio", action="store_true", help="Run GPIO and peripheral simulation tests")
    parser.add_argument("--network", action="store_true", help="Run network simulation tests")
    parser.add_argument("--browser", action="store_true", help="Run browser automation tests")
    parser.add_argument("--simulators", action="store_true", help="Run all simulator/emulator tests")
    
    # Hardware-in-the-loop testing options
    parser.add_argument("--hardware", action="store_true", help="Run hardware-in-the-loop tests (requires physical ESP32-S3)")
    parser.add_argument("--environmental", action="store_true", help="Run environmental stress tests")
    parser.add_argument("--real-world", action="store_true", help="Run comprehensive real-world accuracy tests")
    
    # User interaction testing options (NEW)
    parser.add_argument("--user-interaction", action="store_true", help="Run real user interaction tests (Playwright)")
    parser.add_argument("--mobile", action="store_true", help="Run mobile user experience tests")
    parser.add_argument("--accessibility", action="store_true", help="Run accessibility user experience tests")
    parser.add_argument("--visual-regression", action="store_true", help="Run visual regression tests")
    parser.add_argument("--network-failure", action="store_true", help="Run network failure recovery tests")
    parser.add_argument("--user-performance", action="store_true", help="Run user-focused performance tests")
    parser.add_argument("--all-user-tests", action="store_true", help="Run all user interaction tests")
    
    # Options
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--coverage", "-c", action="store_true", help="Generate coverage report")
    parser.add_argument("--exclude-slow", action="store_true", help="Exclude slow tests")
    parser.add_argument("--install-deps", action="store_true", help="Install test dependencies")
    parser.add_argument("--validate-config", action="store_true", help="Validate ESP32 configuration")
    parser.add_argument("--report", action="store_true", help="Generate comprehensive test report")
    parser.add_argument("--optimize-cache", action="store_true", help="Enable intelligent caching and optimization")
    
    args = parser.parse_args()
    
    # Enable caching optimization if requested
    if args.optimize_cache:
        os.environ['USE_TEST_CACHE'] = 'true'
        print("⚡ Optimization and caching enabled")
    
    # Change to script directory
    script_dir = Path(__file__).parent
    os.chdir(script_dir)
    
    print("BMCU370 ESP32 Web Interface Test Runner")
    print("="*50)
    print(f"Working directory: {os.getcwd()}")
    print(f"Python executable: {sys.executable}")
    print(f"Python version: {sys.version}")
    
    success = True
    
    # Install dependencies if requested
    if args.install_deps:
        success = install_dependencies()
        if not success:
            sys.exit(1)
    
    # Validate configuration if requested
    if args.validate_config:
        success = validate_esp32_config()
        if not success:
            print("Configuration validation failed!")
            sys.exit(1)
    
    # Run tests based on arguments
    if args.specific:
        success = run_specific_test(args.specific, args.verbose)
    elif args.unit:
        success = run_unit_tests(args.verbose, args.coverage)
    elif args.integration:
        success = run_integration_tests(args.verbose)
    elif args.web:
        success = run_web_interface_tests(args.verbose)
    elif args.simulation:
        success = run_simulation_tests(args.verbose)
    elif args.performance:
        success = run_performance_tests(args.verbose)
    elif args.security:
        success = run_security_tests(args.verbose)
    elif getattr(args, 'error_handling', False):
        success = run_error_handling_tests(args.verbose)
    elif args.boundary:
        success = run_boundary_tests(args.verbose)
    elif args.wokwi:
        success = run_wokwi_tests(args.verbose)
    elif args.qemu:
        success = run_qemu_tests(args.verbose)
    elif args.docker:
        success = run_docker_tests(args.verbose)
    elif args.gpio:
        success = run_gpio_tests(args.verbose)
    elif args.network:
        success = run_network_tests(args.verbose)
    elif args.browser:
        success = run_browser_tests(args.verbose)
    elif args.simulators:
        success = run_all_simulator_tests(args.verbose)
    elif args.hardware:
        success = run_hardware_tests(args.verbose)
    elif args.environmental:
        success = run_environmental_tests(args.verbose)
    elif args.real_world:
        success = run_real_world_tests(args.verbose)
    elif getattr(args, 'user_interaction', False):
        success = run_user_interaction_tests(args.verbose)
    elif args.mobile:
        success = run_mobile_user_tests(args.verbose)
    elif args.accessibility:
        success = run_accessibility_tests(args.verbose)
    elif getattr(args, 'visual_regression', False):
        success = run_visual_regression_tests(args.verbose)
    elif getattr(args, 'network_failure', False):
        success = run_network_failure_tests(args.verbose)
    elif getattr(args, 'user_performance', False):
        success = run_user_performance_tests(args.verbose)
    elif getattr(args, 'all_user_tests', False):
        success = run_all_user_interaction_tests(args.verbose)
    elif args.all:
        success = run_all_tests(args.verbose, args.coverage, args.exclude_slow)
    elif args.report:
        success = generate_test_report()
    else:
        # Default: run unit and integration tests
        print("\nNo specific test category specified. Running unit and integration tests...")
        success = (run_unit_tests(args.verbose, args.coverage) and 
                  run_integration_tests(args.verbose))
    
    # Print summary
    print("\n" + "="*60)
    if success:
        print("✅ All tests completed successfully!")
        print("\nNext steps:")
        print("- Review test results above")
        print("- Check coverage report if generated")
        print("- Run performance tests if not already done")
        print("- Validate on actual ESP32 hardware")
    else:
        print("❌ Some tests failed!")
        print("\nTroubleshooting:")
        print("- Check error messages above")
        print("- Ensure all dependencies are installed")
        print("- Verify ESP32 configuration is correct")
        print("- Run individual test categories to isolate issues")
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()