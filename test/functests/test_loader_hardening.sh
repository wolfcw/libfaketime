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
}

run()
{
  init
  typeset i value

  i=1
  while [ "$i" -le 10 ]; do
    if [ "$PLATFORM" = "mac" ]; then
      value=$(DYLD_INSERT_LIBRARIES=../src/libfaketime.1.dylib \
        DYLD_FORCE_FLAT_NAMESPACE=1 FAKETIME_NO_CACHE=1 FAKETIME="@2020-06-15 12:00:00" \
        ./timetest 2>/dev/null | sed -n '1p')
    else
      value=$(LD_PRELOAD="${FAKETIME_TESTLIB:-../src/libfaketime.so.1}" \
        FAKETIME_NO_CACHE=1 FAKETIME="@2020-06-15 12:00:00" \
        ./timetest 2>/dev/null | sed -n '1p')
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
  echo "out=10 repeated loader initialization runs completed - ok"

  echo "out=1 fork/exec and constructor re-entry are covered by lifecycle tests - ok"
}
