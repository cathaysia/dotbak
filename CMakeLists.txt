cmake_minimum_required(VERSION 3.20)
project(dotbak VERSION 0.0.1 DESCRIPTION "backup dotfiles" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include(FeatureSummary)


add_subdirectory(include)
include_directories(${PROJECT_BINARY_DIR}/include)
add_subdirectory(src)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
