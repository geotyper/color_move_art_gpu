# Repository Guidelines

## Project Structure & Module Organization
- App code lives in `src/` (`main.cpp`, `app_window.*`, `mpm_renderer.*`, input helpers). Keep new features grouped by file, and prefer small translation units over mega-files.
- Rendering and compute shaders sit in `shaders/`; they are baked by Qt’s `qt_add_shaders` into `.qsb` outputs inside `build/.qsb/` (generated, do not edit).
- Reference artwork and inspiration are under `art_source/`. Leave originals untouched; add notes if you import new assets.
- `source_examples/` holds external SPH reference material (lava-lamp sample); use it for ideas, not as a runtime dependency.
- `build/` is the CMake build tree. Avoid committing its contents.

## Build, Test, and Development Commands
- Configure (Desktop Qt 6.6+ required for RHI + compute):  
  `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$QT_ROOT/lib/cmake`
- Build: `cmake --build build` (add `--config Release` for multi-config generators). Re-run this after editing any file under `shaders/` to re-bake `.qsb`.
- Run the app from the build tree: `./build/color_move_art`. Set a backend explicitly if needed, e.g. `QT_RHI_BACKEND=vulkan` or `QT_RHI_BACKEND=opengl`.
- Tests are not yet present; if you add them, wire them via CTest and run with `ctest --test-dir build`.

## Coding Style & Naming Conventions
- C++20 with Qt: 4-space indent, no tabs, keep lines ≲ 110 chars. Prefer `std::unique_ptr`, `std::array`, `enum class`, and range-for loops.
- File names: snake_case for sources/headers (`mpm_renderer.cpp`), PascalCase for types, camelCase for functions/locals, SCREAMING_SNAKE_CASE for constants.
- When touching many files, run `clang-format -i src/*.cpp src/*.h` (add a shared `.clang-format` if consistency drifts).
- Keep GPU-facing structs tightly packed and mirrored between C++ and GLSL; update both sides together.

## Testing Guidelines
- Add lightweight, deterministic tests when introducing logic that is not purely visual (e.g., particle emitters, parameter mapping). Place them in `tests/` with names like `test_particles.cpp`.
- Prefer Qt Test or Catch2; keep runtime short so `ctest` can run on every build.

## Commit & Pull Request Guidelines
- Use present-tense, concise commits (Conventional-style is welcome: `feat: add gpu sph grid`, `fix: stabilize pressure solve`).
- One focused change per commit; include a note on testing (`ctest`, manual run, or “not applicable”).
- PRs should summarize intent, call out new runtime flags (backend selection, shader params), and include screenshots/clips for visual changes.
- Document any new assets or shader parameter defaults in the PR description to help reviewers reproduce visuals.
