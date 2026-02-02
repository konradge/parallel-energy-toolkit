import sys
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from baseline import noise_per_s_per_thread

csv_name = sys.argv[1]

# Load the data
df = pd.read_csv(csv_name)

df["runtime_s"] = df["runtime_ns"] / 1e9  # Convert runtime to seconds

if noise_per_s_per_thread > 0:
    print("Using noise per s per thread from baseline:", noise_per_s_per_thread)

df["energy_without_noise"] = df["energy"] - df["distinct_threads"] * noise_per_s_per_thread *  df["runtime_s"]

# Calculate Runtime * Energy (Energy-Delay Product)
df['runtime_energy'] = df['runtime_s'] * df['energy_without_noise']

# Create subplots (1 row, 3 columns)
fig, axes = plt.subplots(1, 3, figsize=(18, 6))

metrics = [
    ('runtime_s', r'Runtime ($s$)', 'Runtime by Worker Count'),
    ('energy_without_noise', r'Energy ($J$)', 'Energy Consumption by Worker Count'),
    ('runtime_energy', r'Energy-Delay Product ($J \cdot s$)', 'Runtime * Energy (EDP) by Worker Count')
]

for i, (col, ylabel, title) in enumerate(metrics):
    # Draw boxplot
    sns.boxplot(ax=axes[i], x='worker_count', y=col, data=df, color='lightgray')
   
    # Add trend line connecting means
    sns.pointplot(ax=axes[i], x='worker_count', y=col, data=df, estimator='mean', 
                  color='red', errorbar=None, markers='o', linestyles='-')
    
    axes[i].set_title(title)
    axes[i].set_xlabel('Worker Count')
    axes[i].set_ylabel(ylabel)

plt.tight_layout()
png_file = csv_name.replace('.csv', '_plots.png')
plt.savefig(png_file)
print("Plots saved to", png_file)