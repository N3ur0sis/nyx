# Releasing NYX

This document describes the release process for cutting a new NYX version.

## Pre-Release Checklist

1. **Version bump**: update `project(nyx VERSION X.Y.Z)` in the root `CMakeLists.txt`. This is the single source of truth -- the generated `nyx_version.h` propagates everywhere.

2. **Changelog**: move items from `[Unreleased]` to a new `[X.Y.Z] - YYYY-MM-DD` section in `CHANGELOG.md`. Add the comparison link at the bottom.

3. **Docs sanity**: run the docs quality check locally:
   ```bash
   bash scripts/docs-check.sh
   pip install -r requirements-docs.txt
   mkdocs build --strict
   ```

4. **Build and smoke test**:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ./bin/nyx version    # should print the new version
   ./bin/nyx info
   ```

5. **Commit the version bump**:
   ```bash
   git add CMakeLists.txt CHANGELOG.md
   git commit -m "Release vX.Y.Z"
   ```

## Cutting the Release

1. **Create and push the tag**:
   ```bash
   git tag -a vX.Y.Z -m "NYX vX.Y.Z"
   git push origin main --tags
   ```

2. **GitHub Actions takes over**:
   - The `release.yml` workflow triggers on the `vX.Y.Z` tag
   - It builds a release tarball, generates checksums, and creates a GitHub Release with assets
   - The `docs.yml` workflow deploys updated docs to GitHub Pages

3. **Verify the release**:
   - Check the GitHub Release page for the uploaded tarball and checksums
   - Verify the GitHub Pages deployment reflects the latest docs
   - Download the tarball and run the installer on a clean system

## Versioning Policy

NYX follows [Semantic Versioning](https://semver.org/):

- **MAJOR**: breaking changes to workflow format, output envelope structure, or shared library API
- **MINOR**: new tools, new workflow features, new library functions
- **PATCH**: bug fixes, documentation updates, build improvements

## Hotfix Process

For urgent fixes on a released version:

1. Branch from the release tag: `git checkout -b hotfix/vX.Y.Z+1 vX.Y.Z`
2. Apply the fix, bump the patch version
3. Tag and push as above
