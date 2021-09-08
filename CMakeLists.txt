cmake_minimum_required(VERSION 3.10)

project(uarch-tools)

find_package(Threads REQUIRED)

add_executable(uarch main.cpp)
target_link_libraries(uarch Threads::Threads)
