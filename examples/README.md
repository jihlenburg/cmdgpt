# Examples Directory

This directory contains example scripts and use cases for cmdgpt, demonstrating various features and integration patterns.

## Basic Usage Examples

### simple_query.sh
Basic command-line usage:
```bash
#!/bin/bash
# Simple query example
export OPENAI_API_KEY="your-api-key"
./cmdgpt "What is the capital of France?"
```

### streaming_example.sh
Streaming response demonstration:
```bash
#!/bin/bash
# Streaming output for better UX
./cmdgpt --stream "Write a haiku about programming"
```

### format_examples.sh
Different output formats:
```bash
#!/bin/bash
# JSON format
./cmdgpt -f json "List 3 programming languages with their uses"

# Markdown format
./cmdgpt -f markdown "Create a README template"

# Code extraction
./cmdgpt -f code "Write a Python function to calculate fibonacci"
```

## Advanced Usage

### pipe_integration.sh
Using cmdgpt with pipes and shell integration:
```bash
#!/bin/bash
# Analyze code from file
cat my_script.py | ./cmdgpt "Review this code and suggest improvements"

# Process command output
ls -la | ./cmdgpt "Explain what these files might be for"

# Multi-line input
cat << EOF | ./cmdgpt "Translate this to Spanish"
Hello, world!
How are you today?
The weather is nice.
EOF

# NEW in v0.4.1: Combined stdin + prompt for powerful context processing
# Git log summarization
git log --oneline -10 | ./cmdgpt "summarize these commits in 3 bullets"

# API response processing
curl -s api.example.com/data.json | ./cmdgpt "extract all email addresses"

# Error log analysis
cat /var/log/app.log | ./cmdgpt "identify the root cause of errors"
```

### interactive_session.sh
Interactive mode examples:
```bash
#!/bin/bash
# Start interactive session with custom prompt
./cmdgpt -i -s "You are a helpful Linux expert"

# With specific model
./cmdgpt -i -m gpt-3.5-turbo

# With logging enabled
./cmdgpt -i -L DEBUG -l session.log
```

### error_handling.sh
Handling errors and retries:
```bash
#!/bin/bash
# cmdgpt now automatically retries on rate limits
response=$(./cmdgpt "Generate a UUID" 2>&1)
if [ $? -eq 0 ]; then
    echo "Success: $response"
else
    echo "Failed after retries: $response"
fi
```

## Shell Scripting Integration

### code_reviewer.sh
Automated code review script:
```bash
#!/bin/bash
# Automated code reviewer
for file in *.py; do
    echo "Reviewing $file..."
    cat "$file" | ./cmdgpt "Review this Python code for best practices" > "${file%.py}_review.md"
done
```

### commit_message_generator.sh
Generate git commit messages:
```bash
#!/bin/bash
# Generate commit message from diff
git diff --staged | ./cmdgpt "Generate a concise commit message for these changes"
```

### documentation_generator.sh
Generate documentation from code:
```bash
#!/bin/bash
# Generate function documentation
grep -E "^def |^class " mycode.py | \
    ./cmdgpt -f markdown "Generate documentation for these Python functions and classes"
```

## Configuration Examples

### custom_config.sh
Using configuration files:
```bash
#!/bin/bash
# Create custom config
cat > ~/.cmdgptrc << EOF
api_key=your-api-key
model=gpt-4
system_prompt=You are a senior software engineer
log_level=INFO
EOF

# Now cmdgpt uses these defaults
./cmdgpt "Review this architecture decision"
```

### environment_setup.sh
Environment variable configuration:
```bash
#!/bin/bash
# Set up environment
export OPENAI_API_KEY="your-api-key"
export OPENAI_GPT_MODEL="gpt-4"
export OPENAI_SYS_PROMPT="You are a helpful coding assistant"
export CMDGPT_LOG_LEVEL="DEBUG"
export CMDGPT_LOG_FILE="/tmp/cmdgpt.log"

# Use with environment settings
./cmdgpt "Explain Docker containers"
```

## Integration Patterns

### chatbot_wrapper.sh
Simple chatbot wrapper:
```bash
#!/bin/bash
# Simple chatbot with history
HISTORY_FILE=".chat_history"

while true; do
    read -p "You: " input
    [ "$input" = "exit" ] && break
    
    echo "You: $input" >> "$HISTORY_FILE"
    response=$(echo "$input" | ./cmdgpt --stream)
    echo "AI: $response"
    echo "AI: $response" >> "$HISTORY_FILE"
    echo
done
```

### batch_processor.sh
Batch processing with progress:
```bash
#!/bin/bash
# Process multiple queries from file
total=$(wc -l < queries.txt)
current=0

while IFS= read -r query; do
    ((current++))
    echo "[$current/$total] Processing: $query"
    ./cmdgpt "$query" > "output_${current}.txt"
    sleep 1  # Be nice to the API
done < queries.txt
```

### api_wrapper_function.sh
Reusable shell function:
```bash
#!/bin/bash
# Define reusable function
ask_ai() {
    local prompt="$1"
    local format="${2:-plain}"
    local model="${3:-gpt-4}"
    
    ./cmdgpt -f "$format" -m "$model" "$prompt"
}

# Usage
ask_ai "What is REST API?"
ask_ai "List Docker commands" "markdown"
ask_ai "Explain recursion" "plain" "gpt-3.5-turbo"
```

## Testing and Development

### test_retry_logic.sh
Test automatic retry functionality:
```bash
#!/bin/bash
# Force rate limit to test retry
for i in {1..10}; do
    echo "Request $i"
    ./cmdgpt "Quick test $i" &
done
wait
echo "All requests completed with automatic retry"
```

### benchmark.sh
Performance benchmarking:
```bash
#!/bin/bash
# Benchmark response times
models=("gpt-3.5-turbo" "gpt-4")
for model in "${models[@]}"; do
    echo "Testing $model..."
    time ./cmdgpt -m "$model" "Say hello"
done
```

## Tips and Best Practices

1. **API Key Security**
   - Never hardcode API keys in scripts
   - Use environment variables or config files
   - Add `.cmdgptrc` to `.gitignore`

2. **Rate Limiting**
   - cmdgpt now handles rate limits automatically
   - Add delays in batch scripts to be respectful
   - Monitor your API usage

3. **Error Handling**
   - Always check exit codes
   - Capture stderr for error messages
   - Use recovery features in interactive mode

4. **Performance**
   - Use streaming for long responses
   - Choose appropriate models for tasks
   - Cache responses when possible

5. **Integration**
   - Install shell completions for better UX
   - Create aliases for common uses
   - Combine with other Unix tools

## Running Examples

All examples assume cmdgpt is built and accessible:
```bash
# Make examples executable
chmod +x examples/*.sh

# Run an example
./examples/simple_query.sh

# Or source to get functions
source examples/api_wrapper_function.sh
ask_ai "Hello, world!"
```