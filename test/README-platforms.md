# Compatibility test matrix

The mandatory matrix keeps macOS and Linux on equal footing for the common
clock, configuration, process, and lifecycle contracts.

| Target | Required mode | Additional coverage |
| --- | --- | --- |
| macOS arm64/arm64e | native `make test` | dyld interpose, Mach clocks, universal library |
| Debian/Fedora/Rocky/Arch x86_64 | Docker baseline | glibc waits, timerfd, semaphores, filesystem hooks |
| Alpine | Docker baseline | musl feature guards and symbol availability |
| Debian ARM64 | Docker/QEMU baseline | ARM64 ABI and preload behavior |
| Debian 32-bit | Docker/QEMU baseline | time32/time64 contract and symbol versions |
| Fedora x86_64 | sanitizer image | ASan/UBSan, sanitizer-first preload ordering |
| Linux x86_64 | Valgrind runner | resource ownership and leak checks |

CentOS 7/RHEL-compatible, language-runtime, GUI, and Proton/Wine checks are
optional integration investigations. They must be time-bounded and include a
minimal reproducer before becoming release gates.

## Runtime integration policy

Every supported build must pass the native functional suite with the preload
library loaded into a short-lived dynamically linked process. The suite covers
constructor re-entry, fork/exec inheritance, repeated loader initialization,
and the command-line `faketime` launcher. These checks are intentionally kept
separate from compiler or libc feature probes: a successful link does not prove
that the dynamic loader can safely initialize the interposer.

When adding a runtime-specific regression, first add a minimal C or shell
reproducer under `test/` and run it on both the native macOS job and at least
one glibc and one musl Linux job. Keep launch timeouts bounded and avoid
asserting distribution-specific diagnostic text. Runtime integrations that
need an interpreter, GUI, or proprietary loader remain informational until a
portable CI fixture is available.

## Optional runtime integrations

For local compatibility investigations, use the same fixed-time probe with
each runtime rather than comparing human-readable diagnostics:

```sh
FAKETIME='@2020-06-15 12:00:00' LD_PRELOAD=../src/libfaketime.so.1 \
  python3 -c 'import time; print(int(time.time()))'
```

Repeat the probe for Python, Ruby, Perl, Java, and Go where installed. Static
executables, setuid programs, GUI launchers, Wine/Proton, and seccomp-constrained
programs remain informational unless a supported loader path and deterministic
CI fixture are available.

## CI timing demonstrations

The Docker baseline invokes the functional suite with `TEST_DEMO=0`. This keeps
the release gate bounded while retaining the longer `test.sh` and `test_OSX.sh`
demonstrations for local runs via the default `TEST_DEMO=1` setting.

When diagnosing a timeout, record the final `Test Suites summary` first and
investigate the optional demonstrations separately.

For legacy loader investigations, increase or reduce the bounded repetition
without changing the production library:

```sh
FAKETIME_LOADER_TEST_ITERATIONS=25 FAKETIME_LOADER_TEST_TIMEOUT=10 make TEST_DEMO=0 test
```

The loader suite reports these values together with libc and kernel details so
old-glibc failures can be compared with current distributions.

## Release checklist

Before treating a change as release-ready, verify:

1. macOS arm64/arm64e builds and passes the full functional suite.
2. Current glibc x86_64, Debian i386/time64, ARM64, and Alpine/musl pass the
   functional suite with bounded completion.
3. Fedora sanitizer and Valgrind checks complete without resource errors.
4. ABI probes identify the selected libc and time-width variant.
5. Optional runtime probes are reported separately from mandatory gates.
6. Parser boundary seeds under `test/fuzz/` have been exercised with bounded
   sanitizer or fuzzing runs, and no generated artifacts are left in the tree.
