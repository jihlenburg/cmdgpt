# Minimum version of CMake required to build this project
cmake_minimum_required(VERSION 3.14)

# Name of the project
project(CmdGpt)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Specify the source files to be compiled
set(SOURCE_FILES cmdgpt.cpp)

# Include the FetchContent module, which allows fetching content from external sources at configure time
include(FetchContent)

# Declare the cpp-httplib library to be fetched from the given git repository
FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG master
)

# Declare the nlohmann-json library to be fetched from the given git repository
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG master
)

# Make the declared content available to the build
FetchContent_MakeAvailable(cpp_httplib nlohmann_json)

# Check if the cpp-httplib library has been fetched, and if not, fetch it and add its directory to the include directories
FetchContent_GetProperties(cpp_httplib)
if(NOT cpp_httplib_POPULATED)
  FetchContent_Populate(cpp_httplib)
  include_directories(${cpp_httplib_SOURCE_DIR})
endif()

# Check if the nlohmann-json library has been fetched, and if not, fetch it and add its directory to the include directories
FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
  FetchContent_Populate(nlohmann_json)
  include_directories(${nlohmann_json_SOURCE_DIR}/single_include)
endif()

# Add the executable target to be built from the specified source files
add_executable(cmdgpt ${SOURCE_FILES})

