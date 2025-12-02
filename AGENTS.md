# Repository Guidelines

## Project Structure & Module Organization
- Keep source under `src/` (implementation) and `include/` (public headers) when adding C++ code; group features by module to ease reuse.
- Place automated tests in `tests/` mirroring module names (e.g., `tests/color/test_palette.cpp` for `src/color/palette.cpp`).
- Store reference assets and inspiration in `art_source/`; add new files with descriptive names and brief README notes when helpful.
- Use `examples/` for small runnable sketches that demonstrate color-motion behaviors without pulling in the full app.

## Build, Test, and Development Commands
- Configure once per machine: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` to generate a local build tree.
- Build: `cmake --build build` compiles all targets; add `--config Release` for optimized binaries if using multi-config generators.
- Run tests: `ctest --test-dir build` executes all registered tests; use `-R name` to focus on a subset.
- Local demo (if present in `examples/`): `./build/examples/<demo_name>`; keep example names short and intent-revealing.

## Coding Style & Naming Conventions
- Follow modern C++17+ style: prefer `std::unique_ptr`/`std::shared_ptr`, `enum class`, and `constexpr` where sensible.
- Indent with 4 spaces; avoid tabs. Keep lines ≤ 100 characters.
- File naming: snake_case for sources/headers (`color_field.cpp`), PascalCase for types, camelCase for functions/variables, SCREAMING_SNAKE_CASE for constants.
- Use `clang-format` with a repository `.clang-format` once added; run it before commits (`clang-format -i src/**/*.cpp include/**/*.hpp`).
- Keep functions single-purpose; extract helpers rather than inlining long lambdas in loops.

## Testing Guidelines
- Prefer GoogleTest or Catch2; name test files `test_<module>.cpp` and test cases `Section`/`TEST` names that describe behavior, not method names.
- Aim for fast, deterministic tests; avoid real file-system or network dependencies without fakes.
- Add regression tests for every bug fix and new feature; keep `ctest` green before pushing.

## Commit & Pull Request Guidelines
- Use concise, present-tense commit messages (recommended Conventional Commits, e.g., `feat: add hsv-to-rgb converter` or `fix: clamp color velocity`).
- One topic per commit; include tests or notes on why tests are not applicable.
- Pull requests should explain intent, link to any issues/tasks, and include screenshots or brief clips for visual changes (art assets or rendering tweaks).
- Keep PRs small and reviewable; document any new dependencies or build flags in the description.
