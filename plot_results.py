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
    return stats

# Directory containing the files
directory = 'results'

# Initialize lists to store data for plotting
plot_data = []

# Iterate over each file in the directory
for filename in os.listdir(directory):
    if filename.endswith('.txt'):
        # Extract experiment type and message size from the filename
        if filename.startswith('async_io_sync_elapsed_time_'):
            parts = filename.split('_')
            experiment_type = 'async io - O_SYNC'
            try:
                message_size = int(parts[-1].split('.')[0])
            except ValueError:
                continue
        elif filename.startswith('async_io_dsync_elapsed_time_'):
            parts = filename.split('_')
            experiment_type = 'async io - O_DSYNC'
            try:
                message_size = int(parts[-1].split('.')[0])
            except ValueError:
                continue
        else:
            parts = filename.split('_')
            experiment_type = '_'.join(parts[:-1])
            try:
                message_size = int(parts[-1].split('.')[0])
            except ValueError:
                continue

        # Read data from the file
        file_path = os.path.join(directory, filename)
        data = read_data(file_path)

        # Determine columns to be used for statistics calculation based on experiment type
        columns = []
        if experiment_type == 'async io - O_SYNC' or experiment_type == 'async io - O_DSYNC':
            columns = ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec', 'elapsed_after_fsync_registered_nsec', 'elapsed_after_fsync_completed_nsec', 'non_blocking_time_nsec']
        elif 'mmap_io' in experiment_type:
            columns = ['elapsed_after_memcpy_nsec', 'elapsed_after_msync_nsec']
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
metrics = ['avg', 'p99'] # Removed 'max'

# Define a consistent linewidth and linestyle
common_linewidth = 1
common_linestyle = '-'

# Create the 'plots' directory if it doesn't exist
os.makedirs("plots", exist_ok=True)

# Function to plot data with color mapping
def plot_with_color_mapping(ax, subset_sorted, experiment_type, columns, metric_type, condition_colors, color_index, colors_list, label_prefix=''):
    for col in columns:
        metric_col = f'{col}_{metric_type}'
        if metric_col in subset_sorted.columns:
            if (experiment_type, col) not in condition_colors:
                condition_colors[(experiment_type, col)] = colors_list[color_index % len(colors_list)]
                color_index += 1
            color = condition_colors[(experiment_type, col)]
            subset_sorted[metric_col] = subset_sorted[metric_col] / 1000
            base_label_parts = col.replace('_duration_nsec', '').replace('_nsec', '').replace('elapsed_after_', '').split('_')
            base_label = ' - '.join([part for part in base_label_parts if part])
            label = ''
            label_generated = False

            if 'mmap_io' in experiment_type and 'msync' in col:
                label = f'mmap_io - msync'
                label_generated = True
            elif 'mmap_io' in experiment_type and 'memcpy' in col:
                label = f'mmap_io - memcpy'
                label_generated = True
            elif experiment_type == 'async io - O_DSYNC':
                if 'fsync_completed' in col:
                    label = f'async_io - O_DSYNC - fsync completed'
                    label_generated = True
                elif 'fsync_registered' in col:
                    label = f'async_io - O_DSYNC - fsync registered'
                    label_generated = True
                elif 'write_completed' in col:
                    label = f'async_io - O_DSYNC - write completed'
                    label_generated = True
                elif 'write_registered' in col:
                    label = f'async_io - O_DSYNC - write registered'
                    label_generated = True
                elif 'non_blocking_time' in col:
                    label = f'async_io - O_DSYNC - non blocking'
                    label_generated = True
            elif experiment_type == 'async io - O_SYNC':
                if 'fsync_completed' in col:
                    label = f'async_io - O_SYNC - fsync completed'
                    label_generated = True
                elif 'fsync_registered' in col:
                    label = f'async_io - O_SYNC - fsync registered'
                    label_generated = True
                elif 'write_completed' in col:
                    label = f'async_io - O_SYNC - write completed'
                    label_generated = True
                elif 'write_registered' in col:
                    label = f'async_io - O_SYNC - write registered'
                    label_generated = True
                elif 'non_blocking_time' in col:
                    label = f'async_io - O_SYNC - non blocking'
                    label_generated = True
            elif 'sync_io' in experiment_type:
                if 'write' in col:
                    label = f'sync_io - write'
                    label_generated = True
                elif 'flush' in col:
                    label = f'sync_io - flush'
                    label_generated = True
            elif 'rdma_send_recv' in experiment_type:
                if 'send_registered' in col or col == 'before wait':
                    label = 'rdma_send_recv - send registered'
                    label_generated = True
                elif 'send_complete' in col or col == 'after wait':
                    label = 'rdma_send_recv - send complete'
                    label_generated = True
                elif 'rtt' in col:
                    label = 'rdma_send_recv - rtt'
                    label_generated = True

            if not label_generated:
                label = f'{experiment_type.replace("async io", "async_io")} - {base_label}'

            final_label = label
            if label_prefix and not label_generated:
                final_label = f'{label_prefix} {label}'.strip()
            elif label_prefix and label_generated and label_prefix not in label:
                final_label = f'{label_prefix} {label}'.strip() # Consider adding prefix if not already present

            ax.plot(subset_sorted['Message Size'], subset_sorted[metric_col], label=final_label, color=color, linewidth=common_linewidth, linestyle=common_linestyle)
    return color_index, condition_colors

# Create the figure and subplots for the original data (all conditions)
fig, axes = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
fig.suptitle('Time spent by threads for I/O')
for i, metric_type in enumerate(metrics):
    ax = axes[i]
    ax.set_title(f'{metric_type.upper()} Time')
    condition_colors_subplot = {}
    color_index = 0
    lines_count = 0
    for experiment_type in unique_experiment_types:
        if experiment_type == 'async io - O_SYNC' or experiment_type == 'async io - O_DSYNC':
            lines_count += 5
        elif 'mmap_io' in experiment_type:
            lines_count += 2
        elif 'rdma_send_recv' in experiment_type:
            lines_count += 3
        elif 'sync_io' in experiment_type:
            lines_count += 2
    cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
    colors_list = [cmap(i) for i in range(cmap.N)]
    for experiment_type in unique_experiment_types:
        subset = plot_df[plot_df['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if experiment_type == 'async io - O_SYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec', 'elapsed_after_fsync_registered_nsec', 'elapsed_after_fsync_completed_nsec', 'non_blocking_time_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif experiment_type == 'async io - O_DSYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec', 'elapsed_after_fsync_registered_nsec', 'elapsed_after_fsync_completed_nsec', 'non_blocking_time_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'mmap_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_memcpy_nsec', 'elapsed_after_msync_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'rdma_send_recv' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['before wait', 'after wait', 'rtt'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'sync_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['write_duration_nsec', 'flush_duration_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
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
plt.tight_layout(rect=(0, 0.15, 1, 1))
plt.savefig("plots/plot_all_conditions.png")

# Create the figure and subplots for the truncated data (all conditions)
fig_truncated_all, axes_truncated_all = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
fig_truncated_all.suptitle(f'Time spent by threads for I/O (Message Size <= 16KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_truncated_all[i]
    ax.set_title(f'{metric_type.upper()} Time')
    condition_colors_subplot = {}
    color_index = 0
    lines_count = 0
    for experiment_type in unique_experiment_types_truncated_all:
        if experiment_type == 'async io - O_SYNC' or experiment_type == 'async io - O_DSYNC':
            lines_count += 5
        elif 'mmap_io' in experiment_type:
            lines_count += 2
        elif 'rdma_send_recv' in experiment_type:
            lines_count += 3
        elif 'sync_io' in experiment_type:
            lines_count += 2
    cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
    colors_list = [cmap(i) for i in range(cmap.N)]
    for experiment_type in unique_experiment_types_truncated_all:
        subset = truncated_df_all[truncated_df_all['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if experiment_type == 'async io - O_SYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec', 'elapsed_after_fsync_registered_nsec', 'elapsed_after_fsync_completed_nsec', 'non_blocking_time_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif experiment_type == 'async io - O_DSYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec', 'elapsed_after_fsync_registered_nsec', 'elapsed_after_fsync_completed_nsec', 'non_blocking_time_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'mmap_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_memcpy_nsec', 'elapsed_after_msync_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'rdma_send_recv' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['before wait', 'after wait', 'rtt'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'sync_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['write_duration_nsec', 'flush_duration_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
    ax.set_xscale('log', base=2)
    ax.set_ylabel('Time (µs)')
    ax.grid(True)
    xticks_truncated = [2**i for i in range(15)]
    ax.set_xticks(xticks_truncated)
    ax.set_xlim(0.9, 20000) # Allow some whitespace for truncated plot (all)
axes_truncated_all[-1].set_xlabel('Message Size (Bytes)')
handles_truncated_all, labels_truncated_all = axes_truncated_all[0].get_legend_handles_labels()
fig_truncated_all.legend(handles_truncated_all, labels_truncated_all, loc='lower center', ncol=3)
plt.tight_layout(rect=(0, 0.15, 1, 1)) # Adjust layout to make space for the legend
plt.savefig("plots/plot_truncated_all_conditions.png")

# Create the figure and subplots for the data beyond the truncation (all conditions)
fig_beyond_all, axes_beyond_all = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
fig_beyond_all.suptitle(f'Time spent by threads for I/O (Message Size > 16KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_beyond_all[i]
    ax.set_title(f'{metric_type.upper()} Time')
    condition_colors_subplot = {}
    color_index = 0
    lines_count = 0
    for experiment_type in unique_experiment_types_beyond_all:
        if experiment_type == 'async io - O_SYNC' or experiment_type == 'async io - O_DSYNC':
            lines_count += 5
        elif 'mmap_io' in experiment_type:
            lines_count += 2
        elif 'rdma_send_recv' in experiment_type:
            lines_count += 3
        elif 'sync_io' in experiment_type:
            lines_count += 2
    cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
    colors_list = [cmap(i) for i in range(cmap.N)]
    for experiment_type in unique_experiment_types_beyond_all:
        subset = beyond_truncated_df_all[beyond_truncated_df_all['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if experiment_type == 'async io - O_SYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec', 'elapsed_after_fsync_registered_nsec', 'elapsed_after_fsync_completed_nsec', 'non_blocking_time_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif experiment_type == 'async io - O_DSYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec', 'elapsed_after_fsync_registered_nsec', 'elapsed_after_fsync_completed_nsec', 'non_blocking_time_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'mmap_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_memcpy_nsec', 'elapsed_after_msync_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'rdma_send_recv' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['before wait', 'after wait', 'rtt'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'sync_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['write_duration_nsec', 'flush_duration_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
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
plt.tight_layout(rect=(0, 0.15, 1, 1))
plt.savefig("plots/plot_beyond_truncated_all_conditions.png")

# Create the figure and subplots for the original data (without the two conditions) - ALL DATA
fig_removed, axes_removed = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
fig_removed.suptitle('Time spent by threads for I/O (Conditions Removed)')
for i, metric_type in enumerate(metrics):
    ax = axes_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    condition_colors_subplot = {}
    color_index = 0
    lines_count = 0
    for experiment_type in unique_experiment_types:
        if experiment_type == 'async io - O_SYNC' or experiment_type == 'async io - O_DSYNC':
            lines_count += 2
        elif 'mmap_io' in experiment_type:
            lines_count += 1
        elif 'rdma_send_recv' in experiment_type:
            lines_count += 3
        elif 'sync_io' in experiment_type:
            lines_count += 1
    cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
    colors_list = [cmap(i) for i in range(cmap.N)]
    for experiment_type in unique_experiment_types:
        subset = plot_df[plot_df['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if experiment_type == 'async io - O_SYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif experiment_type == 'async io - O_DSYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'mmap_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_memcpy_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'rdma_send_recv' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['before wait', 'after wait', 'rtt'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'sync_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['write_duration_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
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

# Create the figure and subplots for the truncated data (without the two conditions)
fig_truncated_removed, axes_truncated_removed = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
fig_truncated_removed.suptitle('Time spent by threads for I/O (Conditions Removed, <= 16KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_truncated_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    condition_colors_subplot = {}
    color_index = 0
    lines_count = 0
    for experiment_type in unique_experiment_types_truncated_removed:
        if experiment_type == 'async io - O_SYNC' or experiment_type == 'async io - O_DSYNC':
            lines_count += 2
        elif 'mmap_io' in experiment_type:
            lines_count += 1
        elif 'rdma_send_recv' in experiment_type:
            lines_count += 3
        elif 'sync_io' in experiment_type:
            lines_count += 1
    cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
    colors_list = [cmap(i) for i in range(cmap.N)]
    for experiment_type in unique_experiment_types_truncated_removed:
        subset = truncated_df_removed[truncated_df_removed['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if experiment_type == 'async io - O_SYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif experiment_type == 'async io - O_DSYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'mmap_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_memcpy_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'rdma_send_recv' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['before wait', 'after wait', 'rtt'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'sync_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['write_duration_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
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

# Create the figure and subplots for the data beyond the truncation (without the two conditions)
fig_beyond_truncated_removed, axes_beyond_truncated_removed = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
fig_beyond_truncated_removed.suptitle('Time spent by threads for I/O (Conditions Removed, > 16KB)')
for i, metric_type in enumerate(metrics):
    ax = axes_beyond_truncated_removed[i]
    ax.set_title(f'{metric_type.upper()} Time')
    condition_colors_subplot = {}
    color_index = 0
    lines_count = 0
    for experiment_type in unique_experiment_types_beyond_removed:
        if experiment_type == 'async io - O_SYNC' or experiment_type == 'async io - O_DSYNC':
            lines_count += 2
        elif 'mmap_io' in experiment_type:
            lines_count += 1
        elif 'rdma_send_recv' in experiment_type:
            lines_count += 3
        elif 'sync_io' in experiment_type:
            lines_count += 1
    cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
    colors_list = [cmap(i) for i in range(cmap.N)]
    for experiment_type in unique_experiment_types_beyond_removed:
        subset = beyond_truncated_df_removed[beyond_truncated_df_removed['Experiment Type'] == experiment_type]
        subset_sorted = subset.sort_values(by='Message Size')
        if experiment_type == 'async io - O_SYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif experiment_type == 'async io - O_DSYNC':
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec', 'elapsed_after_write_completed_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'mmap_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_memcpy_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'rdma_send_recv' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['before wait', 'after wait', 'rtt'], metric_type, condition_colors_subplot, color_index, colors_list)
        elif 'sync_io' in experiment_type:
            color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['write_duration_nsec'], metric_type, condition_colors_subplot, color_index, colors_list)
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
    fig_set1, axes_set1 = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
    title_prefix = f"{plot_set1_base_title}"
    if data_range != "All Data":
        title_prefix += f" - {data_range}"
    fig_set1.suptitle(title_prefix)
    unique_experiment_types_current = df['Experiment Type'].unique()
    for i, metric_type in enumerate(metrics):
        ax = axes_set1[i]
        ax.set_title(f'{metric_type.upper()} Time')
        ax.grid(True) # Ensure grid is always present
        condition_colors_subplot = {}
        color_index = 0
        lines_count = 0
        for experiment_type in unique_experiment_types_current:
            if 'sync_io' in experiment_type:
                lines_count += len(['flush_duration_nsec'])
            elif experiment_type == 'async io - O_SYNC':
                lines_count += len(['elapsed_after_fsync_completed_nsec', 'elapsed_after_fsync_registered_nsec'])
            elif experiment_type == 'async io - O_DSYNC':
                lines_count += len(['elapsed_after_fsync_completed_nsec', 'elapsed_after_fsync_registered_nsec'])
            elif 'mmap_io' in experiment_type:
                lines_count += len(['elapsed_after_msync_nsec'])
            elif 'rdma_send_recv' in experiment_type:
                lines_count += len(['rtt'])

        cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
        colors_list = [cmap(i) for i in range(cmap.N)]

        for experiment_type in unique_experiment_types_current:
            subset = df[df['Experiment Type'] == experiment_type]
            subset_sorted = subset.sort_values(by='Message Size')
            if 'sync_io' in experiment_type:
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['flush_duration_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif experiment_type == 'async io - O_SYNC':
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_fsync_completed_nsec', 'elapsed_after_fsync_registered_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif experiment_type == 'async io - O_DSYNC':
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_fsync_completed_nsec', 'elapsed_after_fsync_registered_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif 'mmap_io' in experiment_type:
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_msync_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif 'rdma_send_recv' in experiment_type:
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['rtt'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
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
    plt.tight_layout(rect=(0, 0.12, 1, 1))
    plt.savefig(f"plots/plot_set1_{filename_suffix}.png")
    plt.close(fig_set1)

# --- New Plot Set 2: Time for 'cached' write ---
plot_set2_base_title = "Time for 'cached' write"
for data_range, df, filename_suffix in [
    ("All Data", plot_df, "all"),
    ("Up to 16KB", plot_df[plot_df['Message Size'] <= 16384], "truncated"),
    ("Beyond 16KB", plot_df[plot_df['Message Size'] > 16384], "beyond_truncated")
]:
    fig_set2, axes_set2 = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
    title_prefix = f"{plot_set2_base_title}"
    if data_range != "All Data":
        title_prefix += f" - {data_range}"
    fig_set2.suptitle(title_prefix)
    unique_experiment_types_current = df['Experiment Type'].unique()
    for i, metric_type in enumerate(metrics):
        ax = axes_set2[i]
        ax.set_title(f'{metric_type.upper()} Time')
        ax.grid(True)
        condition_colors_subplot = {}
        color_index = 0
        lines_count = 0
        for experiment_type in unique_experiment_types_current:
            if 'sync_io' in experiment_type:
                lines_count += len(['write_duration_nsec'])
            elif experiment_type == 'async io - O_SYNC':
                lines_count += len(['elapsed_after_write_completed_nsec'])
            elif experiment_type == 'async io - O_DSYNC':
                lines_count += len(['elapsed_after_write_completed_nsec'])
            elif 'mmap_io' in experiment_type:
                lines_count += len(['elapsed_after_memcpy_nsec'])
            elif 'rdma_send_recv' in experiment_type:
                lines_count += len(['after wait'])

        cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
        colors_list = [cmap(i) for i in range(cmap.N)]

        for experiment_type in unique_experiment_types_current:
            subset = df[df['Experiment Type'] == experiment_type]
            subset_sorted = subset.sort_values(by='Message Size')
            if 'sync_io' in experiment_type:
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['write_duration_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif experiment_type == 'async io - O_SYNC':
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_completed_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif experiment_type == 'async io - O_DSYNC':
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_completed_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif 'mmap_io' in experiment_type:
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_memcpy_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif 'rdma_send_recv' in experiment_type:
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['after wait'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
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
valid_experiment_types_set3 = ['async io - O_SYNC', 'async io - O_DSYNC', 'rdma_send_recv']
for data_range, df, filename_suffix in [
    ("All Data", plot_df, "all"),
    ("Up to 16KB", plot_df[plot_df['Message Size'] <= 16384], "truncated"),
    ("Beyond 16KB", plot_df[plot_df['Message Size'] > 16384], "beyond_truncated")
]:
    fig_set3, axes_set3 = plt.subplots(len(metrics), 1, figsize=(15, 8), sharex=True) # Adjusted figsize
    title_prefix = f"{plot_set3_base_title}"
    if data_range != "All Data":
        title_prefix += f" - {data_range}"
    fig_set3.suptitle(title_prefix)
    unique_experiment_types_current = df['Experiment Type'].unique()
    for i, metric_type in enumerate(metrics):
        ax = axes_set3[i]
        ax.set_title(f'{metric_type.upper()} Time')
        ax.grid(True)
        condition_colors_subplot = {}
        color_index = 0
        lines_count = 0
        for experiment_type in unique_experiment_types_current:
            if experiment_type not in valid_experiment_types_set3:
                continue
            if experiment_type == 'async io - O_SYNC':
                lines_count += len(['elapsed_after_write_registered_nsec'])
            elif experiment_type == 'async io - O_DSYNC':
                lines_count += len(['elapsed_after_write_registered_nsec'])
            elif 'rdma_send_recv' in experiment_type:
                lines_count += len(['before wait'])

        cmap = plt.get_cmap('tab10') if lines_count <= 10 else plt.get_cmap('tab20')
        colors_list = [cmap(i) for i in range(cmap.N)]

        for experiment_type in unique_experiment_types_current:
            if experiment_type not in valid_experiment_types_set3:
                continue
            subset = df[df['Experiment Type'] == experiment_type]
            subset_sorted = subset.sort_values(by='Message Size')
            if experiment_type == 'async io - O_SYNC':
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif experiment_type == 'async io - O_DSYNC':
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['elapsed_after_write_registered_nsec'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
            elif 'rdma_send_recv' in experiment_type:
                color_index, condition_colors_subplot = plot_with_color_mapping(ax, subset_sorted, experiment_type, ['before wait'], metric_type, condition_colors_subplot, color_index, colors_list) # Removed label_prefix
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
    plt.tight_layout(rect=(0, 0.07, 1, 1))
    plt.savefig(f"plots/plot_set3_{filename_suffix}.png")
    plt.close(fig_set3)