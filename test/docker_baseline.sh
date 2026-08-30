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

DOCKER_PLATFORM=${DOCKER_PLATFORM:-}
if [ -n "$DOCKER_PLATFORM" ]; then
    case "$DOCKER_PLATFORM" in
            linux/386|linux/amd64|linux/arm64|linux/arm/v7|linux/ppc64le|linux/s390x) ;;
        *)
            echo "error: unsupported Docker platform: $DOCKER_PLATFORM" >&2
            exit 2
            ;;
    esac
    docker_platform_arg="--platform=$DOCKER_PLATFORM"
else
    docker_platform_arg=
fi

run_baseline() {
    image=$1

    if ! docker image inspect "$image" >/dev/null 2>&1; then
        echo "error: Docker image is not available locally: $image" >&2
        echo "       pull it with: docker pull $image" >&2
        return 2
    fi

    if [ -n "$DOCKER_PLATFORM" ]; then
        image_arch=$(docker image inspect --format '{{.Architecture}}' "$image")
        case "$DOCKER_PLATFORM:$image_arch" in
            linux/386:386|linux/amd64:amd64|linux/arm64:arm64|linux/arm/v7:arm|linux/ppc64le:ppc64le|linux/s390x:s390x) ;;
            *)
                echo "error: $image is $image_arch, incompatible with $DOCKER_PLATFORM" >&2
                echo "       pull an image matching the requested platform and retry" >&2
                return 2
                ;;
        esac
    fi

    case "$image" in
        alpine:*)
            docker run --rm $docker_platform_arg -e "CFLAGS=${CFLAGS:-}" -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" \
                -v "$REPO_DIR:/src:ro" "$image" sh -eu -c '
                apk add --no-cache build-base bash perl coreutils util-linux file tzdata
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
            docker run --rm $docker_platform_arg -e "CFLAGS=${CFLAGS:-}" -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" \
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
        debian:*|ubuntu:*|gcc:*)
            docker run --rm $docker_platform_arg -e "CFLAGS=${CFLAGS:-}" -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" \
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
        fedora:*|rockylinux:*|centos:*|opensuse/tumbleweed:*)
            docker run --rm $docker_platform_arg -e "CFLAGS=${CFLAGS:-}" -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" \
                -v "$REPO_DIR:/src:ro" "$image" sh -eu -c '
                run_phase() {
                    phase=$1
                    shift
                    printf "phase=%s\\n" "$phase"
                    timeout "${FAKETIME_TEST_PHASE_TIMEOUT:-120}s" "$@"
                }
                run_phase package sh -c '\''if command -v dnf >/dev/null 2>&1; then dnf -y install --allowerasing gcc make glibc-devel bash perl coreutils util-linux file; elif command -v zypper >/dev/null 2>&1; then zypper --non-interactive install gcc make glibc-devel bash perl coreutils util-linux file; else yum -y install gcc make glibc-devel bash perl coreutils util-linux file; fi'\''
                rm -rf /tmp/libfaketime
                mkdir /tmp/libfaketime
                cp -a /src/. /tmp/libfaketime/
                cd /tmp/libfaketime
                find src test -type f \( -name "*.o" -o -name "*.so*" -o \
                    -name "timetest" -o -name "faketime" -o -name "set_mtime" -o \
                    -name "*_test" \) -delete
                if [ -r /etc/fedora-release ]; then
                    printf "container=%s\\n" "$(cat /etc/fedora-release)"
                elif [ -r /etc/centos-release ]; then
                    printf "container=%s\\n" "$(cat /etc/centos-release)"
                else
                    . /etc/os-release
                    printf "container=%s %s\\n" "$NAME" "$VERSION_ID"
                fi
                printf "kernel="
                uname -a
                printf "libc="
                getconf GNU_LIBC_VERSION 2>/dev/null || true
                printf "compiler="
                cc --version | head -n 1
                run_phase build sh -c "make -C src all && make -C test sem_contract_test"
                printf "sem_clockwait-symbols\\n"
                readelf -Ws src/libfaketime.so.1 | grep sem_clockwait
                run_phase semaphore env FAKETIME_SEM_TEST_VERBOSE=1 FAKETIME=+0 \
                    LD_PRELOAD="$PWD/src/libfaketime.so.1" ./test/sem_contract_test
                run_phase full-suite make test
            '
            ;;
        *)
            echo "error: unsupported baseline image: $image" >&2
            echo "       pass gcc:13-bookworm, ubuntu:<tag>, fedora:<tag>, rockylinux:<tag>, centos:<tag>, opensuse/tumbleweed:<tag>, alpine:3.20, debian:13, or archlinux:base-devel" >&2
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
