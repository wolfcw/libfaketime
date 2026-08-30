# Fedora compatibility testing

The Fedora branch of `docker_baseline.sh` runs a bounded compatibility
preflight before the complete test suite. The preflight records the Fedora,
glibc, compiler, architecture, and kernel versions, verifies that the
versioned `sem_clockwait` wrapper is exported, and runs the semaphore contract
with per-case progress reporting.

Run the current Fedora image on an x86_64 Docker host with:

```sh
DOCKER_PLATFORM=linux/amd64 test/docker_baseline.sh fedora:latest
```

The phase timeout defaults to 120 seconds and can be shortened for diagnosis:

```sh
FAKETIME_TEST_PHASE_TIMEOUT=30 \
  DOCKER_PLATFORM=linux/amd64 test/docker_baseline.sh fedora:latest
```

The same command can be used with a pinned Fedora tag to compare glibc
versions. Use a matching `DOCKER_PLATFORM` for cross-architecture testing;
Docker must have the requested image architecture available.

A failure in `phase=package`, `phase=build`, `phase=semaphore`, or
`phase=full-suite` identifies the failing layer. The semaphore diagnostics are
enabled only by the harness, so normal test output remains unchanged.
