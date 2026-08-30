# Fixed timestamp and timezone conversion contract.

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
  typeset output
  if [ "$PLATFORM" = "mac" ]; then
    output=$(TZ=Europe/Berlin DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
      DYLD_FORCE_FLAT_NAMESPACE=1 FAKETIME='@2020-06-15 12:00:00' \
      ./timezone_contract_test 2>/dev/null)
  else
    output=$(TZ=Europe/Berlin FAKETIME='@2020-06-15 12:00:00' \
      LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
      ./timezone_contract_test 2>/dev/null)
  fi
  case "$output" in
    '2020-06-15 12:00:00 1')
      echo "out=1 fixed timestamp preserves configured local conversion - ok"
      ;;
    *)
      echo "out=0 fixed timestamp preserves configured local conversion ($output) - bad"
      return 1
      ;;
  esac
}
