cmake_minimum_required(VERSION 3.9)
project(threadpool)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

if(WIN32)
ELSE()
    link_libraries(pthread)
endif ()

add_executable(threadpool main.cpp ethreadpool.h ethreadpool.cpp)