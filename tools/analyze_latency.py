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

def find_csv_files(log_directory, id=""):
    """Find all CSV files in the specified directory."""
    log_path = Path(log_directory)
    
    # looks for component message CSV files
    csv_files = list(log_path.glob(f"*gateway_" + id + "_messages.csv"))
    
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

            file_type = reader.fieldnames and reader.fieldnames[0] == 'latency_us'
            
            for row in reader:
                # filters for RECEIVE messages only
                
                if not file_type and row.get('direction', '').strip().upper() == 'RECEIVE':
                    try:
                        latency_us = float(row.get('latency_us', 0))
                        if row.get('origin', '').strip()[:-1] != row.get('destination', '').strip()[:-1] and latency_us > 0:  # only positive latencies are valid
                            latencies.append(latency_us)
                    except (ValueError, TypeError):
                        continue  # skip invalid values
                if file_type and int(row.get('latency_us', '').strip()) > 0:
                    latencies.append(int(row.get('latency_us', '').strip()))
                        
    except Exception as e:
        print(f"Error reading {csv_file}: {e}")
    
    return latencies

def calculate_outliers(latencies, method='IQR'):
    """Calculate outliers using IQR (Interquartile Range) method."""
    if len(latencies) < 4:
        return [], 0, 0
    
    sorted_latencies = sorted(latencies)
    n = len(sorted_latencies)
    
    # Calculate Q1 (25th percentile) and Q3 (75th percentile)
    q1_index = int(n * 0.25)
    q3_index = int(n * 0.75)
    q1 = sorted_latencies[q1_index]
    q3 = sorted_latencies[q3_index]
    
    # Calculate IQR and bounds
    iqr = q3 - q1
    lower_bound = q1 - 1.5 * iqr
    upper_bound = q3 + 1.5 * iqr
    
    outliers = [lat for lat in latencies if lat < lower_bound or lat > upper_bound]
    
    return outliers, lower_bound, upper_bound

def analyze_latency_data(csv_files):
    """Main analysis function."""
    
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
    
    # filter out latencies above 200,000 μs (200ms)
    total_messages = len(all_latencies)
    cleaned_latencies = [lat for lat in all_latencies if lat <= 1000000]
    filtered_count = total_messages - len(cleaned_latencies)
    filtered_percentage = (filtered_count / total_messages) * 100 if total_messages > 0 else 0
    
    if not cleaned_latencies:
        print("\nNo valid latencies found after filtering!")
        return False
    
    # calculates statistics for cleaned dataset
    cleaned_total = len(cleaned_latencies)
    cleaned_average = statistics.mean(cleaned_latencies)
    cleaned_std_dev = statistics.stdev(cleaned_latencies) if cleaned_total > 1 else 0
    cleaned_min = min(cleaned_latencies)
    cleaned_max = max(cleaned_latencies)
    cleaned_median = statistics.median(cleaned_latencies)
    
    print(f"\n LATENCY ANALYSIS RESULTS")
    print("=" * 60)
    print(f"Total RECEIVE messages analyzed: {filtered_count:,}")
    print(f"Valid messages for analysis:     {cleaned_total:,}")
    print(f"Average latency:                 {cleaned_average:.2f} μs")
    print(f"Standard deviation:              {cleaned_std_dev:.2f} μs")
    print(f"Minimum latency:                 {cleaned_min:.2f} μs")
    print(f"Maximum latency:                 {cleaned_max:.2f} μs")
    print(f"Median latency:                  {cleaned_median:.2f} μs")
    
    # performance assessment
    print(f"\n PERFORMANCE ASSESSMENT")
    print("-" * 40)
    
    if cleaned_average < 1000:
        performance = "EXCELLENT"
        color = "🟢"
    elif cleaned_average < 5000:
        performance = "GOOD"
        color = "🟡"
    elif cleaned_average < 10000:
        performance = "ACCEPTABLE"
        color = "🟠"
    else:
        performance = "NEEDS ATTENTION"
        color = "🔴"
    
    print(f"Overall performance:             {color} {performance}")
    print(f"Consistency (low std dev):       {'🟢 GOOD' if cleaned_std_dev < cleaned_average * 0.3 else '🟡 VARIABLE'}")
    print(f"Filter rate:                     {'🟢 LOW' if filtered_percentage < 5 else '🟡 MODERATE' if filtered_percentage < 15 else '🔴 HIGH'}")
    
    # additional percentiles
    print(f"\n LATENCY PERCENTILES")
    print("-" * 40)
    sorted_percentile_data = sorted(cleaned_latencies)
    n = len(sorted_percentile_data)
    
    percentiles = [50, 75, 90, 95, 99]
    for p in percentiles:
        index = min(int(n * p / 100), n - 1)
        print(f"P{p:2d} (bottom {p:2d}%):                 {sorted_percentile_data[index]:.2f} μs")

    counter = 0
    counter_total = len(cleaned_latencies)
    for latency in sorted_percentile_data:
        if latency > 10000:
            counter += 1
    print(f"Number of latencies > 10ms: {counter}")
    print(f"Percentage of latencies > 10ms: {counter / counter_total * 100:.2f}%")
    
    return True

def main():
    """Main function"""
    
    # default log directory for vehicle 10081
    default_log_dir = "latencies"
    log_directories = []
    
    # checks command line argument for custom directory
    if len(sys.argv) > 1:
        log_directories = sys.argv[1:]
    else:
        log_directories = [default_log_dir]
    
    print(" Vehicle 10081 - Internal Communication Latency Analyzer")
    print("=" * 60)

    csv_files = []
    
    for log_directory in log_directories:
        # checks if directory exists
        if not os.path.exists(log_directory):
            print(f" Directory not found: {log_directory}")
            print(f"\nUsage: python {sys.argv[0]} [log_directory]")
            print(f"Example: python {sys.argv[0]} tests/logs/vehicle_10081")
            return 1

        print(f"Analyzing latency data in: {log_directory}")
        print("=" * 60)
        
        # finds CSV files
        csv_files += find_csv_files(log_directory, log_directory[-1])
        
        if not csv_files:
            print(f"No CSV files found in {log_directory}!")
            print("Please ensure:")
            print("  1. The vehicle test has been run")
            print("  2. CSV logging is enabled")
            print("  3. The log directory path is correct")
            return False
        
        # runs analysis
    success = analyze_latency_data(csv_files)
    
    if success:
        print(f"\n ANALYSIS COMPLETE")
        print(f"Analyzed logs from: {log_directory}")
        print(f"Focus: RECEIVE message latencies for internal communication")
        return 0
    else:
        return 1

if __name__ == "__main__":
    exit(main())