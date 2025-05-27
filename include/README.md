# Include Directory

This directory contains the public header files for cmdgpt.

## Files

- **cmdgpt.h** - Main header file containing:
  - Class declarations (Config, Conversation, ResponseCache, etc.)
  - Function prototypes for the public API
  - Constants and enumerations
  - Exception hierarchy definitions
  - Namespace declarations

## Usage

When building against the cmdgpt library, include this directory in your include path:

```cmake
target_include_directories(your_target PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

Then include the header in your source files:

```cpp
#include "cmdgpt.h"
```