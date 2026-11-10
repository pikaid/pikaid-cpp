# CSPRNG Review Packet

**Status: OUTSTANDING — no independent review has been performed.**
This packet is reviewer-facing evidence. It is not a security approval, and it
must not be signed, copied into a security-evidence record, or cited as proof of
review until a genuinely independent human has inspected the sources at an exact
commit and recorded their own conclusion.

The official suite requires two separate manual checks, both currently
`MANUAL_EVIDENCE_REQUIRED`:

- `P-GENERATE-RNG-SOURCE` — every production generation path obtains 12 fresh
  bytes from a cryptographically secure system/runtime source, with no weak
  randomness fallback.
- `P-GENERATE-RNG-FAILURE` — entropy-source failure aborts generation without a
  predictable fallback, truncated randomness, or partial output.

The definitions and evidence schema live in the pinned suite at
`059780cc0e5b113443812720d95c630649bf3aa3`:
`conformance/docs/SECURITY_AND_RELEASE_GATES.md` and
`conformance/schemas/security-evidence.schema.json`.

## Review target

Review the exact commit that contains this file. Do not review floating `main`.

```sh
git rev-parse HEAD
git log -1 --format='%H %s' -- docs/CSPRNG_REVIEW_PACKET.md
shasum -a 256 src/pikaid.cpp include/pikaid/pikaid.hpp
```

Record the full 40-character commit SHA and the two file hashes in the review
record. The production implementation is `src/pikaid.cpp`; the reviewed public
contract is `include/pikaid/pikaid.hpp`. No generated code, codec stand-in, or
test-only adapter participates in production generation.

## Production generation paths

All current-time generation funnels through `secure_randomness()` exactly once.

| Path | Location | Entropy call |
| --- | --- | --- |
| `generate()` | `src/pikaid.cpp:297-300` | delegates to `generate(current_unix_seconds())` |
| `generate(timestamp)` | `src/pikaid.cpp:302-307` | one `secure_randomness()` call at line 306 |
| `generate(timestamp, source)` | `src/pikaid.cpp:309-318` | supplied source, invoked once at line 317; empty `std::function` is rejected at 312-315 |
| `generate_binary()` | `src/pikaid.cpp:320-323` | delegates to `generate_binary(current_unix_seconds())` |
| `generate_binary(timestamp)` | `src/pikaid.cpp:325-338` | one `secure_randomness()` call at line 335 |
| `generate_binary(timestamp, source)` | `src/pikaid.cpp:340-358` | supplied source, invoked once at line 355; empty source rejected at 343-346 |

`secure_randomness()` is declared at `include/pikaid/pikaid.hpp:119-126` and
implemented at `src/pikaid.cpp:236-280`. It returns
`std::array<std::uint8_t, 12>` (`randomness_bytes`, header lines 58-59).

The 12 bytes are converted to 19 lowercase Base36 characters by exact integer
base conversion, not by per-byte modulo (`encode_randomness`, lines 93-108).
Decoding rejects values above 96 bits rather than wrapping (lines 110-135).
There is no user-space PRNG, no seeding, no process-global state, and no
`std::random_device` reference anywhere in `src/` or `include/`.

## Entropy providers and selection

`secure_randomness()` selects a backend at compile time:

| Platform | Backend | Location | Failure handling |
| --- | --- | --- | --- |
| Windows | `BCryptGenRandom(..., BCRYPT_USE_SYSTEM_PREFERRED_RNG)` | lines 240-248 | non-zero `NTSTATUS` throws; `bcrypt` linked at `CMakeLists.txt:98-100` |
| Linux with `<sys/random.h>` | `getrandom(2)` | lines 249-264 | retries `EINTR`, throws on other errors and on a zero-byte return, loops for short reads |
| Apple/BSD with `<sys/random.h>` | `getentropy(2)` | lines 265-270 | non-zero return throws |
| Other Unix with `<sys/random.h>` | `/dev/urandom` | lines 271-272 | shared helper below |
| Other Unix/POSIX | `/dev/urandom` | lines 273-274 | shared helper below |
| Unsupported platform | compile-time `#error` | lines 275-276 | build fails rather than shipping a weak fallback |

The `PIKAID_USE_URANDOM` compile guard is at lines 36-46, and the shared
`/dev/urandom` helper is at lines 189-226. It opens with `O_CLOEXEC` where
available, loops until 12 bytes are read, retries `EINTR`, throws on read error
and on unexpected EOF, and closes the descriptor. A failed `close()` is also
treated as failure.

## Failure semantics

`srand`-style fallback, zero fill, timestamp-only output, and partial identifiers
do not exist on these paths.

- `throw_randomness_failure` (`src/pikaid.cpp:175-179`) throws
  `error(error_code::randomness_failure, ...)`.
- `throw_errno_failure` (lines 182-187) preserves `errno`, formats the OS error,
  and delegates to the same failure type.
- `error_code::randomness_failure` is stable value `7`
  (`include/pikaid/pikaid.hpp:79`; asserted by
  `tests/validation_test.cpp:270-278`).
- `pikaid::error` derives from `std::invalid_argument`; generation throws before
  returning any identifier. In `generate_binary(timestamp)` the timestamp bytes
  are written to a local object first, but the exception discards that object, so
  no caller-visible partial value is produced.
- A caller-supplied `randomness_source` that is empty throws at
  `src/pikaid.cpp:312-315` / `343-346`; a source that throws propagates that
  exception unchanged. There is no catch-and-continue anywhere on generation
  paths.
- On a platform with no supported provider the build fails at
  `src/pikaid.cpp:275-276`.

## Existing automated evidence

These checks support, but do not replace, human review:

- `tests/generation_test.cpp:140-155` calls `secure_randomness()` repeatedly and
  asserts fresh, non-zero, distinct 12-byte values.
- `tests/validation_test.cpp:261-268` asserts that empty randomness sources raise
  `randomness_failure` from both generation APIs.
- `tests/validation_test.cpp:270-278` pins the numeric error code.
- The official suite's `P-GENERATE-RNG-FAILURE` property drives fault injection
  through `tools/conformance/adapter.cpp:223-241` and 264-267. That injection
  simulates failure at the bridge boundary; it does not exercise a real OS
  entropy outage. It is test-only and never installed.
- Local sanitizer and native runs are recorded in the release report. They show
  no runtime defect; they cannot prove cryptographic quality.

Coverage measured on macOS leaves CSPRNG error branches unexecuted because a
healthy host does not fail. Those branches therefore require source inspection
by the reviewer, not only test observation.

## Reviewer checklist

For `P-GENERATE-RNG-SOURCE`:

1. Confirm every production path in the table above reaches
   `secure_randomness()` or a documented audited replacement exactly once.
2. Confirm each backend is an OS/runtime CSPRNG and that no compile-time branch
   silently substitutes a non-cryptographic source.
3. Confirm the returned buffer is exactly 12 bytes, is not truncated, and is not
   derived from a predictable seed, counter, time value, or process state.
4. Confirm the 96 bits survive encoding without bias or truncation.
5. Confirm `std::random_device`, `rand`, `random`, and hand-rolled PRNGs are
   absent from production code.

For `P-GENERATE-RNG-FAILURE`:

1. Confirm each backend checks its full return status and rejects short reads,
   EOF, `EINTR` handling errors, and unexpected zero-byte results.
2. Confirm a failure propagates as `randomness_failure` and cannot yield an
   identifier, a zeroed randomness field, or a retry with weaker entropy.
3. Confirm no catch block converts CSPRNG failure into success.
4. Confirm the injected-source overloads also fail closed on an empty or
   throwing source.
5. Confirm the unsupported-platform build fails instead of compiling a fallback.

## Evidence record template

**Template only.** Fill it only after an actual independent review of the exact
commit. The schema requires `PASS` for both checks; do not create the record if
either check fails or was not performed. The runner validates the record's shape
and commit binding, not the reviewer's identity or honesty.

```json
{
  "format": "pikaid-csprng-review-v1",
  "implementation_id": "pikaid/pikaid-cpp",
  "implementation_commit": "<40-character reviewed commit>",
  "spec_revision": "1.0.4",
  "suite_revision": "1.0.0",
  "reviewed_at_utc": "<YYYY-MM-DDTHH:MM:SSZ>",
  "reviewer": "<independent reviewer identity>",
  "checks": [
    {
      "id": "P-GENERATE-RNG-SOURCE",
      "result": "PASS",
      "evidence": [
        {"path": "src/pikaid.cpp", "line_start": 236, "line_end": 280}
      ]
    },
    {
      "id": "P-GENERATE-RNG-FAILURE",
      "result": "PASS",
      "evidence": [
        {"path": "src/pikaid.cpp", "line_start": 175, "line_end": 187},
        {"path": "src/pikaid.cpp", "line_start": 189, "line_end": 226}
      ]
    }
  ]
}
```

Evidence paths are repository-relative and must identify the reviewed lines.
Add more ranges when the reviewer relies on them.

## Running the reviewed record

From the pinned suite's `conformance/` directory:

```sh
python3 tools/run_adapter.py \
  --security-evidence /path/to/review.json \
  --implementation-commit "$(git -C /path/to/pikaid-cpp rev-parse HEAD)" \
  -- /path/to/pikaid_conformance_adapter
```

A matching record changes the default outcome from exit `3`
(`AUTOMATED_PASS_AUDIT_REQUIRED`) to exit `0`. Even then, the suite states that
exit `0` is not by itself a conformance certification; the consuming project's
protected release process must approve the evidence.

## Known limitations

- Windows `BCryptGenRandom`, Linux `getrandom`, and BSD `getentropy` paths were
  not executed on the macOS host used for this packet. They require static
  review here and, when hosted runners are available, cross-platform execution.
- No independent human has reviewed this implementation yet. Until one does,
  both audit IDs remain outstanding and the release verdict remains NO-GO for
  the security dimension.
