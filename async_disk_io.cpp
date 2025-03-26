#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <errno.h>
#include <cstring>
#include <random>
#include <vector>
#include <array>
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

array<long, 3> perform_write(int fd, string &data_to_write) {
    size_t write_size = data_to_write.size();
    off_t offset = 0; // Offset is generally ignored in append mode

    struct aiocb cb;
    memset(&cb, 0, sizeof(struct aiocb));
    cb.aio_fildes = fd;
    cb.aio_offset = offset;
    cb.aio_buf = const_cast<char*>(data_to_write.c_str()); // Be careful with const_cast
    cb.aio_nbytes = write_size;
    cb.aio_sigevent.sigev_notify = SIGEV_NONE; // No signal notification

    // 1. Start timer
    auto start_time = chrono::high_resolution_clock::now();

    // 2. Initiate asynchronous write
    int ret = aio_write(&cb);
    if (ret == -1) {
        spdlog::error("Error initiating asynchronous write: {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    // 3. Capture time difference (call this before waiting for completion)
    auto before_wait_time = chrono::high_resolution_clock::now();
    auto initiation_duration = chrono::duration_cast<chrono::nanoseconds>(before_wait_time - start_time).count();
    spdlog::debug("Time taken to initiate async write: {} nanoseconds", initiation_duration);

    // 4. Wait until the write is completed
    const struct aiocb *list[1] = {&cb};
    if (aio_suspend(list, 1, nullptr) == -1) {
        spdlog::error("Error waiting for asynchronous write: {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    // 5. Capture time difference (call this after waiting for completion)
    auto after_wait_time = chrono::high_resolution_clock::now();
    auto duration_before_fsync = chrono::duration_cast<chrono::nanoseconds>(after_wait_time - start_time).count();
    spdlog::debug("Total time taken for async write (before fsync): {} nanoseconds", duration_before_fsync);

    // 6. Force write to disk (fsync)
    auto fsync_start_time = chrono::high_resolution_clock::now();
    if (fsync(fd) < 0) {
        spdlog::error("Error flushing file (fsync): {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
	auto fsync_end_time = chrono::high_resolution_clock::now();
	auto fsync_duration = chrono::duration_cast<chrono::nanoseconds>(fsync_end_time - fsync_start_time).count();
	spdlog::debug("Time taken for fsync: {} nanoseconds", fsync_duration);

	// 7. Capture total duration including fsync
	auto total_duration_with_fsync = chrono::duration_cast<chrono::nanoseconds>(fsync_end_time - start_time).count();
	spdlog::debug("Total time taken for async write (with fsync): {} nanoseconds", total_duration_with_fsync);

    // Check for write errors
    int error = aio_error(&cb);
    if (error != 0) {
        spdlog::error("Asynchronous write error: {}", strerror(error));
    	exit(EXIT_FAILURE);
    } else {
        ssize_t bytes_written = aio_return(&cb);
        if (bytes_written == write_size) {
            spdlog::debug("Successfully wrote {} bytes", bytes_written);
			return {initiation_duration, duration_before_fsync, total_duration_with_fsync};
        }
        spdlog::error("Error in asynchronous write return. Wrote {} bytes", bytes_written);
        exit(EXIT_FAILURE);
    }

	// prevent compiler errors
	return {initiation_duration, duration_before_fsync, total_duration_with_fsync};
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

void writeResultsToFile(const vector<array<long, 3>>& times, int msg_size) {
    // Construct the output file name
    std::string filename = "/hdd2/rdma-libs/results/async_io_" + std::to_string(msg_size) + ".txt";
    std::ofstream outputFile(filename);

	// Check if the file was opened successfully
	if (outputFile.is_open()) {
		// Write the header row
		outputFile << "initiation_duration_nsec\twrite_duration_nsec\tfsync_duration_nsec\n";

		// Write the data from the 'times' vector
		for (const auto& time_array : times) {
		  outputFile << time_array[0] << "\t" << time_array[1] << "\t" << time_array[2] << "\n";
		}

		// Close the file
		outputFile.close();
		std::cout << "Data written to: " << filename << '\n';
	} else {
		std::cerr << "Unable to open file: " << filename << '\n';
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
    string filename = "/hdd2/rdma-libs/files/aio_append_test_" + to_string(num_bytes) + ".txt"; // Replace with your file path
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
	vector<array<long, 3>> times(num_msgs);
	int message_count = 0;
	for (int i = 0, idx = 0; i < num_msgs; ++i, idx = (idx + 1) % saved_msgs_count) {
	string msg = saved_msgs[i];
		array<long, 3> durations = perform_write(fd, msg);
		times[i] = durations;
		message_count++;
	}

	close(fd);

	writeResultsToFile(times, num_bytes);

    return 0;
}