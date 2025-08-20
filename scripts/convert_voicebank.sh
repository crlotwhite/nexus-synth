#!/bin/bash
# ============================================================================
# NexusSynth Voice Bank Conversion Script for Linux/macOS
# ============================================================================
#
# Usage: convert_voicebank.sh [options] <input_path> [output_path]
#
# This script provides an easy way to convert UTAU voice banks to NexusSynth
# .nvm format with comprehensive options and preset configurations.
#
# Examples:
#   ./convert_voicebank.sh "/path/to/voicebank"
#   ./convert_voicebank.sh --preset quality "/path/to/voicebank" "/output/dir"
#   ./convert_voicebank.sh --batch --recursive "/voicebanks" "/output"
#
# ============================================================================

set -euo pipefail  # Exit on error, undefined vars, pipe failures

# Script configuration
SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
NEXUSSYNTH_EXE="${SCRIPT_DIR}/../build/src/nexussynth"
CONFIG_FILE="${SCRIPT_DIR}/nexussynth_batch.json"
LOG_DIR="${SCRIPT_DIR}/../logs"
DEFAULT_PRESET="default"
DEFAULT_THREADS=0

# Initialize variables
INPUT_PATH=""
OUTPUT_PATH=""
PRESET="$DEFAULT_PRESET"
THREADS=$DEFAULT_THREADS
VERBOSE_MODE=0
FORCE_MODE=0
BATCH_MODE=0
RECURSIVE=0
DRY_RUN=0
EXTRA_ARGS=""

# Color definitions (only if stdout is a terminal)
if [[ -t 1 ]]; then
    readonly COLOR_RESET='\033[0m'
    readonly COLOR_RED='\033[31m'
    readonly COLOR_GREEN='\033[32m'
    readonly COLOR_YELLOW='\033[33m'
    readonly COLOR_BLUE='\033[34m'
    readonly COLOR_CYAN='\033[36m'
    readonly COLOR_BOLD='\033[1m'
    readonly COLOR_DIM='\033[2m'
else
    readonly COLOR_RESET=''
    readonly COLOR_RED=''
    readonly COLOR_GREEN=''
    readonly COLOR_YELLOW=''
    readonly COLOR_BLUE=''
    readonly COLOR_CYAN=''
    readonly COLOR_BOLD=''
    readonly COLOR_DIM=''
fi

# Utility functions
log_info() {
    echo -e "${COLOR_BLUE}[INFO]${COLOR_RESET} $*"
}

log_success() {
    echo -e "${COLOR_GREEN}${COLOR_BOLD}✓${COLOR_RESET} $*"
}

log_error() {
    echo -e "${COLOR_RED}${COLOR_BOLD}✗ Error:${COLOR_RESET} $*" >&2
}

log_warning() {
    echo -e "${COLOR_YELLOW}⚠ Warning:${COLOR_RESET} $*"
}

log_verbose() {
    if [[ $VERBOSE_MODE -eq 1 ]]; then
        echo -e "${COLOR_DIM}[VERBOSE]${COLOR_RESET} $*"
    fi
}

print_banner() {
    echo -e "${COLOR_CYAN}${COLOR_BOLD}============================================================================${COLOR_RESET}"
    echo -e "${COLOR_CYAN}${COLOR_BOLD}           NexusSynth Voice Bank Conversion Script v1.0.0${COLOR_RESET}"
    echo -e "${COLOR_CYAN}${COLOR_BOLD}============================================================================${COLOR_RESET}"
    echo
}

show_usage() {
    cat << EOF
${COLOR_BOLD}Usage:${COLOR_RESET}
  $SCRIPT_NAME [options] <input_path> [output_path]

${COLOR_BOLD}Arguments:${COLOR_RESET}
  input_path        Path to UTAU voice bank directory or NVM file
  output_path       Output directory or file path (optional)

${COLOR_BOLD}Options:${COLOR_RESET}
  --preset NAME     Configuration preset (default, fast, quality, batch)
  -j, --threads N   Number of processing threads (0=auto)
  --batch           Enable batch processing mode
  -r, --recursive   Recursively scan input directory
  -f, --force       Force overwrite existing files
  -v, --verbose     Enable verbose output
  --dry-run         Show what would be done without executing
  --config FILE     Custom configuration file
  --no-color        Disable colored output
  -h, --help        Show this help message
  --version         Show version information

${COLOR_BOLD}Presets:${COLOR_RESET}
  default           Balanced quality and performance (recommended)
  fast              Fast conversion with lower quality
  quality           High quality conversion (slower)
  batch             Optimized for batch processing

${COLOR_BOLD}Examples:${COLOR_RESET}
  # Convert single voice bank
  $SCRIPT_NAME "/path/to/voicebank"

  # Convert with quality preset to specific output
  $SCRIPT_NAME --preset quality "/path/to/voicebank" "/output/dir"

  # Batch convert all voice banks in directory tree
  $SCRIPT_NAME --batch --recursive "/voicebanks" "/output"

  # Dry run to see what would be converted
  $SCRIPT_NAME --batch --dry-run --recursive "/voicebanks" "/output"

  # Convert with custom configuration and verbose output
  $SCRIPT_NAME --config custom.json --verbose "/path/to/voicebank"

${COLOR_BOLD}System Requirements:${COLOR_RESET}
  - Built NexusSynth executable
  - Sufficient disk space for output files
  - Memory: Recommended 4GB+ for batch processing
  - CPU: Multi-core recommended for parallel processing

${COLOR_BOLD}Output:${COLOR_RESET}
  - Converted .nvm files
  - Conversion logs in $LOG_DIR
  - Validation reports (if enabled)
  - Processing statistics

EOF
}

show_version() {
    echo "NexusSynth Voice Bank Conversion Script v1.0.0"
    echo "Copyright (c) 2024 NexusSynth Project"
    echo
    if [[ -x "$NEXUSSYNTH_EXE" ]]; then
        "$NEXUSSYNTH_EXE" version 2>/dev/null || echo "NexusSynth executable found but version unavailable"
    else
        echo "NexusSynth executable not available"
    fi
}

check_dependencies() {
    # Check if NexusSynth executable exists and is executable
    if [[ ! -x "$NEXUSSYNTH_EXE" ]]; then
        log_error "NexusSynth executable not found or not executable:"
        echo "  $NEXUSSYNTH_EXE"
        echo
        echo "Please build NexusSynth first:"
        echo "  mkdir build && cd build"
        echo "  cmake .. && make"
        echo
        return 1
    fi

    # Create logs directory if it doesn't exist
    mkdir -p "$LOG_DIR"

    # Check write permissions for log directory
    if [[ ! -w "$LOG_DIR" ]]; then
        log_error "Cannot write to log directory: $LOG_DIR"
        return 1
    fi

    return 0
}

validate_input() {
    if [[ -z "$INPUT_PATH" ]]; then
        log_error "Input path is required"
        echo
        show_usage
        return 1
    fi

    if [[ ! -e "$INPUT_PATH" ]]; then
        log_error "Input path does not exist: $INPUT_PATH"
        return 1
    fi

    # Check if input is a valid UTAU voice bank or NVM file
    if [[ -d "$INPUT_PATH" ]]; then
        if [[ ! -f "$INPUT_PATH/oto.ini" ]]; then
            log_warning "Input directory does not contain oto.ini file"
            log_warning "This may not be a valid UTAU voice bank"
        fi
    elif [[ -f "$INPUT_PATH" ]]; then
        if [[ "${INPUT_PATH##*.}" != "nvm" ]]; then
            log_warning "Input file does not have .nvm extension"
        fi
    fi

    return 0
}

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            --version)
                show_version
                exit 0
                ;;
            --preset)
                if [[ $# -lt 2 ]]; then
                    log_error "--preset requires an argument"
                    return 1
                fi
                PRESET="$2"
                shift 2
                ;;
            -j|--threads)
                if [[ $# -lt 2 ]]; then
                    log_error "--threads requires an argument"
                    return 1
                fi
                if ! [[ "$2" =~ ^[0-9]+$ ]]; then
                    log_error "Thread count must be a number"
                    return 1
                fi
                THREADS="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE_MODE=1
                EXTRA_ARGS="$EXTRA_ARGS --verbose"
                shift
                ;;
            -f|--force)
                FORCE_MODE=1
                EXTRA_ARGS="$EXTRA_ARGS --force"
                shift
                ;;
            --batch)
                BATCH_MODE=1
                shift
                ;;
            -r|--recursive)
                RECURSIVE=1
                EXTRA_ARGS="$EXTRA_ARGS --recursive"
                shift
                ;;
            --dry-run)
                DRY_RUN=1
                EXTRA_ARGS="$EXTRA_ARGS --dry-run"
                shift
                ;;
            --config)
                if [[ $# -lt 2 ]]; then
                    log_error "--config requires an argument"
                    return 1
                fi
                CONFIG_FILE="$2"
                if [[ ! -f "$CONFIG_FILE" ]]; then
                    log_error "Config file does not exist: $CONFIG_FILE"
                    return 1
                fi
                shift 2
                ;;
            --no-color)
                EXTRA_ARGS="$EXTRA_ARGS --no-color"
                shift
                ;;
            -*)
                log_error "Unknown option: $1"
                return 1
                ;;
            *)
                # Positional arguments
                if [[ -z "$INPUT_PATH" ]]; then
                    INPUT_PATH="$1"
                elif [[ -z "$OUTPUT_PATH" ]]; then
                    OUTPUT_PATH="$1"
                else
                    log_error "Too many positional arguments"
                    return 1
                fi
                shift
                ;;
        esac
    done

    return 0
}

show_configuration() {
    log_info "Configuration:"
    echo "  Input Path: $INPUT_PATH"
    if [[ -n "$OUTPUT_PATH" ]]; then
        echo "  Output Path: $OUTPUT_PATH"
    fi
    echo "  Preset: $PRESET"
    echo "  Threads: $THREADS"
    echo "  Config File: $CONFIG_FILE"
    if [[ $BATCH_MODE -eq 1 ]]; then
        echo "  Batch Mode: Enabled"
    fi
    if [[ $RECURSIVE -eq 1 ]]; then
        echo "  Recursive: Enabled"
    fi
    if [[ $VERBOSE_MODE -eq 1 ]]; then
        echo "  Verbose: Enabled"
    fi
    if [[ $FORCE_MODE -eq 1 ]]; then
        echo "  Force Overwrite: Enabled"
    fi
    if [[ $DRY_RUN -eq 1 ]]; then
        echo "  Dry Run: Enabled"
    fi
    echo
}

build_command() {
    local cmd_args=()

    # Choose command based on mode
    if [[ $BATCH_MODE -eq 1 ]]; then
        cmd_args+=("batch")
    else
        cmd_args+=("convert")
    fi

    # Add input path
    cmd_args+=("$INPUT_PATH")

    # Add output path if specified
    if [[ -n "$OUTPUT_PATH" ]]; then
        cmd_args+=("-o" "$OUTPUT_PATH")
    fi

    # Add configuration options
    cmd_args+=("--preset" "$PRESET")
    
    if [[ $THREADS -gt 0 ]]; then
        cmd_args+=("--threads" "$THREADS")
    fi
    
    if [[ -f "$CONFIG_FILE" ]]; then
        cmd_args+=("--config" "$CONFIG_FILE")
    fi
    
    # Generate timestamp for log file
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local log_file="${LOG_DIR}/conversion_${timestamp}.log"
    cmd_args+=("--log-file" "$log_file")
    
    # Enable report generation
    cmd_args+=("--generate-report")

    # Add extra arguments
    if [[ -n "$EXTRA_ARGS" ]]; then
        # Split extra args properly (basic implementation)
        IFS=' ' read -ra EXTRA_ARRAY <<< "$EXTRA_ARGS"
        cmd_args+=("${EXTRA_ARRAY[@]}")
    fi

    echo "${cmd_args[@]}"
}

execute_conversion() {
    local cmd_args
    cmd_args=$(build_command)
    
    log_verbose "Executing: $NEXUSSYNTH_EXE $cmd_args"
    
    echo -e "${COLOR_CYAN}Starting conversion...${COLOR_RESET}"
    echo
    
    # Execute the command
    local result=0
    if [[ $DRY_RUN -eq 1 ]]; then
        echo "DRY RUN: Would execute:"
        echo "$NEXUSSYNTH_EXE $cmd_args"
    else
        # shellcheck disable=SC2086
        "$NEXUSSYNTH_EXE" $cmd_args
        result=$?
    fi
    
    echo
    if [[ $result -eq 0 ]]; then
        log_success "Conversion completed successfully!"
        
        if [[ $DRY_RUN -eq 0 ]]; then
            echo
            echo -e "${COLOR_YELLOW}Next steps:${COLOR_RESET}"
            echo "  • Test the converted .nvm files in NexusSynth"
            echo "  • Check the conversion report for quality metrics"
            echo "  • Consider running validation on the output files"
            echo
            echo -e "${COLOR_BLUE}Useful commands:${COLOR_RESET}"
            echo "  # Validate converted files"
            echo "  $NEXUSSYNTH_EXE validate \"/path/to/output.nvm\""
            echo
            echo "  # Get file information"
            echo "  $NEXUSSYNTH_EXE info \"/path/to/output.nvm\""
        fi
    else
        log_error "Conversion failed with exit code $result"
        echo
        echo -e "${COLOR_YELLOW}Troubleshooting:${COLOR_RESET}"
        echo "  • Run with --verbose for more detailed output"
        echo "  • Verify input voice bank is valid UTAU format"
        echo "  • Check available disk space and memory"
        echo "  • Try with --preset fast for lower resource usage"
        echo "  • Check log files in: $LOG_DIR"
        
        case $result in
            1) echo "  • Check command arguments and options" ;;
            2) echo "  • Verify input files exist and are accessible" ;;
            3) echo "  • Check system resources (memory, disk space)" ;;
            4) echo "  • Review conversion errors above" ;;
            5) echo "  • This may be a software bug - please report it" ;;
        esac
    fi
    
    return $result
}

cleanup() {
    # Cleanup function called on script exit
    log_verbose "Cleaning up..."
}

main() {
    # Set up cleanup trap
    trap cleanup EXIT
    
    # Print banner
    print_banner
    
    # Parse command line arguments
    if ! parse_arguments "$@"; then
        exit 1
    fi
    
    # Check dependencies
    if ! check_dependencies; then
        exit 1
    fi
    
    # Validate input
    if ! validate_input; then
        exit 1
    fi
    
    # Show configuration
    show_configuration
    
    # Execute the conversion
    if ! execute_conversion; then
        exit 1
    fi
    
    echo -e "${COLOR_CYAN}${COLOR_BOLD}============================================================================${COLOR_RESET}"
}

# Run main function with all arguments
main "$@"