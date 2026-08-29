#!/bin/sh

# Run the Linux baseline against ARM64 images from an ARM macOS host.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ "$#" -eq 0 ]; then
    set -- gcc:13-bookworm alpine:3.20 debian:13 archlinux:base-devel
fi

DOCKER_PLATFORM=${DOCKER_PLATFORM:-linux/arm64}
export DOCKER_PLATFORM
exec "$SCRIPT_DIR/docker_baseline.sh" "$@"
