#!/bin/bash

# Script to run thread analysis tools and analyze the results
# This is a wrapper around run_thread_analyzers.sh and analyze_thread_logs.py

# Default values
LOG_DIR="tests/logs/analyzers"
REPORT_DIR="tests/reports"
TOOLS="helgrind,drd,tsan"
NUM_RUNS=3
VERBOSE=0
EXCLUDE_PATTERNS=("debug.h" "stdlib")
INCLUDE_PATTERNS=()

function print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Run thread analysis tools and analyze the results."
    echo ""
    echo "Options:"
    echo "  -n, --num-runs NUM       Number of runs per tool (default: $NUM_RUNS)"
    echo "  -t, --tools TOOLS        Comma-separated list of tools to run (default: $TOOLS)"
    echo "                           Valid tools: helgrind, drd, tsan"
    echo "  -e, --exclude PATTERN    File pattern to exclude (can be specified multiple times)"
    echo "  -i, --include PATTERN    File pattern to specifically include (can be specified multiple times)"
    echo "  -o, --output DIR         Output directory for reports (default: $REPORT_DIR)"
    echo "  -v, --verbose            Enable verbose output"
    echo "  -a, --analyze-only       Skip running tests, only analyze existing logs"
    echo "  -h, --help               Show this help message"
    exit 1
}

# Parse command line arguments
ANALYZE_ONLY=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--num-runs)
            NUM_RUNS="$2"
            shift 2
            ;;
        -t|--tools)
            TOOLS="$2"
            shift 2
            ;;
        -e|--exclude)
            EXCLUDE_PATTERNS+=("$2")
            shift 2
            ;;
        -i|--include)
            INCLUDE_PATTERNS+=("$2")
            shift 2
            ;;
        -o|--output)
            REPORT_DIR="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -a|--analyze-only)
            ANALYZE_ONLY=1
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

# Create report directory
mkdir -p "$REPORT_DIR"

# Run the thread analysis tools if not in analyze-only mode
if [ "$ANALYZE_ONLY" -eq 0 ]; then
    echo "Running thread analysis tools..."
    
    # Build the command with options
    CMD="./run_thread_analyzers.sh --num-runs $NUM_RUNS --tools $TOOLS"
    
    if [ "$VERBOSE" -eq 1 ]; then
        CMD="$CMD --verbose"
    fi
    
    # Run the command
    echo "Executing: $CMD"
    eval "$CMD"
    
    # Check if analysis succeeded
    if [ $? -ne 0 ]; then
        echo "Thread analysis failed. Check logs for details."
        exit 1
    fi
    
    echo "Thread analysis complete."
fi

# Now analyze the logs
echo "Analyzing logs..."

# Build the analyze command
ANALYZE_CMD="python3 analyze_thread_logs.py --log-dir $LOG_DIR --tools $TOOLS"

# Add exclude patterns
for pattern in "${EXCLUDE_PATTERNS[@]}"; do
    ANALYZE_CMD="$ANALYZE_CMD --exclude '$pattern'"
done

# Add include patterns
for pattern in "${INCLUDE_PATTERNS[@]}"; do
    ANALYZE_CMD="$ANALYZE_CMD --include '$pattern'"
done

# Set output files
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
REPORT_FILE="$REPORT_DIR/thread_analysis_report_$TIMESTAMP.txt"
JSON_FILE="$REPORT_DIR/thread_analysis_data_$TIMESTAMP.json"

ANALYZE_CMD="$ANALYZE_CMD --output '$REPORT_FILE' --json '$JSON_FILE'"

# Run the analysis
echo "Executing: $ANALYZE_CMD"
eval "$ANALYZE_CMD"

if [ $? -eq 0 ]; then
    echo "Analysis complete."
    echo "Report saved to: $REPORT_FILE"
    echo "JSON data saved to: $JSON_FILE"
else
    echo "Analysis failed."
    exit 1
fi

echo "Thread analysis pipeline complete." 