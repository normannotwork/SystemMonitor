cmake_minimum_required(VERSION 3.10)
project(SysMon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

find_package(Threads REQUIRED)

add_executable(sysmon 
    src/main.cpp 
    src/monitor.cpp 
    src/server.cpp
)

target_link_libraries(sysmon Threads::Threads)
