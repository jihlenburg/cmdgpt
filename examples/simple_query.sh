#!/bin/bash
# Simple query example for cmdgpt
# This shows basic usage of cmdgpt from the command line

# Check if OPENAI_API_KEY is set
if [ -z "$OPENAI_API_KEY" ]; then
    echo "Error: OPENAI_API_KEY environment variable is not set"
    echo "Please run: export OPENAI_API_KEY='your-api-key'"
    exit 1
fi

# Path to cmdgpt (adjust if needed)
CMDGPT="../build/cmdgpt"

# Simple query
echo "=== Simple Query Example ==="
echo "Query: What is the capital of France?"
echo "Response:"
$CMDGPT "What is the capital of France?"
echo

# Query with specific model
echo "=== Query with Specific Model ==="
echo "Query: Explain quantum computing (using gpt-3.5-turbo)"
echo "Response:"
$CMDGPT -m gpt-3.5-turbo "Explain quantum computing in simple terms"
echo

# Query with custom system prompt
echo "=== Query with Custom System Prompt ==="
echo "Query: Tell me about treasure (as a pirate)"
echo "Response:"
$CMDGPT -s "You are a pirate. Speak like a pirate." "Tell me about treasure"