import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import LogFormatter, FuncFormatter

def analyze_rdma_results(results_dir="compare_results"):
    """
    Analyzes RDMA result files to extract average, p99, and max durations for each column.

    Args:
        results_dir (str): The directory containing the result files.

    Returns:
        dict: A dictionary where keys are experiment types ("cpp", "c wrapper")
              and values are dictionaries with message sizes as keys and
              a dictionary of {'before wait': {'avg': ..., 'p99': ..., 'max': ...},
                             'after wait': {'avg': ..., 'p99': ..., 'max': ...},
                             'rtt': {'avg': ..., 'p99': ..., 'max': ...}}.
    """
    all_data = {"cpp": {}, "c wrapper": {}}
    column_names = ["before wait", "after wait", "rtt"]

    for filename in os.listdir(results_dir):
        if filename.startswith("rdma_send_recv_c_") and filename.endswith(".txt"):
            try:
                msg_size_str = filename[len("rdma_send_recv_c_"):-len(".txt")]
                msg_size = int(msg_size_str)
                filepath = os.path.join(results_dir, filename)
                experiment_type = "c wrapper"
                column_data = {name: [] for name in column_names}
                with open(filepath, 'r') as f:
                    next(f)  # Skip the header line
                    for line in f:
                        values = line.strip().split('\t')
                        if len(values) == len(column_names):
                            try:
                                for i, name in enumerate(column_names):
                                    column_data[name].append(int(values[i]))
                            except ValueError:
                                print(f"Warning: Could not parse values in line: {line.strip()} of {filename}")
                if experiment_type not in all_data:
                    all_data[experiment_type] = {}
                all_data[experiment_type][msg_size] = {}
                for name, data in column_data.items():
                    if data:
                        all_data[experiment_type][msg_size][name] = {
                            'avg': np.mean(data),
                            'p99': np.percentile(data, 99),
                            'max': np.max(data)
                        }
            except ValueError:
                print(f"Warning: Could not parse message size from filename: {filename}")
            except FileNotFoundError:
                print(f"Error: File not found: {filepath}")
                continue

        elif filename.startswith("rdma_send_recv_") and not filename.startswith("rdma_send_recv_c_") and filename.endswith(".txt"):
            try:
                msg_size_str = filename[len("rdma_send_recv_"):-len(".txt")]
                msg_size = int(msg_size_str)
                filepath = os.path.join(results_dir, filename)
                experiment_type = "cpp"
                column_data = {name: [] for name in column_names}
                with open(filepath, 'r') as f:
                    next(f)  # Skip the header line
                    for line in f:
                        values = line.strip().split('\t')
                        if len(values) == len(column_names):
                            try:
                                for i, name in enumerate(column_names):
                                    column_data[name].append(int(values[i]))
                            except ValueError:
                                print(f"Warning: Could not parse values in line: {line.strip()} of {filename}")
                if experiment_type not in all_data:
                    all_data[experiment_type] = {}
                all_data[experiment_type][msg_size] = {}
                for name, data in column_data.items():
                    if data:
                        all_data[experiment_type][msg_size][name] = {
                            'avg': np.mean(data),
                            'p99': np.percentile(data, 99),
                            'max': np.max(data)
                        }
            except ValueError:
                print(f"Warning: Could not parse message size from filename: {filename}")
            except FileNotFoundError:
                print(f"Error: File not found: {filepath}")
                continue

    return all_data

def format_power_of_2(value, tick_number):
    """Formats a tick value as 2^n."""
    if value > 0:
        exponent = int(np.round(np.log2(value)))
        return f"$2^{{{exponent}}}$"
    return ""

def plot_rdma_latency(analyzed_data, save_dir="plots", save_filename="c_and_cpp_comparison.png"):
    """
    Plots the average, p99, and max durations for each column against message size and saves the plot.

    Args:
        analyzed_data (dict): The dictionary returned by analyze_rdma_results.
        save_dir (str): The directory to save the plot.
        save_filename (str): The name of the file to save the plot as.
    """
    os.makedirs(save_dir, exist_ok=True)  # Create the plots directory if it doesn't exist
    fig, axes = plt.subplots(3, 1, figsize=(12, 18), sharex=True)
    msg_sizes_all = set()
    for exp_data in analyzed_data.values():
        msg_sizes_all.update(exp_data.keys())
    sorted_msg_sizes = sorted(list(msg_sizes_all))
    column_names = ["before wait", "after wait", "rtt"]
    experiment_names = list(analyzed_data.keys())
    print(f"Experiment Names: {experiment_names}")
    line_styles = ['-', '--']
    colors = ['r', 'g', 'b']

    statistic_types = ['avg', 'p99', 'max']
    titles = ["Average Latency", "99th Percentile Latency", "Maximum Latency"]
    ylabels = ["Latency ($\mu$s)", "Latency ($\mu$s)", "Latency ($\mu$s)"] # Changed y-axis labels

    for i, stat_type in enumerate(statistic_types):
        ax = axes[i]
        for j, exp_type in enumerate(experiment_names):
            if exp_type in analyzed_data and analyzed_data[exp_type]: # Check if experiment has data
                for k, col_name in enumerate(column_names):
                    latencies = []
                    for size in sorted_msg_sizes:
                        value = analyzed_data[exp_type].get(size, {}).get(col_name, {}).get(stat_type)
                        if value is None:
                            print(f"Warning: No data found for Experiment: {exp_type}, Message Size: {size}, Column: {col_name}, Statistic: {stat_type}")
                            latencies.append(np.nan)
                        else:
                            latencies.append(value / 1000.0)
                    ax.plot(sorted_msg_sizes, latencies, linestyle=line_styles[j], color=colors[k],
                            label=f"{exp_type.capitalize()} - {col_name.capitalize()}")
            else:
                print(f"Warning: No data found for experiment type: {exp_type}")

        ax.set_ylabel(ylabels[i])
        ax.set_title(titles[i])
        ax.grid(True, which="both", ls="-")
        ax.legend()

    axes[-1].set_xlabel("Message Size (Bytes)")
    axes[-1].set_xscale('log', base=2)
    axes[-1].set_xticks(sorted_msg_sizes)
    axes[-1].xaxis.set_major_formatter(FuncFormatter(format_power_of_2))
    axes[-1].tick_params(axis='x', rotation=45, labelbottom=True)

    plt.tight_layout()
    save_path = os.path.join(save_dir, save_filename)
    plt.savefig(save_path)
    print(f"Plot saved to: {save_path}")
    plt.close() # Close the plot to only save

if __name__ == "__main__":
    from matplotlib.ticker import LogFormatter, FuncFormatter
    results_data = analyze_rdma_results(results_dir="compare_results")
    print("Analyzed Data:")
    print(results_data.get("cpp"))
    plot_rdma_latency(results_data, save_dir="plots", save_filename="c_and_cpp_comparison.png")