# Interoperability fixtures

These files are unmodified copies of the official vector fixture and schema
published by the reference PHP implementation:

| File | Source | SHA-256 |
| --- | --- | --- |
| `pikaid-v1.json` | <https://github.com/pikaid/pikaid-php/blob/e3f843edeb5157a5d2d25196cac0d69580810a95/tests/vectors/pikaid-v1.json> | `b0c9dfdd063613424414fef5c9065fbd2f81d645d0ab42b34e27cde715613500` |
| `pikaid-v1.schema.json` | <https://github.com/pikaid/pikaid-php/blob/e3f843edeb5157a5d2d25196cac0d69580810a95/tests/vectors/pikaid-v1.schema.json> | `4441d4dbaa6c5450ae9cbfb2438c53bc0421bba2b7fcc9ef9b158e5d1de0759e` |

Provenance:

- `pikaid-php` tag `v1.2.0`, commit
  `e3f843edeb5157a5d2d25196cac0d69580810a95`.
- Fixture `meta.spec_revision` is `1.0.4`, matching the normative specification
  pinned at `pikaid/pikaid-specs` commit
  `17014d76f568c3ac9f9ce2927c1a018fdbc36d1d`.

The fixture filename is historical. The pikaid **identifier format** is
immutable and unversioned; the version numbers refer to the specification
revision and to the PHP implementation release.

The test suite reads `pikaid-v1.json` at run time and verifies the SHA-256
digest above, so a local edit cannot silently weaken conformance. If you update
the fixture, update the digest in `tests/conformance_test.cpp` and this table in
the same change, and re-read the canonical specification first.

`pikaid-v1.schema.json` is stored for reference and for the structural checks in
`tests/conformance_test.cpp`. It is not used to validate the implementation at
run time, and no third-party JSON Schema validator is required.
