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
#include <cmath> // For std::ceil
#include <array>   // For std::array

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

array<long, 3> perform_mmap_write(MmapInfo& mmap_info, const string& data_to_write, off_t offset) {
    size_t write_size = data_to_write.size();
    if (offset + write_size > mmap_info.map_size) {
        spdlog::error("Write beyond mapped region: offset={}, write_size={}, map_size={}",
                      offset, write_size, mmap_info.map_size);
        exit(EXIT_FAILURE); // Indicate an error
    }

    auto start_time = chrono::high_resolution_clock::now();

    // Perform write to memory
    std::memcpy(static_cast<char*>(mmap_info.mapped_region) + offset, data_to_write.c_str(), write_size);
    auto before_msync_time = chrono::high_resolution_clock::now();
    auto memcpy_duration = chrono::duration_cast<chrono::nanoseconds>(before_msync_time - start_time).count();
    spdlog::debug("Time taken for memcpy at offset {}: {} nanoseconds", offset, memcpy_duration);

    // Wait for write to be flushed to disk (only the written region and its containing pages)
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        spdlog::error("Error getting page size: {}", strerror(errno));
        exit(EXIT_FAILURE);
    }

    off_t page_aligned_start = (offset / page_size) * page_size;
    off_t page_aligned_end = std::ceil((double)(offset + write_size) / page_size) * page_size;
    size_t flush_size = page_aligned_end - page_aligned_start;

    if (msync(static_cast<char*>(mmap_info.mapped_region) + page_aligned_start, flush_size, MS_SYNC) == -1) {
       spdlog::error("Error syncing mapped region to file: {}", strerror(errno));
       exit(EXIT_FAILURE);
    }
    auto after_msync_time = chrono::high_resolution_clock::now();
    auto msync_duration = chrono::duration_cast<chrono::nanoseconds>(after_msync_time - before_msync_time).count();
    spdlog::debug("Time taken for msync (flushing {} bytes from offset {}): {} nanoseconds ",
                  flush_size, page_aligned_start, msync_duration);

    // Add fsync call and capture total elapsed time
    if (fsync(mmap_info.fd) == -1) {
        spdlog::error("Error calling fsync on file descriptor: {}", strerror(errno));
        exit(EXIT_FAILURE);
    }
    auto after_fsync_time = chrono::high_resolution_clock::now();
    auto total_duration = chrono::duration_cast<chrono::nanoseconds>(after_fsync_time - start_time).count();
    spdlog::debug("Total time taken after fsync: {} nanoseconds", total_duration);

    return {memcpy_duration, msync_duration, total_duration};
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


void writeMmapResultsToFile(const vector<array<long, 3>>& times, int msg_size) {
    // Construct the output file name
    std::string filename = "/hdd2/rdma-libs/results/mmap_io_" + std::to_string(msg_size) + ".txt";
    std::ofstream outputFile(filename);

    // Check if the file was opened successfully
    if (outputFile.is_open()) {
       // Write the header row
       outputFile << "elapsed_after_memcpy_nsec\telapsed_after_msync_nsec\telapsed_after_fsync_nsec\n";

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
    string filename = "/hdd2/rdma-libs/files/mmap_append_test_" + to_string(num_bytes) + ".txt"; // Replace with your file path
    size_t initial_map_size = (size_t) 1024 * 1024 * 1024; // 1 GB; I don't expect that to be filled in 1 sec in my tests
    MmapInfo mmap_info = open_mmap_file(filename.c_str(), initial_map_size);

    if (mmap_info.mapped_region == MAP_FAILED) {
        return 1;
    }

    int warm_up_msgs = 1000;
    int saved_msgs_count = min(warm_up_msgs, FLAGS_msg_count);
    string saved_msgs[saved_msgs_count];
    for (int i = 0; i < saved_msgs_count; ++i) {
        saved_msgs[i] = generateRandomString(num_bytes);
    }

    // warm up
    off_t current_offset = 0;
    for (int i = 0, idx = 0; i < warm_up_msgs; ++i, idx = (idx + 1) % saved_msgs_count) {
       string msg = saved_msgs[i];
       perform_mmap_write(mmap_info, msg, current_offset);
       current_offset += msg.size();
    }

    int num_msgs = FLAGS_msg_count;
    vector<array<long, 3>> times(num_msgs);
    int message_count = 0;
    current_offset = 0;
    for (int i = 0, idx = 0; i < num_msgs; ++i, idx = (idx + 1) % saved_msgs_count) {
        string msg = saved_msgs[i];
        array<long, 3> elapsed_nsec = perform_mmap_write(mmap_info, msg, current_offset);
        times[i] = elapsed_nsec;
        message_count++;
        current_offset += msg.size();
        if (current_offset + num_bytes > mmap_info.map_size) {
            spdlog::warn("Reaching mapped region limit, consider increasing initial size or re-mapping.");
            break; // For simplicity, breaking. Real app might need to re-mmap.
        }
    }

    close_mmap_file(mmap_info);

    writeMmapResultsToFile(times, num_bytes);

    return 0;
}