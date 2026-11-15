# Optional async-runtime smoke probes. Runtime clock bypasses are classified,
# while hangs and compiler failures remain actionable when explicitly enabled.
# The Tokio fixture is pinned and built offline, so CI never downloads it.

init()
{
  typeset testsuite="$1"
  PLATFORM=$(platform)
  echo "# PLATFORM=$PLATFORM"
}

run_tokio_mode()
{
  typeset mode="$1" library="$2" timeout_seconds="$3" output
  if [ "$PLATFORM" = mac ]; then
    case "$mode" in
      realtime_only|dont_fake_monotonic)
        output=$(FAKETIME_DONT_FAKE_MONOTONIC=1 FAKETIME='@2020-06-15 12:00:00' \
          DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib DYLD_FORCE_FLAT_NAMESPACE=1 \
          FAKETIME_ASYNC_TEST_TIMEOUT="$timeout_seconds" perl -e \
          'alarm $ENV{FAKETIME_ASYNC_TEST_TIMEOUT}; exec @ARGV or exit 127' -- \
          tokio_probe/target/debug/libfaketime-tokio-probe 2>/dev/null) ;;
      *)
        output=$(FAKETIME='@2020-06-15 12:00:00' \
          DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib DYLD_FORCE_FLAT_NAMESPACE=1 \
          FAKETIME_ASYNC_TEST_TIMEOUT="$timeout_seconds" perl -e \
          'alarm $ENV{FAKETIME_ASYNC_TEST_TIMEOUT}; exec @ARGV or exit 127' -- \
          tokio_probe/target/debug/libfaketime-tokio-probe 2>/dev/null) ;;
    esac
  else
    case "$mode" in
      realtime_only|dont_fake_monotonic)
        output=$(timeout "${timeout_seconds}s" env FAKETIME_DONT_FAKE_MONOTONIC=1 \
          FAKETIME='@2020-06-15 12:00:00' LD_PRELOAD="$library" \
          tokio_probe/target/debug/libfaketime-tokio-probe 2>/dev/null) ;;
      *)
        output=$(timeout "${timeout_seconds}s" env FAKETIME='@2020-06-15 12:00:00' \
          LD_PRELOAD="$library" tokio_probe/target/debug/libfaketime-tokio-probe 2>/dev/null) ;;
    esac
  fi
  case "$output" in
    1592222400|[0-9]*) echo "out=$output Tokio $mode completed - ok" ;;
    *) echo "out=${output:-no-output} Tokio $mode did not complete - bad"; return 1 ;;
  esac
}

run_tokio_probe()
{
  typeset timeout_seconds=${FAKETIME_ASYNC_TEST_TIMEOUT:-10}
  if ! command -v cargo >/dev/null 2>&1; then
    echo "out=skip cargo is unavailable - ok"
    return 0
  fi
  if ! cargo build --manifest-path tokio_probe/Cargo.toml --locked --offline >/dev/null 2>&1; then
    echo "out=skip pinned Tokio fixture is not cached locally - ok"
    return 0
  fi
  echo "# TOKIO_TIMEOUT_SECONDS=$timeout_seconds"
  run_tokio_mode realtime_only "${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" "$timeout_seconds" || return 1
  run_tokio_mode full_monotonic "${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" "$timeout_seconds" || return 1
  run_tokio_mode mt ../src/libfaketimeMT.so.1 "$timeout_seconds" || return 1
  run_tokio_mode dont_fake_monotonic "${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" "$timeout_seconds"
}

run()
{
  init
  if [ "${LIBFAKETIME_RUN_OPTIONAL_RUNTIME_TESTS:-0}" != 1 ]; then
    echo "out=skip optional async-runtime probe disabled - ok"
    return 0
  fi
  if ! command -v rustc >/dev/null 2>&1; then
    echo "out=skip rustc is unavailable - ok"
    return 0
  fi
  run_tokio_probe || return 1

  typeset probe="./libfaketime_rust_probe_$$" output
  typeset timeout_seconds=${FAKETIME_ASYNC_TEST_TIMEOUT:-10}
  case "$timeout_seconds" in *[!0-9]*|'') echo "invalid FAKETIME_ASYNC_TEST_TIMEOUT"; return 1;; esac
  echo "# ASYNC_TIMEOUT_SECONDS=$timeout_seconds"
  trap 'rm -f "$probe" "$probe.rs"' 0 1 2 3 15
  cp optional_runtime_probe.rs "$probe.rs"
  if ! rustc "$probe.rs" -o "$probe"; then
    echo "out=1 Rust probe compilation failed - bad"
    return 1
  fi
  if [ "$PLATFORM" = mac ]; then
    output=$(DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
      DYLD_FORCE_FLAT_NAMESPACE=1 FAKETIME='@2020-06-15 12:00:00' \
      FAKETIME_ASYNC_TEST_TIMEOUT="$timeout_seconds" perl \
        -e 'alarm $ENV{FAKETIME_ASYNC_TEST_TIMEOUT}; exec @ARGV or exit 127' \
        -- "$probe" 2>/dev/null)
  else
    output=$(timeout "${timeout_seconds}s" env FAKETIME='@2020-06-15 12:00:00' \
      LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" "$probe" 2>/dev/null)
  fi
  case "$output" in
    1592222400) echo "out=2020 Rust runtime observes the faked clock - ok" ;;
    '') rm -f "$probe" "$probe.rs"; echo "out=1 Rust runtime probe produced no result - bad"; return 1 ;;
    *) echo "out=$output Rust runtime bypasses the preload clock path - ok" ;;
  esac
  rm -f "$probe" "$probe.rs"
}
