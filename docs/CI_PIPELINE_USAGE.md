# BMCU370 CI Pipeline Usage Guide

## Quick Start

The BMCU370 multi-stage CI pipeline automatically runs on:
- Pull requests to any branch
- Pushes to `main` and `develop` branches
- Manual triggers via GitHub Actions interface

## Pipeline Triggers

### Automatic Triggers

#### Pull Request Trigger
```yaml
on:
  pull_request:
    paths:
      - 'src/**'
      - 'esp32_firmware/**'
      - 'platformio.ini'
      - 'lib/**'
      - 'test/**'
      - '.github/workflows/**'
```

**What happens**: Full pipeline execution except HIL tests and release packaging

#### Push Trigger
```yaml
on:
  push:
    branches: [main, develop]
    paths:
      - 'src/**'
      - 'esp32_firmware/**'
      - 'platformio.ini'
      - 'lib/**'
      - 'test/**'
      - '.github/workflows/**'
```

**What happens**: Full pipeline execution including HIL tests and release packaging

### Manual Triggers

#### Workflow Dispatch
Navigate to Actions → Multi-Stage Firmware CI Pipeline → Run workflow

**Options**:
- `run_hil_tests`: Enable Hardware-in-the-Loop testing
- `debug_mode`: Enable enhanced logging and debugging

## Pipeline Stages Overview

### 1. Pre-build Validation & Security (5 min)
- ✅ Security scanning (Python & C/C++)
- ✅ Dependency vulnerability check
- ✅ Code quality prerequisites

### 2. Static Analysis (10 min, parallel)
- ✅ cppcheck analysis for BMCU370 and ESP32
- ✅ Code complexity analysis
- ✅ Advanced static analysis

### 3. Build Firmware (15 min, parallel)
- ✅ BMCU370 (CH32V203) firmware build
- ✅ ESP32-S3 firmware build + filesystem
- ✅ Memory usage analysis

### 4. Unit & Integration Tests (20 min)
- ✅ Unit tests for both platforms
- ✅ Protocol validation tests
- ✅ Integration testing

### 5. Simulation Testing (15 min)
- ✅ QEMU-based simulation
- ✅ Virtual hardware testing
- ✅ Protocol simulation

### 6. Hardware-in-the-Loop (45 min, conditional)
- 🔌 Real hardware testing
- 🔌 USB CDC communication tests
- 🔌 Web interface validation
- 🔌 BambuBus protocol tests

### 7. Firmware Validation (10 min)
- ✅ Binary integrity validation
- ✅ Security assessment
- ✅ Performance analysis

### 8. Release Package (5 min, conditional)
- 📦 Complete firmware package
- 📦 Documentation generation
- 📦 Deployment guides

### 9. Pipeline Summary (5 min)
- 📋 Execution summary
- 📋 Quality metrics
- 📋 Detailed reporting

## Understanding Pipeline Status

### Status Icons
- ✅ **Success**: Stage completed successfully
- ❌ **Failure**: Stage failed, pipeline stopped
- ⚠️ **Warning**: Stage completed with warnings
- ⏳ **Running**: Stage currently executing
- ⏸️ **Skipped**: Stage skipped due to conditions

### Quality Gates

#### Must Pass
- Security scan (no high-severity vulnerabilities)
- Build success (both platforms)
- Unit tests (all tests pass)
- Firmware validation (integrity verified)

#### May Continue with Warnings
- Static analysis warnings (below threshold)
- Code complexity (below threshold)
- Non-critical test failures

## Artifacts and Downloads

### Build Artifacts
Download from: Actions → Workflow Run → Artifacts

#### BMCU370 Firmware
- `bmcu370-firmware-[commit]`: BMCU370 firmware package
  - `bmcu370_firmware.bin`: DFU-flashable firmware
  - `bmcu370_firmware.elf`: Debug symbols
  - `build_info.txt`: Build information
  - `memory-usage.txt`: Memory analysis

#### ESP32 Firmware
- `esp32-firmware-[commit]`: ESP32 firmware package
  - `esp32_firmware.bin`: Main application
  - `esp32_bootloader.bin`: ESP32 bootloader
  - `esp32_partitions.bin`: Partition table
  - `esp32_littlefs.bin`: Web interface filesystem
  - `build_info.txt`: Build information
  - `memory-usage.txt`: Memory analysis

#### Test Results
- `test-results-[target]-[commit]`: Test reports
  - JUnit XML test results
  - Coverage reports
  - Protocol validation results

#### Analysis Reports
- `static-analysis-[target]-[commit]`: Code analysis
  - cppcheck XML reports
  - Complexity analysis
  - Style check results

- `security-reports-[commit]`: Security analysis
  - Vulnerability scan results
  - Dependency analysis
  - Security pattern detection

#### Validation Reports
- `validation-results-[commit]`: Firmware validation
  - Binary integrity reports
  - Security assessment
  - Performance analysis

#### Complete Release Package
- `bmcu370-validated-firmware-[commit]`: Complete package
  - All firmware components
  - Documentation
  - Deployment guides
  - Validation reports

## Troubleshooting

### Common Failure Scenarios

#### 1. Build Failures

**Symptoms**: Red X on build stage
**Common Causes**:
- Compilation errors in source code
- Missing dependencies
- Platform configuration issues

**How to Debug**:
1. Click on failed build job
2. Expand "Build firmware" step
3. Review compilation errors
4. Check memory usage warnings

**Example Fix**:
```cpp
// Fix compilation error
#ifdef ESP32
  // ESP32-specific code
#endif
```

#### 2. Static Analysis Failures

**Symptoms**: Warnings or errors in static analysis stage
**Common Causes**:
- Code style issues
- Complexity violations
- Potential bugs detected

**How to Debug**:
1. Download static analysis artifacts
2. Review cppcheck XML reports
3. Check complexity analysis

**Example Fix**:
```cpp
// Reduce complexity
void complexFunction() {
  // Break into smaller functions
  doFirstPart();
  doSecondPart();
  doThirdPart();
}
```

#### 3. Test Failures

**Symptoms**: Failed tests in unit test stage
**Common Causes**:
- Regression in functionality
- Test environment issues
- Protocol changes

**How to Debug**:
1. Download test result artifacts
2. Review JUnit XML reports
3. Check test logs

**Example Fix**:
```python
# Update test expectations
def test_firmware_size(self):
    size = os.path.getsize(firmware_file)
    self.assertLess(size, 512*1024)  # Updated size limit
```

#### 4. Security Scan Failures

**Symptoms**: Security issues detected
**Common Causes**:
- Vulnerable dependencies
- Insecure coding patterns
- Hardcoded secrets

**How to Debug**:
1. Download security reports
2. Review vulnerability details
3. Check dependency versions

**Example Fix**:
```ini
# Update vulnerable dependency
lib_deps = 
    robtillaart/CRC@^1.0.4  ; Updated version
```

### Debug Mode

Enable debug mode for enhanced logging:
1. Go to Actions → Multi-Stage Firmware CI Pipeline
2. Click "Run workflow"
3. Check "Enable debug mode"
4. Run workflow

Debug mode provides:
- Verbose build output
- Enhanced test logging
- Detailed analysis reports
- Extended artifact retention

### Getting Help

#### 1. Check Pipeline Documentation
- [CI Pipeline Architecture](docs/CI_PIPELINE_ARCHITECTURE.md)
- [Testing Framework](test/README.md)
- Build configuration files

#### 2. Review Artifacts
- Download and examine build logs
- Check test reports for details
- Review security scan results

#### 3. Manual Verification
```bash
# Test locally
pip install platformio
pio run --environment genericCH32V203C8T6
pio run --environment esp32s3

# Run tests locally
python -m pytest test/ -v
```

#### 4. Create Issue
If pipeline issues persist:
1. Include workflow run URL
2. Attach relevant artifacts
3. Describe expected vs actual behavior
4. Include local test results

## Best Practices

### Development Workflow

#### 1. Local Testing
Always test locally before pushing:
```bash
# Build firmware
./build.sh

# Run tests
python -m pytest test/ -v

# Check static analysis
pio check --environment genericCH32V203C8T6
```

#### 2. Small, Focused Changes
- Make incremental changes
- Test each change locally
- Keep commits focused and atomic

#### 3. Branch Strategy
- Create feature branches from `develop`
- Open PR when ready for review
- Address CI feedback promptly

### Code Quality

#### 1. Follow Coding Standards
- Use consistent formatting
- Add meaningful comments
- Follow naming conventions

#### 2. Write Tests
- Add unit tests for new features
- Update tests when changing functionality
- Ensure good test coverage

#### 3. Security Awareness
- Avoid hardcoded credentials
- Validate input data
- Use secure communication protocols

### CI/CD Best Practices

#### 1. Monitor Pipeline Health
- Review failed pipelines promptly
- Address quality gate failures
- Monitor performance trends

#### 2. Artifact Management
- Download important artifacts
- Keep release packages
- Archive test results

#### 3. Documentation
- Update documentation with changes
- Keep README files current
- Document configuration changes

## Pipeline Configuration

### Customizing the Pipeline

#### 1. Modify Triggers
Edit `.github/workflows/firmware-ci-pipeline.yml`:
```yaml
on:
  pull_request:
    paths:
      - 'src/**'
      - 'your_new_path/**'  # Add new paths
```

#### 2. Adjust Timeouts
```yaml
timeout-minutes: 30  # Increase for slow builds
```

#### 3. Add New Platforms
```yaml
strategy:
  matrix:
    include:
      - target: new_platform
        environment: new_env
        platform: NEW_PLATFORM
```

#### 4. Configure Quality Gates
Edit `.github/ci-config.yml`:
```yaml
QUALITY_GATES:
  max_static_analysis_warnings: 100  # Adjust threshold
  min_test_coverage: 80              # Increase coverage requirement
```

### Environment Variables

Set in repository settings → Secrets and variables → Actions:
- `BMCU370_PORT`: Hardware test port
- `ESP32_PORT`: ESP32 test port
- Custom configuration values

## Monitoring and Metrics

### Pipeline Performance
- Average execution time: ~45 minutes
- Success rate target: >95%
- Quality gate passage rate: >90%

### Key Metrics
- Build success rate
- Test pass rate
- Security scan results
- Code quality trends

### Alerts
Pipeline failures automatically:
- Update GitHub status
- Generate artifact reports
- Log detailed error information

## Future Enhancements

### Planned Features
1. Enhanced HIL testing automation
2. Performance benchmarking
3. Automated dependency updates
4. Advanced security scanning
5. Integration with external tools

### Feedback and Contributions
- Report issues via GitHub Issues
- Suggest improvements via Pull Requests
- Contribute to documentation
- Share usage experiences