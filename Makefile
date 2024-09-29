.PHONY: all check clean configure_debug configure_release format install lint

# PHONY targets

all: build
	ninja -C build

check: build
	ninja -C build test

clean: build
	ninja -C build clean

configure_debug: build
	meson configure build -Dbuildtype=debug -Db_sanitize=address,undefined

configure_release: build
	meson configure build -Dbuildtype=release -Db_sanitize=none

format: build
	ninja -C build clang-format

install: build
	ninja -C build install

lint: build
	ninja -C build scan-build

# Normal targets

build:
	meson setup build
