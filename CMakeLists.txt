cmake_minimum_required(VERSION 3.16)
project(etc_benchmark)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(benchmark benchmark.cc cache_client.cc)
add_executable(cache_server cache_server.cc cache_store.cc fifo_evictor.cc)
