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
