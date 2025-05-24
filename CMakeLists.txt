# Minimum CMake version required (3.13 for FetchContent and policy features)
cmake_minimum_required(VERSION 3.13)

# Project name and language settings
project(cmdgpt)
set(CMAKE_CXX_STANDARD 17)  # Use C++17 features

# Set build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Dependencies Management
# FetchContent automatically downloads and builds dependencies at configure time
include(FetchContent)

# cpp-httplib: Header-only HTTP/HTTPS library for making API requests
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.14.3
)

# nlohmann/json: Popular JSON library for parsing API responses
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

# spdlog: Fast logging library with multiple output sinks
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.13.0
)

# Catch2: Modern unit testing framework for C++
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)

# Configure spdlog before fetching
# CMP0077 allows us to set options before FetchContent downloads the project
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(SPDLOG_BUILD_TESTING OFF)  # Don't build spdlog's tests

# Download and make all dependencies available for use
FetchContent_MakeAvailable(httplib json spdlog Catch2)

# OpenSSL is required for HTTPS support in cpp-httplib
find_package(OpenSSL REQUIRED)
add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)  # Enable SSL/TLS in httplib

# Core Library
# Build the main cmdgpt functionality as a static library
# This allows it to be used by both the main executable and tests
add_library(cmdgpt_lib STATIC cmdgpt.cpp)

# Set include directories for the library
target_include_directories(cmdgpt_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}    # For cmdgpt.h
    ${httplib_SOURCE_DIR}          # For httplib headers
    ${OPENSSL_INCLUDE_DIR})        # For OpenSSL headers

# Link required libraries
target_link_libraries(cmdgpt_lib PUBLIC 
    nlohmann_json::nlohmann_json   # JSON parsing
    spdlog::spdlog                 # Logging
    ${OPENSSL_LIBRARIES})          # SSL/TLS support

# Main Executable
# The command-line interface that users will run
add_executable(cmdgpt main.cpp)
target_link_libraries(cmdgpt PRIVATE cmdgpt_lib)

# Test Executable
# Unit tests using Catch2 framework
add_executable(cmdgpt_tests tests/cmdgpt_tests.cpp)
target_link_libraries(cmdgpt_tests PRIVATE 
    cmdgpt_lib                    # Code being tested
    Catch2::Catch2WithMain)       # Test framework with main()

# Testing Configuration
# Enable CTest support for running tests with 'ctest' command
enable_testing()

# Add Catch2's CMake integration for test discovery
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Automatically discover and register all tests in the test executable
catch_discover_tests(cmdgpt_tests)

# Documentation Configuration
# Find Doxygen for generating documentation
find_package(Doxygen OPTIONAL_COMPONENTS dot mscgen dia)

if(DOXYGEN_FOUND)
    # Set Doxygen input and output directories
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/docs)
    
    # Create docs directory
    file(MAKE_DIRECTORY ${DOXYGEN_OUT})
    
    # Add documentation target
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    
    # Add a clean target for documentation
    add_custom_target(clean-docs
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${DOXYGEN_OUT}
        COMMENT "Cleaning documentation"
    )
    
    message(STATUS "Doxygen found. You can build documentation with 'make docs'")
else()
    message(WARNING "Doxygen not found. Documentation target will not be available")
    message(STATUS "To install Doxygen:")
    message(STATUS "  macOS: brew install doxygen")
    message(STATUS "  Ubuntu/Debian: sudo apt-get install doxygen")
    message(STATUS "  Fedora: sudo dnf install doxygen")
    message(STATUS "  Windows: Download from https://www.doxygen.nl/download.html")
endif()