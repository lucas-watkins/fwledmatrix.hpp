cmake_minimum_required(VERSION 4.1)
project(fwledmatrix.hpp)

option(FWLEDMATRIX_GEN_DOCS "Generate documentation" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -fsanitize=address -fsanitize=undefined -g -pg")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -o2")

if (FWLEDMATRIX_GEN_DOCS)
    find_package(Doxygen)

    if (NOT Doxygen_FOUND)
        message(FATAL_ERROR "Doxygen is not found. To build this project, either get Doxygen or disable generating documentation")
    endif()

    doxygen_add_docs(fwledmatrix_docs include)

endif()

include_directories(include)

add_executable(fwledmatrix_test_fullbright src/test/fullbright.cpp)
add_executable(fwledmatrix_test_pattern src/test/pattern.cpp)

