#!/usr/bin/env python3

import sys
import re
import os
import glob
import argparse
from collections import defaultdict, Counter
from typing import List, Dict, Tuple, Set, Optional
import json

# Define constants for tool types
HELGRIND = "helgrind"
DRD = "drd"
TSAN = "tsan"

class ThreadIssue:
    """Class to represent a threading issue found by an analyzer"""
    def __init__(self, tool: str, issue_type: str, description: str, stack_trace: List[str], 
                 file_locations: Set[str] = None, run_id: str = "", raw_log: List[str] = None):
        self.tool = tool
        self.issue_type = issue_type
        self.description = description
        self.stack_trace = stack_trace
        self.file_locations = file_locations or set()
        self.run_id = run_id
        self.raw_log = raw_log or []
        self.hash = self._compute_hash()
    
    def _compute_hash(self) -> str:
        """Compute a hash for deduplication based on issue type and affected locations"""
        # Use the first few frames of the stack trace as part of the hash
        stack_sig = "|".join(self.stack_trace[:3]) if self.stack_trace else ""
        return f"{self.tool}:{self.issue_type}:{stack_sig}"
    
    def to_dict(self) -> Dict:
        """Convert to dictionary for JSON serialization"""
        return {
            "tool": self.tool,
            "issue_type": self.issue_type,
            "description": self.description,
            "stack_trace": self.stack_trace,
            "file_locations": list(self.file_locations),
            "run_id": self.run_id
        }
    
    @classmethod
    def from_dict(cls, data: Dict) -> 'ThreadIssue':
        """Create a ThreadIssue instance from a dictionary"""
        return cls(
            tool=data["tool"],
            issue_type=data["issue_type"],
            description=data["description"],
            stack_trace=data["stack_trace"],
            file_locations=set(data["file_locations"]),
            run_id=data["run_id"]
        )
    
    def __str__(self) -> str:
        """String representation of the issue"""
        file_locs = "\n    ".join(sorted(self.file_locations)) if self.file_locations else "Unknown"
        return (f"[{self.tool.upper()}] {self.issue_type}\n"
                f"Description: {self.description}\n"
                f"File locations:\n    {file_locs}\n"
                f"Stack trace (first 3 frames):\n    " + 
                "\n    ".join(self.stack_trace[:3] if len(self.stack_trace) > 3 else self.stack_trace))

def parse_args() -> argparse.Namespace:
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description='Analyze logs from thread analysis tools')
    parser.add_argument('--log-dir', '-d', default='tests/logs/analyzers',
                        help='Base directory containing analyzer logs')
    parser.add_argument('--tools', '-t', default='helgrind,drd,tsan',
                        help='Comma-separated list of tools to analyze')
    parser.add_argument('--exclude', '-e', action='append', default=['debug.h'], 
                        help='File patterns to exclude (default: debug.h)')
    parser.add_argument('--include', '-i', action='append', default=[], 
                        help='File patterns to specifically include')
    parser.add_argument('--output', '-o', 
                        help='Output file for report (default: stdout)')
    parser.add_argument('--json', '-j', help='Output JSON file for machine processing')
    parser.add_argument('--summary-only', '-s', action='store_true',
                        help='Show only summary statistics, not individual issues')
    parser.add_argument('--max-issues', '-m', type=int, default=10,
                        help='Maximum number of issues to show (default: 10)')
    return parser.parse_args()

def is_excluded(line: str, exclude_patterns: List[str], include_patterns: List[str]) -> bool:
    """Check if a line should be excluded based on patterns"""
    # Force include patterns take precedence
    for pattern in include_patterns:
        if pattern in line:
            return False
    
    # Then check exclude patterns
    for pattern in exclude_patterns:
        if pattern in line:
            return True
    
    return False

def extract_file_locations(text_block: List[str]) -> Set[str]:
    """Extract file locations from a block of text"""
    locations = set()
    
    # Regular expressions for different file reference formats
    file_patterns = [
        re.compile(r'at\s+([^:]+\.[ch][^:]*):(\d+)'),  # at file.c:123
        re.compile(r'in\s+[^(]+\(([^:]+\.[ch][^:]*):(\d+)\)'),  # in func() (file.c:123)
        re.compile(r'\(([^:]+\.[ch][^:]*):(\d+)\)'),  # (file.c:123)
        re.compile(r'([^:\s]+\.[ch][^:]*):(\d+)'),  # file.c:123
    ]
    
    for line in text_block:
        for pattern in file_patterns:
            for match in pattern.finditer(line):
                try:
                    file_path = match.group(1)
                    line_num = match.group(2)
                    locations.add(f"{file_path}:{line_num}")
                except (IndexError, AttributeError):
                    pass
    
    return locations

def extract_stack_trace(text_block: List[str], tool: str) -> List[str]:
    """Extract stack trace from a block of text based on the tool"""
    stack_trace = []
    
    # Different patterns for different tools
    if tool == HELGRIND or tool == DRD:
        # Valgrind stack traces typically have "at" or "by" at the beginning of lines
        in_stack = False
        for line in text_block:
            if re.match(r'^\s*(?:at|by)\s+', line):
                in_stack = True
                # Clean up the line to just show the function and location
                cleaned = re.sub(r'^\s*(?:at|by)\s+', '', line.strip())
                stack_trace.append(cleaned)
            elif in_stack and not line.strip():
                # Empty line likely ends the stack trace
                break
    
    elif tool == TSAN:
        # TSAN stack traces typically have "#0", "#1", etc.
        for line in text_block:
            if re.match(r'^\s*#\d+\s+', line):
                # Clean up the line to just show the function and location
                cleaned = re.sub(r'^\s*#\d+\s+', '', line.strip())
                stack_trace.append(cleaned)
    
    return stack_trace

def parse_helgrind_log(log_file: str, exclude_patterns: List[str], include_patterns: List[str]) -> List[ThreadIssue]:
    """Parse a Helgrind log file and extract issue details"""
    issues = []
    
    try:
        with open(log_file, 'r', errors='replace') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {log_file}: {e}", file=sys.stderr)
        return []
    
    # Extract run ID from filename
    run_id = os.path.basename(log_file).replace('helgrind_', '').replace('.log', '')
    
    # Split into error blocks
    error_blocks = re.split(r'==\d+== ?---Thread-Announcement--', content)
    
    # Process each error block
    for i, block in enumerate(error_blocks):
        if i == 0:  # Skip header
            continue
        
        # Reassemble complete block with header
        if i > 0:
            block = "==Thread-Announcement--" + block
        
        lines = block.strip().split('\n')
        
        # Skip blocks that match exclude patterns
        if any(is_excluded(line, exclude_patterns, include_patterns) for line in lines):
            continue
        
        # Determine issue type
        issue_type = "Unknown"
        description = "Unknown issue"
        
        if "Possible data race" in block:
            issue_type = "Data Race"
            # Extract description from first line of error
            for line in lines:
                if "Possible data race" in line:
                    description = line.strip()
                    break
        elif "Lock order" in block:
            issue_type = "Lock Order Violation"
            # Extract description
            for line in lines:
                if "Lock order" in line:
                    description = line.strip()
                    break
        elif "Thread #" in block and ("read" in block or "write" in block):
            issue_type = "Data Access Violation"
            # Try to extract a useful description
            for line in lines[:10]:
                if "Thread #" in line and ("read" in line or "write" in line):
                    description = line.strip()
                    break
        
        # Extract file locations and stack trace
        file_locations = extract_file_locations(lines)
        stack_trace = extract_stack_trace(lines, HELGRIND)
        
        # Create issue object
        issue = ThreadIssue(
            tool=HELGRIND,
            issue_type=issue_type,
            description=description,
            stack_trace=stack_trace,
            file_locations=file_locations,
            run_id=run_id,
            raw_log=lines
        )
        
        issues.append(issue)
    
    return issues

def parse_drd_log(log_file: str, exclude_patterns: List[str], include_patterns: List[str]) -> List[ThreadIssue]:
    """Parse a DRD log file and extract issue details"""
    issues = []
    
    try:
        with open(log_file, 'r', errors='replace') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {log_file}: {e}", file=sys.stderr)
        return []
    
    # Extract run ID from filename
    run_id = os.path.basename(log_file).replace('drd_', '').replace('.log', '')
    
    # Split into error blocks - DRD has different section markers than Helgrind
    error_blocks = re.split(r'==\d+== ?(?:Conflicting (?:load|store)|[A-Z][a-z]+ lock order violation)', content)
    prev_header = ""
    
    # Process each error block
    for i, block in enumerate(error_blocks):
        if i == 0:  # Skip header block
            continue
        
        # Find the header that was split off
        header_match = re.search(r'==\d+== ?(?:Conflicting (?:load|store)|[A-Z][a-z]+ lock order violation)', 
                                prev_header)
        
        if header_match:
            # Reassemble complete block with header
            block = header_match.group(0) + block
        
        prev_header = block
        lines = block.strip().split('\n')
        
        # Skip blocks that match exclude patterns
        if any(is_excluded(line, exclude_patterns, include_patterns) for line in lines):
            continue
        
        # Determine issue type
        issue_type = "Unknown"
        description = "Unknown issue"
        
        if "Conflicting load" in block or "Conflicting store" in block:
            issue_type = "Data Race"
            # Extract description
            for line in lines:
                if "Conflicting" in line:
                    description = line.strip()
                    break
        elif "lock order violation" in block:
            issue_type = "Lock Order Violation"
            # Extract description
            for line in lines:
                if "lock order violation" in line:
                    description = line.strip()
                    break
        
        # Extract file locations and stack trace
        file_locations = extract_file_locations(lines)
        stack_trace = extract_stack_trace(lines, DRD)
        
        # Create issue object
        issue = ThreadIssue(
            tool=DRD,
            issue_type=issue_type,
            description=description,
            stack_trace=stack_trace,
            file_locations=file_locations,
            run_id=run_id,
            raw_log=lines
        )
        
        issues.append(issue)
    
    return issues

def parse_tsan_log(log_file: str, exclude_patterns: List[str], include_patterns: List[str]) -> List[ThreadIssue]:
    """Parse a ThreadSanitizer log file and extract issue details"""
    issues = []
    
    try:
        with open(log_file, 'r', errors='replace') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {log_file}: {e}", file=sys.stderr)
        return []
    
    # Extract run ID from filename
    run_id = os.path.basename(log_file).replace('tsan_', '').replace('.log', '')
    
    # ThreadSanitizer has a different format - split on "WARNING: ThreadSanitizer:"
    error_blocks = re.split(r'WARNING: ThreadSanitizer:', content)
    
    # Process each error block
    for i, block in enumerate(error_blocks):
        if i == 0:  # Skip header
            continue
        
        # Reassemble complete block with header
        block = "WARNING: ThreadSanitizer:" + block
        
        # Find the end of this error report (usually empty line after stack traces)
        block_end = block.find("\n\n")
        if block_end > 0:
            block = block[:block_end]
        
        lines = block.strip().split('\n')
        
        # Skip blocks that match exclude patterns
        if any(is_excluded(line, exclude_patterns, include_patterns) for line in lines):
            continue
        
        # Determine issue type
        issue_type = "Unknown"
        description = "Unknown issue"
        
        if "data race" in block.lower():
            issue_type = "Data Race"
            # First line is usually the description
            description = lines[0].strip() if lines else "Data Race"
        elif "lock order inversion" in block.lower() or "deadlock" in block.lower():
            issue_type = "Lock Order Violation"
            description = lines[0].strip() if lines else "Lock Order Violation"
        elif "thread leak" in block.lower():
            issue_type = "Thread Leak"
            description = lines[0].strip() if lines else "Thread Leak"
        elif "use-after-free" in block.lower():
            issue_type = "Use After Free"
            description = lines[0].strip() if lines else "Use After Free"
        else:
            # If we can't identify, use first line as type
            description = lines[0].strip() if lines else "Unknown Issue"
            # Try to extract a more specific type from first line
            type_match = re.search(r'(?:WARNING: ThreadSanitizer:)\s*([^:]+)', description)
            if type_match:
                issue_type = type_match.group(1).strip()
        
        # Extract file locations and stack trace
        file_locations = extract_file_locations(lines)
        stack_trace = extract_stack_trace(lines, TSAN)
        
        # Create issue object
        issue = ThreadIssue(
            tool=TSAN,
            issue_type=issue_type,
            description=description,
            stack_trace=stack_trace,
            file_locations=file_locations,
            run_id=run_id,
            raw_log=lines
        )
        
        issues.append(issue)
    
    return issues

def find_log_files(base_dir: str, tool: str) -> List[str]:
    """Find all log files for a specific tool"""
    tool_dir = os.path.join(base_dir, tool)
    if not os.path.exists(tool_dir):
        print(f"Warning: Directory not found: {tool_dir}", file=sys.stderr)
        return []
    
    # Look for files with pattern like 'helgrind_1.log'
    pattern = os.path.join(tool_dir, f"{tool}_*.log")
    return glob.glob(pattern)

def parse_all_logs(args: argparse.Namespace) -> Dict[str, List[ThreadIssue]]:
    """Parse logs from all specified tools"""
    tools = args.tools.split(',')
    all_issues = {}
    
    for tool in tools:
        tool = tool.strip().lower()
        log_files = find_log_files(args.log_dir, tool)
        
        print(f"Found {len(log_files)} log files for {tool}")
        
        issues = []
        for log_file in log_files:
            if tool == HELGRIND:
                issues.extend(parse_helgrind_log(log_file, args.exclude, args.include))
            elif tool == DRD:
                issues.extend(parse_drd_log(log_file, args.exclude, args.include))
            elif tool == TSAN:
                issues.extend(parse_tsan_log(log_file, args.exclude, args.include))
            else:
                print(f"Warning: Unknown tool {tool}", file=sys.stderr)
        
        all_issues[tool] = issues
    
    return all_issues

def deduplicate_issues(issues: List[ThreadIssue]) -> List[ThreadIssue]:
    """Deduplicate issues based on their hash"""
    unique_issues = {}
    
    for issue in issues:
        if issue.hash not in unique_issues:
            unique_issues[issue.hash] = issue
    
    return list(unique_issues.values())

def generate_report(all_issues: Dict[str, List[ThreadIssue]], args: argparse.Namespace) -> str:
    """Generate a human-readable report of the issues"""
    report = []
    
    # Overall summary
    total_issues = sum(len(issues) for issues in all_issues.values())
    report.append(f"Thread Analysis Summary")
    report.append(f"=====================")
    report.append(f"Total unique issues found: {total_issues}")
    report.append("")
    
    # Summary by tool
    for tool, issues in all_issues.items():
        if not issues:
            report.append(f"{tool.upper()}: No issues found")
            continue
        
        # Count issues by type
        issues_by_type = defaultdict(int)
        for issue in issues:
            issues_by_type[issue.issue_type] += 1
        
        report.append(f"{tool.upper()}: {len(issues)} issues found")
        for issue_type, count in issues_by_type.items():
            report.append(f"  - {issue_type}: {count}")
        report.append("")
    
    # If summary only, return here
    if args.summary_only:
        return "\n".join(report)
    
    # Detailed report
    report.append("Detailed Issues")
    report.append("==============")
    
    for tool, issues in all_issues.items():
        if not issues:
            continue
        
        report.append(f"\n{tool.upper()} Issues:")
        report.append("=" * (len(tool) + 8))
        
        # Sort issues by type for better organization
        sorted_issues = sorted(issues, key=lambda x: x.issue_type)
        
        # Show up to max_issues
        for i, issue in enumerate(sorted_issues):
            if i >= args.max_issues:
                report.append(f"\n... and {len(sorted_issues) - args.max_issues} more {tool.upper()} issues")
                break
            
            report.append(f"\nIssue #{i+1}: {issue.issue_type}")
            report.append("-" * (10 + len(issue.issue_type)))
            report.append(f"Description: {issue.description}")
            
            # Show file locations
            if issue.file_locations:
                report.append("File locations:")
                for loc in sorted(issue.file_locations)[:5]:  # Limit to 5 locations
                    report.append(f"  - {loc}")
                if len(issue.file_locations) > 5:
                    report.append(f"  - ...and {len(issue.file_locations) - 5} more locations")
            
            # Show stack trace
            if issue.stack_trace:
                report.append("Stack trace (first 3 frames):")
                for frame in issue.stack_trace[:3]:
                    report.append(f"  - {frame}")
    
    return "\n".join(report)

def main() -> int:
    """Main entry point"""
    args = parse_args()
    
    # Parse logs from all tools
    all_issues = parse_all_logs(args)
    
    # Deduplicate issues for each tool
    for tool in all_issues:
        all_issues[tool] = deduplicate_issues(all_issues[tool])
    
    # Generate report
    report = generate_report(all_issues, args)
    
    # Output report
    if args.output:
        with open(args.output, 'w') as f:
            f.write(report)
        print(f"Report written to {args.output}")
    else:
        print(report)
    
    # Output JSON if requested
    if args.json:
        # Convert issues to dictionaries for JSON serialization
        json_data = {}
        for tool, issues in all_issues.items():
            json_data[tool] = [issue.to_dict() for issue in issues]
        
        with open(args.json, 'w') as f:
            json.dump(json_data, f, indent=2)
        print(f"JSON data written to {args.json}")
    
    return 0

if __name__ == '__main__':
    sys.exit(main()) 