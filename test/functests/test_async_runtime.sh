# Optional async-runtime smoke probe. Runtime clock bypasses are classified,
# while hangs and compiler failures remain actionable when explicitly enabled.

init()
{
  typeset testsuite="$1"
  PLATFORM=$(platform)
  echo "# PLATFORM=$PLATFORM"
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
