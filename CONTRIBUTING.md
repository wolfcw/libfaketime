# Contributing

## Compatibility checks

Every change affecting clock or wait behavior should be checked with the
native macOS suite and the Linux baseline suite. The baseline runner supports
glibc, musl, Fedora, Rocky Linux, Debian, Ubuntu, Arch Linux, and explicit
Docker architectures. Fedora-specific diagnostics are documented in
`test/README-fedora.md`.

Before committing, run:

```sh
make test
git diff --check
```

For Linux-only changes, also run the relevant Docker baseline or focused
contract test. Keep test commands bounded when investigating hangs and retain
the resulting logs.

## C style

The repository uses two-space indentation, Allman braces, and C99-compatible
constructs. `.clang-format` records the agreed formatting defaults. Apply
formatting only to the lines or files being changed; broad reformatting should
be a separate, reviewed change.

## Commit structure

Prefer small commits with one behavior change and its tests. Commit messages
should use an imperative subject, for example:

```text
Fix monotonic semaphore deadline conversion
```

Do not push compatibility experiments or generated build artifacts.
