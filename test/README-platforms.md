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
