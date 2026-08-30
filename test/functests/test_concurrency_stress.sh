# Concurrent clock calls must remain independent across processes.

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
  typeset iteration pid status
  typeset pids=""
  for iteration in 1 2 3 4 5 6 7 8; do
    if [ "$PLATFORM" = "mac" ]; then
      env FAKETIME='+0' DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
        DYLD_FORCE_FLAT_NAMESPACE=1 ./clock_consistency_test >/dev/null 2>&1 &
    else
      env FAKETIME='+0' LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
        ./clock_consistency_test >/dev/null 2>&1 &
    fi
    pids="$pids $!"
  done
  status=0
  for pid in $pids; do
    if ! wait "$pid"; then
      status=1
    fi
  done
  if [ "$status" -ne 0 ]; then
    echo "out=$status concurrent clock stress runs completed - bad"
    return 1
  fi
  echo "out=1 concurrent clock stress runs completed - ok"
}
