#!/bin/sh

# Run the Linux test suite with AddressSanitizer and UndefinedBehaviorSanitizer.
# The checkout is mounted read-only; generated files stay inside the container.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
IMAGE=${1:-gcc:13-bookworm}
DOCKER_PLATFORM=${DOCKER_PLATFORM:-}

if ! command -v docker >/dev/null 2>&1; then
    echo "error: docker is required" >&2
    exit 127
fi

if ! docker info >/dev/null 2>&1; then
    echo "error: Docker daemon is not reachable" >&2
    exit 125
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "error: Docker image is not available locally: $IMAGE" >&2
    echo "       pull it with: docker pull $IMAGE" >&2
    exit 2
fi

docker_platform_arg=
if [ -n "$DOCKER_PLATFORM" ]; then
    case "$DOCKER_PLATFORM" in
        linux/amd64|linux/arm64|linux/arm/v7|linux/ppc64le|linux/s390x) ;;
        *)
            echo "error: unsupported Docker platform: $DOCKER_PLATFORM" >&2
            exit 2
            ;;
    esac
    image_arch=$(docker image inspect --format '{{.Architecture}}' "$IMAGE")
    case "$DOCKER_PLATFORM:$image_arch" in
        linux/amd64:amd64|linux/arm64:arm64|linux/arm/v7:arm|linux/ppc64le:ppc64le|linux/s390x:s390x) ;;
        *)
            echo "error: $IMAGE is $image_arch, incompatible with $DOCKER_PLATFORM" >&2
            echo "       pull an image matching the requested platform and retry" >&2
            exit 2
            ;;
    esac
    docker_platform_arg="--platform=$DOCKER_PLATFORM"
fi

echo "==> Running sanitizer baseline in $IMAGE${DOCKER_PLATFORM:+ ($DOCKER_PLATFORM)}"
docker run --rm $docker_platform_arg -e "IMAGE=$IMAGE" -v "$REPO_DIR:/src:ro" "$IMAGE" sh -eu -c '
    case "$IMAGE" in
        fedora:*|rockylinux:*)
            # The Fedora `perl` meta-package pulls in hundreds of optional
            # modules and documentation packages.  The test suite only needs
            # the interpreter and its runtime modules, so use the minimal
            # package to keep Docker setup reliable on current Fedora.
            dnf -y -q --setopt=install_weak_deps=False --setopt=tsflags=nodocs \
                install gcc libasan libubsan make glibc-devel bash perl-interpreter coreutils util-linux file >/dev/null
            ;;
        *)
            apt-get update -qq
            DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
                build-essential bash perl coreutils util-linux file >/dev/null
            ;;
    esac
    rm -rf /tmp/libfaketime
    mkdir /tmp/libfaketime
    cp -a /src/. /tmp/libfaketime/
    cd /tmp/libfaketime
    find src test -type f \( -name "*.o" -o -name "*.so*" -o \
        -name "timetest" -o -name "faketime" -o -name "set_mtime" -o \
        -name "*_test" \) -delete
    # On Fedora, libasan.so is a linker script rather than a preloadable ELF
    # object.  Use the versioned runtime library for LD_PRELOAD.
    ASAN_LIB=$(gcc -print-file-name=libasan.so.8)
    test -f "$ASAN_LIB"
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:allocator_may_return_null=0:verify_asan_link_order=0 \
    UBSAN_OPTIONS=halt_on_error=1 \
    FAKETIME_SANITIZER_LIB="$ASAN_LIB:../src/libfaketime.so.1" \
    CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    LDFLAGS="-fsanitize=address,undefined" \
    timeout 240s make test
'
