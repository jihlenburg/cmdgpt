#!/bin/bash
# Streaming example for cmdgpt
# Demonstrates real-time output streaming

# Check if OPENAI_API_KEY is set
if [ -z "$OPENAI_API_KEY" ]; then
    echo "Error: OPENAI_API_KEY environment variable is not set"
    echo "Please run: export OPENAI_API_KEY='your-api-key'"
    exit 1
fi

# Path to cmdgpt
CMDGPT="../build/cmdgpt"

echo "=== Streaming Response Example ==="
echo "Watch as the response appears character by character..."
echo
echo "Prompt: Write a short story about a robot learning to paint"
echo "Response:"
echo "---"

# Enable streaming for real-time output
$CMDGPT --stream "Write a short story (3 paragraphs) about a robot learning to paint"

echo
echo "---"
echo "Streaming provides a better user experience for longer responses!"