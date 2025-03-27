#!/bin/bash

# Array of message sizes to test
msg_sizes=(1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576)

# Remote server details
remote_host="192.168.252.211"
remote_user="mjasoria"
remote_server_path="/hdd2/rdma-libs"
remote_log_path="$remote_server_path/logs"
msg_count=1000

echo "Starting RDMA tests (using C++ client and server)..."
# Loop through each message size for RDMA tests (C++ binaries)
for msg_size in "${msg_sizes[@]}"; do
    echo "Running RDMA experiment with message size: $msg_size bytes (C++)"

    server_pid_file="$remote_log_path/rdma_server_$msg_size.pid"

    # Start the server on the remote machine
    echo "Starting server on $remote_host..."
    ssh -n $remote_user@$remote_host "mkdir -p $remote_log_path && nohup $remote_server_path/server --msg_size=$msg_size > $remote_log_path/rdma_send_recv_server_$msg_size.txt 2>&1 & echo \$! > $server_pid_file" &
    echo "Server started on $remote_host"
    sleep 2 # Give the server a moment to start

    # Run the client locally and redirect output to a file
    echo "Running RDMA test for $msg_size (C++)"
    ./client --msg_size=$msg_size --msg_count=$msg_count > "logs/rdma_send_recv_client_$msg_size.txt" 2>&1
    echo "Client finished for message size: $msg_size bytes (C++). Results saved to logs/rdma_send_recv_client_$msg_size.txt"

    # Wait for a bit to allow the server to receive the termination message and shut down
    echo "Sleeping for a bit to allow server shutdown..."
    sleep 5 # You might need to adjust this value

    # Kill the remote server (using PID file if implemented in server)
    if [ -f "$server_pid_file" ]; then
        ssh -n $remote_user@$remote_host "kill $(cat "$server_pid_file")" &
        echo "Sent kill signal to server (PID from $server_pid_file) after message size: $msg_size bytes (C++)."
    else
        ssh -n $remote_user@$remote_host "pkill -f '$remote_server_path/server --msg_size=$msg_size'" &
        echo "Ensured server is stopped (using pkill) after message size: $msg_size bytes (C++)."
    fi
    sleep 1

done
echo "All RDMA experiments using C++ client and server completed."

echo ""
echo "Starting RDMA tests (using C client and server)..."
# Loop through each message size for RDMA tests (C binaries)
for msg_size in "${msg_sizes[@]}"; do
    echo "Running RDMA experiment with message size: $msg_size bytes (C)"

    server_pid_file_c="$remote_log_path/rdma_server_c_$msg_size.pid"

    # Start the C server on the remote machine
    echo "Starting C server on $remote_host..."
    ssh -n $remote_user@$remote_host "mkdir -p $remote_log_path && nohup $remote_server_path/c_server --msg_size=$msg_size > $remote_log_path/rdma_send_recv_c_server_$msg_size.txt 2>&1 & echo \$! > $server_pid_file_c" &
    echo "C server started on $remote_host"
    sleep 2 # Give the server a moment to start

    # Run the C client locally and redirect output to a file
    echo "Running RDMA test for $msg_size (C)"
    ./c_client --msg_size=$msg_size --msg_count=$msg_count > "logs/rdma_send_recv_c_client_$msg_size.txt" 2>&1
    echo "C client finished for message size: $msg_size bytes. Results saved to logs/rdma_send_recv_c_client_$msg_size.txt"

    # Wait for a bit to allow the server to receive the termination message and shut down
    echo "Sleeping for a bit to allow server shutdown..."
    sleep 5 # You might need to adjust this value

    # Kill the remote C server (using PID file if implemented in server)
    if [ -f "$server_pid_file_c" ]; then
        ssh -n $remote_user@$remote_host "kill $(cat "$server_pid_file_c")" &
        echo "Sent kill signal to C server (PID from $server_pid_file_c) after message size: $msg_size bytes."
    else
        ssh -n $remote_user@$remote_host "pkill -f '$remote_server_path/c_server --msg_size=$msg_size'" &
        echo "Ensured C server is stopped (using pkill) after message size: $msg_size bytes."
    fi
    sleep 1

done
echo "All RDMA experiments using C client and server completed."

echo "All RDMA tests completed."