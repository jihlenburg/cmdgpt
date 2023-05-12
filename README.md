# CmdGPT

CmdGPT is a command-line interface (CLI) tool designed to interact with the OpenAI GPT API. It enables users to send prompts and receive responses from the GPT model.

## Prerequisites

For building and running this application, you need:

- A compiler that supports C++17.
- CMake version 3.14 or higher.

## Building Instructions

The project uses the [cpp-httplib](https://github.com/yhirose/cpp-httplib) and [nlohmann-json](https://github.com/nlohmann/json) libraries. These are fetched and built automatically during the project build with CMake.

Follow these steps to build the project:

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

4. Build the project using make:

    ```sh
    make
    ```

The `cmdgpt` executable will be located in the `build` directory upon successful compilation.

## Usage

The usage format is: `cmdgpt [options] prompt`

Options:

- `-h, --help`: Display the help message.
- `-k, --api_key`: Enter the API key for the OpenAI GPT API.
- `-s, --sys_prompt`: Enter the system prompt for the OpenAI GPT API.
- `-l, --log_file`: Specify the logfile to record messages.
- `-m, --gpt_model`: Choose the GPT model to use (default: gpt-4).
- `-L, --log_level`: Set the log level (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL). The default is WARN.

## Environment Variables

You can use the following environment variables to set the corresponding parameters:

- `OPENAI_API_KEY`: API key for the OpenAI GPT API.
- `OPENAI_SYS_PROMPT`: System prompt for the OpenAI GPT API.
- `CMDGPT_LOG_FILE`: Logfile to record messages.
- `OPENAI_GPT_MODEL`: GPT model to use.
- `CMDGPT_LOG_LEVEL`: Log level.

If both a command-line option and an environment variable are provided, the command-line option will be prioritized.

## Exit Status Codes

The tool utilizes the following exit status codes:

- 0: Success.
- 64: Command-line usage error.
- 78: Configuration error.
- 75: Temporary failure.
- 1: Other unspecified errors.

## Note

Please note that the `-L` option and the `CMDGPT_LOG_LEVEL` environment variable expect a log level in uppercase. If an invalid log level is provided, the default log level (WARN) will be used.
