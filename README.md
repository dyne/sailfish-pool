<!--
SPDX-FileCopyrightText: 2025 Dyne.org foundation
SPDX-License-Identifier: GPL-3.0-or-later
-->

![](https://raw.githubusercontent.com/dyne/sailfish-pool/refs/heads/main/sailfish-pool.jpg)

This is a lightweight pool manager for small memory allocations in C,
optimized for data privacy and speed.

It does not work as a drop-in replacement of memory functions using
the "LD_PRELOAD trick") because it also requires initialization and
teardown.

Also a single sfpool cannot share concurrent memory access:
multi-threaded applications should create and initialize a different
sfpool for each running thread.

## Features

- **Portable**: Tested to run on 32 and 64 bit targets: Apple/OSX and MS/Windows, ARM and x86 as well WASM
- **Fast**: Efficiently manages small, fixed-size memory blocks using a preallocated memory pool.
- **Private**: Ensures memory access is locked whenever possible and contents deleted on release.
- **Transparent**: Supports `realloc()` for transparent transition to system alloc on big sizes.
- **Steady**: Hashtable lookup on allocated memory grants O(1) constant time operations.
- **Fallback**: Resorts to system `malloc()` when pool is exhausted to continue functioning.

## Intended use case

The primary use case for 🌊 sailfish-pool is within
[Zenroom](https://zenroo.org), our small virtual machine (VM) designed
for cryptographic operations where Lua is embedded.

Zenroom's main target is WebAssembly (WASM), which [we extensively use
at work](https://forkbomb.solutions) to simplify end-to-end
encryption. This explains the 32-bit support of this allocator.

In Zenroom, a significant amount of small memory allocations occur,
typically involving octets for hashes, elliptic curve points,
cryptographic keys, zero-knowledge proofs, and other cryptographic
operations. These allocations frequently have sizes below 256
bytes. As cryptographic algorithms evolve, particularly with the
advent of post-quantum cryptography, the pool size can be fine-tuned
to accommodate these growing requirements.

## Usage

To use the custom memory manager in your project, include `sfpool.h`
and use the provided functions.

### High Level API

The main entry point is the [🌊 High Level API documented
here](https://dyne.org/sailfish-pool/group__sfpool.html) and
constituted by init/teardown functions initalizing an sfpool context
and malloc/free/realloc functions for common memory operations. Also a
function to verify if a pointer is contained in the pool and one to
report status.

### [Utilities API](https://dyne.org/sailfish-pool/group__sfutil.html)

Some internal functions are exposed as the [🌊 utilities API
documented here](https://dyne.org/sailfish-pool/group__sfutil.html)
and they may be useful also to host applications: fast and portable
memory zeroing, memory alignment and the internal portable
implementations for secure allocation and free.

## Quality Assurance

There is a makefile target in this repository running tests with the
address sanitizer: just type `make check`.

To run these tests inside your source you can always do:

    gcc -D SFPOOL_TEST -o sfpool_test sfpool.c
    time ./sfpool_test

The test suite will allocate and deallocate memory in various patterns
to simulate different usage scenarios and assert the correctness of
the memory management.

Additional tests are available: `make wasm` builds and runs the
test as a WASM binary when `EMSDK` is available and pointing to an
Emscripten installation.

Then the `make check-lua` target downloads the latest stable Lua
codebase and compiles it applying sfpool as its main memory allocator,
then runs the Lua test suite.

All tests are constantly verified in [continuous integration by Github
actions](https://github.com/dyne/sailfish-pool/actions).


## License

Copyright (C) 2025 Dyne.org foundation

Designed and written by [Jaromil](https://jaromil.dyne.org).

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

[![software by Dyne.org](https://files.dyne.org/software_by_dyne.png)](http://www.dyne.org)
