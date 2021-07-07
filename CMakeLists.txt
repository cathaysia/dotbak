cmake_minimum_required(VERSION 3.20)
project(dotbak VERSION 0.0.1 DESCRIPTION "backup dotfiles" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(include)
include_directories(${PROJECT_BINARY_DIR}/include)
add_subdirectory(src)
