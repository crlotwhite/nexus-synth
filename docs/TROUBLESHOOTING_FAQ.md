# NexusSynth Troubleshooting and FAQ

This comprehensive guide covers common issues, troubleshooting procedures, and frequently asked questions for NexusSynth users, developers, and testers.

## Table of Contents

1. [Quick Troubleshooting](#quick-troubleshooting)
2. [Installation Issues](#installation-issues)
3. [Runtime Problems](#runtime-problems)
4. [Performance Issues](#performance-issues)
5. [Audio Quality Problems](#audio-quality-problems)
6. [UTAU Integration Issues](#utau-integration-issues)
7. [Development and Testing FAQ](#development-and-testing-faq)
8. [System-Specific Issues](#system-specific-issues)
9. [Advanced Diagnostics](#advanced-diagnostics)
10. [Getting Help](#getting-help)

## Quick Troubleshooting

### Before You Start

Before diving into specific issues, try these quick fixes that resolve 80% of common problems:

```bash
# 1. Verify installation
./nexussynth --version
./nexussynth --test-synthesis

# 2. Check system resources
free -h                    # Linux/macOS memory check
df -h                      # Disk space check

# 3. Test with minimal configuration
./nexussynth --render simple_test.ust output.wav --verbose
```

### Emergency Diagnostics

If NexusSynth is completely non-functional, run this diagnostic script:

```bash
#!/bin/bash
echo "=== NexusSynth Emergency Diagnostics ==="
echo "Date: $(date)"
echo "User: $(whoami)"
echo "Working Directory: $(pwd)"
echo ""

echo "=== System Information ==="
uname -a
echo ""

echo "=== NexusSynth Status ==="
./nexussynth --version 2>&1
./nexussynth --test-config 2>&1
echo ""

echo "=== File Permissions ==="
ls -la nexussynth*
echo ""

echo "=== Available Resources ==="
free -h
df -h .
echo ""

echo "=== Process Information ==="
ps aux | grep nexus | head -5
echo ""

echo "=== Recent Logs ==="
tail -20 ~/.nexussynth/logs/nexussynth.log 2>/dev/null || echo "No log file found"
```

### Common Error Patterns

| Error Message | Quick Fix | Reference Section |
|---------------|-----------|-------------------|
| "Command not found" | Add to PATH or use ./nexussynth | [Installation Issues](#installation-issues) |
| "Permission denied" | Check file permissions | [Installation Issues](#installation-issues) |
| "Segmentation fault" | Check dependencies | [Runtime Problems](#runtime-problems) |
| "Out of memory" | Close other applications | [Performance Issues](#performance-issues) |
| "Audio device not found" | Check audio configuration | [Audio Quality Problems](#audio-quality-problems) |
| "UTAU integration failed" | Verify UTAU settings | [UTAU Integration Issues](#utau-integration-issues) |

## Installation Issues

### 1. Binary Not Found or Permission Errors

**Symptoms**:
```bash
$ nexussynth
nexussynth: command not found

$ ./nexussynth
bash: ./nexussynth: Permission denied
```

**Solutions**:
```bash
# Fix PATH issue
export PATH=$PATH:/path/to/nexussynth/bin
# Add to ~/.bashrc for permanent fix
echo 'export PATH=$PATH:/path/to/nexussynth/bin' >> ~/.bashrc

# Fix permission issue
chmod +x nexussynth

# Check file integrity
file nexussynth
# Should show: "ELF 64-bit LSB executable" (Linux) or similar
```

### 2. Missing Dependencies

**Symptoms**:
```bash
./nexussynth: error while loading shared libraries: libworld.so.3: cannot open shared object file
```

**Solutions**:

#### Linux
```bash
# Check missing libraries
ldd nexussynth

# Install system dependencies
sudo apt-get update
sudo apt-get install libomp-dev libeigen3-dev

# Or install from source dependencies
cd external/
make install-system-deps
```

#### macOS
```bash
# Check dependencies
otool -L nexussynth

# Install with Homebrew
brew install libomp eigen

# Fix library paths if needed
install_name_tool -change old_path new_path nexussynth
```

#### Windows
```
# Ensure Visual C++ Redistributable is installed
# Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe

# Check DLL dependencies with Dependency Walker
# Available at: http://www.dependencywalker.com/
```

### 3. Build from Source Issues

**CMake Configuration Failures**:
```bash
# Common CMake fixes
rm -rf build/
mkdir build && cd build

# Specify compiler explicitly
cmake .. -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11

# Disable problematic features if needed
cmake .. -DNEXUSSYNTH_BUILD_TESTS=OFF -DNEXUSSYNTH_USE_CUDA=OFF

# Verbose build for debugging
make VERBOSE=1
```

**Dependency Download Failures**:
```bash
# Manual dependency setup
cd external/
git submodule update --init --recursive

# Or download specific dependencies
wget https://github.com/mmorise/World/archive/refs/tags/v0.3.2.tar.gz
tar -xzf v0.3.2.tar.gz
cd World-0.3.2 && make && sudo make install
```

## Runtime Problems

### 1. Application Crashes

**Segmentation Faults**:
```bash
# Run with debugger
gdb ./nexussynth
(gdb) run --render test.ust output.wav
(gdb) bt  # backtrace when it crashes

# Or use core dump analysis
ulimit -c unlimited  # enable core dumps
./nexussynth --render test.ust output.wav
gdb ./nexussynth core
(gdb) bt
```

**Memory Access Violations (Windows)**:
```cmd
REM Run with Windows Error Reporting enabled
schtasks /create /tn "WER" /tr "wer.exe" /sc onstart
nexussynth.exe --render test.ust output.wav
```

**Common Crash Causes**:
1. **Corrupted Voice Bank**: Try with a different, known-good voice bank
2. **Invalid Audio Format**: Ensure input files are valid WAV format
3. **Memory Exhaustion**: Close other applications, increase swap space
4. **Threading Issues**: Try running with `--single-threaded` flag

### 2. Infinite Loops or Hangs

**Diagnosis**:
```bash
# Monitor system resources
top -p $(pgrep nexussynth)

# Check system calls (Linux)
strace -p $(pgrep nexussynth)

# Send interrupt signal to get backtrace
kill -USR1 $(pgrep nexussynth)
```

**Common Causes**:
- **Circular Dependencies**: Check voice bank oto.ini for loops
- **Network Timeouts**: Disable network features with `--offline` mode
- **File Locking**: Ensure no other processes are using the same files

### 3. Error Message Analysis

**WORLD Vocoder Errors**:
```
Error: WORLD parameter extraction failed for sample: /path/to/sample.wav
```
**Solution**: Check audio file format, bit depth, and sample rate. WORLD requires PCM WAV files.

**HMM Model Errors**:
```
Error: Failed to load HMM model: model file corrupted or incompatible
```
**Solution**: Re-download or rebuild voice bank models. Check disk space and permissions.

**Memory Allocation Errors**:
```
Error: std::bad_alloc - Unable to allocate memory
```
**Solution**: Increase available RAM, close other applications, or use `--low-memory` mode.

## Performance Issues

### 1. Slow Rendering Performance

**Benchmarking Your System**:
```bash
# Quick performance test
./nexussynth --benchmark-quick

# Comprehensive benchmark
./performance_benchmark_tool suite basic --iterations 10
```

**Expected Performance Targets**:
| System Class | Target Performance | Typical Range |
|--------------|-------------------|---------------|
| High-end Desktop | >5x real-time | 4-8x real-time |
| Mid-range Laptop | >3x real-time | 2-4x real-time |
| Budget System | >1x real-time | 1-2x real-time |

**Performance Optimization**:
```bash
# Enable all CPU features
export OMP_NUM_THREADS=$(nproc)
export OMP_PROC_BIND=spread
export OMP_PLACES=cores

# Use high-performance mode
./nexussynth --render input.ust output.wav --performance-mode high

# Profile to identify bottlenecks
./nexussynth --render input.ust output.wav --profile --verbose
```

### 2. Memory Usage Issues

**Memory Monitoring**:
```bash
# Monitor memory usage in real-time
watch -n 1 'ps -o pid,vsz,rss,comm -p $(pgrep nexussynth)'

# Check for memory leaks
valgrind --tool=memcheck --leak-check=full ./nexussynth --render test.ust output.wav
```

**Memory Optimization**:
```bash
# Reduce memory footprint
./nexussynth --render input.ust output.wav --low-memory --cache-size 256MB

# Clear caches periodically for long sessions
./nexussynth --clear-cache
```

### 3. CPU Usage Problems

**High CPU Usage**:
```bash
# Check thread distribution
htop -u $(whoami)

# Limit CPU usage if needed
nice -n 10 ./nexussynth --render input.ust output.wav --threads 2
```

**Low CPU Usage (Underutilization)**:
```bash
# Force multi-threading
./nexussynth --render input.ust output.wav --threads $(nproc) --parallel-synthesis

# Check for I/O bottlenecks
iotop -p $(pgrep nexussynth)
```

## Audio Quality Problems

### 1. Poor Audio Quality

**Quality Analysis**:
```bash
# Built-in quality analysis
./nexussynth --analyze-quality output.wav reference.wav

# External analysis with FFmpeg
ffmpeg -i output.wav -af showspectrumpic=s=1024x512:legend=1 spectrum.png
```

**Common Quality Issues and Fixes**:

#### Distortion or Clipping
```bash
# Check for clipping
./nexussynth --analyze-levels output.wav

# Reduce output gain
./nexussynth --render input.ust output.wav --gain 0.7
```

#### Metallic or Robotic Sound
```bash
# Adjust WORLD parameters
./nexussynth --render input.ust output.wav --world-frame-period 10.0 --world-fft-size 2048

# Try different synthesis modes  
./nexussynth --render input.ust output.wav --synthesis-mode natural
```

#### Inconsistent Timbre
```bash
# Enable advanced formant correction
./nexussynth --render input.ust output.wav --formant-correction --spectral-smoothing
```

### 2. Timing and Synchronization Issues

**Timing Analysis**:
```bash
# Check timing accuracy
./nexussynth --analyze-timing input.ust output.wav

# Compare with reference
./nexussynth --compare-timing output_nexus.wav output_reference.wav
```

**Timing Corrections**:
```bash
# Adjust timing precision
./nexussynth --render input.ust output.wav --timing-mode precise

# Compensate for latency
./nexussynth --render input.ust output.wav --timing-offset -50ms
```

### 3. Pitch and Formant Issues

**"Chipmunk Effect"**:
```bash
# Enable advanced pitch processing
./nexussynth --render input.ust output.wav --advanced-pitch --formant-preservation

# Manual formant correction
./nexussynth --render input.ust output.wav --formant-shift-ratio 0.8
```

**Pitch Accuracy Problems**:
```bash
# Analyze pitch accuracy
./nexussynth --analyze-pitch output.wav input.ust

# Adjust pitch tracking
./nexussynth --render input.ust output.wav --pitch-tracking-mode accurate
```

## UTAU Integration Issues

### 1. UTAU Cannot Find NexusSynth

**Check UTAU Configuration**:
1. Open UTAU preferences
2. Go to "Tools" → "Options" → "Plugins"
3. Verify resampler path points to correct NexusSynth executable
4. Test path by clicking "Test" button

**Common Path Issues**:
```
# Correct paths by platform:
Windows: C:\NexusSynth\bin\nexussynth.exe
Linux: /usr/local/bin/nexussynth
macOS: /Applications/NexusSynth.app/Contents/MacOS/nexussynth
```

### 2. Flag Processing Problems

**Supported UTAU Flags**:
| Flag | Support | Notes |
|------|---------|--------|
| g | ✓ Full | Gender parameter |
| P | ✓ Full | Peak control |
| Mt | ✓ Full | Modulation |
| Mb | ✓ Partial | Basic breathiness |
| bre | ✓ Full | Advanced breathiness |
| bri | ✓ Full | Brightness control |
| CL | ✓ Full | Clarity adjustment |

**Flag Testing**:
```bash
# Test specific flags
./nexussynth --test-flag g=10 --voice-bank test_bank --phoneme a

# Debug flag processing
./nexussynth --render input.ust output.wav --debug-flags --verbose
```

### 3. Voice Bank Compatibility

**Voice Bank Validation**:
```bash
# Validate voice bank structure
./nexussynth --validate-voicebank /path/to/voicebank

# Check oto.ini parsing
./nexussynth --parse-oto /path/to/voicebank/oto.ini --verbose
```

**Common Voice Bank Issues**:
1. **Invalid oto.ini entries**: Use `--fix-oto` flag to attempt automatic repair
2. **Missing audio files**: Check file paths and permissions
3. **Encoding problems**: Ensure oto.ini uses correct character encoding (UTF-8 or Shift-JIS)

## Development and Testing FAQ

### General Development Questions

**Q: How do I set up a development environment?**
A: Follow the setup in `CLAUDE.md`. Key requirements:
```bash
# Install dependencies
sudo apt-get install build-essential cmake ninja-build libomp-dev

# Clone and build
git clone https://github.com/nexussynth/nexussynth.git
cd nexussynth
mkdir build && cd build
cmake .. -DNEXUSSYNTH_BUILD_TESTS=ON
make -j$(nproc)
```

**Q: How do I run the test suite?**
A: Use CTest for comprehensive testing:
```bash
# Run all tests
ctest --parallel 4 --output-on-failure

# Run specific test categories
ctest -L unit     # Unit tests only
ctest -L integration  # Integration tests only
ctest -L performance  # Performance benchmarks
```

**Q: How do I contribute to the project?**
A: See contribution guidelines:
1. Fork the repository
2. Create a feature branch
3. Make changes and add tests
4. Submit a pull request
5. Address review feedback

### Testing Questions

**Q: The CI/CD pipeline is failing. How do I debug it?**
A: Check the GitHub Actions logs:
1. Go to the Actions tab in GitHub
2. Click on the failing workflow run
3. Examine the step-by-step logs
4. Run the same commands locally to reproduce

**Q: Performance tests are showing regressions. What should I do?**
A: Use the regression detection system:
```bash
# Run regression analysis
python .github/scripts/performance_regression_detector.py \
  --input ./benchmark_results \
  --database .performance_db/metrics.db

# View detailed dashboard
python .github/scripts/generate_performance_dashboard.py \
  --database .performance_db/metrics.db \
  --output ./dashboard
```

**Q: How do I add new performance benchmarks?**
A: Add benchmarks to the framework:
```cpp
// In tests/integration/benchmarks/
class CustomBenchmark : public PerformanceBenchmark {
public:
    BenchmarkResult run_custom_test() {
        // Your benchmark implementation
    }
};
```

### Code Quality Questions

**Q: How do I check code style and formatting?**
A: Use the project's style tools:
```bash
# Check C++ code style  
clang-format -style=file -Werror --dry-run src/**/*.cpp

# Check Python code style
flake8 .github/scripts/
pylint .github/scripts/

# Run static analysis
cppcheck --enable=all --std=c++17 src/
```

**Q: How do I add documentation?**
A: Follow the documentation standards:
1. Use Doxygen comments for C++ code
2. Use clear, descriptive function names
3. Add examples for complex APIs
4. Update relevant .md files in docs/

## System-Specific Issues

### Windows Issues

#### 1. Visual Studio Build Issues
```cmd
REM Use VS Developer Command Prompt
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Or use cmake with specific generator
cmake .. -G "Visual Studio 17 2022" -A x64
```

#### 2. Windows Defender False Positives
Windows Defender may flag NexusSynth as suspicious. To resolve:
1. Add NexusSynth directory to Windows Defender exclusions
2. Alternatively, download from official sources only

#### 3. Audio Device Issues
```cmd
REM Check Windows audio configuration
dxdiag

REM Test ASIO drivers
nexussynth.exe --list-audio-devices
nexussynth.exe --audio-driver asio --audio-device "Your ASIO Device"
```

### Linux Issues

#### 1. Audio System Problems
```bash
# Check ALSA configuration
aplay -l
cat /proc/asound/cards

# Check PulseAudio  
pulseaudio --check -v
pactl list short sinks

# Fix permissions
sudo usermod -a -G audio $USER
```

#### 2. Library Version Conflicts
```bash
# Check library versions
ldd nexussynth | grep -E "(eigen|world|omp)"

# Use specific library paths
LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH ./nexussynth
```

#### 3. Threading Issues
```bash
# Check thread limits
ulimit -u
cat /proc/sys/kernel/threads-max

# Increase if needed
echo 'soft nproc 4096' | sudo tee -a /etc/security/limits.conf
```

### macOS Issues

#### 1. Code Signing and Security
```bash
# Remove quarantine flag
xattr -d com.apple.quarantine nexussynth

# Sign binary (for distribution)
codesign -s "Developer ID Application" nexussynth
```

#### 2. Apple Silicon Compatibility
```bash
# Check architecture
file nexussynth
# Should show: "Mach-O 64-bit executable arm64" on Apple Silicon

# Run x86_64 version on Apple Silicon (if needed)
arch -x86_64 ./nexussynth
```

#### 3. Audio Unit Issues
```bash
# Check Core Audio configuration
system_profiler SPAudioDataType

# Reset Core Audio if needed
sudo killall coreaudiod
```

## Advanced Diagnostics

### Performance Profiling

#### Linux Profiling Tools
```bash
# CPU profiling with perf
perf record -g ./nexussynth --render test.ust output.wav
perf report

# Memory profiling with Valgrind
valgrind --tool=massif --detailed-freq=1 ./nexussynth --render test.ust output.wav
ms_print massif.out.*

# Real-time monitoring
htop -p $(pgrep nexussynth)
iotop -p $(pgrep nexussynth)
```

#### macOS Profiling Tools
```bash
# Use Instruments for detailed profiling
instruments -t "Time Profiler" ./nexussynth --render test.ust output.wav

# Sample running process
sample nexussynth 10 -file nexussynth_profile.txt
```

#### Windows Profiling Tools
```cmd
REM Use Windows Performance Toolkit
wpr -start GeneralProfile
nexussynth.exe --render test.ust output.wav  
wpr -stop profile.etl
wpa profile.etl
```

### Network and I/O Diagnostics

#### File I/O Analysis
```bash
# Monitor file operations (Linux)
strace -e trace=file ./nexussynth --render test.ust output.wav

# Monitor file access patterns
inotifywait -m -r . --format '%w%f %e' &
./nexussynth --render test.ust output.wav
```

#### Memory Layout Analysis
```bash
# Analyze memory maps
cat /proc/$(pgrep nexussynth)/maps

# Check memory fragmentation
cat /proc/$(pgrep nexussynth)/smaps | grep -E "(Size|Rss|Private)"
```

### Custom Debugging Tools

#### Debug Build Configuration
```bash
# Build with debug symbols and sanitizers
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DNEXUSSYNTH_ENABLE_ASAN=ON \
         -DNEXUSSYNTH_ENABLE_UBSAN=ON \
         -DNEXUSSYNTH_ENABLE_TSAN=ON

# Run with debug output
./nexussynth --render test.ust output.wav --debug --verbose --log-level debug
```

#### Custom Diagnostic Scripts
```python
#!/usr/bin/env python3
"""Advanced NexusSynth diagnostics script."""

import subprocess
import psutil
import json
import time

def diagnose_nexussynth():
    """Run comprehensive diagnostics."""
    
    results = {
        'timestamp': time.time(),
        'system_info': {},
        'process_info': {},
        'performance_metrics': {}
    }
    
    # System information
    results['system_info'] = {
        'cpu_count': psutil.cpu_count(),
        'memory_total': psutil.virtual_memory().total,
        'memory_available': psutil.virtual_memory().available,
        'disk_usage': psutil.disk_usage('.').percent
    }
    
    # Process information
    for proc in psutil.process_iter(['pid', 'name', 'cpu_percent', 'memory_info']):
        if 'nexussynth' in proc.info['name'].lower():
            results['process_info'] = proc.info
            break
    
    # Performance test
    start_time = time.time()
    result = subprocess.run([
        './nexussynth', '--benchmark-quick'
    ], capture_output=True, text=True)
    end_time = time.time()
    
    results['performance_metrics'] = {
        'benchmark_time': end_time - start_time,
        'benchmark_output': result.stdout,
        'benchmark_errors': result.stderr
    }
    
    return results

if __name__ == '__main__':
    diagnostics = diagnose_nexussynth()
    print(json.dumps(diagnostics, indent=2))
```

## Getting Help

### Self-Help Resources

#### Documentation
- **Main Documentation**: `docs/TESTING_GUIDE.md`
- **CI/CD Guide**: `docs/CICD_WORKFLOWS.md`
- **Performance Guide**: `docs/PERFORMANCE_BENCHMARKING.md`
- **Development Guide**: `CLAUDE.md`

#### Built-in Help
```bash
# Command-line help
./nexussynth --help
./nexussynth --help-advanced

# Configuration help  
./nexussynth --help-config
./nexussynth --list-options

# Debugging help
./nexussynth --help-debug
./nexussynth --diagnose
```

### Community Support

#### GitHub Resources
- **Issues**: https://github.com/nexussynth/nexussynth/issues
- **Discussions**: https://github.com/nexussynth/nexussynth/discussions
- **Wiki**: https://github.com/nexussynth/nexussynth/wiki

#### Communication Channels
- **Discord**: [Invite Link] - Real-time chat and support
- **Reddit**: r/UTAU - Community discussions
- **Twitter**: @NexusSynth - Updates and announcements

### Reporting Guidelines

#### Before Reporting
1. **Search existing issues** for similar problems
2. **Try the latest version** if possible
3. **Test with minimal configuration** to isolate the issue
4. **Gather relevant information** (system specs, logs, etc.)

#### Issue Report Template
```markdown
## Issue Summary
Brief description of the problem...

## Environment
- OS: [e.g., Ubuntu 22.04.3 LTS]
- NexusSynth Version: [e.g., v0.9.0-beta.2]
- Hardware: [e.g., Intel i7-9700K, 16GB RAM]

## Steps to Reproduce
1. Step one...
2. Step two...
3. Issue occurs

## Expected vs Actual Behavior
**Expected**: What should happen
**Actual**: What actually happens

## Diagnostic Information
```bash
./nexussynth --diagnose
```
[Paste output here]

## Additional Context
Any other relevant information...
```

### Professional Support

For commercial users or critical production environments:

#### Consulting Services
- **Performance Optimization**: Custom tuning for specific workloads
- **Integration Support**: Help with complex UTAU setups
- **Custom Development**: Feature development for specific needs

#### Training and Documentation
- **Team Training**: Custom training sessions for development teams
- **Documentation**: Custom documentation for specific use cases
- **Best Practices**: Optimization and workflow guidance

#### Contact Information
- **Email**: support@nexussynth.org
- **Business Inquiries**: business@nexussynth.org
- **Security Issues**: security@nexussynth.org

---

*This troubleshooting guide is maintained by the NexusSynth development team. If you found this guide helpful or have suggestions for improvement, please let us know through our GitHub repository.*

**Last Updated**: August 2025  
**Version**: 1.0.0  
**Contributors**: NexusSynth Development Team, Community Contributors