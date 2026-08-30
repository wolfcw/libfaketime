# ABI and filesystem contract inventory checks.

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
  if ! ./abi_info_test > .abi_info_test.$$; then
    echo "out=0 ABI information probe completed - bad"
    rm -f .abi_info_test.$$
    return 1
  fi
  if grep -q 'pointer_bits=' .abi_info_test.$$ &&
     grep -q 'time_t_bits=' .abi_info_test.$$; then
    echo "out=1 ABI information probe completed - ok"
  else
    echo "out=0 ABI information probe completed - bad"
    rm -f .abi_info_test.$$
    return 1
  fi
  rm -f .abi_info_test.$$

  if [ "$PLATFORM" = "linuxlike" ]; then
    if ./time64_contract_test; then
      echo "out=1 Linux time64 contract completed - ok"
    else
      echo "out=0 Linux time64 contract completed - bad"
      return 1
    fi
  else
    echo "out=skip Linux time64 ABI execution is unavailable on macOS - ok"
  fi
}
