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

# Cross-architecture containers on GitHub's Linux runners occasionally lose
# DNS connectivity through Docker's bridge while QEMU is active.  Allow the
# architecture job to opt into the runner's network namespace instead.  Keep
# the default bridge network for ordinary local and distribution tests.
DOCKER_NETWORK=${DOCKER_NETWORK:-}
case "$DOCKER_NETWORK" in
    "") docker_network_arg= ;;
    host) docker_network_arg="--network=host" ;;
    *)
        echo "error: unsupported Docker network mode: $DOCKER_NETWORK" >&2
        exit 2
        ;;
esac

run_baseline() {
    image=$1

    if ! docker image inspect "$image" >/dev/null 2>&1; then
        echo "error: Docker image is not available locally: $image" >&2
        echo "       pull it with: docker pull $image" >&2
        return 2
    fi

    if [ -n "$DOCKER_PLATFORM" ]; then
        # A multi-platform image pulled with --platform may remain a manifest
        # list locally.  Ask Docker for the selected platform explicitly; an
        # older daemon without that option is allowed to let docker run make
        # the final compatibility decision.
        image_arch=$(docker image inspect --platform "$DOCKER_PLATFORM" \
            --format '{{.Architecture}}' "$image" 2>/dev/null || true)
        if [ -n "$image_arch" ]; then
            case "$DOCKER_PLATFORM:$image_arch" in
                linux/386:386|linux/amd64:amd64|linux/arm64:arm64|linux/arm/v7:arm|linux/ppc64le:ppc64le|linux/s390x:s390x) ;;
                *)
                    echo "error: $image is $image_arch, incompatible with $DOCKER_PLATFORM" >&2
                    echo "       pull an image matching the requested platform and retry" >&2
                    return 2
                    ;;
            esac
        fi
    fi

    case "$image" in
        alpine:*)
            docker run --rm $docker_platform_arg $docker_network_arg -e "CFLAGS=${CFLAGS:-}" -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" -e "LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS=${LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS:-0}" \
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
                timeout 180s make TEST_DEMO=0 test
            '
            ;;
        archlinux:*)
            docker run --rm $docker_platform_arg $docker_network_arg -e "CFLAGS=${CFLAGS:-}" -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" -e "LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS=${LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS:-0}" \
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
                timeout 180s make TEST_DEMO=0 test
            '
            ;;
        debian:*|ubuntu:*|gcc:*)
            docker run --rm $docker_platform_arg $docker_network_arg -e "CFLAGS=${CFLAGS:-}" -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" -e "LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS=${LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS:-0}" \
                -v "$REPO_DIR:/src:ro" "$image" sh -eu -c '
                . /etc/os-release
                # Debian 11 remains useful for its older glibc ABI, but its
                # LTS security metadata can reference packages no longer
                # available from the live mirror.  Pin its disposable build
                # to an intact Debian snapshot instead.
                if [ "${VERSION_CODENAME:-}" = bullseye ]; then
                    rm -f /etc/apt/sources.list.d/*.list
                    cat > /etc/apt/sources.list <<"EOF"
deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/20260903T220410Z/ bullseye main
deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/20260903T220410Z/ bullseye-updates main
deb [check-valid-until=no] http://snapshot.debian.org/archive/debian-security/20260903T220410Z/ bullseye-security main
EOF
                fi
                rm -rf /var/lib/apt/lists/*
                apt_update_attempt=1
                while :; do
                    apt-get update || true
                    if apt-cache show build-essential >/dev/null 2>&1 && \
                       apt-cache show perl >/dev/null 2>&1 && \
                       apt-cache show file >/dev/null 2>&1; then
                        break
                    fi
                    if [ "$apt_update_attempt" -ge 3 ]; then
                        echo "error: apt package indexes are unavailable after $apt_update_attempt attempts" >&2
                        exit 1
                    fi
                    echo "warning: apt package indexes are incomplete; retrying ($((apt_update_attempt + 1))/3)" >&2
                    apt_update_attempt=$((apt_update_attempt + 1))
                    sleep 2
                done
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
                timeout 180s make TEST_DEMO=0 test
            '
            ;;
        fedora:*|rockylinux:*|centos:*|quay.io/centos/centos:*|opensuse/tumbleweed:*)
            docker run --rm $docker_platform_arg $docker_network_arg -e "CFLAGS=${CFLAGS:-}" -e "FAKETIME_COMPILE_CFLAGS=${FAKETIME_COMPILE_CFLAGS:-}" -e "LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS=${LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS:-0}" \
                -v "$REPO_DIR:/src:ro" "$image" sh -eu -c '
                run_phase() {
                    phase=$1
                    shift
                    printf "phase=%s\\n" "$phase"
                    timeout "${FAKETIME_TEST_PHASE_TIMEOUT:-120}s" "$@"
                }
                # CentOS Linux 7 reached end of life.  Its final packages are
                # kept by the kernel.org archive rather than the retired
                # mirrorlist endpoint.  The primary vault currently rejects
                # the obsolete CentOS 7 TLS client for package downloads.
                if [ -r /etc/centos-release ] && grep -q "release 7" /etc/centos-release; then
                    cat > /etc/yum.repos.d/CentOS-Base.repo <<"EOF"
[base]
name=CentOS-7.9.2009 - Base
baseurl=https://archive.kernel.org/centos-vault/7.9.2009/os/$basearch/
gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-7

[updates]
name=CentOS-7.9.2009 - Updates
baseurl=https://archive.kernel.org/centos-vault/7.9.2009/updates/$basearch/
gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-7

[extras]
name=CentOS-7.9.2009 - Extras
baseurl=https://archive.kernel.org/centos-vault/7.9.2009/extras/$basearch/
gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-7
EOF
                fi
                run_phase package sh -c '\''if command -v dnf >/dev/null 2>&1; then dnf -y install --allowerasing gcc make glibc-devel bash perl coreutils findutils util-linux file; elif command -v zypper >/dev/null 2>&1; then zypper --non-interactive install gcc make glibc-devel bash perl coreutils findutils util-linux file; else yum -y install gcc make glibc-devel bash perl coreutils findutils util-linux file; fi'\''
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
                run_phase full-suite make TEST_DEMO=0 test
            '
            ;;
        *)
            echo "error: unsupported baseline image: $image" >&2
            echo "       pass gcc:13-bookworm, ubuntu:<tag>, fedora:<tag>, rockylinux:<tag>, centos:<tag>, quay.io/centos/centos:<tag>, opensuse/tumbleweed:<tag>, alpine:3.20, debian:13, or archlinux:base-devel" >&2
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
