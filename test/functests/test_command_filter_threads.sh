# Command filtering must apply to threads created after libfaketime initializes.

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

run_filter_test()
{
  if [ "$PLATFORM" = "mac" ]; then
    env TZ=UTC DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
      DYLD_FORCE_FLAT_NAMESPACE=1 "$@" ./command_filter_thread_test
  else
    env TZ=UTC LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
      "$@" ./command_filter_thread_test
  fi
}

run_case()
{
  typeset description="$1"
  shift
  if run_filter_test "$@"; then
    echo "out=1 $description - ok"
  else
    echo "out=0 $description - bad"
    return 1
  fi
}

run()
{
  init
  run_case "ONLY_CMDS exclusion applies to all threads with absolute time" \
    THREAD_EXPECT_FAKE=0 THREAD_CHECK_STAT=1 FAKETIME='@2000-01-01 00:00:00' \
    FAKETIME_ONLY_CMDS=not-command_filter_thread_test
  run_case "ONLY_CMDS exclusion applies to all threads with relative time" \
    THREAD_EXPECT_FAKE=0 THREAD_CHECK_STAT=1 FAKETIME='-1000d' \
    FAKETIME_ONLY_CMDS=not-command_filter_thread_test
  run_case "SKIP_CMDS exclusion applies to all threads" \
    THREAD_EXPECT_FAKE=0 THREAD_CHECK_STAT=1 FAKETIME='@2000-01-01 00:00:00' \
    FAKETIME_SKIP_CMDS=command_filter_thread_test
  run_case "ONLY_CMDS inclusion still fakes all threads" \
    THREAD_EXPECT_FAKE=1 FAKETIME='@2000-01-01 00:00:00' \
    FAKETIME_ONLY_CMDS=command_filter_thread_test
}
