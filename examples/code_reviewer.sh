#!/bin/bash
# Automated code review script using cmdgpt
# Reviews Python files and generates markdown reports

# Check if OPENAI_API_KEY is set
if [ -z "$OPENAI_API_KEY" ]; then
    echo "Error: OPENAI_API_KEY environment variable is not set"
    echo "Please run: export OPENAI_API_KEY='your-api-key'"
    exit 1
fi

# Path to cmdgpt
CMDGPT="../build/cmdgpt"

# Create output directory for reviews
REVIEW_DIR="code_reviews_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$REVIEW_DIR"

echo "=== Automated Code Reviewer ==="
echo "Output directory: $REVIEW_DIR"
echo

# Function to review a single file
review_file() {
    local file="$1"
    local basename=$(basename "$file")
    local review_file="$REVIEW_DIR/${basename%.py}_review.md"
    
    echo "Reviewing: $file"
    
    # Generate review with markdown formatting
    {
        echo "# Code Review: $basename"
        echo "Date: $(date)"
        echo
        echo "## Original Code"
        echo '```python'
        cat "$file"
        echo '```'
        echo
        echo "## Review"
        cat "$file" | $CMDGPT -f markdown "Review this Python code for:
1. Code quality and style
2. Potential bugs or issues
3. Performance improvements
4. Best practices
5. Security concerns

Provide specific suggestions with code examples where applicable."
    } > "$review_file"
    
    echo "  → Review saved to: $review_file"
}

# Check if Python files were provided as arguments
if [ $# -gt 0 ]; then
    # Review specific files
    for file in "$@"; do
        if [ -f "$file" ] && [[ "$file" == *.py ]]; then
            review_file "$file"
        else
            echo "Skipping: $file (not a Python file or doesn't exist)"
        fi
    done
else
    # Review all Python files in current directory
    python_files=(*.py)
    
    if [ ${#python_files[@]} -eq 0 ] || [ "${python_files[0]}" == "*.py" ]; then
        echo "No Python files found in current directory"
        echo "Usage: $0 [file1.py file2.py ...]"
        echo "   Or run in a directory with Python files"
        exit 1
    fi
    
    echo "Found ${#python_files[@]} Python files to review"
    echo
    
    for file in "${python_files[@]}"; do
        review_file "$file"
        # Small delay to be nice to the API
        sleep 1
    done
fi

echo
echo "=== Review Complete ==="
echo "All reviews saved in: $REVIEW_DIR"
echo
echo "To view a review:"
echo "  cat $REVIEW_DIR/<filename>_review.md"
echo
echo "To convert to HTML (requires pandoc):"
echo "  pandoc $REVIEW_DIR/<filename>_review.md -o review.html"