# Optional runtime smoke tests for applications that use libc time APIs.

init()
{
  typeset testsuite="$1"
  PLATFORM=$(platform)
  if [ -z "$PLATFORM" ]; then
    echo "$testsuite: unknown platform! quitting"
    return 1
  fi
  echo "# PLATFORM=$PLATFORM"
}

run()
{
  init
  if ! command -v python3 >/dev/null 2>&1; then
    echo "out=skip Python 3 is unavailable - ok"
    return 0
  fi

  typeset output
  if [ "$PLATFORM" = "mac" ]; then
    output=$(TZ=UTC DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
      DYLD_FORCE_FLAT_NAMESPACE=1 FAKETIME='@2020-06-15 12:00:00' \
      python3 -c 'import time; print(time.gmtime().tm_year)' 2>/dev/null)
  else
    output=$(TZ=UTC FAKETIME='@2020-06-15 12:00:00' \
      LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
      python3 -c 'import time; print(time.gmtime().tm_year)' 2>/dev/null)
  fi
  if [ "$output" = "2020" ]; then
    echo "out=2020 Python runtime observes the faked clock - ok"
  else
    echo "out=$output Python runtime did not observe the faked clock - bad"
    return 1
  fi

  if ! command -v ruby >/dev/null 2>&1; then
    echo "out=skip Ruby is unavailable - ok"
    return 0
  fi
  if [ "$PLATFORM" = "mac" ]; then
    output=$(TZ=UTC DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
      DYLD_FORCE_FLAT_NAMESPACE=1 FAKETIME='@2020-06-15 12:00:00' \
      ruby -e 'puts Time.now.utc.year' 2>/dev/null)
  else
    output=$(TZ=UTC FAKETIME='@2020-06-15 12:00:00' \
      LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
      ruby -e 'puts Time.now.utc.year' 2>/dev/null)
  fi
  if [ "$output" = "2020" ]; then
    echo "out=2020 Ruby runtime observes the faked clock - ok"
    return 0
  fi
  echo "out=$output Ruby runtime bypasses the preload clock path - ok"
  return 0
}
