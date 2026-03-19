cmake_minimum_required(VERSION 3.16)
project(YAFsys LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(fileserver
    src/main.cpp
    src/keyfile.cpp
    src/filesystem.cpp
    src/shell.cpp
)

target_include_directories(fileserver PRIVATE ${CMAKE_SOURCE_DIR}/include)
