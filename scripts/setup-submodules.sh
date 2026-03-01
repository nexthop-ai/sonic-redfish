#!/bin/bash
# Initialize and update git submodules
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$REPO_ROOT"

echo "Initializing git submodules..."

# Check if bmcweb has local changes (patches applied)
if [ -d "bmcweb/.git" ]; then
    cd bmcweb
    if ! git diff --quiet || ! git diff --cached --quiet; then
        echo "  bmcweb has local changes (patches applied), skipping checkout"
        cd ..
        echo "Submodules ready!"
        git submodule status
        exit 0
    fi
    cd ..
fi

git submodule update --init --recursive

# Ensure bmcweb is at the correct commit (not latest master)
if [ -f ".bmcweb-commit" ]; then
    BMCWEB_COMMIT=$(cat .bmcweb-commit)
    echo "Checking out bmcweb at commit: $BMCWEB_COMMIT"
    cd bmcweb
    git checkout "$BMCWEB_COMMIT" 2>/dev/null || echo "  Already at correct commit or has local changes"
    cd ..
fi

echo "Submodules initialized successfully!"
git submodule status
