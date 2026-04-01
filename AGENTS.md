# Sailfish Pool Agent Guide

## Purpose

This repository is a very small, header-only C allocator.

- Product: `sfpool`, a secure-ish small-block memory pool with fallback to system allocation.
- Primary artifact: [`sfpool.h`](/home/jrml/devel/sailfish-pool/sfpool.h)
- Main consumers in this repo: the stress test [`sfpool_test.c`](/home/jrml/devel/sailfish-pool/sfpool_test.c) and the Lua allocator integration test [`test_lua.c`](/home/jrml/devel/sailfish-pool/test_lua.c)
- Non-code support: [`README.md`](/home/jrml/devel/sailfish-pool/README.md), [`GNUmakefile`](/home/jrml/devel/sailfish-pool/GNUmakefile), CI workflows under [`.github/workflows`](/home/jrml/devel/sailfish-pool/.github/workflows)

The entire meaningful codebase is under 650 lines. Do not invent layers, wrappers, or abstractions unless explicitly asked. Preserve the header-only design.

## Context Loading Order

Load files in this order and stop as soon as you have enough context:

1. [`README.md`](/home/jrml/devel/sailfish-pool/README.md) for intent, portability claims, and supported usage.
2. [`GNUmakefile`](/home/jrml/devel/sailfish-pool/GNUmakefile) for the real test/build entrypoints.
3. [`sfpool.h`](/home/jrml/devel/sailfish-pool/sfpool.h) because it contains both API and implementation.
4. [`sfpool_test.c`](/home/jrml/devel/sailfish-pool/sfpool_test.c) if the task touches allocator semantics or regressions.
5. [`test_lua.c`](/home/jrml/devel/sailfish-pool/test_lua.c) if the task touches embedded-Lua integration or long-running allocator behavior.
6. [`.github/workflows/main.yml`](/home/jrml/devel/sailfish-pool/.github/workflows/main.yml) only for CI-specific work.

Usually you do not need anything else.

## Repository Map

- [`sfpool.h`](/home/jrml/devel/sailfish-pool/sfpool.h): canonical implementation and public API.
- [`sfpool_test.c`](/home/jrml/devel/sailfish-pool/sfpool_test.c): randomized stress test for allocate/free/realloc patterns.
- [`test_lua.c`](/home/jrml/devel/sailfish-pool/test_lua.c): plugs `sfpool` into Lua via `lua_newstate`.
- [`GNUmakefile`](/home/jrml/devel/sailfish-pool/GNUmakefile): local QA commands, Lua download/build, WASM build.
- [`README.md`](/home/jrml/devel/sailfish-pool/README.md): public project narrative and usage.
- [`Doxyfile`](/home/jrml/devel/sailfish-pool/Doxyfile), [`doxygen-header.html`](/home/jrml/devel/sailfish-pool/doxygen-header.html), [`doxygen-custom.css`](/home/jrml/devel/sailfish-pool/doxygen-custom.css): documentation publishing assets.
- [`.github/workflows/main.yml`](/home/jrml/devel/sailfish-pool/.github/workflows/main.yml): CI matrix.
- [`.github/workflows/doxygen-gh-pages.yml`](/home/jrml/devel/sailfish-pool/.github/workflows/doxygen-gh-pages.yml): docs deployment.

Ignore downloaded Lua artifacts unless the task is about `check-lua`.

## Mental Model

`sfpool` is a fixed-block allocator:

- One `sfpool_t` owns one preallocated arena.
- `block_size` must be a power of two.
- The pool embeds a singly linked free list inside free blocks.
- Allocations `<= block_size` come from the pool while free blocks remain.
- Larger allocations, or small allocations after exhaustion, fall back to system `malloc` when `FALLBACK` is enabled.
- `sfpool_free` decides between pool return and system `free` via pointer-range membership.
- `sfpool_realloc` keeps in-pool pointers in place when the new size still fits; otherwise it migrates to system allocation.

This is not a general allocator replacement:

- callers must explicitly create and tear down `sfpool_t`
- a pool is not thread-safe
- intended workload is many small allocations, especially sub-256-byte objects

## Core Data And API

Important fields in `sfpool_t`:

- `buffer`: raw allocation returned by platform allocator
- `data`: aligned pool base used for blocks
- `free_list`: head of embedded free-list
- `free_count`, `total_blocks`, `total_bytes`, `block_size`: allocator state
- profiling counters behind `PROFILING`

Public high-level API:

- `sfpool_init`
- `sfpool_teardown`
- `sfpool_malloc`
- `sfpool_free`
- `sfpool_realloc`
- `sfpool_contains`
- `sfpool_status`

Utility API exposed publicly:

- `sfutil_zero`
- `sfutil_memalign`
- `sfutil_secalloc`
- `sfutil_secfree`

## Platform Behavior

Allocation backend varies by target:

- Emscripten: `malloc` / `free`
- Windows: `VirtualAlloc` / `VirtualFree`
- Apple: `mmap` plus `mlock`
- Other POSIX: `mmap`, optionally `MAP_LOCKED` if `RLIMIT_MEMLOCK` allows it

Pointer-size configuration is compile-time:

- 64-bit targets use `uint64_t`, 8-byte pointer alignment, 16-byte struct alignment
- 32-bit targets use `uint32_t`, 4-byte pointer alignment, 8-byte struct alignment

## Compile-Time Switches

These macros are enabled by default inside [`sfpool.h`](/home/jrml/devel/sailfish-pool/sfpool.h):

- `SECURE_ZERO`
- `FALLBACK`
- `PROFILING`

They materially change behavior. Do not remove or redefine them casually in downstream code without checking tests and expected stats output.

## Tests And Commands

Primary local commands:

- `make sfpool_test`: build the standalone stress test with sanitizers
- `make check`: run six stress-test parameter combinations
- `make check-lua`: download/build Lua 5.4.7, compile `test_lua`, run Lua upstream tests with `sfpool`
- `make wasm`: build and run the WASM test through Emscripten
- `make clean`

CI covers:

- REUSE compliance
- cpplint
- Linux with `gcc` and `clang`
- Windows native
- macOS native
- ARM32 via runner
- WASM build

## Observed Quirks

These are current-repo realities. Assume they are intentional only after verifying.

- `sfpool.h` is the implementation. There is no `sfpool.c`.
- [`README.md`](/home/jrml/devel/sailfish-pool/README.md) still shows an old compile example using `sfpool.c`.
- [`.github/workflows/main.yml`](/home/jrml/devel/sailfish-pool/.github/workflows/main.yml) points cpplint at `src`, but this repo has no `src/` directory.
- [`.gitignore`](/home/jrml/devel/sailfish-pool/.gitignore) still contains the old name `fastalloc32_test`.
- [`REUSE.toml`](/home/jrml/devel/sailfish-pool/REUSE.toml) still refers to `fastmempool32`.
- In my local verification on 2026-04-01, `make check` produced successful runs but some sanitizer-backed permutations also crashed intermittently without causing the overall make command to fail hard.
- In the same verification, `make check-lua` completed the Lua upstream tests and then ended with a segmentation fault after reporting pool stats.

Treat the two runtime items above as known investigation areas when touching teardown, fallback allocations, or sanitizer-sensitive behavior.

## Change Guidance

When editing this project:

- Keep the one-header design unless the task explicitly requires a structural change.
- Favor direct C over helper layers.
- Maintain portability across POSIX, Windows, and Emscripten.
- Respect the current public API names; downstream users likely include this header directly.
- Preserve Doxygen comments on public functions.
- If you change allocation semantics, update both tests and README wording.
- If you change platform allocation code, re-check `sfutil_secalloc` and `sfutil_secfree` together.
- If you change free-list behavior, inspect `sfpool_init`, `sfpool_malloc`, `sfpool_free`, and `sfpool_realloc` as a set.

## Fast Task Triage

Use this shortcut before reading everything:

- Docs-only task: read [`README.md`](/home/jrml/devel/sailfish-pool/README.md) and maybe [`Doxyfile`](/home/jrml/devel/sailfish-pool/Doxyfile).
- API or bugfix task: read [`sfpool.h`](/home/jrml/devel/sailfish-pool/sfpool.h) and [`sfpool_test.c`](/home/jrml/devel/sailfish-pool/sfpool_test.c).
- Lua regression task: add [`test_lua.c`](/home/jrml/devel/sailfish-pool/test_lua.c) and `make check-lua`.
- CI task: add [`.github/workflows/main.yml`](/home/jrml/devel/sailfish-pool/.github/workflows/main.yml) and [`GNUmakefile`](/home/jrml/devel/sailfish-pool/GNUmakefile).

## Verification Notes

Verified locally on 2026-04-01:

- `make check`
- `./sfpool_test 2048 256`
- `make check-lua`

Interpret those results with the quirks above in mind.
