#!/usr/bin/env python3
import os
import glob
import re
from collections import Counter

# Path to logs directory
LOGS_DIR = "logs/"
REPORT_FILE = "incomplete_logs_report.txt"

def is_incomplete_log(log_content):
    """Check if the log file did not complete properly"""
    # A complete log should end with this message
    complete_message = "Vehicle object deleted and terminated cleanly."
    
    return complete_message not in log_content

def extract_end_segment(log_content):
    """Extract the content from 'lifetime ended. Stopping vehicle.' to the end"""
    match = re.search(r'\[Vehicle \d+\] lifetime ended\. Stopping vehicle\..*', 
                      log_content, re.DOTALL)
    if match:
        return match.group(0)
    return None

def identify_last_component(segment):
    """Identify the last component being processed in the log"""
    component_matches = re.findall(r'Stopping component (\w+)', segment)
    if component_matches:
        return component_matches[-1]
    return "Unknown"

def main():
    # Find all vehicle log files
    log_files = glob.glob(os.path.join(LOGS_DIR, "vehicle_*.log"))
    
    incomplete_logs = []
    component_counter = Counter()
    
    # Check each log file
    for log_file in log_files:
        try:
            with open(log_file, 'r', errors='replace') as f:
                content = f.read()
                
            if is_incomplete_log(content):
                vehicle_id = os.path.basename(log_file).replace('vehicle_', '').replace('.log', '')
                end_segment = extract_end_segment(content)
                
                if end_segment:
                    last_component = identify_last_component(end_segment)
                    component_counter[last_component] += 1
                    incomplete_logs.append((vehicle_id, end_segment, last_component))
        except Exception as e:
            print(f"Error processing {log_file}: {e}")
    
    # Sort by vehicle ID (numeric order)
    incomplete_logs.sort(key=lambda x: int(x[0]))
    
    # Generate report
    if incomplete_logs:
        with open(REPORT_FILE, 'w') as f:
            f.write(f"Found {len(incomplete_logs)} incomplete vehicle logs:\n\n")
            
            # Write component summary
            f.write("=== Component Summary ===\n")
            for component, count in component_counter.items():
                f.write(f"{component}: {count} vehicles\n")
            f.write("\n\n")
            
            # Write detailed logs
            for vehicle_id, segment, component in incomplete_logs:
                f.write(f"{'=' * 20} Vehicle {vehicle_id} (Last Component: {component}) {'=' * 20}\n")
                f.write(segment)
                f.write("\n\n")
        
        print(f"Found {len(incomplete_logs)} incomplete vehicle logs. Report saved to {REPORT_FILE}")
        print("\n=== Component Summary ===")
        for component, count in component_counter.items():
            print(f"{component}: {count} vehicles")
    else:
        print("No incomplete vehicle logs found.")

if __name__ == "__main__":
    main() 