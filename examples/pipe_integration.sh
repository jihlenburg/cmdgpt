#!/bin/bash
# Pipe integration examples for cmdgpt
# Shows how to use cmdgpt with Unix pipes and multi-line input

# Check if OPENAI_API_KEY is set
if [ -z "$OPENAI_API_KEY" ]; then
    echo "Error: OPENAI_API_KEY environment variable is not set"
    echo "Please run: export OPENAI_API_KEY='your-api-key'"
    exit 1
fi

# Path to cmdgpt
CMDGPT="../build/cmdgpt"

echo "=== Pipe Integration Examples ==="
echo

# Example 1: Analyze directory contents
echo "1. ANALYZING DIRECTORY CONTENTS"
echo "Command: ls -la | cmdgpt 'What do these files appear to be?'"
echo "Response:"
ls -la | $CMDGPT "What do these files appear to be based on their names and sizes?"
echo
echo "---"

# Example 2: Multi-line input with here-document
echo "2. MULTI-LINE INPUT WITH HERE-DOCUMENT"
echo "Translating multiple lines to Spanish:"
cat << EOF | $CMDGPT "Translate the following to Spanish:"
Hello, world!
How are you today?
The weather is nice.
Thank you very much.
EOF
echo
echo "---"

# Example 3: Process code from a file
echo "3. CODE REVIEW FROM FILE"
echo "Creating a sample Python file..."
cat > /tmp/sample_code.py << 'EOF'
def calculate_average(numbers):
    total = 0
    for num in numbers:
        total += num
    return total / len(numbers)

def find_max(numbers):
    max_num = numbers[0]
    for num in numbers:
        if num > max_num:
            max_num = num
    return max_num
EOF

echo "Reviewing the code:"
cat /tmp/sample_code.py | $CMDGPT "Review this Python code and suggest improvements"
rm -f /tmp/sample_code.py
echo
echo "---"

# Example 4: Process command output
echo "4. ANALYZING COMMAND OUTPUT"
echo "Command: ps aux | head -10 | cmdgpt 'Explain what these processes are'"
echo "Response:"
ps aux | head -10 | $CMDGPT "Explain what these processes are and what they do"
echo
echo "---"

# Example 5: Chain commands
echo "5. CHAINING COMMANDS"
echo "Generate code and save to file:"
$CMDGPT -f code "Write a bash function to check if a file exists" > /tmp/file_check.sh
echo "Generated function saved to /tmp/file_check.sh:"
cat /tmp/file_check.sh
rm -f /tmp/file_check.sh
echo
echo "---"

# Example 6: Combined stdin + prompt (NEW in v0.4.1)
echo "6. COMBINED STDIN + PROMPT"
echo "Git log analysis:"
echo "Command: git log --oneline -10 | cmdgpt 'summarize these commits in 3 bullets'"
echo "Response:"
git log --oneline -10 | $CMDGPT "summarize these commits in 3 bullets"
echo
echo "---"

# Example 7: API data processing
echo "7. API DATA PROCESSING"
echo "Creating sample JSON data..."
cat > /tmp/sample_data.json << 'EOF'
{
  "users": [
    {"name": "Alice", "age": 30, "email": "alice@example.com"},
    {"name": "Bob", "age": 25, "email": "bob@example.com"},
    {"name": "Charlie", "age": 35, "email": "charlie@example.com"}
  ]
}
EOF

echo "Processing JSON data:"
cat /tmp/sample_data.json | $CMDGPT "extract all email addresses from this JSON"
rm -f /tmp/sample_data.json
echo
echo "---"

echo "Tips for pipe integration:"
echo "  - cmdgpt automatically detects pipe vs terminal input"
echo "  - Multi-line input is fully supported"
echo "  - Use -f code to extract just code from responses"
echo "  - Combine with grep, awk, sed for powerful workflows"
echo "  - NEW: Combine piped data with prompts for context-aware processing"