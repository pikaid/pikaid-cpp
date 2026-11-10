# Changelog

All notable changes to this library are documented here. The pikaid
**identifier format** is immutable and unversioned; these versions describe the
C++ library only.

The format is `<major>.<minor>.<patch>` (semantic versioning). The CMake
package is exported with `SameMajorVersion` compatibility and the shared
library uses `SOVERSION 1`.

## [1.0.0] — unreleased

First public version of the official C++ implementation of pikaid
specification revision 1.0.4 (pinned commit
`17014d76f568c3ac9f9ce2927c1a018fdbc36d1d`).

### Added

- C++17 library implementing the canonical 26-character lowercase Base36 text
  form, the 17-byte binary form (5-byte big-endian Unix seconds plus 12
  randomness bytes) and lossless conversion between them.
- Strict validation with separate shape (`is_well_formed`) and semantic
  (`is_valid`) checks, and a stable `pikaid::error_code` set for invalid
  length, invalid character, timestamp overflow, randomness overflow and
  CSPRNG failure.
- Operating-system CSPRNG backends: `BCryptGenRandom` on Windows,
  `getrandom(2)` on Linux, `getentropy(2)` on Apple/BSD platforms, and a
  `/dev/urandom` POSIX fallback. Entropy failures fail closed; no
  `std::random_device` fallback and no partial reads.
- CMake build, install and export of the `pikaid::pikaid` target, a
  `SameMajorVersion` package version file, and CPack source and binary
  archives that exclude the local-only `.ai/` workspace.
- Conformance suite driven by the official pikaid-php v1.2.0 vector fixture
  (pinned by SHA-256), plus independent known-answer, boundary, overflow,
  malformed-input, round-trip, property and CSPRNG-failure tests.
- CI matrix: GCC and Clang on Linux, AppleClang on macOS and MSVC on Windows
  in Debug and Release, ASan+UBSan, an enforced 90% line-coverage floor, and a
  packaging/consumer smoke job.

### Changed

- `PROJECT_VERSION` is `1.0.0` (was the in-development value `0.1.0`).
  Nothing was ever published from `0.1.0`: no tag, archive, release or
  registry entry existed, so this is the first public version rather than a
  breaking change.
- The overbroad `/.??` ignore rule was replaced by an explicit `/.ai/` rule so
  legitimate dotfiles such as `.gitattributes` and `.github/` are no longer
  masked.

### Notes

- The `.ai/` directory is a local-only workspace. It is not tracked, not
  packaged and not published; `git ls-files -- .ai` is empty and the CPack
  source ignore list rejects it explicitly.
- No release tag or registry publication is claimed by this version. The
  repository is private and namespace, signing and registry credentials have
  not been verified by the owner.

[1.0.0]: https://github.com/pikaid/pikaid-cpp/compare/49f2dc3...HEAD
