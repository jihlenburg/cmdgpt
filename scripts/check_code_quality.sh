#!/bin/bash
# Local code quality checks script
# Run this before committing to ensure CI will pass

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# This is now a wrapper around the centralized check script
exec "${SCRIPT_DIR}/check_all.sh"