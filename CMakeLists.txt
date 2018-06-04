cmake_minimum_required(VERSION 2.8)
project(myHttp)

include_directories("./include")

file(GLOB SOURCES "src/*.c" "include/*.h")
#去除Werror编译选项
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-g -O2 -Wall -Wextra -pthread -DNDEBUG")
add_executable(myHttp ${SOURCES})
#add_subdirectory(tests)
