<p align="center">
  <img src="https://raw.githubusercontent.com/pikaid/pikaid-specs/v1.0.4/logo.png" alt="pikaid" width="300" />
</p>

<p align="center">
  <strong>Small · Sortable · Secure ID generator</strong>
</p>

# pikaid — Official C++ Implementation

This is the official C++ implementation of [pikaid](https://github.com/pikaid), a
compact, chronologically sortable identifier format for databases, APIs, and
distributed systems. The pikaid identifier format is immutable and unversioned:
**26 lowercase Base36 characters** with a **17-byte binary representation**.

**Status: v1.0.0 source, engineering-complete — no tagged release or registry
package yet, and release is not authorized.** The C++17 source, CMake package
export and test suite are complete and validated by the local checks below.
Release remains gated: the official suite's two manual security audits
(`P-GENERATE-RNG-SOURCE`, `P-GENERATE-RNG-FAILURE`) require a genuinely
independent human reviewer, so without accepted evidence the suite's default
exit is `3` (`AUTOMATED_PASS_AUDIT_REQUIRED`). See
[docs/CSPRNG_REVIEW_PACKET.md](docs/CSPRNG_REVIEW_PACKET.md). The project is
not published to Conan, vcpkg, Homebrew or any other registry, so
`find_package(pikaid)` requires a local install from this repository. See
[CHANGELOG.md](CHANGELOG.md).

The authoritative definition is the
[pikaid specification revision 1.0.4](https://github.com/pikaid/pikaid-specs/tree/v1.0.4)
(pinned commit `17014d76f568c3ac9f9ce2927c1a018fdbc36d1d`).

## Format

| Property | Value |
| --- | --- |
| Text length | exactly 26 characters |
| Alphabet | `0123456789abcdefghijklmnopqrstuvwxyz` (lowercase Base36, ASCII only) |
| Timestamp | 7 Base36 characters, Unix seconds, `0..78,364,164,095` |
| Randomness | 19 Base36 characters carrying exactly 96 bits |
| Binary length | exactly 17 bytes |
| Binary layout | 5-byte unsigned big-endian timestamp, then 12 randomness bytes |

The timestamp occupies the first 7 characters, so identifiers with different
timestamps sort chronologically in both text and unsigned binary order. Within
one second, ordering is determined by the randomness suffix, not by generation
order; pikaid is **not monotonic**.

## Requirements

- A C++17 compiler. The GitHub Actions matrix builds and tests GCC and Clang on
  Linux, AppleClang on macOS, and MSVC on Windows; see the Actions tab for the
  current status. Other conforming C++17 compilers work where one of the
  supported CSPRNG backends is available.
- CMake 3.16 or newer.
- Ninja is optional; any CMake generator works.
- No external runtime dependencies. On Windows the library links the system
  `bcrypt` library.

## Build, test, and install

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The example is built by default and can be run directly:

```sh
./build/pikaid_example
```

Example output (values differ on every run):

```text
id:         0to3mjy1v5pu5yjpvydwsdxfpe
timestamp:  1793999086
randomness: 3e2c70cb0c50b22784c82ac2
valid:      yes
round trip: ok
```

Install and consume from another CMake project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /some/prefix
```

```cmake
find_package(pikaid 1.0 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE pikaid::pikaid)
```

The package follows semantic versioning: the installed
`pikaidConfigVersion.cmake` uses `SameMajorVersion` compatibility, and the
shared library sets `SOVERSION 1`.

Source and binary archives can be produced with CPack; both exclude the
local-only `.ai/` workspace, VCS metadata and build trees:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target package        # binary archives
cmake --build build --target package_source # source archive
```

Build options:

| Option | Default | Effect |
| --- | --- | --- |
| `PIKAID_BUILD_TESTS` | `ON` when top level | Build the test suite and register CTest tests |
| `PIKAID_BUILD_EXAMPLES` | `ON` when top level | Build `pikaid_example` |
| `PIKAID_WARNINGS_AS_ERRORS` | `OFF` | Add `-Werror` or `/WX` |
| `PIKAID_SANITIZE` | `OFF` | Add AddressSanitizer and UndefinedBehaviorSanitizer (not MSVC) |
| `PIKAID_COVERAGE` | `OFF` | Add Clang or GCC coverage instrumentation |

## Usage

```cpp
#include <pikaid/pikaid.hpp>

#include <iostream>
#include <string>

int main()
{
    const std::string id = pikaid::generate();          // current Unix time
    const std::string fixed = pikaid::generate(1700000000);

    if (!pikaid::is_valid(id)) {
        return 1;
    }

    const std::uint64_t seconds = pikaid::timestamp(id);
    const pikaid::randomness_bytes entropy = pikaid::randomness(id);
    const pikaid::binary bytes = pikaid::to_binary(id);

    std::cout << id << '\n'
              << fixed << '\n'
              << pikaid::from_binary(bytes) << '\n'
              << seconds << ' ' << pikaid::randomness_hex(id) << '\n';

    return pikaid::from_binary(bytes) == id ? 0 : 1;
}
```

### Public API

| Function | Purpose |
| --- | --- |
| `secure_randomness()` | Read 12 bytes from the operating system CSPRNG |
| `generate()` | Generate text using the current Unix time |
| `generate(timestamp)` | Generate text using an explicit timestamp |
| `generate(timestamp, source)` | Deterministic generation for tests and migrations |
| `generate_binary()`, `generate_binary(timestamp[, source])` | Binary equivalents |
| `is_well_formed(id)` | Shape check only: 26 characters from `[0-9a-z]` |
| `is_valid(id)` | Full check: shape plus a randomness value within 96 bits |
| `validate_timestamp(timestamp)` | Validate a timestamp for generation |
| `parse(id)` | Return `{timestamp, randomness}` or throw `pikaid::error` |
| `timestamp(id)`, `randomness(id)`, `randomness_hex(id)` | Typed accessors |
| `to_binary(id)`, `from_binary(binary)` | Lossless conversion |
| `compare(a, b)`, `compare_binary(a, b)` | `-1`, `0`, or `1` |

`is_well_formed` and `is_valid` are deliberately different. A 19-character
Base36 suffix can encode values above `2^96 - 1`; `is_well_formed` only checks
the shape, while `is_valid` also rejects those non-canonical values. Prefer
`is_valid` at trust boundaries, or call `parse`/`to_binary`, which throw
`pikaid::error` with a stable `error_code`.

```cpp
try {
    const pikaid::parts decoded = pikaid::parse(candidate);
    // ...
} catch (const pikaid::error& error) {
    switch (error.code()) {
    case pikaid::error_code::invalid_string_length: /* ... */ break;
    case pikaid::error_code::invalid_character:     /* ... */ break;
    case pikaid::error_code::randomness_overflow:   /* ... */ break;
    default:                                        /* ... */ break;
    }
}
```

`pikaid::error` derives from `std::invalid_argument`, so existing code that
catches `std::invalid_argument` or `std::exception` continues to work. Invalid
input is never trimmed, case-folded, truncated, or normalized.

## Security and correctness notes

- Randomness comes from the operating system CSPRNG (`BCryptGenRandom` on
  Windows, `getrandom(2)` on Linux, `getentropy(2)` on Apple/BSD platforms, and
  `/dev/urandom` as a POSIX fallback). The library never uses
  `std::random_device` alone, and fails closed with
  `error_code::randomness_failure` on CSPRNG errors or short reads.
- The 96-bit randomness field gives collision resistance up to the usual
  birthday bound; pikaid makes **no absolute uniqueness guarantee**. Check a
  database uniqueness constraint if duplicates are unacceptable.
- pikaid is an identifier format, not an authentication token, capability, or
  encrypted value. It is not secret: the timestamp is plaintext and the
  randomness is not a signature. Do not use it for authorization decisions.
- The timestamp comes from the system clock and is not trusted input. Clock
  skew or rollback affects ordering; pikaid is not monotonic within a second.
- The `randomness_source` overloads exist for deterministic tests and data
  migrations. Never supply predictable randomness in production.

An independent-review packet with the exact file/line map, entropy-provider
matrix, failure semantics and evidence template is in
[docs/CSPRNG_REVIEW_PACKET.md](docs/CSPRNG_REVIEW_PACKET.md). It records the two
outstanding manual audits; it is not an approval and does not certify entropy
security.

## Testing

The suite runs every official vector from the reference implementation and
adds independent known-answer cases for both conversion and deterministic
generation, malformed-input cases (including CRLF and zero-width Unicode),
overflow, boundary, property, cross-representation, generated-identifier, and
CSPRNG-failure cases. The official fixture is read at run time and pinned by
SHA-256; see `tests/vectors/README.md` for provenance.

Local results on macOS with AppleClang 21.0.0:

```text
45 test cases, 46550 checks, 0 failing checks
```

Line coverage of `src/pikaid.cpp` is 95.74% (95.97% of regions, 97.14% of
branches); the public header is fully covered. The 12 uncovered lines are
CSPRNG failure branches that cannot be triggered on a healthy macOS host, plus
one defensive `digit < 0` return that is unreachable after the shape check.
Sanitizer builds (ASan + UBSan) pass.

The GitHub Actions workflow builds and tests the same suite with warnings as
errors on Linux GCC, Linux Clang, macOS AppleClang, and Windows MSVC, runs
ASan+UBSan on Linux and macOS, and enforces a line-coverage floor of 90%. A
packaging job additionally proves that `.ai/` is untracked, that the CPack
source archive contains exactly the tracked file set, and that static and
shared binary packages install and are consumed by an out-of-tree CMake
project through `find_package(pikaid 1.0 CONFIG REQUIRED)`. The workflow and
its public run history are in the Actions tab; local results alone are not a
substitute for that cross-platform matrix.

### Package contents and stability

Local packaging checks (macOS, AppleClang 21.0.0, CMake 4.4.5) compared two
fresh static and shared TGZ builds:

- The CPack source archive contains exactly the `git ls-files` set and no
  `.ai/`, `.git/`, or `.DS_Store` entry.
- Both source archives have identical file lists and identical extracted
  content.
- Both static and shared packages have identical file lists, and the shared
  `libpikaid` bytes are identical across builds.
- The static `libpikaid.a` bytes differ only in archive-member timestamps; the
  extracted `pikaid.cpp.o` is byte-for-byte identical.

Archive SHA-256 values therefore differ between builds because CPack/tar and
`ar` record timestamps. Neither this repository nor the official specification
states a byte-for-byte reproducible-archive contract, so this project does not
claim reproducibility of archive files. The supported guarantees are the file
manifest above, identical source/object/binary content, and successful
`find_package(pikaid 1.0 CONFIG REQUIRED)` consumption of the extracted static
and shared packages.

### Official conformance status

This library is validated against the official cross-language conformance
suite (`pikaid/pikaid-specs`) pinned at commit
`059780cc0e5b113443812720d95c630649bf3aa3` (suite package revision `1.0.0`,
identifier specification revision `1.0.4`). `tools/conformance/` contains the
test-only JSON-Lines adapter that the suite drives; it is never installed or
shipped. Results are reported exactly as the suite emits them, never upgraded:

- `run_adapter.py --automated-only` currently exits `0` with status
  `AUTOMATED_PASS_AUDIT_NOT_PERFORMED`. Every automated vector and property
  passed, but the run deliberately did not perform the manual security audits,
  and its own output states `certification: NOT_A_CONFORMANCE_CERTIFICATION`.
- The default invocation (without `--automated-only`) currently exits `3` with
  status `AUTOMATED_PASS_AUDIT_REQUIRED`, because the two manual CSPRNG review
  obligations `P-GENERATE-RNG-SOURCE` and `P-GENERATE-RNG-FAILURE` remain
  `MANUAL_EVIDENCE_REQUIRED`. Completing them requires independent human review
  evidence; an automated run cannot claim them.

A green `--automated-only` run, and this README, are therefore not proof that
the manual security audits were completed or that conformance was certified.

## Versioning and releases

The identifier format is immutable and unversioned; version numbers describe
this library and the specification revision it implements, never the
identifiers it produces.

- This source tree declares library version `1.0.0` (CMake `PROJECT_VERSION`,
  `pikaidConfigVersion.cmake`, `SOVERSION 1`).
- `0.1.0` was an in-development value only: no tag, archive, release or
  registry entry was ever published from it, so `1.0.0` is the first public
  version rather than a compatibility break.
- Release prerequisites that live outside this repository are not satisfied
  yet, so no tag or publication is claimed here: the GitHub repository is
  private, and namespace ownership plus signing/registry credentials have not
  been verified by the owner.

## License

MIT. See [LICENSE](LICENSE).

Copyright (c) 2026 larananas (Lara G.).
