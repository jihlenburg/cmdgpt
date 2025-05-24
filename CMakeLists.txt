# CMake version requirement
cmake_minimum_required(VERSION 3.13) # FetchContent is available in 3.11+, "set option" needs 3.13+

# Define the project and set the C++ standard
project(cmdgpt)
set(CMAKE_CXX_STANDARD 17)

# Set build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Include FetchContent module used for downloading dependencies
include(FetchContent)

# Declare httplib dependency: Define the name, the git repository, and the tag to download
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.14.3
)

# Declare json dependency: Define the name, the git repository, and the tag to download
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

# Declare spdlog dependency: Define the name, the git repository, and the tag to download
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.13.0
)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)

# Setting the policy to NEW will cause the option() command in spdlog's CMakeLists.txt
# to do nothing when SPDLOG_BUILD_TESTING is already set
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
# Don't build the test suite for SPDLOG
set(SPDLOG_BUILD_TESTING OFF)
# Fetch the declared content. This will download the dependencies if they are not already present.
FetchContent_MakeAvailable(httplib json spdlog Catch2)

# We need OpenSSL... 
if(WIN32)
    # Windows needs special handling for OpenSSL
    if(DEFINED ENV{OPENSSL_ROOT_DIR})
        set(OPENSSL_ROOT_DIR $ENV{OPENSSL_ROOT_DIR})
    endif()
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

add_library(cmdgpt_lib STATIC cmdgpt.cpp)
target_include_directories(cmdgpt_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${httplib_SOURCE_DIR} 
    ${json_SOURCE_DIR}/include 
    ${spdlog_SOURCE_DIR}/include)
target_link_libraries(cmdgpt_lib PUBLIC 
    nlohmann_json::nlohmann_json 
    spdlog::spdlog 
    ${OPENSSL_LIBRARIES})

add_executable(cmdgpt main.cpp)
target_link_libraries(cmdgpt PRIVATE cmdgpt_lib)

add_executable(cmdgpt_tests tests/cmdgpt_tests.cpp)
target_link_libraries(cmdgpt_tests PRIVATE cmdgpt_lib Catch2::Catch2WithMain)

# Add Catch2 testing support
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(cmdgpt_tests)