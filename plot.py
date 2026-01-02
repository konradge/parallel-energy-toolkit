import pandas as pd
import matplotlib.pyplot as plt

# 1. Load the data
# skipinitialspace=True fixes the "KeyError: 'energy'" by ignoring spaces after commas
df = pd.read_csv('benchmark_results.csv', skipinitialspace=True)

# 2. Use a nice style
plt.style.use('bmh')

# 3. Create subplots
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

# --- Plot Energy ---
ax1.plot(df['iteration'], df['energy'], color='#2E86C1', linewidth=2, label='Total Energy')
ax1.fill_between(df['iteration'], df['min_core_energy'], df['max_core_energy'], 
                 color='#2E86C1', alpha=0.3, label='Core Range (Min-Max)')
ax1.set_ylabel('Energy')
ax1.set_title('Energy Consumption per Iteration')
ax1.legend(loc='upper left')

# --- Plot Runtime ---
ax2.plot(df['iteration'], df['runtime'], color='#E74C3C', linewidth=2, label='Total Runtime')
ax2.fill_between(df['iteration'], df['min_core_runtime'], df['max_core_runtime'], 
                 color='#E74C3C', alpha=0.3, label='Core Range (Min-Max)')
ax2.set_xlabel('Iteration')
ax2.set_ylabel('Runtime (s)')
ax2.set_title('Runtime per Iteration')
ax2.legend(loc='upper left')

# 4. Save the plot instead of showing it to avoid server errors
plt.tight_layout()
plt.savefig('energy_plot.png', dpi=300)

print("Plot saved successfully as 'energy_plot.png'")