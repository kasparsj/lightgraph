# Lightgraph API Policy

This document defines compatibility rules for Lightgraph as a reusable library.

## Versioning

Lightgraph follows semantic versioning for the stable installable API:

- `MAJOR`: incompatible API changes
- `MINOR`: backward-compatible additions
- `PATCH`: backward-compatible fixes

Stable API scope:

- `lightgraph/lightgraph.hpp`
- `lightgraph/engine.hpp`
- `lightgraph/types.hpp`
- `lightgraph/status.hpp`
- `lightgraph/version.hpp`

## Compatibility Guarantees

Within a major version:

- Existing stable public symbols are not removed without a deprecation period.
- Existing stable function semantics remain compatible, except for bug fixes.
- Error codes remain stable for existing failure classes.

Source-integration headers under `lightgraph/integration*` are intentionally less stable
and may evolve with internal architecture changes.

## Deprecation Policy

Deprecation mechanism:

- Use `LIGHTGRAPH_DEPRECATED("message")` from `lightgraph/version.hpp`.

Deprecation lifecycle for stable API:

1. Introduce replacement API.
2. Mark old API as deprecated in at least one `MINOR` release.
3. Remove deprecated API only in the next `MAJOR` release.

## Bug Fix Policy

Behavior changes are allowed in `PATCH`/`MINOR` releases only when correcting
incorrect behavior, memory safety issues, or undefined behavior.

Each behavior-changing fix must include:

- a rationale in changelog/migration notes
- a regression test
