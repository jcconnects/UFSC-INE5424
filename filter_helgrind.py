#!/usr/bin/env python3

import sys
import re
import os
from collections import defaultdict, Counter

def parse_args():
    """Parse command line arguments."""
    import argparse
    parser = argparse.ArgumentParser(description='Filter and analyze Helgrind logs')
    parser.add_argument('logfile', help='Path to Helgrind log file')
    parser.add_argument('--exclude', '-e', action='append', default=['debug.h'], 
                        help='File patterns to exclude (default: debug.h)')
    parser.add_argument('--include', '-i', action='append', default=[], 
                        help='File patterns to specifically include')
    parser.add_argument('--summary', '-s', action='store_true', 
                        help='Show only summary statistics')
    parser.add_argument('--output', '-o', 
                        help='Output file for filtered log (default: stdout)')
    parser.add_argument('--max-errors', '-m', type=int, default=20, 
                        help='Maximum number of errors to show (default: 20)')
    parser.add_argument('--count-by-file', '-c', action='store_true',
                        help='Show error counts by file')
    return parser.parse_args()

def is_excluded(line, exclude_patterns, include_patterns):
    """Check if a line should be excluded based on patterns."""
    # Force include patterns take precedence
    for pattern in include_patterns:
        if pattern in line:
            return False
    
    # Then check exclude patterns
    for pattern in exclude_patterns:
        if pattern in line:
            return True
    
    return False

def extract_error_context(lines, start_idx, max_lines=20):
    """Extract the error context from the log lines."""
    context = []
    
    # Start from the line before the error marker
    idx = start_idx - 1
    
    # Look for the start of this error block
    while idx >= 0 and not lines[idx].startswith('=='):
        idx -= 1
    
    # This is where the error block starts
    start = idx
    
    # Now collect lines until the end of this error block or max_lines
    line_count = 0
    idx = start
    while idx < len(lines) and line_count < max_lines:
        if idx >= start_idx + 4 and lines[idx].startswith('==') and '==' in lines[idx][2:]:
            # We've reached the next error block
            break
        context.append(lines[idx])
        idx += 1
        line_count += 1
    
    return context

def count_errors_by_file(lines):
    """Count errors by source file."""
    file_counts = Counter()
    
    # Regular expression to extract filenames
    filename_pattern = re.compile(r'\(([^:)]+\.[ch][^:)]*)')
    
    for i, line in enumerate(lines):
        if 'Possible data race' in line or 'Lock order' in line:
            # Scan the next 10 lines for filename references
            for j in range(i, min(i+10, len(lines))):
                matches = filename_pattern.findall(lines[j])
                for match in matches:
                    file_counts[match] += 1
    
    return file_counts

def main():
    """Main entry point."""
    args = parse_args()
    
    # Check if the log file exists
    if not os.path.exists(args.logfile):
        print(f"Error: Log file '{args.logfile}' does not exist.", file=sys.stderr)
        return 1
    
    # Read the log file
    with open(args.logfile, 'r') as f:
        lines = f.readlines()
    
    # Count errors by file for statistics
    file_error_counts = count_errors_by_file(lines)
    
    # Prepare output file if specified
    output_file = None
    if args.output:
        output_file = open(args.output, 'w')
    
    # Extract and filter error contexts
    error_contexts = []
    error_count = 0
    data_race_count = 0
    lock_order_count = 0
    
    for i, line in enumerate(lines):
        if 'Possible data race' in line:
            data_race_count += 1
            error_count += 1
            context = extract_error_context(lines, i)
            # Check if this context should be excluded
            exclude_context = False
            for c_line in context:
                if is_excluded(c_line, args.exclude, args.include):
                    exclude_context = True
                    break
            
            if not exclude_context:
                error_contexts.append((context, "Data Race"))
        
        elif 'Lock order' in line:
            lock_order_count += 1
            error_count += 1
            context = extract_error_context(lines, i)
            # Check if this context should be excluded
            exclude_context = False
            for c_line in context:
                if is_excluded(c_line, args.exclude, args.include):
                    exclude_context = True
                    break
            
            if not exclude_context:
                error_contexts.append((context, "Lock Order Violation"))
    
    # Print summary
    print(f"Total errors: {error_count}")
    print(f"Data races: {data_race_count}")
    print(f"Lock order violations: {lock_order_count}")
    print(f"Filtered errors (excluding {args.exclude}): {len(error_contexts)}")
    print("=" * 80)
    
    # Print error counts by file if requested
    if args.count_by_file:
        print("\nError counts by file:")
        for filename, count in file_error_counts.most_common():
            if any(pattern in filename for pattern in args.exclude) and not any(pattern in filename for pattern in args.include):
                filename = f"{filename} (excluded)"
            print(f"  {filename}: {count}")
        print("=" * 80)
    
    # If only summary was requested, we're done
    if args.summary:
        if output_file:
            output_file.close()
        return 0
    
    # Output the filtered errors
    error_count = 0
    for context, error_type in error_contexts:
        error_count += 1
        if error_count > args.max_errors:
            print(f"\n... and {len(error_contexts) - args.max_errors} more errors (use --max-errors to see more)")
            break
        
        print(f"\n=== {error_type} #{error_count} ===")
        for line in context:
            print(line.rstrip())
            if output_file:
                output_file.write(line)
        
        print("-" * 80)
    
    if output_file:
        output_file.close()
    
    return 0

if __name__ == '__main__':
    sys.exit(main()) 