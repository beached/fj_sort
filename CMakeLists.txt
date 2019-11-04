cmake_minimum_required(VERSION 3.13)
project(fj_sort)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

include("${CMAKE_SOURCE_DIR}/glean/CMakeListsCompiler.txt")
include("${CMAKE_SOURCE_DIR}/glean/CMakeLists.txt")

find_package(Boost COMPONENTS system iostreams filesystem thread REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

include_directories("${CMAKE_SOURCE_DIR}/include")

add_executable(fj_test include/daw/fj_sort.h tests/fj_test.cpp )
target_link_libraries(fj_test ${Boost_LIBRARIES})


