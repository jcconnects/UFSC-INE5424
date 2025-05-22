#!/bin/bash

# Number of test runs
NUM_RUNS=10
# Whether to use Helgrind (set to "yes" or "no")
USE_HELGRIND="yes"
# Log directory for debug outputs
DEBUG_LOG_DIR="tests/logs/debug_runs"

# Create debug log directory and ensure correct permissions
sudo mkdir -p "$DEBUG_LOG_DIR"
sudo chmod 777 "$DEBUG_LOG_DIR"  # Make it writeable by all

# Clean up any old interfaces first
sudo ./setup_test_iface.sh cleanup

# Function to run a single test
run_test() {
    local run_num=$1
    local log_file="$DEBUG_LOG_DIR/run_${run_num}.log"
    local helgrind_log="$DEBUG_LOG_DIR/helgrind_${run_num}.log"
    
    echo "Starting run $run_num..."
    
    # Ensure the test is compiled
    echo "Compiling system test demo..."
    if ! make -s bin/system_tests/demo > /dev/null 2>&1; then
        echo "Compilation failed for system test demo"
        return 1
    fi
    
    # Setup the interface
    echo "Setting up network interface..."
    if ! sudo ./setup_test_iface.sh setup > "${DEBUG_LOG_DIR}/iface_setup_${run_num}.log" 2>&1; then
        echo "Failed to set up network interface. Check ${DEBUG_LOG_DIR}/iface_setup_${run_num}.log"
        return 1
    fi
    
    # Run the test and capture its output
    if [ "$USE_HELGRIND" = "yes" ]; then
        # Run with Helgrind with detailed options
        echo "Running with Helgrind (run $run_num)..."
        sudo valgrind --tool=helgrind \
            --read-var-info=yes \
            --track-lockorders=yes \
            --history-level=full \
            --num-callers=40 \
            --free-is-write=yes \
            --log-file="$helgrind_log" \
            ./bin/system_tests/demo > "$log_file" 2>&1
        
        local exit_code=$?
        
        # Clean up the interface even if the test fails
        sudo ./setup_test_iface.sh cleanup > "${DEBUG_LOG_DIR}/iface_cleanup_${run_num}.log" 2>&1
        
        # Fix permissions on log files
        sudo chmod 666 "$helgrind_log" "$log_file" || true
        
        # Check if Helgrind found any issues
        if grep -q "ERROR SUMMARY: [1-9]" "$helgrind_log"; then
            echo "Helgrind found issues in run $run_num!"
            echo "Check $helgrind_log for details"
            echo "Program output is in $log_file"
            return 1
        fi
        
        if [ $exit_code -ne 0 ]; then
            echo "Run $run_num failed with exit code $exit_code"
            echo "Debug log saved to $log_file"
            echo "Helgrind log saved to $helgrind_log"
            return 1
        fi
    else
        # Run normally
        echo "Running test normally..."
        sudo ./bin/system_tests/demo > "$log_file" 2>&1
        
        local exit_code=$?
        
        # Clean up the interface even if the test fails
        sudo ./setup_test_iface.sh cleanup > "${DEBUG_LOG_DIR}/iface_cleanup_${run_num}.log" 2>&1
        
        # Fix permissions on log files
        sudo chmod 666 "$log_file" || true
        
        if [ $exit_code -ne 0 ]; then
            echo "Run $run_num failed with exit code $exit_code"
            echo "Debug log saved to $log_file"
            return 1
        fi
    fi
    
    echo "Run $run_num succeeded"
    if [ "$USE_HELGRIND" = "yes" ]; then
        echo "Helgrind log saved to $helgrind_log"
    fi
    return 0
}

# Main test loop
echo "Starting $NUM_RUNS test runs..."
if [ "$USE_HELGRIND" = "yes" ]; then
    echo "Running with Helgrind enabled"
    echo "Helgrind logs will be saved to $DEBUG_LOG_DIR/helgrind_*.log"
fi
echo "Program logs will be saved to $DEBUG_LOG_DIR/run_*.log"

for ((i=1; i<=NUM_RUNS; i++)); do
    # Run the test
    if ! run_test $i; then
        echo "Test failed on run $i. Stopping for investigation."
        if [ "$USE_HELGRIND" = "yes" ]; then
            echo "Check both:"
            echo "  - $DEBUG_LOG_DIR/helgrind_${i}.log for Helgrind issues"
            echo "  - $DEBUG_LOG_DIR/run_${i}.log for program output"
        else
            echo "Check $DEBUG_LOG_DIR/run_${i}.log for details"
        fi
        exit 1
    fi
    
    # Small delay between runs
    sleep 1
done

echo "All $NUM_RUNS runs completed!"
if [ "$USE_HELGRIND" = "yes" ]; then
    echo "Check $DEBUG_LOG_DIR/helgrind_*.log for any Helgrind issues"
fi 