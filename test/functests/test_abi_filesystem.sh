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
  time64_lib="../src/libfaketime.so.1"
  if grep -q '^libc=glibc$' .abi_info_test.$$ && \
     grep -q '^time_t_bits=32$' .abi_info_test.$$; then
    time64_lib="../src/libfaketime-time64.so.1"
  fi
  rm -f .abi_info_test.$$

  if ./shm_layout_test >/dev/null; then
    echo "out=1 shared-memory ABI layout is valid - ok"
  else
    echo "out=0 shared-memory ABI layout is valid - bad"
    return 1
  fi

  if [ "$PLATFORM" = "linuxlike" ]; then
    if ./time64_contract_test; then
      echo "out=1 Linux time64 contract completed - ok"
    else
      echo "out=0 Linux time64 contract completed - bad"
      return 1
    fi
    if FAKETIME='@2040-01-01 00:00:00' FAKETIME_EXPECT_POST2033=1 \
      LD_PRELOAD="$time64_lib" \
      ./time64_contract_test >/dev/null; then
      echo "out=1 post-2033 time64 contract completed - ok"
    else
      echo "out=0 post-2033 time64 contract completed - bad"
      return 1
    fi
  else
    echo "out=skip Linux time64 ABI execution is unavailable on macOS - ok"
  fi
}
