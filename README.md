# CmdGpt

CmdGpt is a command-line tool for interacting with the OpenAI GPT-4 API.

## Prerequisites

To build and run this application, you'll need:

- A compiler that supports C++17
- CMake version 3.14 or later

## Building

The project uses the [cpp-httplib](https://github.com/yhirose/cpp-httplib) and [nlohmann-json](https://github.com/nlohmann/json) libraries, which are fetched and built automatically when building the project with CMake.

To build the project, follow these steps:

1. Open a terminal in the project root directory.
2. Create a new directory for the build output:

    ```sh
    mkdir build
    cd build
    ```

3. Run CMake to configure the build and generate the Makefile:

    ```sh
    cmake ..
    ```

4. Run make to build the project:

    ```sh
    make
    ```

The resulting `cmdgpt` executable will be in the `build` directory.

## Usage

You can run the `cmdgpt` tool with the following command:

```sh
./cmdgpt [prompt] [-k api_key] [-s system_prompt]
