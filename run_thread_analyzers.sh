#!/bin/bash

# Configuration
NUM_RUNS=5                      # Default number of test runs per tool
LOG_BASE_DIR="tests/logs"       # Base directory for all logs
DEBUG_BASE_DIR="$LOG_BASE_DIR/analyzers"  # Base directory for analyzer logs
TOOLS=("helgrind" "drd" "tsan") # Default tools to run
COMPILE_FLAGS=""                # Additional compile flags

# Create log directories
mkdir -p "$LOG_BASE_DIR"
mkdir -p "$DEBUG_BASE_DIR"
sudo chmod -R 777 "$DEBUG_BASE_DIR"  # Make it writeable by all

# Parse command line arguments
function print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Run thread analysis tools on the demo application."
    echo ""
    echo "Options:"
    echo "  -n, --num-runs NUM      Number of runs per tool (default: $NUM_RUNS)"
    echo "  -t, --tools TOOLS       Comma-separated list of tools to run (default: helgrind,drd,tsan)"
    echo "                          Valid tools: helgrind, drd, tsan"
    echo "  -v, --verbose           Enable verbose output"
    echo "  -h, --help              Show this help message"
    exit 1
}

VERBOSE=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--num-runs)
            NUM_RUNS="$2"
            shift 2
            ;;
        -t|--tools)
            IFS=',' read -ra TOOLS <<< "$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            print_usage
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            ;;
    esac
done

# Clean up any old interfaces first
sudo ./setup_test_iface.sh cleanup

# Function to print verbose messages
verbose_print() {
    if [ "$VERBOSE" -eq 1 ]; then
        echo "$@"
    fi
}

# Function to run a test with a specific tool
run_test_with_tool() {
    local tool=$1
    local run_num=$2
    local tool_dir="$DEBUG_BASE_DIR/$tool"
    local log_file="$tool_dir/run_${run_num}.log"
    local tool_log="$tool_dir/${tool}_${run_num}.log"
    
    mkdir -p "$tool_dir"
    sudo chmod 777 "$tool_dir"
    
    echo "Running test with $tool (run $run_num)..."
    
    # Build with the appropriate flags if using ThreadSanitizer
    if [ "$tool" = "tsan" ]; then
        if [ -z "$TSAN_BUILD_COMPLETE" ]; then
            echo "Compiling system test demo with ThreadSanitizer..."
            make clean
            CXXFLAGS="-fsanitize=thread -g -O1" make -s bin/system_tests/demo > /dev/null 2>&1
            export TSAN_BUILD_COMPLETE=1
        fi
    else
        # For Valgrind-based tools, ensure normal build
        if [ -n "$TSAN_BUILD_COMPLETE" ] || [ -z "$NORMAL_BUILD_COMPLETE" ]; then
            echo "Compiling system test demo normally..."
            make clean
            make -s bin/system_tests/demo > /dev/null 2>&1
            export NORMAL_BUILD_COMPLETE=1
            unset TSAN_BUILD_COMPLETE
        fi
    fi
    
    # Setup the interface
    verbose_print "Setting up network interface..."
    if ! sudo ./setup_test_iface.sh setup > "${tool_dir}/iface_setup_${run_num}.log" 2>&1; then
        echo "Failed to set up network interface. Check ${tool_dir}/iface_setup_${run_num}.log"
        return 1
    fi
    
    # Run the test with the appropriate tool
    case $tool in
        "helgrind")
            sudo valgrind --tool=helgrind \
                --read-var-info=yes \
                --track-lockorders=yes \
                --history-level=full \
                --num-callers=40 \
                --free-is-write=yes \
                --log-file="$tool_log" \
                ./bin/system_tests/demo > "$log_file" 2>&1
            ;;
        "drd")
            sudo valgrind --tool=drd \
                --check-stack-var=yes \
                --free-is-write=yes \
                --report-signal-unlocked=yes \
                --segment-merging=no \
                --show-confl-seg=yes \
                --show-stack-usage=yes \
                --num-callers=40 \
                --log-file="$tool_log" \
                ./bin/system_tests/demo > "$log_file" 2>&1
            ;;
        "tsan")
            # ThreadSanitizer needs to be compiled in
            TSAN_OPTIONS="history_size=7 verbose=1" \
            sudo ./bin/system_tests/demo > "$log_file" 2>&1
            # For ThreadSanitizer, the log is mixed with program output, so copy it
            sudo cp "$log_file" "$tool_log"
            ;;
        *)
            echo "Unknown tool: $tool"
            return 1
            ;;
    esac
    
    local exit_code=$?
    
    # Clean up the interface even if the test fails
    sudo ./setup_test_iface.sh cleanup > "${tool_dir}/iface_cleanup_${run_num}.log" 2>&1
    
    # Fix permissions on log files
    sudo chmod 666 "$tool_log" "$log_file" || true
    
    # Check if the tool reported any issues
    if [ "$tool" = "helgrind" ] || [ "$tool" = "drd" ]; then
        if grep -q "ERROR SUMMARY: [1-9]" "$tool_log"; then
            echo "$tool found issues in run $run_num!"
            echo "Check $tool_log for details"
            echo "Program output is in $log_file"
            return 1
        fi
    elif [ "$tool" = "tsan" ]; then
        if grep -q "WARNING: ThreadSanitizer:" "$tool_log"; then
            echo "ThreadSanitizer found issues in run $run_num!"
            echo "Check $tool_log for details"
            return 1
        fi
    fi
    
    if [ $exit_code -ne 0 ]; then
        echo "Run $run_num failed with exit code $exit_code"
        echo "Debug log saved to $log_file"
        echo "$tool log saved to $tool_log"
        return 1
    fi
    
    echo "Run $run_num with $tool succeeded"
    echo "$tool log saved to $tool_log"
    return 0
}

# Run tests with each tool
for tool in "${TOOLS[@]}"; do
    echo "================================================"
    echo "Starting $NUM_RUNS test runs with $tool..."
    echo "Logs will be saved to $DEBUG_BASE_DIR/$tool/"
    
    tool_success=0
    for ((i=1; i<=NUM_RUNS; i++)); do
        if ! run_test_with_tool $tool $i; then
            echo "Test with $tool failed on run $i. Stopping this tool and moving to next."
            tool_success=1
            break
        fi
        
        # Small delay between runs
        sleep 1
    done
    
    echo "$tool analysis completed with status $tool_success"
    echo "================================================"
done

# Final clean-up
sudo ./setup_test_iface.sh cleanup

echo "All thread analysis tools have completed!"
echo "Check $DEBUG_BASE_DIR/ for all logs"
echo "You can use analyze_thread_logs.py to parse and analyze these logs." 