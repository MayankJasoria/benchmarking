#!/bin/bash

# Array of message sizes to test
msg_sizes=(1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576)

# Remote server details
remote_host="192.168.252.211"
remote_user="mjasoria"
remote_server_path="/hdd2/rdma-libs/server"
remote_log_path="/hdd2/rdma-libs/logs"

# Remote server details
remote_host="192.168.252.211"
remote_user="mjasoria"
remote_server_path="/hdd2/rdma-libs/server"
remote_log_path="/hdd2/rdma-libs/logs"

# Ensure the results directory exists
mkdir -p results logs files

echo "Starting RDMA tests..."
# Loop through each message size for RDMA tests
for msg_size in "${msg_sizes[@]}"; do
    echo "Running RDMA experiment with message size: $msg_size bytes"

    # Start the server on the remote machine
    echo "Starting server on $remote_host..."
    ssh -n $remote_user@$remote_host "nohup $remote_server_path --msg-size=$msg_size > $remote_log_path/rdma_send_recv_server_$msg_size.txt 2>&1" &
    echo "Server started on $remote_host"

    # Run the client locally and redirect output to a file
    echo "Running RDMA test for $msg_size"
    ./client --msg_size=$msg_size > "results/rdma_send_recv_client_$msg_size.txt" 2>&1
    echo "Client finished for message size: $msg_size bytes. Results saved to logs/rdma_send_recv_client_$msg_size.txt"

    # Wait for a bit to allow the server to receive the termination message and shut down
    echo "Sleeping for a bit to allow server shutdown..."
    sleep 5 # You might need to adjust this value based on how long your server takes to shut down gracefully

    # Kill the remote server (you might want to refine this based on your previous discussion)
    ssh -n $remote_user@$remote_host "pkill -f '$remote_server_path --msg-size=$msg_size'" &
    sleep 1 # Give it a moment to attempt to kill
    echo "Ensured server is stopped after message size: $msg_size bytes."

done
echo "All RDMA experiments completed."

echo ""
echo "Starting Disk I/O tests..."
# Loop through each message size for Disk I/O tests
for msg_size in "${msg_sizes[@]}"; do
    echo "Running Disk I/O experiments with message size: $msg_size bytes"

    # Run asynchronous disk I/O test
    echo "Running asynchronous disk I/O test for $msg_size"
    ./disk_async --msg-size=$msg_size > "logs/disk_async_$msg_size.txt" 2>&1
    echo "Asynchronous disk I/O test finished."

    # Run mmap disk I/O test
    echo "Running mmap disk I/O test for $msg_size"
    ./mmap_disk --msg-size=$msg_size > "logs/mmap_disk_$msg_size.txt" 2>&1
    echo "mmap disk I/O test finished."

    # Run synchronous disk I/O test
    echo "Running synchronous disk I/O test for $msg_size"
    ./sync_disk --msg-size=$msg_size > "logs/sync_disk_$msg_size.txt" 2>&1
    echo "Synchronous disk I/O test finished."

    echo "Disk I/O tests finished for message size: $msg_size bytes."
done

echo "All Disk I/O experiments completed."

echo "All tests completed."