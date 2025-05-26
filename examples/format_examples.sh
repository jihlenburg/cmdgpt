#!/bin/bash
# Format examples for cmdgpt
# Demonstrates different output formats: plain, json, markdown, code

# Check if OPENAI_API_KEY is set
if [ -z "$OPENAI_API_KEY" ]; then
    echo "Error: OPENAI_API_KEY environment variable is not set"
    echo "Please run: export OPENAI_API_KEY='your-api-key'"
    exit 1
fi

# Path to cmdgpt
CMDGPT="../build/cmdgpt"

echo "=== Output Format Examples ==="
echo

# Plain format (default)
echo "1. PLAIN FORMAT (default)"
echo "Prompt: List 3 benefits of exercise"
echo "Response:"
$CMDGPT "List 3 benefits of exercise"
echo
echo "---"

# JSON format
echo "2. JSON FORMAT"
echo "Prompt: List 3 programming languages with their primary use cases"
echo "Response:"
$CMDGPT -f json "List 3 programming languages with their primary use cases"
echo
echo "---"

# Markdown format
echo "3. MARKDOWN FORMAT"
echo "Prompt: Create a simple README template for a Python project"
echo "Response:"
$CMDGPT -f markdown "Create a simple README template for a Python project"
echo
echo "---"

# Code format (extracts only code blocks)
echo "4. CODE FORMAT (extracts code blocks only)"
echo "Prompt: Write a Python function to calculate factorial"
echo "Response:"
$CMDGPT -f code "Write a Python function to calculate factorial with examples"
echo
echo "---"

echo "Tip: Use different formats based on your needs:"
echo "  - plain: General text output"
echo "  - json: Structured data for parsing"
echo "  - markdown: Documentation and formatted text"
echo "  - code: Extract only code portions"