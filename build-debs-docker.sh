#!/bin/bash
# Build Debian packages for sonic-redfish using Docker
# This provides a clean, reproducible build environment

set -e  # Exit on error
set -u  # Exit on undefined variable

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$REPO_ROOT/output/debs"
BUILD_JOBS="${BUILD_JOBS:-$(nproc)}"
DOCKER_IMAGE="${DOCKER_IMAGE:-debian:trixie}"

# Component flags
BUILD_BRIDGE="${BUILD_BRIDGE:-1}"
BUILD_BMCWEB="${BUILD_BMCWEB:-1}"

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Print banner
print_banner() {
    echo "========================================="
    echo "  sonic-redfish Docker Package Builder"
    echo "========================================="
    echo ""
    echo "Repository:    $REPO_ROOT"
    echo "Output:        $OUTPUT_DIR"
    echo "Docker Image:  $DOCKER_IMAGE"
    echo "Jobs:          $BUILD_JOBS"
    echo ""
}

# Check Docker availability
check_docker() {
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed or not in PATH"
        exit 1
    fi
    
    if ! docker info &> /dev/null; then
        log_error "Docker daemon is not running or you don't have permission"
        log_info "Try: sudo usermod -aG docker $USER && newgrp docker"
        exit 1
    fi
    
    log_success "Docker is available"
}

# Build packages inside Docker
build_in_docker() {
    log_info "Starting Docker build..."
    
    # Create output directory
    mkdir -p "$OUTPUT_DIR"
    
    # Build script to run inside container
    local build_script='
set -e
set -u

echo "========================================="
echo "Inside Docker Container"
echo "========================================="
echo ""

# Install dependencies
echo "[INFO] Installing build dependencies..."
apt-get update -qq

# Core build tools
apt-get install -y -qq \
    debhelper \
    devscripts \
    build-essential \
    fakeroot \
    dpkg-dev \
    git

# bmcweb dependencies
apt-get install -y -qq \
    meson \
    ninja-build \
    g++ \
    pkg-config \
    libpam0g-dev \
    libssl-dev \
    libsystemd-dev \
    zlib1g-dev \
    nlohmann-json3-dev \
    libnghttp2-dev \
    libtinyxml2-dev \
    python3 \
    python3-yaml \
    python3-mako \
    python3-inflection

# sonic-dbus-bridge dependencies
apt-get install -y -qq \
    libhiredis-dev \
    libjsoncpp-dev \
    libboost-dev \
    libboost-system-dev

echo "[SUCCESS] Dependencies installed"
echo ""

# Configure git
git config --global --add safe.directory /workspace
git config --global --add safe.directory /workspace/bmcweb

# Setup repository
echo "[INFO] Setting up repository..."
cd /workspace

# Check bmcweb source
if [ ! -d "bmcweb" ]; then
    echo "[ERROR] bmcweb directory not found"
    exit 1
fi

if [ -d "bmcweb/.git" ]; then
    cd bmcweb
    if ! git diff --quiet 2>/dev/null; then
        echo "  bmcweb has local changes (patches applied), ready"
        cd /workspace
    else
        echo "  bmcweb source is clean, ready for patches"
        cd /workspace
    fi
else
    echo "  bmcweb source directory ready"
fi

make -f Makefile.build copy-patches
make -f Makefile.build apply-patches
echo "[SUCCESS] Repository setup complete"
echo ""

# Build sonic-dbus-bridge
if [[ "'"$BUILD_BRIDGE"'" == "1" ]]; then
    echo "[INFO] Building sonic-dbus-bridge..."
    cd /workspace/sonic-dbus-bridge
    dpkg-buildpackage -us -uc -b -j'"$BUILD_JOBS"'
    echo "[SUCCESS] sonic-dbus-bridge built"
    echo ""
fi

# Build bmcweb
if [[ "'"$BUILD_BMCWEB"'" == "1" ]]; then
    echo "[INFO] Building bmcweb..."
    cd /workspace/bmcweb
    dpkg-buildpackage -us -uc -b -j'"$BUILD_JOBS"'
    echo "[SUCCESS] bmcweb built"
    echo ""
fi

# Collect artifacts
echo "[INFO] Collecting build artifacts..."
mkdir -p /workspace/output/debs

if [[ "'"$BUILD_BRIDGE"'" == "1" ]]; then
    cp /workspace/sonic-dbus-bridge_*.deb /workspace/output/debs/ 2>/dev/null || true
fi

if [[ "'"$BUILD_BMCWEB"'" == "1" ]]; then
    cp /workspace/bmcweb_*.deb /workspace/output/debs/ 2>/dev/null || true
    cp /workspace/bmcweb-dbg_*.deb /workspace/output/debs/ 2>/dev/null || true
fi

echo "[SUCCESS] Artifacts collected"
echo ""
echo "========================================="
echo "Docker Build Complete"
echo "========================================="
echo ""
echo "Built packages:"
ls -lh /workspace/output/debs/*.deb 2>/dev/null || echo "No packages found"
'
    
    # Run Docker container
    docker run --rm \
        -v "$REPO_ROOT:/workspace" \
        -w /workspace \
        -e BUILD_BRIDGE="$BUILD_BRIDGE" \
        -e BUILD_BMCWEB="$BUILD_BMCWEB" \
        -e BUILD_JOBS="$BUILD_JOBS" \
        "$DOCKER_IMAGE" \
        bash -c "$build_script"
    
    log_success "Docker build completed"
}

# Display build summary
show_summary() {
    echo ""
    echo "========================================="
    echo "  Build Summary"
    echo "========================================="
    echo ""

    if [[ -d "$OUTPUT_DIR" ]]; then
        log_info "Built packages:"
        ls -lh "$OUTPUT_DIR"/*.deb 2>/dev/null || log_warning "No .deb files found in $OUTPUT_DIR"

        echo ""
        log_info "Package details:"
        for deb in "$OUTPUT_DIR"/*.deb; do
            if [[ -f "$deb" ]]; then
                local size=$(du -h "$deb" | cut -f1)
                local name=$(basename "$deb")
                echo "  - $name ($size)"
            fi
        done
    else
        log_error "Output directory not found: $OUTPUT_DIR"
    fi

    echo ""
    echo "========================================="
    log_success "Build complete!"
    echo "========================================="
    echo ""
    echo "To verify packages:"
    echo "  dpkg-deb -c $OUTPUT_DIR/<package>.deb"
    echo "  dpkg-deb -I $OUTPUT_DIR/<package>.deb"
    echo ""
    echo "To install packages:"
    echo "  sudo dpkg -i $OUTPUT_DIR/*.deb"
    echo "  sudo apt-get install -f  # Fix dependencies if needed"
    echo ""
}

# Show usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build Debian packages for sonic-redfish using Docker.

OPTIONS:
    -h, --help              Show this help message
    -b, --bridge-only       Build only sonic-dbus-bridge
    -w, --bmcweb-only       Build only bmcweb
    -j, --jobs N            Number of parallel build jobs (default: nproc)
    -i, --image IMAGE       Docker image to use (default: debian:trixie)

ENVIRONMENT VARIABLES:
    BUILD_BRIDGE=0          Skip sonic-dbus-bridge build
    BUILD_BMCWEB=0          Skip bmcweb build
    BUILD_JOBS=N            Number of parallel build jobs
    DOCKER_IMAGE=IMAGE      Docker image to use

EXAMPLES:
    # Build all packages
    $0

    # Build only bmcweb
    $0 --bmcweb-only

    # Build with 4 parallel jobs
    $0 --jobs 4

    # Use custom Docker image
    $0 --image debian:bookworm

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -b|--bridge-only)
            BUILD_BMCWEB=0
            shift
            ;;
        -w|--bmcweb-only)
            BUILD_BRIDGE=0
            shift
            ;;
        -j|--jobs)
            BUILD_JOBS="$2"
            shift 2
            ;;
        -i|--image)
            DOCKER_IMAGE="$2"
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    print_banner
    check_docker
    build_in_docker
    show_summary
}

# Run main function
main

