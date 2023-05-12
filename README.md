# CmdGpt

This is a command-line tool to interact with the OpenAI GPT API. It allows you to send prompts and receive responses from the GPT model.

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

cmdgpt [options] prompt

Options:

-h, --help: Show this help message
-k, --api_key: API key for OpenAI GPT API
-s, --sys_prompt: System prompt for OpenAI GPT API
-l, --log_file: Logfile to log messages
-m, --gpt_model: GPT model to use (default: gpt-4)
-L, --log_level: Log level (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL). Default is WARN.

## Environment Variables

The following environment variables can also be used to set the corresponding parameters:

OPENAI_API_KEY: API key for OpenAI GPT API
OPENAI_SYS_PROMPT: System prompt for OpenAI GPT API
CMDGPT_LOG_FILE: Logfile to log messages
OPENAI_GPT_MODEL: GPT model to use
CMDGPT_LOG_LEVEL: Log level
If both a command-line option and an environment variable are provided, the command-line option takes precedence.

## Exit Status Codes

The tool uses the following exit status codes:

0: Success
64: Command-line usage error
78: Configuration error
75: Temporary failure
1: Other errors

## Note

Note: The `-L` option expects a log level in uppercase. The `CMDGPT_LOG_LEVEL` environment variable also expects an uppercase log level. If an invalid log level is provided, the default log level (WARN) will be used.

