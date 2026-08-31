# Resource ownership and non-progressing I/O regression checks.

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
  if [ "$PLATFORM" = "linuxlike" ]; then
    typeset iteration
    for iteration in 1 2 3; do
      if FAKETIME=+0 LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
        ./fd_leak_test; then
        :
      else
        status=$?
        echo "out=$status repeated shared-memory clock calls do not leak descriptors (run $iteration) - bad"
        return 1
      fi
    done
    echo "out=1 repeated shared-memory clock calls do not leak descriptors - ok"
  else
    echo "out=skip /proc fd accounting is Linux-only - ok"
  fi

  if FAKETIME=+0 LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
    ./fd_liveness_test; then
    echo "out=1 sentinel descriptor remains valid during clock calls - ok"
  else
    status=$?
    echo "out=$status sentinel descriptor remains valid during clock calls - bad"
    return 1
  fi

  if FAKETIME=+0 LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
    ./emfile_clock_test; then
    echo "out=1 clock calls remain safe under EMFILE - ok"
  else
    status=$?
    if [ "$status" -eq 77 ]; then
      echo "out=skip RLIMIT_NOFILE is unavailable - ok"
    else
      echo "out=$status clock calls remain safe under EMFILE - bad"
      return 1
    fi
  fi
}
