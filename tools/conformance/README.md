# Official conformance adapter (test-only)

This directory contains the JSON-Lines bridge used by the official
[PikaID conformance suite](https://github.com/pikaid/pikaid-specs). It is a
**test tool**, not part of the library, the installed package or any binary
release.

The adapter delegates every PikaID operation to the public C++17 API declared
in `include/pikaid/pikaid.hpp`. It only translates the wire format (JSON and
lowercase hexadecimal) and never synthesizes identifiers, validates them or
produces randomness on its own.

## Protocol coverage

| Suite operation | Public C++ API |
| --- | --- |
| `generate_string` | `pikaid::generate()` |
| `text_to_binary` | `pikaid::to_binary()` |
| `binary_to_text` | `pikaid::from_binary(std::string_view)` |
| `boolean_validation` | `pikaid::is_valid()` |
| `format_only_validation` | `pikaid::is_well_formed()` |
| `parse_components` | `pikaid::parse()` |
| `generate_with_timestamp` | `pikaid::generate(std::int64_t)` |
| `generate_binary` | `pikaid::generate_binary()` |
| `generate_binary_with_timestamp` | `pikaid::generate_binary(std::int64_t)` |
| `public_compare` | `pikaid::compare()` |
| `csprng_failure_injection` | `pikaid::generate(ts, source)` and `pikaid::generate_binary(ts, source)` with a source that throws `error_code::randomness_failure`; the current-time operations have no public randomness seam, so the bridge fails them closed with `entropy_unavailable` while injection is active |

`parse_datetime` is not advertised. The public API exposes lossless integer
timestamps only; claiming the capability would require a UTC datetime for
every legal timestamp through year 4453.

## Build and run

The adapter is built against an **installed** `pikaid` package, which also
proves the exported CMake package is consumable:

```sh
cmake -S . -B build -DPIKAID_BUILD_TESTS=OFF -DPIKAID_BUILD_EXAMPLES=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/prefix"
cmake --build build
cmake --install build

cmake -S tools/conformance -B build-conformance \
  -DCMAKE_PREFIX_PATH="$PWD/build/prefix"
cmake --build build-conformance

python3 /path/to/pikaid-specs/conformance/tools/run_adapter.py \
  --automated-only -- "$PWD/build-conformance/pikaid_conformance_adapter"
```

## Interpreting the result

The suite distinguishes automated checks from the manual CSPRNG audits, and
this adapter does not blur that line:

- `--automated-only` exits `0` with status `AUTOMATED_PASS_AUDIT_NOT_PERFORMED`
  when every automated vector and property passes. That status is **not** a
  conformance certification.
- Without `--automated-only`, the same implementation exits `3` with status
  `AUTOMATED_PASS_AUDIT_REQUIRED` while `P-GENERATE-RNG-SOURCE` and
  `P-GENERATE-RNG-FAILURE` are outstanding. They are human review obligations
  and cannot be satisfied by an automated run.
- Both modes report `certification: NOT_A_CONFORMANCE_CERTIFICATION`.

Pin the suite to the official commit recorded in the top-level `README.md`
(`059780cc0e5b113443812720d95c630649bf3aa3`) rather than a mutable branch.
