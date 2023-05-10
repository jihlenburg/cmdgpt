# Specify the minimum CMake version
cmake_minimum_required(VERSION 3.14)

# Project name
project(GptCmd)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)

# Specify the C++ files
set(SOURCE_FILES gptcmd.cpp)

# Specify the executable
add_executable(gptcmd ${SOURCE_FILES})

# Include directories
target_include_directories(gptcmd PRIVATE ${CMAKE_SOURCE_DIR}/libs/cpp-httplib)
target_include_directories(gptcmd PRIVATE ${CMAKE_SOURCE_DIR}/libs/nlohmann-json/single_include)

# Fetch and make available the external libraries
include(FetchContent)

FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG master
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG master
)

FetchContent_MakeAvailable(cpp_httplib nlohmann_json)

# Link the libraries
target_link_libraries(gptcmd PRIVATE cpp_httplib nlohmann_json)
