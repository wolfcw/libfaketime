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
  check_timezone Europe/Berlin '2020-06-15 12:00:00 1' \
    'fixed timestamp preserves configured local conversion'
  check_timezone UTC '2020-06-15 12:00:00 0' \
    'fixed timestamp preserves UTC conversion'
  check_dst Europe/Berlin '@2020-01-15 12:00:00' 0 \
    'winter timestamp reports standard time'
  check_dst Europe/Berlin '@2020-07-15 12:00:00' 1 \
    'summer timestamp reports daylight time'
  check_dst UTC '@1970-01-01 00:00:00' 0 \
    'epoch timestamp remains representable in UTC'
}

check_dst()
{
  typeset zone="$1" timestamp="$2" expected="$3" description="$4" output
  if [ "$PLATFORM" = "mac" ]; then
    output=$(TZ="$zone" DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
      DYLD_FORCE_FLAT_NAMESPACE=1 FAKETIME="$timestamp" \
      ./timezone_contract_test 2>/dev/null)
  else
    output=$(TZ="$zone" FAKETIME="$timestamp" \
      LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
      ./timezone_contract_test 2>/dev/null)
  fi
  case "$output" in
    *" $expected")
      echo "out=$output $description - ok"
      ;;
    *)
      echo "out=$output $description - bad"
      return 1
      ;;
  esac
}

check_timezone()
{
  typeset zone="$1" expected="$2" description="$3" output
  if [ "$PLATFORM" = "mac" ]; then
    output=$(TZ="$zone" DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
      DYLD_FORCE_FLAT_NAMESPACE=1 FAKETIME='@2020-06-15 12:00:00' \
      ./timezone_contract_test 2>/dev/null)
  else
    output=$(TZ="$zone" FAKETIME='@2020-06-15 12:00:00' \
      LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
      ./timezone_contract_test 2>/dev/null)
  fi
  case "$output" in
    "$expected")
      echo "out=1 $description - ok"
      ;;
    *)
      echo "out=0 $description ($output) - bad"
      return 1
      ;;
  esac
}
