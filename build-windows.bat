@echo off
REM NexusSynth Windows x64 Build Script
REM Requires Visual Studio 2019 or later with C++ tools

echo Building NexusSynth for Windows x64...

REM Check for CMake
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found. Please install CMake and add it to PATH.
    exit /b 1
)

REM Create build directory
if not exist build mkdir build
cd build

REM Configure for Windows x64 with Visual Studio
cmake .. -G "Visual Studio 16 2019" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=../install ^
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)

REM Build Release configuration
cmake --build . --config Release --parallel

if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

REM Run tests
echo Running tests...
ctest -C Release --output-on-failure

if errorlevel 1 (
    echo WARNING: Tests failed.
)

REM Install to local directory
cmake --install . --config Release

echo.
echo Build completed successfully!
echo Executable: build\src\Release\nexussynth.exe
echo Tests: build\tests\Release\nexussynth_tests.exe
echo Installation: install\

cd ..