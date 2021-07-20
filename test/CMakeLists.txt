find_package(GTest CONFIG REQUIRED)
add_executable(main_test main.cpp)
target_include_directories(main_test PRIVATE GTest::gtest ../src)
target_link_libraries(main_test PRIVATE libDotFile GTest::gtest)
