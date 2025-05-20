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
    parser.add_argument('--include-dir', default='include', help='Directory of implementation headers to match (default: include)')
    parser.add_argument('--summary', '-s', action='store_true', 
                        help='Show only summary statistics')
    parser.add_argument('--output', '-o', 
                        help='Output file for filtered log (default: stdout)')
    parser.add_argument('--max-errors', '-m', type=int, default=20, 
                        help='Maximum number of errors to show (default: 20)')
    parser.add_argument('--count-by-file', '-c', action='store_true',
                        help='Show error counts by file')
    parser.add_argument('--only-files', '-f', type=str, default=None,
                        help='Comma-separated list of filenames to include (e.g., sharedMemoryEngine.h,socketEngine.h)')
    parser.add_argument('--filter-threads', action='store_true',
                        help='Filter out thread announcement lines (e.g., Thread # created/finished)')
    return parser.parse_args()

def is_excluded(line, exclude_patterns, include_patterns):
    """Check if a line should be excluded based on patterns."""
    for pattern in include_patterns:
        if pattern in line:
            return False
    for pattern in exclude_patterns:
        if pattern in line:
            return True
    return False

def get_include_files(include_dir):
    """Return a set of all .h files (with relative paths) in the include_dir and subdirs."""
    include_files = set()
    for root, dirs, files in os.walk(include_dir):
        for f in files:
            if f.endswith('.h'):
                rel_path = os.path.relpath(os.path.join(root, f), include_dir)
                include_files.add(f)
                include_files.add(rel_path)
    return include_files

def extract_file_line_refs(block):
    """Extract (filename, line) pairs from a block of log lines."""
    refs = []
    # Patterns: file.h:123, (file.h:123), at file.h:123, etc.
    file_line_re = re.compile(r'([\w\./\\-]+\.h):(\d+)')
    for line in block:
        for match in file_line_re.finditer(line):
            refs.append((os.path.basename(match.group(1)), int(match.group(2)), match.group(1)))
    return refs

def main():
    args = parse_args()
    if not os.path.exists(args.logfile):
        print(f"Error: Log file '{args.logfile}' does not exist.", file=sys.stderr)
        return 1
    include_files = get_include_files(args.include_dir)
    with open(args.logfile, 'r') as f:
        lines = []
        for line in f:
            # Skip DWARF warnings
            if 'warning: evaluate_Dwarf' in line:
                continue
            # Optionally skip thread announcements
            if args.filter_threads and 'Thread #' in line:
                continue
            lines.append(line)
    output_file = open(args.output, 'w') if args.output else None
    error_blocks = []
    block = []
    in_thread_announcement = False
    for line in lines:
        # Detect start of thread announcement block
        if '---Thread-Announcement---' in line:
            in_thread_announcement = True
            block = []  # Clear any partial block
            continue
        # Detect end of thread announcement block (next block separator or end)
        if in_thread_announcement:
            if line.strip().startswith('==') and '----------------------------------------------------------------' in line:
                in_thread_announcement = False
            continue
        block.append(line)
        if line.strip().startswith('==') and '----------------------------------------------------------------' in line:
            error_blocks.append(block)
            block = []
    if block:
        error_blocks.append(block)
    # For each error block, check if it should be excluded, and extract refs
    file_line_hits = defaultdict(list)  # {filename: [line numbers]}
    only_files = None
    if args.only_files:
        only_files = set(f.strip() for f in args.only_files.split(','))
    # Deduplication: track unique issue signatures
    seen_signatures = set()
    for block in error_blocks:
        block_str = ''.join(block)
        if any(is_excluded(l, args.exclude, args.include) for l in block):
            continue
        refs = extract_file_line_refs(block)
        # Create a signature for this block: sorted tuple of (filename, line) pairs
        sig = tuple(sorted(set((fname, lineno) for fname, lineno, _ in refs)))
        if sig in seen_signatures:
            continue  # Skip duplicate
        seen_signatures.add(sig)
        for fname, lineno, fullpath in refs:
            if (fname in include_files or fullpath in include_files):
                if only_files is None or fname in only_files:
                    file_line_hits[fname].append((lineno, fullpath, block_str))
    # Print summary (to both stdout and output_file if specified)
    summary_lines = []
    summary_lines.append(f"Found issues in the following include files:")
    for fname in sorted(file_line_hits):
        unique_lines = sorted(set(lineno for lineno, _, _ in file_line_hits[fname]))
        summary_lines.append(f"  {fname}: {len(file_line_hits[fname])} issues")
        for lineno in unique_lines:
            # Find a fullpath for this line (first occurrence)
            fullpath = next((fp for l, fp, _ in file_line_hits[fname] if l == lineno), fname)
            summary_lines.append(f"    - {fullpath}:{lineno}")
    summary_lines.append("="*80)
    for line in summary_lines:
        print(line)
        if output_file:
            output_file.write(line + "\n")
    # Optionally print error blocks
    if not args.summary:
        count = 0
        for fname in sorted(file_line_hits):
            for lineno, fullpath, block_str in file_line_hits[fname]:
                count += 1
                if count > args.max_errors:
                    msg = f"... and more (use --max-errors to see more)"
                    print(msg)
                    if output_file:
                        output_file.write(msg + "\n")
                    break
                header = f"\n=== Issue in {fullpath}:{lineno} ==="
                print(header)
                if output_file:
                    output_file.write(header + "\n")
                print(block_str)
                if output_file:
                    output_file.write(block_str)
    if output_file:
        output_file.close()
    return 0

if __name__ == '__main__':
    sys.exit(main()) 