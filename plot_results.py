import os
import pandas as pd
import matplotlib.pyplot as plt

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
            columns = ['before wait', 'after wait']
        elif 'mmap_io' in experiment_type:
            columns = ['memcpy_duration_nsec', 'msync_duration_nsec']
        elif 'rdma_send_recv' in experiment_type:
            columns = ['before wait', 'after wait', 'rtt']
        elif 'sync_io' in experiment_type:
            columns = ['write_duration_nsec']
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

# Filter the DataFrame for message sizes up to 2048 (2^11)
truncated_df_all = plot_df[plot_df['Message Size'] <= 2048]

# Filter the DataFrame for message sizes beyond 2048 (2^11)
beyond_truncated_df_all = plot_df[plot_df['Message Size'] > 2048]

# Filter the DataFrame for message sizes up to 65536 (2^16) for removed conditions
truncated_df_removed = plot_df[plot_df['Message Size'] <= 65536]

# Filter the DataFrame for message sizes beyond 65536 (2^16) for removed conditions
beyond_truncated_df_removed = plot_df[plot_df['Message Size'] > 65536]

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

# Choose a colormap with 8 distinct colors
cmap = plt.get_cmap('Set2')
colors = [cmap(i) for i in range(8)]

# Create a mapping for conditions to colors (for all conditions)
condition_colors_all = {}
color_index_all = 0
for experiment_type in unique_experiment_types:
    if 'async_io' in experiment_type:
        for col in ['before wait', 'after wait']:
            condition_colors_all[(experiment_type, col)] = colors[color_index_all % 8]
            color_index_all += 1
    elif 'mmap_io' in experiment_type:
        for col in ['memcpy_duration_nsec', 'msync_duration_nsec']:
            condition_colors_all[(experiment_type, col)] = colors[color_index_all % 8]
            color_index_all += 1
    elif 'rdma_send_recv' in experiment_type:
        for col in ['before wait', 'after wait', 'rtt']:
            condition_colors_all[(experiment_type, col)] = colors[color_index_all % 8]
            color_index_all += 1
    elif 'sync_io' in experiment_type:
        for col in ['write_duration_nsec']:
            condition_colors_all[(experiment_type, col)] = colors[color_index_all % 8]
            color_index_all += 1

# Create a mapping for conditions to colors (with specified conditions removed)
condition_colors_removed = {}
color_index_removed = 0
unique_experiment_types_removed_cmap = unique_experiment_types # Reusing for simplicity
for experiment_type in unique_experiment_types_removed_cmap:
    if 'async_io' in experiment_type:
        for col in ['before wait', 'after wait']:
            condition_colors_removed[(experiment_type, col)] = colors[color_index_removed % 8]
            color_index_removed += 1
    elif 'mmap_io' in experiment_type:
        for col in ['memcpy_duration_nsec']: # Removed msync_duration_nsec
            condition_colors_removed[(experiment_type, col)] = colors[color_index_removed % 8]
            color_index_removed += 1
    elif 'rdma_send_recv' in experiment_type:
        for col in ['before wait', 'after wait', 'rtt']:
            condition_colors_removed[(experiment_type, col)] = colors[color_index_removed % 8]
            color_index_removed += 1
    elif 'sync_io' in experiment_type:
        pass # Removed write_duration_nsec

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
            for col in ['before wait', 'after wait']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec', 'msync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
axes[-1].set_xlabel('Message Size (Bytes)')
handles, labels = axes[0].get_legend_handles_labels()
fig.legend(handles, labels, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, .1, 1, 1))
plt.savefig("plots/plot_all_conditions.png")

# Create the figure and subplots for the truncated data (all conditions)
fig_truncated_all, axes_truncated_all = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_truncated_all.suptitle('Time spent by threads for I/O (Message Size <= 2KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_truncated_all[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types_truncated_all:
        subset = truncated_df_all[truncated_df_all['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['before wait', 'after wait']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec', 'msync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
axes_truncated_all[-1].set_xlabel('Message Size (Bytes)')
handles_truncated_all, labels_truncated_all = axes_truncated_all[0].get_legend_handles_labels()
fig_truncated_all.legend(handles_truncated_all, labels_truncated_all, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, .1, 1, 1)) # Adjust layout to make space for the legend
plt.savefig("plots/plot_truncated_all_conditions.png")

# Create the figure and subplots for the data beyond the truncation (all conditions)
fig_beyond_all, axes_beyond_all = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_beyond_all.suptitle('Time spent by threads for I/O (Message Size > 2KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_beyond_all[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types_beyond_all:
        subset = beyond_truncated_df_all[beyond_truncated_df_all['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['before wait', 'after wait']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec', 'msync_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            for col in ['write_duration_nsec']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_all.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
axes_beyond_all[-1].set_xlabel('Message Size (Bytes)')
handles_beyond_all, labels_beyond_all = axes_beyond_all[0].get_legend_handles_labels()
fig_beyond_all.legend(handles_beyond_all, labels_beyond_all, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, .1, 1, 1))
plt.savefig("plots/plot_beyond_truncated_all_conditions.png")

# Create the figure and subplots for the original data (without the two conditions)
fig_removed, axes_removed = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_removed.suptitle('Time spent by threads for I/O (Conditions Removed)')
for i, metric_type in enumerate(metrics):
    ax = axes_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types:
        subset = plot_df[plot_df['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['before wait', 'after wait']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec']: # Removed msync_duration_nsec
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            pass # Removed write_duration_nsec
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
axes_removed[-1].set_xlabel('Message Size (Bytes)')
handles_removed, labels_removed = axes_removed[0].get_legend_handles_labels()
fig_removed.legend(handles_removed, labels_removed, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, .05, 1, 1))
plt.savefig("plots/plot_removed_conditions.png")

# Create the figure and subplots for the truncated data (without the two conditions)
fig_truncated_removed, axes_truncated_removed = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_truncated_removed.suptitle('Time spent by threads for I/O (Message Size <= 64KB, Conditions Removed)')
for i, metric_type in enumerate(metrics):
    ax = axes_truncated_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types_truncated_removed:
        subset = truncated_df_removed[truncated_df_removed['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['before wait', 'after wait']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec']: # Removed msync_duration_nsec
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            pass # Removed write_duration_nsec
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
axes_truncated_removed[-1].set_xlabel('Message Size (Bytes)')
handles_truncated_removed, labels_truncated_removed = axes_truncated_removed[0].get_legend_handles_labels()
fig_truncated_removed.legend(handles_truncated_removed, labels_truncated_removed, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, 0.05, 1, 1)) # Adjust layout to make space for the legend
plt.savefig("plots/plot_truncated_removed_conditions.png")

# Create the figure and subplots for the data beyond the truncation (without the two conditions)
fig_beyond_removed, axes_beyond_removed = plt.subplots(3, 1, figsize=(15, 10), sharex=True)
fig_beyond_removed.suptitle('Time spent by threads for I/O (Message Size > 64KB, Conditions Removed)')
for i, metric_type in enumerate(metrics):
    ax = axes_beyond_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    for experiment_type in unique_experiment_types_beyond_removed:
        subset = beyond_truncated_df_removed[beyond_truncated_df_removed['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if 'async_io' in experiment_type:
            for col in ['before wait', 'after wait']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'mmap_io' in experiment_type:
            for col in ['memcpy_duration_nsec']: # Removed msync_duration_nsec
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'rdma_send_recv' in experiment_type:
            for col in ['before wait', 'after wait', 'rtt']:
                metric_col = f'{col}_{metric_type}'
                if metric_col in subset_sorted.columns:
                    color = condition_colors_removed.get((experiment_type, col))
                    subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
                    ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=f'{experiment_type} - {col}', color=color, linewidth=common_linewidth, linestyle=common_linestyle)
        elif 'sync_io' in experiment_type:
            pass # Removed write_duration_nsec
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
axes_beyond_removed[-1].set_xlabel('Message Size (Bytes)')
handles_beyond_removed, labels_beyond_removed = axes_beyond_removed[0].get_legend_handles_labels()
fig_beyond_removed.legend(handles_beyond_removed, labels_beyond_removed, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, .05, 1, 1)) # Adjust layout to make space for the legend
plt.savefig("plots/plot_beyond_truncated_removed_conditions.png")