# Documentation Directory

This directory contains documentation resources and generated API documentation for the cmdgpt project.

## Directory Structure

```
docs/
├── README.md          # This file
├── images/           # Images and diagrams for documentation
│   └── logo.png     # Project logo
├── mainpage.dox     # Doxygen main page content
└── html/            # Generated API documentation (after build)
    ├── index.html   # Documentation entry point
    ├── classes.html # Class hierarchy
    ├── files.html   # File listing
    └── ...          # Other generated files
```

## Building Documentation

### Quick Build
```bash
# From project root
./scripts/build_docs.sh

# Build and open in browser
./scripts/build_docs.sh --open

# Auto-install dependencies (macOS only)
./scripts/build_docs.sh --auto-install --open
```

### Manual Build
```bash
# Using CMake
cd build
make docs

# Using Doxygen directly
doxygen docs/Doxyfile
```

## Documentation Features

### API Reference
- Complete class documentation
- Function signatures and descriptions
- Parameter and return value documentation
- Usage examples in comments

### Visual Elements
- Class hierarchy diagrams
- Call graphs (requires Graphviz)
- Collaboration diagrams
- Include dependency graphs

### Code Examples
Embedded throughout the documentation:
```cpp
// Example from Config class
cmdgpt::Config config;
config.set_api_key("your-key");
config.set_model("gpt-4");
```

## Viewing Documentation

### Local Viewing
```bash
# macOS
open docs/html/index.html

# Linux
xdg-open docs/html/index.html

# Windows
start docs/html/index.html
```

### Navigation Tips
1. **Main Page**: Overview and getting started
2. **Classes**: Hierarchical view of all classes
3. **Files**: Browse by source file
4. **Namespaces**: Browse by namespace (cmdgpt)
5. **Search**: Use the search box for quick access

## Documentation Standards

### Comment Style
All public APIs use Doxygen-style comments:

```cpp
/**
 * @brief Short description
 * 
 * Detailed description explaining the purpose,
 * behavior, and any important notes.
 * 
 * @param param_name Description of parameter
 * @return Description of return value
 * 
 * @throws ExceptionType When this exception is thrown
 * 
 * @note Additional information
 * @warning Important warnings
 * 
 * @code
 * // Example usage
 * function_name(parameter);
 * @endcode
 */
```

### Documentation Requirements

1. **All Public APIs**
   - Must have at least @brief description
   - Parameters and return values documented
   - Exceptions documented if thrown

2. **Classes**
   - Class-level overview
   - Usage examples
   - Relationship to other classes

3. **Files**
   - File-level @file tag
   - Brief description of file purpose
   - Author and date information

## Adding Documentation

### For New Features
1. Add comprehensive header comments
2. Include usage examples
3. Document all parameters and return values
4. Explain error conditions
5. Add to relevant groups

### For Examples
```cpp
/**
 * @example example_streaming.cpp
 * This example shows how to use streaming responses
 */
```

### For Groups
```cpp
/**
 * @defgroup config Configuration Management
 * @brief Classes and functions for configuration
 * @{
 */
// ... code ...
/** @} */
```

## Doxygen Configuration

Key settings in `Doxyfile`:
- `EXTRACT_ALL = YES` - Document all entities
- `HAVE_DOT = YES` - Generate diagrams (if Graphviz installed)
- `CALL_GRAPH = YES` - Generate call graphs
- `CALLER_GRAPH = YES` - Generate caller graphs
- `SOURCE_BROWSER = YES` - Include source code
- `GENERATE_TREEVIEW = YES` - Navigation sidebar

## mainpage.dox Content

The main documentation page includes:
1. Project overview
2. Quick start guide
3. Feature highlights
4. Installation instructions
5. Basic usage examples
6. Links to detailed documentation

## Updating Documentation

### After Code Changes
1. Update relevant comments
2. Rebuild documentation
3. Verify changes in HTML output
4. Check for broken links or missing docs

### Adding New Sections
1. Edit `mainpage.dox` for main page
2. Use @page for additional pages
3. Link pages with @ref commands
4. Rebuild and verify navigation

## Tools and Requirements

### Required
- Doxygen (1.8.13 or higher)
- C++ compiler (for preprocessing)

### Optional
- Graphviz (for diagrams)
- PlantUML (for UML diagrams)
- Mscgen (for message sequence charts)

### Installation
```bash
# macOS
brew install doxygen graphviz

# Ubuntu/Debian
sudo apt-get install doxygen graphviz

# Windows
# Download from doxygen.org and graphviz.org
```

## Troubleshooting

### Missing Diagrams
- Install Graphviz
- Ensure `dot` is in PATH
- Check `HAVE_DOT = YES` in Doxyfile

### Broken Links
- Rebuild documentation completely
- Check for renamed classes/functions
- Verify @ref targets exist

### Formatting Issues
- Check Doxygen version compatibility
- Validate comment syntax
- Ensure proper tag closure

## Contributing

When contributing documentation:
1. Follow existing comment style
2. Include examples for complex features
3. Test documentation builds locally
4. Verify all links work
5. Check generated output for clarity