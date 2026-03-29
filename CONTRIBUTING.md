# Contributing to NYX

Thank you for considering contributing to NYX. This document covers the conventions and workflows you need to know.

## Getting Started

```bash
git clone https://github.com/N3ur0sis/nyx.git
cd nyx
cmake -B build
cmake --build build
```

Verify the build:

```bash
./bin/nyx version
./bin/nyx info
```

## Code Style

NYX uses a consistent C style enforced by `.clang-format`. Before submitting a PR:

```bash
# Check formatting (dry run)
bash scripts/format-check.sh

# Auto-format changed files
clang-format -i path/to/file.c
```

Key conventions:

- 4-space indentation, no tabs
- Linux brace style (`{` on same line for control flow, next line for functions)
- 100-character column limit
- Pointer star attached to the variable: `char *name`
- K&R-style function declarations with full prototypes
- Public APIs documented with `@file`, `@brief`, `@param`, `@return` Doxygen comments
- No comments that just narrate what the code does; only document non-obvious intent

## Adding a New Tool

See the [Adding a Tool](docs/contributors/add-a-tool.md) guide for a complete walkthrough. The short version:

1. Create `tools/<module>/<tool>/` with `CMakeLists.txt`, `src/`, and `man/`
2. Split into API (`*_api.h`), implementation (`*_impl.c`), command layer (`*_cmd.c`), and CLI frontend (`*_cli.c`)
3. Register the tool in `tools/nyx/src/nyx_tools_builtin.c`
4. Link the tool library into `nyx` and `nyx-run` in their `CMakeLists.txt`
5. Add a docs page at `docs/tools/<tool>.md`
6. Add a man page at `tools/<module>/<tool>/man/nyx-<tool>.8`

## Documentation

NYX documentation lives in `docs/` and is built with MkDocs. To preview locally:

```bash
pip install -r requirements-docs.txt
mkdocs serve
```

Run the docs quality check:

```bash
bash scripts/docs-check.sh
```

Every shipped tool needs both a `docs/tools/<name>.md` page and a `man/nyx-<name>.8` man page.

## Pull Requests

- One feature or fix per PR
- Write a clear title and description explaining **why**, not just what
- Fill out the PR template checklist
- Ensure the build passes: `cmake -B build && cmake --build build`
- Ensure docs checks pass: `bash scripts/docs-check.sh`
- Update documentation if you change tool parameters, result fields, or CLI behavior
- Update `CHANGELOG.md` under the `[Unreleased]` section

## Commit Messages

Use imperative mood for the subject line:

```
Add DNS lookup tool to phobos module
Fix CIDR parsing for /32 subnets
Update pingsweep docs with new timeout parameter
```

Keep the subject under 72 characters. Add a body if the change needs explanation.

## Versioning

NYX uses semantic versioning. The single source of truth is `project(nyx VERSION X.Y.Z)` in the root `CMakeLists.txt`. A generated header `cmake/nyx_version.h` propagates the version to all binaries automatically. Do not hardcode version strings in tool source files.

## Security

If you discover a security vulnerability in NYX itself (not in target systems), please follow the process described in [SECURITY.md](SECURITY.md). Do not open a public issue for security vulnerabilities.

## License

By contributing to NYX, you agree that your contributions will be licensed under the MIT License.
