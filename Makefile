.ONESHELL:
SHELL = /bin/bash
.SHELLFLAGS += -ex

# Build configuration
SONIC_CONFIG_MAKE_JOBS ?= $(shell nproc)
CONFIGURED_ARCH ?= amd64
CROSS_BUILD_ENVIRON ?= n

# bmcweb source configuration
BMCWEB_HEAD_COMMIT ?= 6926d430
BMCWEB_REPO_URL ?= https://github.com/openbmc/bmcweb.git

# Directories
BMCWEB_DIR := bmcweb
BRIDGE_DIR := sonic-dbus-bridge
PATCHES_DIR := patches
SERIES_FILE := $(PATCHES_DIR)/series

# Package names
BMCWEB = bmcweb_$(SONIC_REDFISH_VERSION)_$(CONFIGURED_ARCH).deb
BMCWEB_DBG = bmcweb-dbg_$(SONIC_REDFISH_VERSION)_$(CONFIGURED_ARCH).deb
SONIC_DBUS_BRIDGE = sonic-dbus-bridge_$(SONIC_REDFISH_VERSION)_$(CONFIGURED_ARCH).deb
SONIC_DBUS_BRIDGE_DBGSYM = sonic-dbus-bridge-dbgsym_$(SONIC_REDFISH_VERSION)_$(CONFIGURED_ARCH).deb

$(addprefix $(DEST)/, $(BMCWEB)): $(DEST)/% :
	# Clone bmcweb if not present
	if [ ! -d "$(BMCWEB_DIR)" ]; then
		echo "Cloning bmcweb from $(BMCWEB_REPO_URL)..."
		git clone $(BMCWEB_REPO_URL) $(BMCWEB_DIR)
		cd $(BMCWEB_DIR) && git checkout $(BMCWEB_HEAD_COMMIT)
	fi

	# Apply patches if not already applied
	cd $(BMCWEB_DIR)
	if git diff --quiet 2>/dev/null; then
		echo "Applying patches from $(PATCHES_DIR)/series..."
		while IFS= read -r patch || [ -n "$$patch" ]; do
			patch=$$(echo "$$patch" | sed 's/#.*//;s/^[[:space:]]*//;s/[[:space:]]*$$//')
			[ -z "$$patch" ] && continue
			echo "  Applying: $$patch"
			if [ -f "../$(PATCHES_DIR)/$$patch" ]; then
				git apply "../$(PATCHES_DIR)/$$patch" || { echo "Error applying $$patch"; exit 1; }
			else
				echo "Error: Patch file not found: $$patch"
				exit 1
			fi
		done < ../$(SERIES_FILE)
	fi
	cd ..

	# Build bmcweb package using dpkg-buildpackage
	pushd $(BMCWEB_DIR)

ifeq ($(CROSS_BUILD_ENVIRON), y)
	dpkg-buildpackage -b -us -uc -a$(CONFIGURED_ARCH) -Pcross,nocheck -j$(SONIC_CONFIG_MAKE_JOBS)
else
	dpkg-buildpackage -b -us -uc -j$(SONIC_CONFIG_MAKE_JOBS)
endif
	popd

ifneq ($(DEST),)
	mv $(BMCWEB) $(BMCWEB_DBG) $(DEST)/
endif

# Derived package (debug symbols) depends on main package
$(addprefix $(DEST)/, $(BMCWEB_DBG)): $(DEST)/% : $(DEST)/$(BMCWEB)

clean:
	# Clean bmcweb
	if [ -d "$(BMCWEB_DIR)" ]; then
		cd $(BMCWEB_DIR) && debian/rules clean || true
	fi
	rm -f bmcweb_*.deb bmcweb-dbg_*.deb
	rm -f bmcweb_*.changes bmcweb_*.buildinfo bmcweb_*.dsc

.PHONY: clean

