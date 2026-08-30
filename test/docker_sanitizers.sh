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
sanitizer_image=$IMAGE
case "$IMAGE" in
    fedora:*|rockylinux:*)
        sanitizer_image=libfaketime-sanitizer:local
        docker build --pull=false $docker_platform_arg \
            --build-arg "BASE_IMAGE=$IMAGE" \
            -t "$sanitizer_image" -f "$SCRIPT_DIR/Dockerfile.fedora-sanitizer" "$SCRIPT_DIR"
        ;;
esac
run_sanitizer_container() {
docker run --rm $docker_platform_arg -e "IMAGE=$IMAGE" -v "$REPO_DIR:/src:ro" "$sanitizer_image" sh -eu -c '
    case "$IMAGE" in
        fedora:*|rockylinux:*)
            # Packages are installed while building the derived image above.
            :
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
    export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:allocator_may_return_null=0:verify_asan_link_order=0
    export UBSAN_OPTIONS=halt_on_error=1
    export FAKETIME_SANITIZER_LIB="$ASAN_LIB:../src/libfaketime.so.1"
    export FAKETIME_TESTLIB="$ASAN_LIB:../src/libfaketime.so.1"
    export CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
    export LDFLAGS="-fsanitize=address,undefined"
    set +e
    timeout --foreground 240s make test
    test_status=$?
    set -e
    exit "$test_status"
'
}

# Docker reserves status 125 for failures in the daemon or docker-run
# invocation.  Fedora's package setup can expose transient daemon failures
# after a successful transaction, so retry the whole disposable container.
container_attempt=1
while :; do
    set +e
    run_sanitizer_container
    container_status=$?
    set -e
    if [ "$container_status" -eq 0 ]; then
        exit 0
    fi
    if [ "$container_status" -ne 125 ] || [ "$container_attempt" -ge 3 ]; then
        exit "$container_status"
    fi
    echo "warning: Docker run failed with status 125; retrying (attempt $((container_attempt + 1))/3)" >&2
    container_attempt=$((container_attempt + 1))
    sleep 2
done
