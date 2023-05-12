# CMake version requirement
cmake_minimum_required(VERSION 3.13) # FetchContent is available in 3.11+, "set option" needs 3.13+

# Define the project and set the C++ standard
project(cmdgpt)
set(CMAKE_CXX_STANDARD 17)
# set build type to Debug
set(CMAKE_BUILD_TYPE Debug)

# Include FetchContent module used for downloading dependencies
include(FetchContent)

# Declare httplib dependency: Define the name, the git repository, and the tag to download
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG master
)

# Declare json dependency: Define the name, the git repository, and the tag to download
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG master
)

# Declare json dependency: Define the name, the git repository, and the tag to download
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG master
)

# Setting the policy to NEW will cause the option() command in spdlog's CMakeLists.txt
# to do nothing when SPDLOG_BUILD_TESTING is already set
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
# Don't build the test suite for SPDLOG
set(SPDLOG_BUILD_TESTING OFF)
# Fetch the declared content. This will download the dependencies if they are not already present.
FetchContent_MakeAvailable(httplib json spdlog)

# We need OpenSSL... 
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

# Add the cmdgpt executable and its source files
add_executable(cmdgpt cmdgpt.cpp)

# Since httplib and json are header-only libraries, we only need to add their directories to the include directories
target_include_directories(cmdgpt PRIVATE ${httplib_SOURCE_DIR} ${json_SOURCE_DIR}/include ${spdlog_SOURCE_DIR}/include)

# Link the json library and OpenSSL to the cmdgpt target
# The nlohmann_json::nlohmann_json target brings in include paths and dependencies automatically
target_link_libraries(cmdgpt PRIVATE nlohmann_json::nlohmann_json spdlog ${OPENSSL_LIBRARIES})

