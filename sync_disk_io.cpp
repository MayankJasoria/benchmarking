#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <errno.h>
#include <cstring>
#include <random>
#include <gflags/gflags.h>
#include "spdlog/spdlog.h"

DEFINE_int32(msg_size, 1024, "Number of bytes to write to file in each iteration");
DEFINE_int32(msg_count, 1000, "Number of messages to send");

using namespace std;

string generateRandomString(size_t numBytes) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t charsetSize = sizeof(charset) - 1;
    string result;
    result.reserve(numBytes);

    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<> distribution(0, charsetSize - 1);

    for (size_t i = 0; i < numBytes; ++i) {
       result += charset[distribution(generator)];
    }

    return result;
}

long perform_write (int fd, string &data_to_write) {
    size_t write_size = data_to_write.size();
    off_t offset = 0; // Offset is generally ignored in append mode

    // 1. Start timer
    auto start_time = chrono::high_resolution_clock::now();

    // Perform synchronous write
    ssize_t bytes_written = pwrite(fd, data_to_write.c_str(), write_size, offset);
    if (bytes_written == -1) {
        spdlog::error("Error in synchronous write: {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Force write to disk
    if (fsync(fd) < 0) {
        spdlog::error("Error flushing file: {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    // 2. Capture time difference
    auto end_time = chrono::high_resolution_clock::now();
    long total_duration = chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count();
    spdlog::debug("Time taken for sync write: {} nanoseconds", total_duration);

    return total_duration;
}

int open_file(const char* filename) {
    int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU); // Open in append mode, create if not exists
    if (fd == -1) {
       spdlog::error("Error opening file: {}", strerror(errno));
       return -1;
    }
    return fd;
}

int close_file(int fd) {
    int ret = close(fd);
    if (ret == -1) {
       spdlog::error("Error closing file: {}", strerror(errno));
       return -1;
    }
    return 0;
}

void writeResultsToFile(const std::vector<long>& times, int msg_size) {
    // Construct the output file name
    std::string filename = "/hdd2/rdma-libs/results/sync_io_" + std::to_string(msg_size) + ".txt";
    std::ofstream outputFile(filename);

    // Check if the file was opened successfully
    if (outputFile.is_open()) {
       // Write the header row
       outputFile << "write_duration_nsec\n";

       // Write the data from the 'times' vector
       for (long time : times) {
          outputFile << time << "\n";
       }

       // Close the file
       outputFile.close();
       std::cout << "Data written to: " << filename << std::endl;
    } else {
       std::cerr << "Unable to open file: " << filename << std::endl;
    }
}

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
#ifdef DEBUG_BUILD
    spdlog::set_level(spdlog::level::debug);
#endif

#ifndef DEBUG_BUILD
    spdlog::set_level(spdlog::level::info);
#endif

    int num_bytes = FLAGS_msg_size;
    string filename = "/hdd2/rdma-libs/files/sync_append_test_" + to_string(num_bytes) + ".txt"; // Different filename for sync test
    int fd = open_file(filename.c_str());

	int warm_up_msgs = 1000;
    int saved_msgs_count = min(warm_up_msgs, FLAGS_msg_count); // ensure there are sufficient random messages
    string saved_msgs[saved_msgs_count];
    for (int i = 0; i < saved_msgs_count; ++i) {
       saved_msgs[i] = generateRandomString(num_bytes);
    }

    // warm up
    for (int i = 0, idx = 0; i < warm_up_msgs; ++i, idx = (idx + 1) % saved_msgs_count) {
       string msg = saved_msgs[i];
       perform_write(fd, msg);
    }

    // resetting file and ensuring disk head is placed at the start of the file
    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);

    int num_msgs = FLAGS_msg_count;
    vector<long> times(num_msgs);
    for (int i = 0, idx = 0; i < num_msgs; ++i, idx = (idx + 1) % saved_msgs_count) {
       string msg = saved_msgs[i];
       long duration = perform_write(fd, msg);
       times[i] = duration;
    }

    close(fd);

    writeResultsToFile(times, num_bytes);

    return 0;
}