# sonic-redfish

SONiC Redfish implementation providing bmcweb and sonic-dbus-bridge as Debian packages.

## Table of Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [Build System](#build-system)
- [Patch Management](#patch-management)
- [Cleanup Targets](#cleanup-targets)
- [Dependency Management](#dependency-management)
- [Components](#components)
- [Troubleshooting](#troubleshooting)
- [License](#license)

## Overview

This repository contains:
- **bmcweb**: OpenBMC web server source code with SONiC-specific patches
- **sonic-dbus-bridge**: Bridge between SONiC Redis and D-Bus for bmcweb integration

Both components are built as Debian packages (`.deb`) for easy integration with SONiC.

## Quick Start

### Prerequisites

- Docker installed on your system
- Git
- sudo access (for cleaning root-owned build artifacts)

### Build

```bash
# Build all components (Docker-based, produces .deb packages)
make -f Makefile.build

# Or explicitly
make -f Makefile.build all
```

Build artifacts will be available in `target/debs/trixie/`:
- `bmcweb_1.0.0_arm64.deb`
- `bmcweb-dbg_1.0.0_arm64.deb`
- `sonic-dbus-bridge_1.0.0_arm64.deb`
- `sonic-dbus-bridge-dbgsym_1.0.0_arm64.deb`

### Build Targets

```bash
# Show all available targets
make -f Makefile.build help

# Build individual components (automatically runs clean + dependencies)
make -f Makefile.build build-bmcweb    # Runs: clean → setup-bmcweb → apply-patches → build
make -f Makefile.build build-bridge    # Runs: clean → build

# Clean build artifacts (removes build dirs, resets bmcweb source)
make -f Makefile.build clean

# Complete reset (clean + remove Docker images + full git reset)
make -f Makefile.build reset
```

### Build Options

```bash
# Use custom number of parallel jobs (default: nproc)
make -f Makefile.build SONIC_CONFIG_MAKE_JOBS=8

# Use custom output directory (default: target/debs/trixie)
make -f Makefile.build SONIC_REDFISH_TARGET=output/debs

# Build with specific bmcweb commit (default: 6926d430)
make -f Makefile.build BMCWEB_HEAD_COMMIT=abc123
```

## Build System

The build system is designed for **Debian Trixie** and uses:

1. **Docker-based builds**: All compilation happens inside a `debian:trixie` container for consistency
2. **Debian packaging**: Uses `dpkg-buildpackage` to create `.deb` packages
3. **Meson subprojects**: Dependencies (sdbusplus, stdexec) are managed via `.wrap` files
4. **Automatic dependencies**: Build targets automatically trigger required cleanup and setup steps
5. **Patch management**: Uses a `patches/series` file to define patch order

### Build Flow

![Build Flow Chart](images/BuildFlowChart.png)

```
make -f Makefile.build all

1. Build Docker image (sonic-redfish-builder:latest)
   - Base: debian:trixie
   - Installs: build-essential, meson, debhelper, C++23 toolchain

2. Build sonic-dbus-bridge
   - Meson downloads dependencies (sdbusplus, stdexec) via .wrap files
   - dpkg-buildpackage creates .deb packages

3. Build bmcweb
   - Setup bmcweb source (auto-clone if needed)
   - Apply patches from patches/series
   - Meson downloads dependencies via .wrap files
   - dpkg-buildpackage creates .deb packages

4. Collect artifacts to target/debs/trixie/
   - bmcweb_1.0.0_arm64.deb
   - bmcweb-dbg_1.0.0_arm64.deb
   - sonic-dbus-bridge_1.0.0_arm64.deb
   - sonic-dbus-bridge-dbgsym_1.0.0_arm64.deb
   - Plus .changes, .buildinfo, .dsc files
```

### Automatic Dependencies

The build system automatically handles dependencies:

- **`build-bmcweb`**: Automatically runs `clean` → `setup-bmcweb` → `apply-patches` → build
- **`build-bridge`**: Automatically runs `clean` → build

This ensures a clean, reproducible build every time.

## Patch Management

Patches are located in `patches/` directory:
- `patches/series` - Defines patch order (lines starting with `#` are comments)
- `patches/*.patch` - Individual patch files

Current patches:
1. `0001-Integrating-bmcweb-with-SONiC-s-build-system.patch` - Adds Debian packaging


To add a new patch:
1. Make changes in bmcweb source directory
2. Generate patch: `cd bmcweb && git format-patch -1 HEAD`
3. Move patch to `patches/` directory
4. Add patch filename to `patches/series`

## Cleanup Targets

### `clean` - Remove build artifacts, reset source
- Removes: `obj-*`, `debian/`, `.deb` files, subproject builds
- Resets: bmcweb source to clean git state (so patches can be reapplied)
- Keeps: Docker images, target directory
- Use when: You want to rebuild from scratch

### `reset` - Complete cleanup
- Does everything `clean` does, plus:
- Removes: Docker images, target directory
- Resets: bmcweb to base commit with `git clean -fdx`
- Use when: You want to start completely fresh

## Dependency Management

Dependencies are managed via **Meson wrap files** (`.wrap`):

### bmcweb dependencies:
- `bmcweb/subprojects/sdbusplus.wrap` - D-Bus C++ bindings
- `bmcweb/subprojects/stdexec.wrap` - C++23 executors
- Plus other dependencies defined in bmcweb upstream

### sonic-dbus-bridge dependencies:
- `sonic-dbus-bridge/subprojects/sdbusplus.wrap` - D-Bus C++ bindings
- `sonic-dbus-bridge/subprojects/stdexec.wrap` - C++23 executors

Meson automatically downloads and builds these dependencies during the build process.

The Debian packages can be installed in SONiC images.

## Components

### bmcweb
- **Source**: https://github.com/openbmc/bmcweb
- **Base commit**: 6926d430
- **License**: Apache-2.0
- **Purpose**: Redfish API server
- **Build system**: Meson + Debian packaging
- **Output**: `bmcweb_1.0.0_arm64.deb`, `bmcweb-dbg_1.0.0_arm64.deb`

### sonic-dbus-bridge
- **License**: Apache-2.0
- **Purpose**: Bridge SONiC Redis database to D-Bus for bmcweb
- **Build system**: Meson + Debian packaging
- **Output**: `sonic-dbus-bridge_1.0.0_arm64.deb`, `sonic-dbus-bridge-dbgsym_1.0.0_arm64.deb`

## Troubleshooting

### Build fails with "debian/changelog: No such file or directory"
Run `make -f Makefile.build reset` to completely clean the workspace, then rebuild.

### Permission denied when cleaning
The build creates root-owned files inside Docker. The Makefile uses `sudo rm` to clean them.
Make sure you have sudo access.

### Docker image build fails
Check your internet connection - the build downloads packages from Debian repositories.

### Meson subproject download fails
Check internet connection and firewall settings. Meson needs to access GitHub to download dependencies.

## License

Apache-2.0
