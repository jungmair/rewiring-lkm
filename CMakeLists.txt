cmake_minimum_required(VERSION 3.1)
project(rewiring)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_PREFIX_PATH .)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")
add_executable(deque bench/deque.cpp)
add_executable(alltoone bench/alltoone.cpp)
