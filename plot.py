import sys
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

csv_name = sys.argv[1]

# Load the data
df = pd.read_csv(csv_name)

# Calculate Runtime * Energy (Energy-Delay Product)
df['runtime_energy'] = df['runtime'] * df['energy']

# Create subplots (1 row, 3 columns)
fig, axes = plt.subplots(1, 3, figsize=(18, 6))

metrics = [
    ('runtime', r'Runtime ($s$)', 'Runtime by Core Count'),
    ('energy', r'Energy ($J$)', 'Energy Consumption by Core Count'),
    ('runtime_energy', r'Energy-Delay Product ($J \cdot s$)', 'Runtime * Energy (EDP) by Core Count')
]

for i, (col, ylabel, title) in enumerate(metrics):
    # Draw boxplot
    sns.boxplot(ax=axes[i], x='thread_count', y=col, data=df, color='lightgray')
   
    # Add trend line connecting means
    sns.pointplot(ax=axes[i], x='thread_count', y=col, data=df, estimator='mean', 
                  color='red', errorbar=None, markers='o', linestyles='-')
    
    axes[i].set_title(title)
    axes[i].set_xlabel('Core Count')
    axes[i].set_ylabel(ylabel)

plt.tight_layout()
png_file = csv_name.replace('.csv', '_plots.png')
plt.savefig(png_file)
print("Plots saved to", png_file)