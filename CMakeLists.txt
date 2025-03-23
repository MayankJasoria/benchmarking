cmake_minimum_required(VERSION 3.30)
project(benchmarking)

set(CMAKE_CXX_STANDARD 17)

# Set the build type if it's not already set
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
            "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
endif()

# Conditionally define a preprocessor macro based on the build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG_BUILD)
endif()

# Find gflags library
find_package(Gflags REQUIRED)
include_directories(${GFLAGS_INCLUDE_DIRS})

# Find Threads library (for pthreads)
find_package(Threads REQUIRED)

find_package(spdlog REQUIRED)

# Add executable for server.cpp
add_executable(server server.cpp)
target_link_libraries(server gflags ibverbs Threads::Threads)

# Add executable for client.cpp
add_executable(client client.cpp)
target_link_libraries(client gflags ibverbs Threads::Threads)

add_executable(client2 client2.cpp)
target_link_libraries(client2 gflags ibverbs Threads::Threads)

add_executable(server2 server2.cpp)
target_link_libraries(server2 gflags ibverbs Threads::Threads)

add_executable(disk_async async_disk_io.cpp)
target_link_libraries(disk_async gflags rt spdlog::spdlog)

add_executable(mmap_disk mmap_disk_io.cpp)
target_link_libraries(mmap_disk gflags spdlog::spdlog)

add_executable(sync_disk sync_disk_io.cpp)
target_link_libraries(sync_disk gflags spdlog::spdlog)
