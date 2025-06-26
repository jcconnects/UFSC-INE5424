"""
Vehicle 10081 Internal Communication Latency Analyzer

Analyzes CSV logs to calculate average latency, standard deviation, and outliers
for RECEIVE messages in vehicle internal communication.

CSV Format Expected: timestamp_us,message_type,direction,origin,destination,unit,period_us,value_size,latency_us
"""

import os
import csv
import glob
import statistics
import sys
from pathlib import Path

def find_csv_files(log_directory):
    """Find all CSV files in the specified directory."""
    log_path = Path(log_directory)
    
    # looks for component message CSV files
    csv_files = list(log_path.glob("*_messages.csv"))
    
    # if no specific message files, look for any CSV files
    if not csv_files:
        csv_files = list(log_path.glob("*.csv"))
    
    return csv_files

def extract_receive_latencies(csv_file):
    """Extract latency values from RECEIVE messages in a CSV file."""
    latencies = []
    
    try:
        with open(csv_file, 'r', newline='') as file:
            reader = csv.DictReader(file)
            
            for row in reader:
                # filters for RECEIVE messages only
                if row.get('direction', '').strip().upper() == 'RECEIVE':
                    try:
                        latency_us = float(row.get('latency_us', 0))
                        if latency_us > 0:  # only positive latencies are valid
                            latencies.append(latency_us)
                    except (ValueError, TypeError):
                        continue  # skip invalid values
                        
    except Exception as e:
        print(f"Error reading {csv_file}: {e}")
    
    return latencies

def calculate_outliers(latencies, method='2sigma'):
    """Calculate outliers using 2-sigma method."""
    if len(latencies) < 2:
        return [], 0, 0
    
    mean = statistics.mean(latencies)
    std_dev = statistics.stdev(latencies)
    
    # 2-sigma outliers (values beyond 2 standard deviations)
    lower_bound = mean - 2 * std_dev
    upper_bound = mean + 2 * std_dev
    
    outliers = [lat for lat in latencies if lat < lower_bound or lat > upper_bound]
    
    return outliers, lower_bound, upper_bound

def analyze_latency_data(log_directory):
    """Main analysis function."""
    
    print(f"Analyzing latency data in: {log_directory}")
    print("=" * 60)
    
    # finds CSV files
    csv_files = find_csv_files(log_directory)
    
    if not csv_files:
        print(f"No CSV files found in {log_directory}!")
        print("Please ensure:")
        print("  1. The vehicle test has been run")
        print("  2. CSV logging is enabled")
        print("  3. The log directory path is correct")
        return False
    
    print(f"Found {len(csv_files)} CSV files:")
    for csv_file in csv_files:
        print(f"   • {csv_file.name}")
    print()
    
    # extracts all latencies from RECEIVE messages
    all_latencies = []
    file_stats = {}
    
    for csv_file in csv_files:
        latencies = extract_receive_latencies(csv_file)
        all_latencies.extend(latencies)
        file_stats[csv_file.name] = len(latencies)
        print(f"{csv_file.name}: {len(latencies)} RECEIVE messages")
    
    if not all_latencies:
        print("\nNo RECEIVE messages with valid latencies found!")
        print("This could mean:")
        print("  1. No consumer components were active")
        print("  2. No data was exchanged between components")
        print("  3. CSV logging format differs from expected")
        return False
    
    # calculates statistics
    print(f"\n LATENCY ANALYSIS RESULTS")
    print("=" * 60)
    
    total_messages = len(all_latencies)
    average_latency = statistics.mean(all_latencies)
    std_deviation = statistics.stdev(all_latencies) if total_messages > 1 else 0
    min_latency = min(all_latencies)
    max_latency = max(all_latencies)
    median_latency = statistics.median(all_latencies)
    
    # calculates outliers
    outliers, lower_bound, upper_bound = calculate_outliers(all_latencies)
    outlier_count = len(outliers)
    outlier_percentage = (outlier_count / total_messages) * 100
    
    # prints main statistics
    print(f"Total RECEIVE messages analyzed: {total_messages:,}")
    print(f"Average latency:                 {average_latency:.2f} μs")
    print(f"Standard deviation:              {std_deviation:.2f} μs")
    print(f"Minimum latency:                 {min_latency:.2f} μs")
    print(f"Maximum latency:                 {max_latency:.2f} μs")
    print(f"Median latency:                  {median_latency:.2f} μs")
    
    print(f"\n OUTLIER ANALYSIS (2-sigma method)")
    print("-" * 40)
    print(f"Outlier bounds:                  {lower_bound:.2f} - {upper_bound:.2f} μs")
    print(f"Number of outliers:              {outlier_count:,}")
    print(f"Percentage of outliers:          {outlier_percentage:.2f}%")
    
    if outlier_count > 0:
        print(f"Outlier latencies (first 10):    ", end="")
        print(", ".join([f"{lat:.1f}" for lat in sorted(outliers)[:10]]))
        if len(outliers) > 10:
            print(f"                                 ... and {len(outliers)-10} more")
    
    # performance assessment
    print(f"\n PERFORMANCE ASSESSMENT")
    print("-" * 40)
    if average_latency < 1000:
        performance = "EXCELLENT"
        color = "🟢"
    elif average_latency < 5000:
        performance = "GOOD"
        color = "🟡"
    elif average_latency < 10000:
        performance = "ACCEPTABLE"
        color = "🟠"
    else:
        performance = "NEEDS ATTENTION"
        color = "🔴"
    
    print(f"Overall performance:             {color} {performance}")
    print(f"Consistency (low std dev):       {'🟢 GOOD' if std_deviation < average_latency * 0.3 else '🟡 VARIABLE'}")
    print(f"Outlier rate:                    {'🟢 LOW' if outlier_percentage < 5 else '🟡 MODERATE' if outlier_percentage < 15 else '🔴 HIGH'}")
    
    # additional percentiles
    print(f"\n LATENCY PERCENTILES")
    print("-" * 40)
    sorted_latencies = sorted(all_latencies)
    n = len(sorted_latencies)
    
    percentiles = [50, 75, 90, 95, 99]
    for p in percentiles:
        index = min(int(n * p / 100), n - 1)
        print(f"P{p:2d} (bottom {p:2d}%):                 {sorted_latencies[index]:.2f} μs")
    
    return True

def main():
    """Main function"""
    
    # default log directory for vehicle 10081
    default_log_dir = "tests/logs/vehicle_10081"
    
    # checks command line argument for custom directory
    if len(sys.argv) > 1:
        log_directory = sys.argv[1]
    else:
        log_directory = default_log_dir
    
    print(" Vehicle 10081 - Internal Communication Latency Analyzer")
    print("=" * 60)
    
    # checks if directory exists
    if not os.path.exists(log_directory):
        print(f" Directory not found: {log_directory}")
        print(f"\nUsage: python {sys.argv[0]} [log_directory]")
        print(f"Example: python {sys.argv[0]} tests/logs/vehicle_10081")
        return 1
    
    # runs analysis
    success = analyze_latency_data(log_directory)
    
    if success:
        print(f"\n ANALYSIS COMPLETE")
        print(f"Analyzed logs from: {log_directory}")
        print(f"Focus: RECEIVE message latencies for internal communication")
        return 0
    else:
        return 1

if __name__ == "__main__":
    exit(main()) 