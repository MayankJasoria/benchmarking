import os
import pandas as pd
import matplotlib.pyplot as plt

# Function to read data from a file and return a DataFrame
def read_data(file_path):
    return pd.read_csv(file_path, delim_whitespace=True)

# Function to calculate average, p99, and max times for a given DataFrame
def calculate_statistics(data, columns):
    avg_time = data[columns].mean()
    p99_time = data[columns].quantile(0.99)
    max_time = data[columns].max()
    return avg_time, p99_time, max_time

# Directory containing the files
directory = 'Results'

# Initialize lists to store statistics for each experiment type and message size
experiment_types = []
message_sizes = []
avg_times = []
p99_times = []
max_times = []

# Iterate over each file in the directory
for filename in os.listdir(directory):
    if filename.endswith('.txt'):
        # Extract experiment type and message size from the filename
        parts = filename.split('_')
        experiment_type = '_'.join(parts[:-1])
        message_size = parts[-1].split('.')[0]

        # Read data from the file
        file_path = os.path.join(directory, filename)
        data = read_data(file_path)

        # Determine columns to be used for statistics calculation based on experiment type
        if 'async_io' in experiment_type:
            columns = ['before wait', 'after wait']
        elif 'mmap_io' in experiment_type:
            columns = ['memcpy_duration_nsec', 'msync_duration_nsec']
        elif 'rdma_send_recv' in experiment_type:
            columns = ['before wait', 'after wait', 'rtt']
        elif 'sync_io' in experiment_type:
            columns = ['write_duration_nsec']

        # Calculate statistics for the current file
        avg_time, p99_time, max_time = calculate_statistics(data, columns)

        # Store statistics along with experiment type and message size
        experiment_types.append(experiment_type)
        message_sizes.append(message_size)
        avg_times.append(avg_time)
        p99_times.append(p99_time)
        max_times.append(max_time)

# Create a DataFrame to hold all statistics
stats_df = pd.DataFrame({
    'Experiment Type': experiment_types,
    'Message Size': message_sizes,
    'Average Time': avg_times,
    'P99 Time': p99_times,
    'Max Time': max_times
})

# Plot the statistics in three subplots
plt.figure(figsize=(15, 10))
plt.suptitle('Time spent by threads for I/O')

plt.subplot(3, 1, 1)
for experiment_type in stats_df['Experiment Type'].unique():
    subset = stats_df[stats_df['Experiment Type'] == experiment_type]
    plt.plot(subset['Message Size'], subset['Average Time'], label=experiment_type)
plt.xlabel('Message Size (Bytes)')
plt.ylabel('Average Time (ns)')
plt.legend()
plt.grid(True)

plt.subplot(3, 1, 2)
for experiment_type in stats_df['Experiment Type'].unique():
    subset = stats_df[stats_df['Experiment Type'] == experiment_type]
    plt.plot(subset['Message Size'], subset['P99 Time'], label=experiment_type)
plt.xlabel('Message Size (Bytes)')
plt.ylabel('P99 Time (ns)')
plt.legend()
plt.grid(True)

plt.subplot(3, 1, 3)
for experiment_type in stats_df['Experiment Type'].unique():
    subset = stats_df[stats_df['Experiment Type'] == experiment_type]
    plt.plot(subset['Message Size'], subset['Max Time'], label=experiment_type)
plt.xlabel('Message Size (Bytes)')
plt.ylabel('Max Time (ns)')
plt.legend()
plt.grid(True)

plt.tight_layout(rect=[0, 0.03, 1, 0.95])
plt.show()