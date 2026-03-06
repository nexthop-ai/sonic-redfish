#!/bin/bash
# Quick build script for sonic-redfish Debian packages
# Minimal script for fast builds without dependency installation

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$REPO_ROOT/output/debs"

echo "========================================="
echo "  Quick Debian Package Build"
echo "========================================="
echo ""

# Setup repository
echo "[1/4] Setting up repository..."
cd "$REPO_ROOT"

# Check bmcweb source
if [[ ! -d "bmcweb" ]]; then
    echo "Error: bmcweb directory not found"
    exit 1
fi

if [[ -d "bmcweb/.git" ]]; then
    cd bmcweb
    if ! git diff --quiet 2>/dev/null; then
        echo "  bmcweb has local changes (patches applied), ready"
        cd "$REPO_ROOT"
    else
        echo "  bmcweb source is clean, ready for patches"
        cd "$REPO_ROOT"
    fi
else
    echo "  bmcweb source directory ready"
fi

make -f Makefile.build copy-patches
make -f Makefile.build apply-patches

# Build sonic-dbus-bridge
echo ""
echo "[2/4] Building sonic-dbus-bridge..."
cd "$REPO_ROOT/sonic-dbus-bridge"
dpkg-buildpackage -us -uc -b -j$(nproc)

# Build bmcweb
echo ""
echo "[3/4] Building bmcweb..."
cd "$REPO_ROOT/bmcweb"
dpkg-buildpackage -us -uc -b -j$(nproc)

# Collect artifacts
echo ""
echo "[4/4] Collecting artifacts..."
mkdir -p "$OUTPUT_DIR"
cp "$REPO_ROOT"/sonic-dbus-bridge_*.deb "$OUTPUT_DIR/" 2>/dev/null || true
cp "$REPO_ROOT"/bmcweb_*.deb "$OUTPUT_DIR/" 2>/dev/null || true
cp "$REPO_ROOT"/bmcweb-dbg_*.deb "$OUTPUT_DIR/" 2>/dev/null || true

echo ""
echo "========================================="
echo "  Build Complete!"
echo "========================================="
echo ""
echo "Packages built:"
ls -lh "$OUTPUT_DIR"/*.deb
echo ""
echo "Location: $OUTPUT_DIR"

