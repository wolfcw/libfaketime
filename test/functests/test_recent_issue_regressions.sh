# Consolidated regression gate for the recent safety reports #549--#552.

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
  if [ "$PLATFORM" = "mac" ]; then
    echo "out=skip Linux-specific recent issue probes are covered by the Linux CI gate - ok"
    return 0
  fi

  if [ ! -x ./fd_leak_test ]; then
    echo "out=0 descriptor regression helper was built - bad"
    return 1
  fi
  if FAKETIME=+0 LD_PRELOAD=../src/libfaketime.so.1 ./fd_leak_test; then
    echo "out=1 #552 shared-memory descriptors remain bounded - ok"
  else
    echo "out=0 #552 shared-memory descriptors remain bounded - bad"
    return 1
  fi

  if timeout 5s env FAKETIME='@2005-03-29 14:14:14' \
      FAKETIME_SAVE_FILE=/dev/full LD_PRELOAD=../src/libfaketime.so.1 date >/dev/null 2>&1; then
    echo "out=1 #550 save failure returns without hanging - ok"
  else
    status=$?
    if [ "$status" -eq 124 ]; then
      echo "out=124 #550 save failure timed out - bad"
      return 1
    fi
    echo "out=1 #550 save failure returns without hanging - ok"
  fi

  echo "out=1 #549 and #551 parser/wrapper failure paths covered by dedicated suites - ok"
}
