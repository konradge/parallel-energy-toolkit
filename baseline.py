import os
import pandas as pd

csv_name = "results/sleep_1000.csv"

# check if file exists

noise_per_s_per_thread = 0

if os.path.exists(csv_name):
    # Load the data
    df = pd.read_csv(csv_name)

    df["runtime_s"] = df["runtime_ns"] / 1e9  # Convert runtime to seconds

    df["noise_per_s_per_thread"] = df["energy"] / df["runtime_s"] / df["distinct_threads"]

    noise_per_s_per_thread = df["noise_per_s_per_thread"].mean()
    
print(f"Using Baseline: {noise_per_s_per_thread} J")
