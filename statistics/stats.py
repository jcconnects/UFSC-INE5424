import csv
import sys
from os import path
import statistics

import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def main():
    # garantee number of vehicles is passed as parameter
    if len(sys.argv) != 2:
        sys.exit("Usage: python3 statictics/stats.py [n_vehicles]")

    # read csv files
    n_vehicles = int(sys.argv[1])
    latencies = list()
    for i in range(1, n_vehicles + 1):
        file_path = path.join('logs', f'vehicle_{i}_receiver.csv')
        with open(file_path, 'r', newline='') as f:

            reader = csv.DictReader(f)
            for row in reader:
                latencies.append(int(row['latency_us']))

    # calculate stats
    median = statistics.median(latencies)
    mean = statistics.mean(latencies)
    q1, med_q, q3 = statistics.quantiles(latencies)
    std_dev = statistics.stdev(latencies)
    var = statistics.variance(latencies)
    n_upper_outliers = 0
    n_lower_outliers = 0
    iqr = q3 - q1
    lower_bound = q1 - 1.5 * iqr
    upper_bound = q3 + 1.5 * iqr

    for value in latencies: 
        if value > upper_bound: n_upper_outliers += 1
        elif value < lower_bound: n_lower_outliers += 1

    lower_str = ['.'] * n_lower_outliers
    lower_str = "".join(lower_str)
    upper_str = ['.'] * n_upper_outliers
    upper_str = "".join(upper_str)

    print(
        f"""Statistics:
Median: {median}; Mean: {mean}; Variance: {var}; Standard Deviation: {std_dev};
{lower_str} | Q1 = {q1} | Q2 = {med_q} | Q3 = {q3} | {upper_str}
Max: {max(latencies)}; Min: {min(latencies)}
"""
    )

    data_series = pd.Series(latencies, name="Latência")
    print(f"Calculated Lower Whisker bound (approx): {lower_bound:.2f}")
    print(f"Calculated Upper Whisker bound (approx): {upper_bound:.2f}")

    plt.figure(figsize=(6, 8))

    ax = sns.boxplot(y=data_series,
                    showfliers=True,
                    color="skyblue",
                    linewidth=1.5,  
                    )

    ax.set_title('Distribution and Quartiles Visualization with 64 buffers', fontsize=16)
    ax.set_ylabel(data_series.name, fontsize=12)

    plt.grid(axis='y', linestyle='--', alpha=0.7)

    image_filename = path.join('statistics','quartile_boxplot.png')
    plt.savefig(image_filename, dpi=300, bbox_inches='tight') 
    print(f"\nPlot saved as '{image_filename}'")
    print(len(latencies))

    plt.show()

main()