include(FetchContent)

FetchContent_Declare(ini
    GIT_REPOSITORY https://hub.fastgit.org/Rookfighter/inifile-cpp.git
    GIT_TAG 41cb535626299eaa16b272ed1549ad54aed1db18)

FetchContent_GetProperties(ini)
if(NOT ini_POPULATED)
    FetchContent_Populate(ini)
endif()

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Boost 1.75.0 REQUIRED COMPONENTS program_options)

add_library(libDotFile STATIC Dotfile.cpp Dotfile.h)

add_executable(dotbak main.cpp)

target_include_directories(libDotFile PRIVATE spdlog::spdlog fmt::fmt Boost::headers ${ini_SOURCE_DIR}/include)
target_link_libraries(libDotFile PRIVATE spdlog::spdlog fmt::fmt Boost::program_options)

target_include_directories(dotbak PRIVATE spdlog::spdlog fmt::fmt Boost::headers ${ini_SOURCE_DIR}/include)
target_link_libraries(dotbak PRIVATE spdlog::spdlog fmt::fmt Boost::program_options libDotFile)

install(TARGETS dotbak
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
