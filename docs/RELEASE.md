# Release Process

This project uses a source-first release flow anchored on Git tags and
[GitHub Releases](https://github.com/kasparsj/lightgraph/releases).

## 1) Prepare the release branch

1. Update version in:
   - `CMakeLists.txt` (`project(lightgraph VERSION X.Y.Z)`)
   - `conanfile.py` (`version = "X.Y.Z"`)
   - `packaging/vcpkg/vcpkg.json` (`version-string`)
2. Update `CHANGELOG.md` and `MIGRATION.md` (if API changes).
3. Run quality gates:
   - `cmake --preset default && cmake --build --preset default && ctest --preset default`
   - `cmake --preset warnings && cmake --build --preset warnings && ctest --preset warnings`
   - `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
   - `cmake --preset ubsan && cmake --build --preset ubsan && ctest --preset ubsan`
   - `cmake --preset static-analysis && cmake --build --preset static-analysis`
   - `./scripts/check-benchmark.sh build/preset-static-analysis/lightgraph_core_benchmark`
   - `cmake --preset coverage && cmake --build --preset coverage && ctest --preset coverage`
   - `./scripts/generate-coverage.sh build/preset-coverage`

## 2) Tag and publish

1. Create a signed tag (example: `v1.1.0`):

```bash
git tag -s v1.1.0 -m "Lightgraph v1.1.0"
git push origin v1.1.0
```

2. Create a GitHub Release from that tag and attach:
   - release notes from `CHANGELOG.md`
   - optional coverage artifact (`coverage.xml`)

## 3) Distribution channels

- **CMake package**: consume from source/build/install using `find_package(lightgraph)`.
- **Conan**: `conanfile.py` in repo root is the canonical recipe source.
- **vcpkg overlay**: `packaging/vcpkg/` is the canonical port template source.

When publishing to external registries (ConanCenter/private remotes or official
vcpkg ports), always publish from a Git tag and reference the exact tag in the
registry metadata.

