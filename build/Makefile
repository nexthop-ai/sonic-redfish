# Makefile for sonic-redfish
.PHONY: all clean setup build-bmcweb build-dbus-bridge docker docker-build help

REPO_ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
SCRIPTS_DIR := $(REPO_ROOT)/../scripts
BUILD_SCRIPT := $(REPO_ROOT)/build.sh

all: docker-build

help:
	@echo "Available targets:"
	@echo "  all              - Build using Docker (default)"
	@echo "  docker-build     - Build inside Docker container (recommended)"
	@echo "  local-build      - Build using local system packages"
	@echo "  setup            - Initialize submodules and apply patches"
	@echo "  build-bmcweb     - Build bmcweb (local)"
	@echo "  build-dbus-bridge - Build sonic-dbus-bridge (local)"
	@echo "  docker           - Build runtime Docker container"
	@echo "  clean            - Clean build artifacts"
	@echo ""
	@echo "Examples:"
	@echo "  make              # Build using Docker (recommended)"
	@echo "  make local-build  # Build using local packages"
	@echo "  make clean        # Clean all build artifacts"

docker-build:
	@$(BUILD_SCRIPT)

local-build:
	@$(BUILD_SCRIPT) --no-docker

setup:
	@$(SCRIPTS_DIR)/setup-submodules.sh
	@$(SCRIPTS_DIR)/apply-patches.sh

build-bmcweb:
	@cd $(REPO_ROOT)/../bmcweb && meson setup build && meson compile -C build

build-dbus-bridge:
	@cd $(REPO_ROOT)/../sonic-dbus-bridge && meson setup build && meson compile -C build

docker:
	@cd $(REPO_ROOT)/.. && docker build -t sonic-redfish:latest -f docker/Dockerfile .

clean:
	@rm -rf $(REPO_ROOT)/../bmcweb/build
	@rm -rf $(REPO_ROOT)/../sonic-dbus-bridge/build
	@echo "Build artifacts cleaned"
