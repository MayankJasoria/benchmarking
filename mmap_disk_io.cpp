#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <errno.h>
#include <cstring>
#include <random>
#include <gflags/gflags.h>
#include "spdlog/spdlog.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

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

struct MmapInfo {
    void* mapped_region = MAP_FAILED;
    int fd = -1;
    size_t map_size = 0;
};

MmapInfo open_mmap_file(const char* filename, size_t size) {
    MmapInfo info;
    info.map_size = size;
    info.fd = open(filename, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if (info.fd == -1) {
        spdlog::error("Error opening file {}: {}", filename, strerror(errno));
        return info;
    }

    if (ftruncate(info.fd, info.map_size) == -1) {
        spdlog::error("Error setting file size for {}: {}", filename, strerror(errno));
        close(info.fd);
        info.fd = -1;
        return info;
    }

    info.mapped_region = mmap(nullptr, info.map_size, PROT_READ | PROT_WRITE, MAP_SHARED, info.fd, 0);
    if (info.mapped_region == MAP_FAILED) {
        spdlog::error("Error mapping file {}: {}", filename, strerror(errno));
        close(info.fd);
        info.fd = -1;
        return info;
    }
    return info;
}

pair<long, long> perform_mmap_write(MmapInfo& mmap_info, const string& data_to_write, off_t offset) {
    size_t write_size = data_to_write.size();
    if (offset + write_size > mmap_info.map_size) {
        spdlog::error("Write beyond mapped region: offset={}, write_size={}, map_size={}",
                      offset, write_size, mmap_info.map_size);
        exit(EXIT_FAILURE); // Indicate an error
    }

    auto start_time = chrono::high_resolution_clock::now();

	// Perform write to memory
    std::memcpy(static_cast<char*>(mmap_info.mapped_region) + offset, data_to_write.c_str(), write_size);
    auto before_wait_time = chrono::high_resolution_clock::now();
    auto initialization_duration = chrono::duration_cast<chrono::nanoseconds>(before_wait_time - start_time).count();
	spdlog::debug("Time taken for memcpy at offset {}: {} nanoseconds", offset, initialization_duration);

	// Wait for write to be flushed to disk
	if (msync(mmap_info.mapped_region, mmap_info.map_size, MS_SYNC) == -1) {
		spdlog::error("Error syncing mapped region to file: {}", strerror(errno));
		exit(EXIT_FAILURE);
	}
	auto after_wait_time = chrono::high_resolution_clock::now();
	auto total_duration = chrono::duration_cast<chrono::nanoseconds>(after_wait_time - start_time).count();
    spdlog::debug("Time taken for write to disk: {} ", total_duration);

	return make_pair(initialization_duration, total_duration);
}

int close_mmap_file(MmapInfo& mmap_info) {
    if (mmap_info.mapped_region != MAP_FAILED) {
        if (munmap(mmap_info.mapped_region, mmap_info.map_size) == -1) {
            spdlog::error("Error unmapping file: {}", strerror(errno));
        }
    }
    if (mmap_info.fd != -1) {
        if (close(mmap_info.fd) == -1) {
            spdlog::error("Error closing file: {}", strerror(errno));
            return -1;
        }
    }
    return 0;
}


void writeMmapResultsToFile(const std::vector<long>& initiation_times, const std::vector<double>& sync_times, int msg_size) {
	// Construct the output file name
	std::string filename = "/hdd2/rdma-libs/results/mmap_io_" + std::to_string(msg_size) + ".txt";
	std::ofstream outputFile(filename);

	// Check if the file was opened successfully
	if (outputFile.is_open()) {
		// Write the header row
		outputFile << "memcpy_duration_nsec\tmsync_duration_nsec\n";

		// Write the data from the 'times' vector
		for (size_t i = 0; i < initiation_times.size(); ++i) {
			outputFile << initiation_times[i] << "\t" << sync_times[i] << "\n";
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
	string filename = "/hdd2/rdma-libs/files/mmap_append_test_" + to_string(num_bytes) + ".txt"; // Replace with your file path
    size_t initial_map_size = (size_t) 1024 * 1024 * 1024; // 1 GB; I don't expect that to be filled in 1 sec in my tests
    MmapInfo mmap_info = open_mmap_file(filename.c_str(), initial_map_size);

    if (mmap_info.mapped_region == MAP_FAILED) {
        return 1;
    }

	int warm_up_msgs = 1000;
    int saved_msgs_count = max(warm_up_msgs, FLAGS_msg_count);
    string saved_msgs[saved_msgs_count];
    for (int i = 0; i < saved_msgs_count; ++i) {
        saved_msgs[i] = generateRandomString(num_bytes);
    }

	// warm up
	off_t current_offset = 0;
	for (int i = 0; i < warm_up_msgs; ++i) {
		string msg = saved_msgs[i];
		perform_mmap_write(mmap_info, msg, current_offset);
		current_offset += msg.size();
	}

    vector<pair<long, long>> times;
    int message_count = 0;
    current_offset = 0;
    for (int i = 0; i < saved_msgs_count; ++i) {
        string msg = saved_msgs[i];
        pair<long, long> elapsed_nsec = perform_mmap_write(mmap_info, msg, current_offset);
        times[i] = elapsed_nsec;
        message_count++;
        current_offset += msg.size();
        if (current_offset + num_bytes > mmap_info.map_size) {
            spdlog::warn("Reaching mapped region limit, consider increasing initial size or re-mapping.");
            break; // For simplicity, breaking. Real app might need to re-mmap.
        }
    }

    close_mmap_file(mmap_info);

	spdlog::info("Number of writes: {}", message_count);
	for (pair passed_nsec : times) {
		spdlog::info("{}, {}", passed_nsec.first, passed_nsec.second);
	}

    return 0;
}