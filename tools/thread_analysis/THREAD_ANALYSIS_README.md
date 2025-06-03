# Thread Analysis Tools

This set of scripts provides a comprehensive solution for analyzing threading issues in C++ applications using multiple analysis tools:

- **Valgrind Helgrind**: Detects race conditions and lock ordering problems
- **Valgrind DRD (Data Race Detector)**: Alternative data race detection with different sensitivity
- **ThreadSanitizer (TSan)**: Google's data race detector (requires compilation with special flags)

## Prerequisites

- Valgrind (for Helgrind and DRD)
- GCC or Clang with ThreadSanitizer support
- Python 3.6+ (for log analysis)

## Quick Start

The simplest way to run a full analysis is:

```bash
# Make scripts executable
chmod +x run_thread_analysis.sh
chmod +x run_thread_analyzers.sh
chmod +x setup_test_iface.sh

# Run complete analysis with default settings
./run_thread_analysis.sh
```

This will:
1. Run all three analysis tools (Helgrind, DRD, and ThreadSanitizer)
2. Generate logs for each tool in `tests/logs/analyzers/[tool]/`
3. Analyze the logs to identify and deduplicate issues
4. Generate a report in `tests/reports/`

## Script Details

### run_thread_analysis.sh

The main wrapper script that coordinates running the analyzers and processing the results.

```
Usage: ./run_thread_analysis.sh [OPTIONS]

Options:
  -n, --num-runs NUM       Number of runs per tool (default: 3)
  -t, --tools TOOLS        Comma-separated list of tools to run (default: helgrind,drd,tsan)
                           Valid tools: helgrind, drd, tsan
  -e, --exclude PATTERN    File pattern to exclude (can be specified multiple times)
  -i, --include PATTERN    File pattern to specifically include (can be specified multiple times)
  -o, --output DIR         Output directory for reports (default: tests/reports)
  -v, --verbose            Enable verbose output
  -a, --analyze-only       Skip running tests, only analyze existing logs
  -h, --help               Show this help message
```

### run_thread_analyzers.sh

Runs each thread analysis tool with appropriate settings.

```
Usage: ./run_thread_analyzers.sh [OPTIONS]

Options:
  -n, --num-runs NUM      Number of runs per tool (default: 5)
  -t, --tools TOOLS       Comma-separated list of tools to run (default: helgrind,drd,tsan)
                          Valid tools: helgrind, drd, tsan
  -v, --verbose           Enable verbose output
  -h, --help              Show this help message
```

### analyze_thread_logs.py

Parses and analyzes logs from the various tools, extracting and categorizing threading issues.

```
Usage: python3 analyze_thread_logs.py [OPTIONS]

Options:
  --log-dir, -d DIR         Base directory containing analyzer logs (default: tests/logs/analyzers)
  --tools, -t TOOLS         Comma-separated list of tools to analyze (default: helgrind,drd,tsan)
  --exclude, -e PATTERN     File patterns to exclude (default: debug.h)
  --include, -i PATTERN     File patterns to specifically include
  --output, -o FILE         Output file for report (default: stdout)
  --json, -j FILE           Output JSON file for machine processing
  --summary-only, -s        Show only summary statistics, not individual issues
  --max-issues, -m NUM      Maximum number of issues to show (default: 10)
```

## Advanced Usage

### Running Selected Tools

```bash
# Run only Helgrind and DRD (skip ThreadSanitizer)
./run_thread_analysis.sh --tools helgrind,drd

# Run only ThreadSanitizer
./run_thread_analysis.sh --tools tsan
```

### Analyzing Existing Logs

```bash
# Analyze logs without running the tests again
./run_thread_analysis.sh --analyze-only

# Analyze logs with custom filter patterns
./run_thread_analysis.sh --analyze-only --exclude debug.h --include mutex.cpp
```

### Filtering Results

```bash
# Exclude certain system files from results
./run_thread_analysis.sh --exclude debug.h --exclude /usr/include/

# Only show issues in specific files
./run_thread_analysis.sh --include src/core/ --include src/components/
```

### More Verbose Output

```bash
# Get more detailed output during test runs
./run_thread_analysis.sh --verbose
```

## Output Files

The scripts generate several types of output files:

1. **Raw Tool Logs**: Located in `tests/logs/analyzers/[tool]/[tool]_[run].log`
2. **Program Output Logs**: Located in `tests/logs/analyzers/[tool]/run_[run].log`
3. **Analysis Report**: Text report in `tests/reports/thread_analysis_report_[timestamp].txt`
4. **Analysis Data**: JSON format in `tests/reports/thread_analysis_data_[timestamp].json`

## Troubleshooting

### Common Issues

1. **Permission denied**: Make sure all scripts are executable (`chmod +x *.sh`)
2. **Network interface issues**: If setup_test_iface.sh fails, check if you have the proper permissions to create network interfaces
3. **ThreadSanitizer build failures**: Ensure your compiler supports `-fsanitize=thread`

### Debug Tips

- Run with `--verbose` to see more detailed output
- Check the interface setup logs in case of network-related failures
- For ThreadSanitizer issues, check that your application can be compiled with TSAN flags 