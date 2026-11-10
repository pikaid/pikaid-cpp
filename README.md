<p align="center">
  <a href="https://github.com/pikaid/pikaid-specs">
    <img src="https://raw.githubusercontent.com/pikaid/pikaid-specs/v1.0.4/logo.png" alt="pikaid" width="300">
  </a>
</p>

<p align="center">
  <strong>Small · Sortable · Secure ID generator</strong>
</p>

<div align="center">

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/language-C%2B%2B-00599C.svg)](https://github.com/pikaid/pikaid-cpp)
[![CI](https://github.com/pikaid/pikaid-cpp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/pikaid/pikaid-cpp/actions/workflows/ci.yml)
[![Official implementation](https://img.shields.io/badge/official-implementation-2e7d32.svg)](https://github.com/pikaid/pikaid-specs)
[![Specification 1.0.4](https://img.shields.io/badge/spec-1.0.4-4b56a6.svg)](https://github.com/pikaid/pikaid-specs/tree/v1.0.4)

</div>

**pikaid** is the **official C++ implementation** of a compact, chronologically sortable, collision-resistant identifier format for databases, APIs, and distributed systems.

This is the immutable, unversioned [pikaid format](https://github.com/pikaid/pikaid-specs/tree/v1.0.4), documented in specification revision **1.0.4**.

## ✨ Why pikaid?

- **Short:** Compact, readable IDs for URLs, logs, and API resources.
- **Sortable:** Lexicographically ordered by timestamp, with **one-second precision**.
- **Collision-resistant:** 96 bits of cryptographic randomness make collisions extremely unlikely, never impossible.
- **Storage-friendly:** Use `BINARY(17)` or readable `CHAR(26)`.
- **Distributed-friendly:** No central sequence generator or coordination required.

## 🧬 Identifier format

A pikaid contains **26 lowercase Base36 characters**: a **7-character Unix-seconds timestamp** followed by **19 characters encoding 96 bits of cryptographic randomness**. Its binary form is **17 bytes**: 5 unsigned big-endian timestamp bytes followed by 12 randomness bytes.

![Identifier structure](https://raw.githubusercontent.com/pikaid/pikaid-specs/v1.0.4/structure.svg)

> **Ordering:** Different seconds sort chronologically. Within the same second, randomness—not issuance order—decides the order.

> **Security:** These are identifiers, **not secrets**, session credentials, authentication tokens, or API keys.

## ⚙️ Requirements

- A C++17 compiler. The GitHub Actions matrix builds and tests GCC and Clang on
  Linux, AppleClang on macOS, and MSVC on Windows; see the Actions tab for the
  current status. Other conforming C++17 compilers work where one of the
  supported CSPRNG backends is available.
- CMake 3.16 or newer.
- Ninja is optional; any CMake generator works.
- No external runtime dependencies. On Windows the library links the system
  `bcrypt` library.

## 📦 Installation

From a checkout of the public source repository:

```sh
git clone https://github.com/pikaid/pikaid-cpp.git
cd pikaid-cpp
```

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

## 🚀 Basic Usage

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

## 💡 Example Use Case

Use pikaid to identify **users, orders, invoices, uploads, events, and API resources**. Independent services can generate compact IDs without a shared counter, while timestamp-based ordering helps with indexing, sorting, and logs.

## 📚 Canonical specification

- [Canonical specification](https://github.com/pikaid/pikaid-specs)
- [Revision 1.0.4](https://github.com/pikaid/pikaid-specs/tree/v1.0.4)

Specification revisions describe the format; they do not change the encoded identifiers.

## 🧩 API Reference

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

## 🔄 Order Guarantee

Text and 17-byte binary values preserve the canonical timestamp-then-randomness ordering. Same-second values are **not** ordered by issuance.

## 🛢 Storage Recommendations

| Representation | Size | Recommended for |
| --- | ---: | --- |
| **BINARY(17)** | 17 bytes | Compact primary keys and indexes |
| **CHAR(26)** | 26 characters | Readable SQL, debugging, URLs, and logs |

Use an **ASCII bytewise/binary collation** for text columns (for example, `ascii_bin` on MariaDB/MySQL). Case-insensitive or locale-aware sorting is not a substitute for canonical bytewise comparison.

## ✅ Verification and release status

The [CI workflow](https://github.com/pikaid/pikaid-cpp/actions/workflows/ci.yml) runs native tests, pinned automated conformance checks, and language-specific quality gates. To run the native checks locally from a checkout: `ctest --test-dir build --output-on-failure`.

**Automated passing tests are not a security certification.** The independent CSPRNG source and failure-path audits (`P-GENERATE-RNG-SOURCE` and `P-GENERATE-RNG-FAILURE`) still require separate human evidence.

<details>
<summary>Platform, tooling, and conformance details</summary>

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

</details>

Registry publication is not implied by a green CI: use the installation method documented above.

## 🤝 Contributing

Contributions are always welcome! 💙

Whether you're fixing a typo, improving documentation, adding tests, or making pikaid even better, every contribution is appreciated.

**Want to help? Here's how:**

- 🐛 **Found a bug?** [Open an issue](https://github.com/pikaid/pikaid-cpp/issues) with clear reproduction steps.
- 💡 **Have an idea?** Start with an issue before making major changes.
- 🛠️ **Ready to contribute?** Open a pull request explaining your changes.
- 🧪 **Keep it tested.** Add meaningful regression tests for behavior changes and follow the existing conventions.
- ✅ **Make sure CI passes.** All required checks must be green before a pull request is merged. No shortcuts!

**One important rule:** pikaid's identifier format is immutable. Contributions must preserve the [canonical specification (revision 1.0.4)](https://github.com/pikaid/pikaid-specs/tree/v1.0.4).

🔐 **Found a security vulnerability?** Report it privately using GitHub's security reporting (when available), or [contact the maintainer](mailto:larananas.dev@proton.me)—not through a public issue.

**Every improvement matters, big or small. Thanks for helping make pikaid better!** 🐇

## 📄 License

The implementation is available under the [MIT License](LICENSE).

## ✉️ Contact

For licensing or project-related questions: [larananas.dev@proton.me](mailto:larananas.dev@proton.me).

