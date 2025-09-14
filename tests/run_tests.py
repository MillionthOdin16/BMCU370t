#!/usr/bin/env python3
"""
Test runner script for BMCU370 ESP32 Web Interface tests.

Provides easy execution of different test categories and generates reports.
"""

import argparse
import sys
import os
import subprocess
import time
from pathlib import Path


def run_command(cmd, description=""):
    """Run a command and return the result."""
    print(f"\n{'='*60}")
    if description:
        print(f"Running: {description}")
    print(f"Command: {' '.join(cmd)}")
    print('='*60)
    
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
    """Run unit tests."""
    cmd = [sys.executable, "-m", "pytest", "tests/unit/"]
    
    if verbose:
        cmd.append("-v")
    
    if coverage:
        cmd.extend(["--cov=esp32_firmware/src", "--cov-report=term-missing"])
    
    cmd.extend(["-m", "unit"])
    
    result = run_command(cmd, "Unit Tests")
    return result.returncode == 0


def run_integration_tests(verbose=False):
    """Run integration tests."""
    cmd = [sys.executable, "-m", "pytest", "tests/integration/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "integration"])
    
    result = run_command(cmd, "Integration Tests")
    return result.returncode == 0


def run_web_interface_tests(verbose=False):
    """Run web interface tests."""
    cmd = [sys.executable, "-m", "pytest", "tests/web_interface/"]
    
    if verbose:
        cmd.append("-v")
    
    cmd.extend(["-m", "web"])
    
    result = run_command(cmd, "Web Interface Tests")
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
    
    # Options
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--coverage", "-c", action="store_true", help="Generate coverage report")
    parser.add_argument("--exclude-slow", action="store_true", help="Exclude slow tests")
    parser.add_argument("--install-deps", action="store_true", help="Install test dependencies")
    parser.add_argument("--validate-config", action="store_true", help="Validate ESP32 configuration")
    parser.add_argument("--report", action="store_true", help="Generate comprehensive test report")
    
    args = parser.parse_args()
    
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