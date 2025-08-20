@echo off
REM ============================================================================
REM NexusSynth Voice Bank Conversion Script for Windows
REM ============================================================================
REM
REM Usage: convert_voicebank.bat [options] <input_path> [output_path]
REM
REM This script provides an easy way to convert UTAU voice banks to NexusSynth
REM .nvm format with drag-and-drop support and preset configurations.
REM
REM Examples:
REM   convert_voicebank.bat "C:\VoiceBanks\MyVoice"
REM   convert_voicebank.bat --preset quality "C:\VoiceBanks\MyVoice" "C:\Output"
REM   convert_voicebank.bat --batch --recursive "C:\VoiceBanks" "C:\Output"
REM
REM ============================================================================

setlocal enabledelayedexpansion

REM Script configuration
set SCRIPT_NAME=%~nx0
set SCRIPT_DIR=%~dp0
set NEXUSSYNTH_EXE=%SCRIPT_DIR%..\build\src\nexussynth.exe
set CONFIG_FILE=%SCRIPT_DIR%nexussynth_batch.json
set LOG_DIR=%SCRIPT_DIR%..\logs
set DEFAULT_PRESET=default
set DEFAULT_THREADS=0
set VERBOSE_MODE=0
set FORCE_MODE=0
set DRY_RUN=0

REM Color definitions for output
set COLOR_RESET=[0m
set COLOR_RED=[31m
set COLOR_GREEN=[32m
set COLOR_YELLOW=[33m
set COLOR_BLUE=[34m
set COLOR_CYAN=[36m
set COLOR_BOLD=[1m

REM Create logs directory if it doesn't exist
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

REM Generate timestamp for log files
for /f "tokens=1-6 delims=/: " %%a in ("%date% %time%") do (
    set timestamp=%%c%%a%%b_%%d%%e%%f
)
set timestamp=%timestamp:~0,15%
set LOG_FILE=%LOG_DIR%\conversion_%timestamp%.log

echo %COLOR_CYAN%%COLOR_BOLD%============================================================================%COLOR_RESET%
echo %COLOR_CYAN%%COLOR_BOLD%           NexusSynth Voice Bank Conversion Script v1.0.0%COLOR_RESET%
echo %COLOR_CYAN%%COLOR_BOLD%============================================================================%COLOR_RESET%
echo.

REM Check if NexusSynth executable exists
if not exist "%NEXUSSYNTH_EXE%" (
    echo %COLOR_RED%Error:%COLOR_RESET% NexusSynth executable not found at:
    echo   %NEXUSSYNTH_EXE%
    echo.
    echo Please build NexusSynth first:
    echo   mkdir build ^&^& cd build
    echo   cmake .. ^&^& make
    echo.
    goto :show_usage
)

REM Parse command line arguments
set INPUT_PATH=
set OUTPUT_PATH=
set PRESET=%DEFAULT_PRESET%
set THREADS=%DEFAULT_THREADS%
set BATCH_MODE=0
set RECURSIVE=0
set EXTRA_ARGS=

:parse_args
if "%~1"=="" goto :args_parsed
if "%~1"=="--help" goto :show_help
if "%~1"=="-h" goto :show_help
if "%~1"=="--version" goto :show_version
if "%~1"=="--preset" (
    set PRESET=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="-j" (
    set THREADS=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--threads" (
    set THREADS=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--verbose" (
    set VERBOSE_MODE=1
    set EXTRA_ARGS=!EXTRA_ARGS! --verbose
    shift
    goto :parse_args
)
if "%~1"=="-v" (
    set VERBOSE_MODE=1
    set EXTRA_ARGS=!EXTRA_ARGS! --verbose
    shift
    goto :parse_args
)
if "%~1"=="--force" (
    set FORCE_MODE=1
    set EXTRA_ARGS=!EXTRA_ARGS! --force
    shift
    goto :parse_args
)
if "%~1"=="-f" (
    set FORCE_MODE=1
    set EXTRA_ARGS=!EXTRA_ARGS! --force
    shift
    goto :parse_args
)
if "%~1"=="--batch" (
    set BATCH_MODE=1
    shift
    goto :parse_args
)
if "%~1"=="--recursive" (
    set RECURSIVE=1
    set EXTRA_ARGS=!EXTRA_ARGS! --recursive
    shift
    goto :parse_args
)
if "%~1"=="-r" (
    set RECURSIVE=1
    set EXTRA_ARGS=!EXTRA_ARGS! --recursive
    shift
    goto :parse_args
)
if "%~1"=="--dry-run" (
    set DRY_RUN=1
    set EXTRA_ARGS=!EXTRA_ARGS! --dry-run
    shift
    goto :parse_args
)
if "%~1"=="--config" (
    set CONFIG_FILE=%~2
    shift
    shift
    goto :parse_args
)

REM Positional arguments
if "!INPUT_PATH!"=="" (
    set INPUT_PATH=%~1
    shift
    goto :parse_args
)
if "!OUTPUT_PATH!"=="" (
    set OUTPUT_PATH=%~1
    shift
    goto :parse_args
)

REM Additional arguments are passed through
set EXTRA_ARGS=!EXTRA_ARGS! %~1
shift
goto :parse_args

:args_parsed

REM Validate input path
if "!INPUT_PATH!"=="" (
    echo %COLOR_RED%Error:%COLOR_RESET% Input path is required
    echo.
    goto :show_usage
)

if not exist "!INPUT_PATH!" (
    echo %COLOR_RED%Error:%COLOR_RESET% Input path does not exist: !INPUT_PATH!
    echo.
    goto :error_exit
)

REM Show configuration
echo %COLOR_BLUE%Configuration:%COLOR_RESET%
echo   Input Path: !INPUT_PATH!
if not "!OUTPUT_PATH!"=="" echo   Output Path: !OUTPUT_PATH!
echo   Preset: !PRESET!
echo   Threads: !THREADS!
echo   Config File: !CONFIG_FILE!
echo   Log File: !LOG_FILE!
if !BATCH_MODE! equ 1 echo   Batch Mode: Enabled
if !RECURSIVE! equ 1 echo   Recursive: Enabled
if !VERBOSE_MODE! equ 1 echo   Verbose: Enabled
if !FORCE_MODE! equ 1 echo   Force Overwrite: Enabled
if !DRY_RUN! equ 1 echo   Dry Run: Enabled
echo.

REM Build command arguments
set NEXUS_ARGS=

REM Choose command based on mode
if !BATCH_MODE! equ 1 (
    set NEXUS_ARGS=batch
) else (
    set NEXUS_ARGS=convert
)

REM Add input/output paths
set NEXUS_ARGS=!NEXUS_ARGS! "!INPUT_PATH!"
if not "!OUTPUT_PATH!"=="" set NEXUS_ARGS=!NEXUS_ARGS! -o "!OUTPUT_PATH!"

REM Add configuration options
set NEXUS_ARGS=!NEXUS_ARGS! --preset !PRESET!
if !THREADS! gtr 0 set NEXUS_ARGS=!NEXUS_ARGS! --threads !THREADS!
if exist "!CONFIG_FILE!" set NEXUS_ARGS=!NEXUS_ARGS! --config "!CONFIG_FILE!"
set NEXUS_ARGS=!NEXUS_ARGS! --log-file "!LOG_FILE!"
set NEXUS_ARGS=!NEXUS_ARGS! --generate-report

REM Add extra arguments
set NEXUS_ARGS=!NEXUS_ARGS! !EXTRA_ARGS!

REM Show command that will be executed
if !VERBOSE_MODE! equ 1 (
    echo %COLOR_YELLOW%Executing:%COLOR_RESET%
    echo   "%NEXUSSYNTH_EXE%" !NEXUS_ARGS!
    echo.
)

REM Execute the conversion
echo %COLOR_CYAN%Starting conversion...%COLOR_RESET%
echo.

"%NEXUSSYNTH_EXE%" !NEXUS_ARGS!
set RESULT=%ERRORLEVEL%

echo.
if !RESULT! equ 0 (
    echo %COLOR_GREEN%%COLOR_BOLD%✓ Conversion completed successfully!%COLOR_RESET%
    if exist "!LOG_FILE!" (
        echo %COLOR_BLUE%Log file:%COLOR_RESET% !LOG_FILE!
    )
    if !DRY_RUN! equ 0 (
        echo.
        echo %COLOR_YELLOW%Next steps:%COLOR_RESET%
        echo   • Test the converted .nvm files in NexusSynth
        echo   • Check the conversion report for quality metrics
        echo   • Consider running validation on the output files
    )
) else (
    echo %COLOR_RED%%COLOR_BOLD%✗ Conversion failed with exit code !RESULT!%COLOR_RESET%
    echo.
    echo %COLOR_YELLOW%Troubleshooting:%COLOR_RESET%
    if exist "!LOG_FILE!" (
        echo   • Check the log file: !LOG_FILE!
    )
    echo   • Run with --verbose for more detailed output
    echo   • Verify input voice bank is valid UTAU format
    echo   • Check available disk space and memory
    echo   • Try with --preset fast for lower resource usage
)

echo.
echo %COLOR_CYAN%%COLOR_BOLD%============================================================================%COLOR_RESET%
goto :end

:show_usage
echo %COLOR_BOLD%Usage:%COLOR_RESET%
echo   %SCRIPT_NAME% [options] ^<input_path^> [output_path]
echo.
echo %COLOR_BOLD%Arguments:%COLOR_RESET%
echo   input_path        Path to UTAU voice bank directory or NVM file
echo   output_path       Output directory or file path (optional)
echo.
echo %COLOR_BOLD%Options:%COLOR_RESET%
echo   --preset NAME     Configuration preset (default, fast, quality, batch)
echo   -j, --threads N   Number of processing threads (0=auto)
echo   --batch           Enable batch processing mode
echo   -r, --recursive   Recursively scan input directory
echo   -f, --force       Force overwrite existing files
echo   -v, --verbose     Enable verbose output
echo   --dry-run         Show what would be done without executing
echo   --config FILE     Custom configuration file
echo   -h, --help        Show this help message
echo   --version         Show version information
echo.
echo %COLOR_BOLD%Examples:%COLOR_RESET%
echo   REM Convert single voice bank
echo   %SCRIPT_NAME% "C:\VoiceBanks\MyVoice"
echo.
echo   REM Convert with quality preset
echo   %SCRIPT_NAME% --preset quality "C:\VoiceBanks\MyVoice" "C:\Output"
echo.
echo   REM Batch convert all voice banks in directory
echo   %SCRIPT_NAME% --batch --recursive "C:\VoiceBanks" "C:\Output"
echo.
echo   REM Drag and drop support - just drag voice bank folder onto this script
echo.
goto :end

:show_help
goto :show_usage

:show_version
echo NexusSynth Voice Bank Conversion Script v1.0.0
echo Copyright (c) 2024 NexusSynth Project
echo.
"%NEXUSSYNTH_EXE%" version 2>nul
if !ERRORLEVEL! neq 0 (
    echo NexusSynth executable not available
)
goto :end

:error_exit
exit /b 1

:end
if !VERBOSE_MODE! equ 1 (
    echo.
    echo Press any key to continue...
    pause >nul
)
exit /b %RESULT%