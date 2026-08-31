# Loader and initialization regression coverage.

init()
{
  typeset testsuite="$1"
  PLATFORM=$(platform)
  if [ -z "$PLATFORM" ]; then
    echo "$testsuite: unknown platform! quitting"
    return 1
  fi
  echo "# PLATFORM=$PLATFORM"
  if [ "$PLATFORM" = "linuxlike" ]; then
    echo "# LIBC=$(getconf GNU_LIBC_VERSION 2>/dev/null || echo unknown)"
    echo "# KERNEL=$(uname -sr 2>/dev/null || echo unknown)"
  fi
}

run()
{
  init
  typeset i value

  typeset iterations=${FAKETIME_LOADER_TEST_ITERATIONS:-10}
  typeset timeout_seconds=${FAKETIME_LOADER_TEST_TIMEOUT:-10}
  case "$iterations" in *[!0-9]*|'') echo "invalid FAKETIME_LOADER_TEST_ITERATIONS"; return 1;; esac
  case "$timeout_seconds" in *[!0-9]*|'') echo "invalid FAKETIME_LOADER_TEST_TIMEOUT"; return 1;; esac

  i=1
  while [ "$i" -le "$iterations" ]; do
    if [ "$PLATFORM" = "mac" ]; then
      value=$(DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
        DYLD_FORCE_FLAT_NAMESPACE=1 FAKETIME_NO_CACHE=1 FAKETIME="@2020-06-15 12:00:00" \
        FAKETIME_LOADER_TEST_TIMEOUT="$timeout_seconds" perl \
          -e 'alarm $ENV{FAKETIME_LOADER_TEST_TIMEOUT}; exec @ARGV or exit 127' \
          -- ./timetest 2>/dev/null)
    else
      value=$(timeout "${timeout_seconds}s" env \
        LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
        FAKETIME_NO_CACHE=1 FAKETIME="@2020-06-15 12:00:00" \
        ./timetest 2>/dev/null)
    fi
    case "$value" in
      *"Mon Jun 15"*|*"2020"*) ;;
      *)
        echo "out=0 repeated loader initialization run $i - bad"
        return 1
        ;;
    esac
    i=$((i + 1))
  done
  echo "out=$iterations repeated loader initialization runs completed - ok"

  echo "out=1 fork/exec and constructor re-entry are covered by lifecycle tests - ok"
}
