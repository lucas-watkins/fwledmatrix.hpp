cmake_minimum_required(VERSION 4.1)
project(fwledmatrix_hpp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -fsanitize=address -fsanitize=undefined -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -o2")

add_executable(fwledmatrix_test_fullbright src/test/fullbright.cpp)
