# sonic-redfish

SONiC Redfish implementation providing bmcweb and sonic-dbus-bridge as a unified Docker image.

## Overview

This repository contains:
- **bmcweb**: OpenBMC web server source code with SONiC-specific patches
- **sonic-dbus-bridge**: Bridge between SONiC Redis and D-Bus for bmcweb integration

## Quick Start

### Prerequisites

- Docker installed on your system
- Git

### Build

```bash
# Build all components (Docker-based)
make -f Makefile.build

# Or simply
make -f Makefile.build all
```

### Build Targets

```bash
# Show all available targets
make -f Makefile.build help

# Build individual components
make -f Makefile.build build-bmcweb
make -f Makefile.build build-bridge

# Clean build artifacts
make -f Makefile.build clean

# Complete reset (clean + reset bmcweb source + remove Docker images)
make -f Makefile.build reset
```

### Build Options

```bash
# Use custom number of parallel jobs (default: nproc)
make -f Makefile.build SONIC_CONFIG_MAKE_JOBS=8
```

## Build System

The build system follows the **sonic-linux-kernel pattern** from sonic-buildimage:

1. **Docker-only build**: All compilation happens inside a Docker container for consistency
2. **Patch management**: Uses a `patches/series` file to define patch order
3. **Patch staging**: Patches are copied to `bmcweb/debian/` before application
4. **Single entry point**: All build logic is in `Makefile.build`

### Build Flow

```
make -f Makefile.build
  ↓
1. Build Docker image (sonic-redfish-builder)
  ↓
2. Check bmcweb source is ready
  ↓
3. Copy patches to bmcweb/debian/
  ↓
4. Apply patches from series file
  ↓
5. Build bmcweb (Meson)
  ↓
6. Build sonic-dbus-bridge (Meson)
  ↓
Build artifacts:
  - bmcweb/build/bmcweb
  - sonic-dbus-bridge/build/sonic-dbus-bridge
```

## Patch Management

Patches are located in `patches/` directory:
- `patches/series` - Defines patch order
- `patches/*.patch` - Individual patch files

To add a new patch:
1. Make changes in bmcweb source directory
2. Generate patch: `cd bmcweb && git format-patch -1 HEAD`
3. Move patch to `patches/` directory
4. Add patch filename to `patches/series`

## Integration with sonic-buildimage

This repository is designed to be integrated into sonic-buildimage:

```bash
cd sonic-buildimage
git submodule add https://github.com/sonic-net/sonic-redfish.git src/sonic-redfish
```

## Components

### bmcweb
- **Source**: https://github.com/openbmc/bmcweb
- **Base commit**: 6926d430
- **License**: Apache-2.0
- **Purpose**: Redfish API server

### sonic-dbus-bridge
- **License**: Apache-2.0
- **Purpose**: Bridge SONiC Redis database to D-Bus for bmcweb

## License

Apache-2.0
