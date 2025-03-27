import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Function to read data from a file and return a DataFrame
def read_data(file_path):
    return pd.read_csv(file_path, sep='\t')

# Function to calculate statistics for a given DataFrame and columns
def calculate_statistics(data, columns):
    stats = {}
    for col in columns:
        stats[f'{col}_avg'] = data[col].mean()
        stats[f'{col}_p99'] = data[col].quantile(0.99)
        stats[f'{col}_max'] = data[col].max()
    return stats

# Directory containing the files
directory = 'results'

# Initialize lists to store data for plotting
plot_data = []

# Iterate over each file in the directory
for filename in os.listdir(directory):
    if filename.endswith('.txt'):
        # Extract experiment type and message size from the filename
        parts = filename.split('_')
        experiment_type = '_'.join(parts[:-1])
        message_size = int(parts[-1].split('.')[0])

        # Read data from the file
        file_path = os.path.join(directory, filename)
        data = read_data(file_path)

        # Determine columns to be used for statistics calculation based on experiment type
        columns = []
        if 'async_io' in experiment_type:
            columns = ['initiation_duration_nsec', 'write_duration_nsec', 'fsync_duration_nsec']
        elif 'mmap_io' in experiment_type:
            columns = ['memcpy_duration_nsec', 'msync_duration_nsec']
        elif 'rdma_send_recv' in experiment_type:
            columns = ['before wait', 'after wait', 'rtt']
        elif 'sync_io' in experiment_type:
            columns = ['write_duration_nsec', 'flush_duration_nsec']
        else:
            continue

        # Calculate statistics for the current file
        stats = calculate_statistics(data, columns)

        # Store statistics along with experiment type and message size
        row_data = {'Experiment Type': experiment_type, 'Message Size': message_size}
        row_data.update(stats)
        plot_data.append(row_data)

# Create a DataFrame for plotting
plot_df = pd.DataFrame(plot_data)

# Filter the DataFrame for message sizes up to 16384 (2^14) for 'all' conditions
truncated_df_all = plot_df[plot_df['Message Size'] <= 16384]

# Filter the DataFrame for message sizes beyond 16384 (2^14) for 'all' conditions
beyond_truncated_df_all = plot_df[plot_df['Message Size'] > 16384]

# Filter the DataFrame for message sizes up to 16384 (2^14) for 'removed' conditions
truncated_df_removed = plot_df[plot_df['Message Size'] <= 16384]

# Filter the DataFrame for message sizes beyond 16384 (2^14) for 'removed' conditions
beyond_truncated_df_removed = plot_df[plot_df['Message Size'] > 16384]

# Get unique experiment types
unique_experiment_types = plot_df['Experiment Type'].unique()
unique_experiment_types_truncated_all = truncated_df_all['Experiment Type'].unique()
unique_experiment_types_beyond_all = beyond_truncated_df_all['Experiment Type'].unique()
unique_experiment_types_truncated_removed = truncated_df_removed['Experiment Type'].unique()
unique_experiment_types_beyond_removed = beyond_truncated_df_removed['Experiment Type'].unique()

# Define metrics to plot
metrics = ['avg', 'p99', 'max']

# Define a consistent linewidth and linestyle
common_linewidth = 1
common_linestyle = '-'

# Choose a colormap with 10 distinct colors
cmap = plt.get_cmap('tab10')
colors = [cmap(i) for i in range(10)]

# Create a mapping for conditions to colors (for all conditions)
condition_colors_all = {}
color_index_all = 0
for experiment_type in unique_experiment_types:
    if 'async_io' in experiment_type:
        for col in ['initiation_duration_nsec', 'write_duration_nsec', 'fsync_duration_nsec']:
            condition_colors_all[(experiment_type, col)] = colors[color_index_all % 10]
            color_index_all += 1
    elif 'mmap_io' in experiment_type:
        for col in ['memcpy_duration_nsec', 'msync_duration_nsec']:
            condition_colors_all[(experiment_type, col)] = colors[color_index_all % 10]
            color_index_all += 1
    elif 'rdma_send_recv' in experiment_type:
        for col in ['before wait', 'after wait', 'rtt']:
            condition_colors_all[(experiment_type, col)] = colors[color_index_all % 10]
            color_index_all += 1
    elif 'sync_io' in experiment_type:
        for col in ['write_duration_nsec', 'flush_duration_nsec']:
            condition_colors_all[(experiment_type, col)] = colors[color_index_all % 10]
            color_index_all += 1

# Create a mapping for conditions to colors (with specified conditions removed)
condition_colors_removed = {}
color_index_removed = 0
unique_experiment_types_removed_cmap = unique_experiment_types # Reusing for simplicity
for experiment_type in unique_experiment_types_removed_cmap:
    if 'async_io' in experiment_type:
        for col in ['initiation_duration_nsec', 'write_duration_nsec']:
            condition_colors_removed[(experiment_type, col)] = colors[color_index_removed % 10]
            color_index_removed += 1
    elif 'mmap_io' in experiment_type:
        for col in ['memcpy_duration_nsec']:
            condition_colors_removed[(experiment_type, col)] = colors[color_index_removed % 10]
            color_index_removed += 1
    elif 'rdma_send_recv' in experiment_type:
        for col in ['before wait', 'after wait', 'rtt']:
            condition_colors_removed[(experiment_type, col)] = colors[color_index_removed % 10]
            color_index_removed += 1
    elif 'sync_io' in experiment_type:
        for col in ['write_duration_nsec']:
            condition_colors_removed[(experiment_type, col)] = colors[color_index_removed % 10]
            color_index_removed += 1

# Create the 'plots' directory if it doesn't exist
os.makedirs("plots", exist_ok=True)

# Create the figure and subplots for the original data (all conditions)
fig, axes = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig.suptitle('Time spent by threads for I/O')
for i, metric_type in enumerate(metrics):
    ax = axes[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types:
        subset = plot_df[plot_df['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['initiation_duration_nsec', 'write_duration_nsec', 'fsync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec', 'msync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec', 'flush_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
    max_size_all = plot_df['Message Size'].max()
    upper_power_all = int(np.ceil(np.log2(max_size_all))) if max_size_all > 0 else 0
    xticks_all = [2**i for i in range(upper_power_all + 1)]
    ax.set_xticks(xticks_all)
axes[-1].set_xlabel('Message Size (Bytes)')
handles, labels = axes[0].get_legend_handles_labels()
fig.legend(handles, labels, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, 0.1, 1, 1))
plt.savefig("plots/plot_all_conditions.png")

# Create the figure and subplots for the truncated data (all conditions)
fig_truncated_all, axes_truncated_all = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_truncated_all.suptitle(f'Time spent by threads for I/O (Message Size <= 16KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_truncated_all[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types_truncated_all:
        subset = truncated_df_all[truncated_df_all['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['initiation_duration_nsec', 'write_duration_nsec', 'fsync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec', 'msync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec', 'flush_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
    xticks_truncated = [2**i for i in range(15)]
    ax.set_xticks(xticks_truncated)
    ax.set_xlim(0.9, 20000) # Allow some whitespace for truncated plot (all)
axes_truncated_all[-1].set_xlabel('Message Size (Bytes)')
handles_truncated_all, labels_truncated_all = axes_truncated_all[0].get_legend_handles_labels()
fig_truncated_all.legend(handles_truncated_all, labels_truncated_all, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, 0.1, 1, 1)) # Adjust layout to make space for the legend
plt.savefig("plots/plot_truncated_all_conditions.png")

# Create the figure and subplots for the data beyond the truncation (all conditions)
fig_beyond_all, axes_beyond_all = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_beyond_all.suptitle(f'Time spent by threads for I/O (Message Size > 16KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_beyond_all[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types_beyond_all:
        subset = beyond_truncated_df_all[beyond_truncated_df_all['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['initiation_duration_nsec', 'write_duration_nsec', 'fsync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec', 'msync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec', 'flush_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
    max_size_beyond_all = beyond_truncated_df_all['Message Size'].max()
    upper_power_beyond_all = int(np.ceil(np.log2(max_size_beyond_all))) if max_size_beyond_all > 0 else 15
    xticks_beyond = [2**i for i in range(15, upper_power_beyond_all + 1)]
    ax.set_xticks(xticks_beyond)
axes_beyond_all[-1].set_xlabel('Message Size (Bytes)')
handles_beyond_all, labels_beyond_all = axes_beyond_all[0].get_legend_handles_labels()
fig_beyond_all.legend(handles_beyond_all, labels_beyond_all, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, 0.1, 1, 1))
plt.savefig("plots/plot_beyond_truncated_all_conditions.png")

# Create the figure and subplots for the original data (without the two conditions) - ALL DATA
fig_removed, axes_removed = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_removed.suptitle('Time spent by threads for I/O (Conditions Removed)')
for i, metric_type in enumerate(metrics):
    ax = axes_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types:
        subset = plot_df[plot_df['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['initiation_duration_nsec', 'write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
    max_size_removed = plot_df['Message Size'].max()
    upper_power_removed = int(np.ceil(np.log2(max_size_removed))) if max_size_removed > 0 else 0
    xticks_removed = [2**i for i in range(upper_power_removed + 1)]
    ax.set_xticks(xticks_removed)
axes_removed[-1].set_xlabel('Message Size (Bytes)')
handles_removed, labels_removed = axes_removed[0].get_legend_handles_labels()
fig_removed.legend(handles_removed, labels_removed, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, 0.1, 1, 1))
plt.savefig("plots/plot_removed_conditions.png")

# Create the figure and subplots for the original data (without the two conditions) - TRUNCATED
fig_truncated_removed, axes_truncated_removed = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_truncated_removed.suptitle('Time spent by threads for I/O (Conditions Removed, <= 16KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_truncated_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types_truncated_removed:
        subset = truncated_df_removed[truncated_df_removed['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['initiation_duration_nsec', 'write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
    xticks_truncated_removed = [2**i for i in range(15)]
    ax.set_xticks(xticks_truncated_removed)
    ax.set_xlim(0.9, 20000)
axes_truncated_removed[-1].set_xlabel('Message Size (Bytes)')
handles_truncated_removed, labels_truncated_removed = axes_truncated_removed[0].get_legend_handles_labels()
fig_truncated_removed.legend(handles_truncated_removed, labels_truncated_removed, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, 0.1, 1, 1))
plt.savefig("plots/plot_truncated_removed_conditions.png")

# Create the figure and subplots for the original data (without the two conditions) - BEYOND TRUNCATED
fig_beyond_truncated_removed, axes_beyond_truncated_removed = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_beyond_truncated_removed.suptitle('Time spent by threads for I/O (Conditions Removed, > 16KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_beyond_truncated_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types_beyond_removed:
        subset = beyond_truncated_df_removed[beyond_truncated_df_removed['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['initiation_duration_nsec', 'write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}'.replace('_duration_nsec', ''), color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
    max_size_beyond_removed = beyond_truncated_df_removed['Message Size'].max()
    upper_power_beyond_removed = int(np.ceil(np.log2(max_size_beyond_removed))) if max_size_beyond_removed > 0 else 15
    xticks_beyond_removed = [2**i for i in range(15, upper_power_beyond_removed + 1)]
    ax.set_xticks(xticks_beyond_removed)
axes_beyond_truncated_removed[-1].set_xlabel('Message Size (Bytes)')
handles_beyond_truncated_removed, labels_beyond_truncated_removed = axes_beyond_truncated_removed[0].get_legend_handles_labels()
fig_beyond_truncated_removed.legend(handles_beyond_truncated_removed, labels_beyond_truncated_removed, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, 0.1, 1, 1))
plt.savefig("plots/plot_beyond_truncated_removed_conditions.png")

# --- New Plot Set 1: Time for complete write ---
plot_set1_base_title = "Time for complete write"
for data_range, df, filename_suffix in [
    ("All Data", plot_df, "all"),
    ("Up to 16KB", plot_df[plot_df['Message Size'] <= 16384], "truncated"),
    ("Beyond 16KB", plot_df[plot_df['Message Size'] > 16384], "beyond_truncated")
]:
    fig_set1, axes_set1 = plt.subplots(len(metrics), 1, figsize=(15, 10), sharex=True) # Uniform figsize
    title_prefix = f"{plot_set1_base_title}"
    if data_range != "All Data":
        title_prefix += f" - {data_range}"
    fig_set1.suptitle(title_prefix)
    unique_experiment_types_current = df['Experiment Type'].unique()
    for i, metric_type in enumerate(metrics):
        ax = axes_set1[i]
        ax.set_title(f'{metric_type.upper()} Time')
        ax.grid(True) # Ensure grid is always present
        for experiment_type in unique_experiment_types_current:
            subset = df[df['Experiment Type'] == experiment_type]
            subset_sorted = subset.sort_values(by='Message Size')
            if 'sync_io' in experiment_type:
                metric_col = f'flush_duration_nsec_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'flush_duration_nsec'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - flush_duration_nsec', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
            elif 'async_io' in experiment_type:
                metric_col = f'fsync_duration_nsec_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'fsync_duration_nsec'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - fsync_duration_nsec', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
            elif 'mmap_io' in experiment_type:
                metric_col = f'msync_duration_nsec_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'msync_duration_nsec'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - msync_duration_nsec', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
            elif 'rdma_send_recv' in experiment_type:
                metric_col = f'rtt_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'rtt'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - rtt', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        ax.set_xscale('log', base=2)
        ax.set_ylabel('Time (µs)')
        max_size = df['Message Size'].max() if not df.empty else 0
        upper_power = int(np.ceil(np.log2(max_size))) if max_size > 0 else 0
        xticks = [2**i for i in range(upper_power + 1)]
        ax.set_xticks(xticks)
        if data_range == "Beyond 16KB":
            min_beyond = df['Message Size'].min() if not df.empty else 2**15
            max_beyond = df['Message Size'].max() if not df.empty else 2**20
            ax.set_xlim(min_beyond * 0.9, max_beyond * 1.1)
    axes_set1[-1].set_xlabel('Message Size (Bytes)')
    handles_set1, labels_set1 = axes_set1[0].get_legend_handles_labels()
    fig_set1.legend(handles_set1, labels_set1, loc='lower center', ncol=2)
    plt.tight_layout(rect=(0, 0.1, 1, 1))
    plt.savefig(f"plots/plot_set1_{filename_suffix}.png")
    plt.close(fig_set1)

# --- New Plot Set 2: Time for 'cached' write ---
plot_set2_base_title = "Time for 'cached' write"
for data_range, df, filename_suffix in [
    ("All Data", plot_df, "all"),
    ("Up to 16KB", plot_df[plot_df['Message Size'] <= 16384], "truncated"),
    ("Beyond 16KB", plot_df[plot_df['Message Size'] > 16384], "beyond_truncated")
]:
    fig_set2, axes_set2 = plt.subplots(len(metrics), 1, figsize=(15, 10), sharex=True) # Uniform figsize
    title_prefix = f"{plot_set2_base_title}"
    if data_range != "All Data":
        title_prefix += f" - {data_range}"
    fig_set2.suptitle(title_prefix)
    unique_experiment_types_current = df['Experiment Type'].unique()
    for i, metric_type in enumerate(metrics):
        ax = axes_set2[i]
        ax.set_title(f'{metric_type.upper()} Time')
        ax.grid(True)
        for experiment_type in unique_experiment_types_current:
            subset = df[df['Experiment Type'] == experiment_type]
            subset_sorted = subset.sort_values(by='Message Size')
            if 'sync_io' in experiment_type:
                metric_col = f'write_duration_nsec_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'write_duration_nsec'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - write_duration_nsec', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
            elif 'async_io' in experiment_type:
                metric_col = f'write_duration_nsec_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'write_duration_nsec'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - write_duration_nsec', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
            elif 'mmap_io' in experiment_type:
                metric_col = f'memcpy_duration_nsec_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'memcpy_duration_nsec'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - memcpy_duration_nsec', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
            elif 'rdma_send_recv' in experiment_type:
                metric_col = f'after wait_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'after wait'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - after wait', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        ax.set_xscale('log', base=2)
        ax.set_ylabel('Time (µs)')
        max_size = df['Message Size'].max() if not df.empty else 0
        upper_power = int(np.ceil(np.log2(max_size))) if max_size > 0 else 0
        xticks = [2**i for i in range(upper_power + 1)]
        ax.set_xticks(xticks)
        if data_range == "Beyond 16KB":
            min_beyond = df['Message Size'].min() if not df.empty else 2**15
            max_beyond = df['Message Size'].max() if not df.empty else 2**20
            ax.set_xlim(min_beyond * 0.9, max_beyond * 1.1)
    axes_set2[-1].set_xlabel('Message Size (Bytes)')
    handles_set2, labels_set2 = axes_set2[0].get_legend_handles_labels()
    fig_set2.legend(handles_set2, labels_set2, loc='lower center', ncol=2)
    plt.tight_layout(rect=(0, 0.1, 1, 1))
    plt.savefig(f"plots/plot_set2_{filename_suffix}.png")
    plt.close(fig_set2)

# --- New Plot Set 3: Time for 'registering' write ---
plot_set3_base_title = "Time for 'registering' write"
for data_range, df, filename_suffix in [
    ("All Data", plot_df, "all"),
    ("Up to 16KB", plot_df[plot_df['Message Size'] <= 16384], "truncated"),
    ("Beyond 16KB", plot_df[plot_df['Message Size'] > 16384], "beyond_truncated")
]:
    fig_set3, axes_set3 = plt.subplots(len(metrics), 1, figsize=(15, 10), sharex=True) # Uniform figsize
    title_prefix = f"{plot_set3_base_title}"
    if data_range != "All Data":
        title_prefix += f" - {data_range}"
    fig_set3.suptitle(title_prefix)
    unique_experiment_types_current = df['Experiment Type'].unique()
    for i, metric_type in enumerate(metrics):
        ax = axes_set3[i]
        ax.set_title(f'{metric_type.upper()} Time')
        ax.grid(True)
        for experiment_type in unique_experiment_types_current:
            subset = df[df['Experiment Type'] == experiment_type]
            subset_sorted = subset.sort_values(by='Message Size')
            if 'async_io' in experiment_type:
                metric_col = f'initiation_duration_nsec_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'initiation_duration_nsec'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - initiation_duration_nsec', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
            elif 'rdma_send_recv' in experiment_type:
                metric_col = f'before wait_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, 'before wait'))
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col] / 1000, label=f'{experiment_type} - before wait', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        ax.set_xscale('log', base=2)
        ax.set_ylabel('Time (µs)')
        max_size = df['Message Size'].max() if not df.empty else 0
        upper_power = int(np.ceil(np.log2(max_size))) if max_size > 0 else 0
        xticks = [2**i for i in range(upper_power + 1)]
        ax.set_xticks(xticks)
        if data_range == "Beyond 16KB":
            min_beyond = df['Message Size'].min() if not df.empty else 2**15
            max_beyond = df['Message Size'].max() if not df.empty else 2**20
            ax.set_xlim(min_beyond * 0.9, max_beyond * 1.1)
    axes_set3[-1].set_xlabel('Message Size (Bytes)')
    handles_set3, labels_set3 = axes_set3[0].get_legend_handles_labels()
    fig_set3.legend(handles_set3, labels_set3, loc='lower center', ncol=2)
    plt.tight_layout(rect=(0, 0.1, 1, 1))
    plt.savefig(f"plots/plot_set3_{filename_suffix}.png")
    plt.close(fig_set3)

# --- Adjusting legend labels for the original plots ---
def adjust_legend_labels(filename):
    fig = plt.figure() # Create a dummy figure to load the saved plot
    fig.canvas.draw() # Need to draw the canvas before we can access the renderer
    with open(filename, 'rb') as f:
        image = plt.imread(f)
    plt.imshow(image)
    handles, labels = plt.gca().get_legend_handles_labels()
    if handles:
        plt.legend(handles, [label.replace('_duration_nsec', '') for label in labels], loc='lower center', ncol=3)
        plt.tight_layout(rect=(0, 0.1, 1, 1)) # Ensure rect is correct
        plt.savefig(filename)
    plt.close()

adjust_legend_labels("plots/plot_all_conditions.png")
adjust_legend_labels("plots/plot_truncated_all_conditions.png")
adjust_legend_labels("plots/plot_beyond_truncated_all_conditions.png")
adjust_legend_labels("plots/plot_removed_conditions.png")
adjust_legend_labels("plots/plot_truncated_removed_conditions.png")
adjust_legend_labels("plots/plot_beyond_truncated_removed_conditions.png")