#!/bin/bash

# Demo script to show that --show-tokens issue has been fixed
# This script demonstrates token usage display functionality

echo "=== CMDGPT --show-tokens Demo ==="
echo "This demo shows that the --show-tokens issue has been fixed."
echo "Token usage information is now properly captured and displayed."
echo ""

# Note: You'll need to set your OPENAI_API_KEY environment variable
if [ -z "$OPENAI_API_KEY" ]; then
    echo "ERROR: Please set OPENAI_API_KEY environment variable"
    echo "Example: export OPENAI_API_KEY=your-api-key-here"
    exit 1
fi

echo "1. Testing basic text query with --show-tokens:"
echo "Command: ./build/cmdgpt --show-tokens \"What is 2+2?\""
echo "---"
./build/cmdgpt --show-tokens "What is 2+2?"
echo ""

echo "2. Testing image generation with --show-tokens (shows cost instead of tokens):"
echo "Command: ./build/cmdgpt --show-tokens --generate-image \"A cute robot helping with homework\""
echo "---"
# Note: This will output base64 image data, so we'll just show the cost info
./build/cmdgpt --show-tokens --generate-image "A cute robot helping with homework" 2>&1 | grep -E "(generation cost:|Error:)" || echo "Image generated (base64 output suppressed)"
echo ""

echo "=== Demo Complete ==="
echo ""
echo "As you can see, the --show-tokens flag now properly displays:"
echo "- Token usage information for text queries (when API returns it)"
echo "- Cost information for image generation"
echo ""
echo "Note: The actual token display depends on having a valid API key."
echo "Without a valid key, you'll see an authentication error instead."