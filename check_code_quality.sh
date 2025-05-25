#!/bin/bash
# Local code quality checks script
# Run this before committing to ensure CI will pass

# This is now a wrapper around the centralized check script
exec ./scripts/check_all.sh