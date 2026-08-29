#!/bin/sh

# Run the common test suite in disposable Linux containers.
# The checkout is mounted read-only; generated files stay inside the container.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

if ! command -v docker >/dev/null 2>&1; then
    echo "error: docker is required" >&2
    exit 127
fi

if ! docker info >/dev/null 2>&1; then
    echo "error: Docker daemon is not reachable" >&2
    echo "       start Docker Desktop or the Docker service and retry" >&2
    exit 125
fi

if [ "$#" -eq 0 ]; then
    set -- gcc:13-bookworm alpine:3.20 debian:13 archlinux:base-devel
fi

run_baseline() {
    image=$1

    if ! docker image inspect "$image" >/dev/null 2>&1; then
        echo "error: Docker image is not available locally: $image" >&2
        echo "       pull it with: docker pull $image" >&2
        return 2
    fi

    case "$image" in
        alpine:*)
            docker run --rm -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" \
                -v "$REPO_DIR:/src:ro" "$image" sh -eu -c '
                apk add --no-cache build-base bash perl coreutils util-linux file
                rm -rf /tmp/libfaketime
                mkdir /tmp/libfaketime
                cp -a /src/. /tmp/libfaketime/
                cd /tmp/libfaketime
                find src test -type f \( -name "*.o" -o -name "*.so*" -o \
                    -name "timetest" -o -name "faketime" -o -name "set_mtime" -o \
                    -name "*_test" \) -delete
                printf "container=%s\\n" "$(cat /etc/alpine-release)"
                printf "kernel="
                uname -a
                timeout 180s make test
            '
            ;;
        archlinux:*)
            docker run --rm -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" \
                -e "DOCKER_ARCH_DISABLE_PACMAN_SANDBOX=${DOCKER_ARCH_DISABLE_PACMAN_SANDBOX:-0}" \
                -v "$REPO_DIR:/src:ro" "$image" bash -eu -c '
                if [ "${DOCKER_ARCH_DISABLE_PACMAN_SANDBOX:-0}" = 1 ]; then
                    sed -i "s/^#DisableSandboxSyscalls/DisableSandboxSyscalls/" /etc/pacman.conf
                fi
                pacman -Syu --noconfirm --needed base-devel bash perl coreutils util-linux file
                rm -rf /tmp/libfaketime
                mkdir /tmp/libfaketime
                cp -a /src/. /tmp/libfaketime/
                cd /tmp/libfaketime
                find src test -type f \( -name "*.o" -o -name "*.so*" -o \
                    -name "timetest" -o -name "faketime" -o -name "set_mtime" -o \
                    -name "*_test" \) -delete
                . /etc/os-release
                printf "container=%s %s\\n" "$NAME" "$VERSION_ID"
                printf "kernel="
                uname -a
                timeout 180s make test
            '
            ;;
        debian:*|gcc:*)
            docker run --rm -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" \
                -v "$REPO_DIR:/src:ro" "$image" sh -eu -c '
                apt-get update
                DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
                    build-essential bash perl coreutils util-linux file
                rm -rf /tmp/libfaketime
                mkdir /tmp/libfaketime
                cp -a /src/. /tmp/libfaketime/
                cd /tmp/libfaketime
                find src test -type f \( -name "*.o" -o -name "*.so*" -o \
                    -name "timetest" -o -name "faketime" -o -name "set_mtime" -o \
                    -name "*_test" \) -delete
                . /etc/os-release
                printf "container=%s %s\\n" "$NAME" "$VERSION_ID"
                printf "kernel="
                uname -a
                timeout 180s make test
            '
            ;;
        *)
            echo "error: unsupported baseline image: $image" >&2
            echo "       pass gcc:13-bookworm, alpine:3.20, debian:13, or archlinux:base-devel" >&2
            return 2
            ;;
    esac
}

status=0
for image in "$@"; do
    echo "==> Running libfaketime baseline in $image"
    if ! run_baseline "$image"; then
        status=1
    fi
done

exit "$status"
