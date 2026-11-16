#!/usr/bin/env bash
# Reproduce and classify GitHub issue #545 without making Rust/Tokio a CI gate.
set -euo pipefail

image=${TOKIO_545_IMAGE:-libfaketime/tokio-545:local}
timeout_seconds=${TOKIO_545_TIMEOUT_SECONDS:-8}

case "$timeout_seconds" in
  ''|*[!0-9]*) echo "TOKIO_545_TIMEOUT_SECONDS must be a positive integer" >&2; exit 2 ;;
esac

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required" >&2
  exit 2
fi

docker build -f test/Dockerfile.tokio-545 -t "$image" .

docker run --rm -e TOKIO_545_REQUIRE_PASS="${TOKIO_545_REQUIRE_PASS:-0}" \
  -v "$PWD":/src:ro "$image" bash -ec '
  set -euo pipefail
  rm -rf /work
  mkdir /work
  cp -a /src/. /work/
  cd /work
  make -C src clean all

  probe=/opt/tokio-issue-545/target/release/libfaketime-tokio-issue-545
  timeout_seconds="${TOKIO_545_TIMEOUT_SECONDS:-8}"

  run_case() {
    local label="$1" library="$2" faketime="$3" method="$4" monotonic="$5"
    local output status
    set +e
    if [ "$monotonic" = dont_fake ]; then
      output=$(timeout "${timeout_seconds}s" env FAKETIME_DONT_FAKE_MONOTONIC=1 \
        FAKETIME="$faketime" LD_PRELOAD="$PWD/src/$library" \
        "$probe" "$method" 1 2>&1)
    else
      output=$(timeout "${timeout_seconds}s" env FAKETIME="$faketime" \
        LD_PRELOAD="$PWD/src/$library" "$probe" "$method" 1 2>&1)
    fi
    status=$?
    set -e
    printf "case=%s status=%s\\n%s\\n" "$label" "$status" "$output"
    if [ "$status" -ne 0 ] || [[ "$output" != *"[$method] Done sleep."* ]]; then
      return 1
    fi
  }

  failures=0
  check_case() {
    if ! run_case "$@"; then
      failures=$((failures + 1))
    fi
  }
  check_case std-absolute-mt libfaketimeMT.so.1 "1970-01-01 00:00:00" std_thread_sleep full
  check_case tokio-absolute-mt libfaketimeMT.so.1 "1970-01-01 00:00:00" tokio_time_sleep full
  check_case std-relative-mt libfaketimeMT.so.1 -30d std_thread_sleep full
  check_case tokio-relative-mt libfaketimeMT.so.1 -30d tokio_time_sleep full
  check_case tokio-absolute-normal libfaketime.so.1 "1970-01-01 00:00:00" tokio_time_sleep full
  check_case tokio-absolute-no-monotonic libfaketimeMT.so.1 "1970-01-01 00:00:00" tokio_time_sleep dont_fake
  printf "tokio-545-classification failures=%s\\n" "$failures"
  if [ "$failures" -ne 0 ] && [ "${TOKIO_545_REQUIRE_PASS:-0}" = 1 ]; then
    exit 1
  fi
'
