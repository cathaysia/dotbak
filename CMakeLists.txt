cmake_minimum_required(VERSION 3.20)
project(dotbak VERSION 0.0.1 DESCRIPTION "backup dotfiles" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include(FeatureSummary)

add_subdirectory(include)
include_directories(${PROJECT_BINARY_DIR}/include)
add_subdirectory(src)

# 打包
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pack)
set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${CMAKE_PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_GENERATOR "RPM")
set(CPACK_STRIP_FILES ON)
include(CPack)
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME})


enable_testing()
add_subdirectory(test)
add_test(NAME DotFileTest COMMAND main_test)
