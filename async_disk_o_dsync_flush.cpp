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

array<long, 5> perform_write(int fd, string &data_to_write) {
    size_t write_size = data_to_write.size();
    off_t offset = 0;

    struct aiocb cb;
    memset(&cb, 0, sizeof(struct aiocb));
    cb.aio_fildes = fd;
    cb.aio_offset = offset;
    cb.aio_buf = const_cast<char*>(data_to_write.c_str());
    cb.aio_nbytes = write_size;
    cb.aio_sigevent.sigev_notify = SIGEV_NONE;

    // 1. Start timer for overall operation
    auto start_time = chrono::high_resolution_clock::now();

    // 2. Initiate asynchronous write
    auto before_aio_write_time = chrono::high_resolution_clock::now();
    int ret_write = aio_write(&cb);
    auto after_aio_write_time = chrono::high_resolution_clock::now();
    long aio_write_duration = chrono::duration_cast<chrono::nanoseconds>(after_aio_write_time - before_aio_write_time).count();
    long elapsed_after_write_registered = chrono::duration_cast<chrono::nanoseconds>(after_aio_write_time - start_time).count();
    spdlog::debug("Time elapsed after async write registered: {} nanoseconds", elapsed_after_write_registered);
    if (ret_write == -1) {
        spdlog::error("Error initiating asynchronous write: {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    // 3. Wait for asynchronous write to complete
    const struct aiocb *list_write[1] = {&cb};
    while (aio_error(&cb) == EINPROGRESS) {
        if (aio_suspend(list_write, 1, nullptr) == -1) {
            if (errno == EINTR) {
            	continue;
			}
            spdlog::error("Error waiting for asynchronous write: {}", strerror(errno));
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
    auto write_completion_end_wait_time = chrono::high_resolution_clock::now();
    long elapsed_after_write_completed = chrono::duration_cast<chrono::nanoseconds>(write_completion_end_wait_time - start_time).count();
    spdlog::debug("Time elapsed after async write completed: {} nanoseconds", elapsed_after_write_completed);

    // 4. Get result of write
    if (aio_error(&cb) != 0) {
        spdlog::error("Asynchronous write error: {}", strerror(aio_error(&cb)));
        exit(EXIT_FAILURE);
    }
    if (aio_return(&cb) == -1) {
        spdlog::error("Error getting result of asynchronous write: {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    // 5. Initiate asynchronous flush
    auto before_aio_fsync_time = chrono::high_resolution_clock::now();
    if (aio_fsync(O_DSYNC, &cb) < 0) {
        spdlog::error("Error initiating asynchronous flush: {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    auto after_aio_fsync_time = chrono::high_resolution_clock::now();
    long aio_fsync_duration = chrono::duration_cast<chrono::nanoseconds>(after_aio_fsync_time - before_aio_fsync_time).count();
    long elapsed_after_fsync_registered = chrono::duration_cast<chrono::nanoseconds>(after_aio_fsync_time - start_time).count();
    spdlog::debug("Time elapsed after async fsync registered: {} nanoseconds", elapsed_after_fsync_registered);

    // 6. Wait for asynchronous flush to complete
    const struct aiocb *list_fsync[1] = {&cb};
    while (aio_error(&cb) == EINPROGRESS) {
        if (aio_suspend(list_fsync, 1, nullptr) == -1) {
            if (errno == EINTR) continue;
            spdlog::error("Error waiting for asynchronous flush: {}", strerror(errno));
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
    auto fsync_completion_end_wait_time = chrono::high_resolution_clock::now();
    long elapsed_after_fsync_completed = chrono::duration_cast<chrono::nanoseconds>(fsync_completion_end_wait_time - start_time).count();
    spdlog::debug("Time elapsed after async fsync completed: {} nanoseconds", elapsed_after_fsync_completed);

    // 7. Get result of flush
    if (aio_error(&cb) != 0) {
        spdlog::error("Asynchronous flush error: {}", strerror(aio_error(&cb)));
        exit(EXIT_FAILURE);
    }
    if (aio_return(&cb) == -1) {
        spdlog::error("Error getting result of asynchronous flush: {}", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    long non_blocking_time = aio_write_duration + aio_fsync_duration;
    spdlog::debug("Total non-blocking time (aio_write + aio_fsync): {} nanoseconds", non_blocking_time);

    return {elapsed_after_write_registered, elapsed_after_write_completed, elapsed_after_fsync_registered, elapsed_after_fsync_completed, non_blocking_time};
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

void writeResultsToFile(const vector<array<long, 5>>& times, int msg_size) {
	// Construct the output file name
	std::string filename = "/hdd2/rdma-libs/results/async_io_dsync_elapsed_time_" + std::to_string(msg_size) + ".txt";
	std::ofstream outputFile(filename);

	// Check if the file was opened successfully
	if (outputFile.is_open()) {
		// Write the header row
		outputFile << "elapsed_after_write_registered_nsec\telapsed_after_write_completed_nsec\telapsed_after_fsync_registered_nsec\telapsed_after_fsync_completed_nsec\tnon_blocking_time_nsec\n";

		// Write the data from the 'times' vector
		for (const auto& time_array : times) {
			outputFile << time_array[0] << "\t" << time_array[1] << "\t" << time_array[2] << "\t" << time_array[3] << "\t" << time_array[4] << "\n";
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
    vector<array<long,5>> times(num_msgs);
    int message_count = 0;
    for (int i = 0, idx = 0; i < num_msgs; ++i, idx = (idx + 1) % saved_msgs_count) {
    string msg = saved_msgs[i];
       array<long, 5> durations = perform_write(fd, msg);
       times[i] = durations;
       message_count++;
    }

    close(fd);

    writeResultsToFile(times, num_bytes);

    return 0;
}