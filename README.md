<!--
SPDX-FileCopyrightText: 2025 Dyne.org foundation
SPDX-License-Identifier: GPL-3.0-or-later
-->

# 🌊 Sailfish pool - small and fast memory pool

![](https://raw.githubusercontent.com/dyne/sailfish-pool/refs/heads/main/sailfish-pool.jpg)

This is a lightweight pool manager for small memory allocations in C,
optimized for speed, safety and privacy. It mainly consists of three
functions to be used in place of standard C memory allocations.

1. allocate memory `void *sfpool_malloc(void *pool, size_t size)`
2. free memory `bool sfpool_free(void *pool, void *ptr)`
3. resize allocated memory `void *sfpool_realloc(void *pool, void *ptr)`

It does not work as a drop-in replacement of memory functions (for
instance using the "LD_PRELOAD trick") because it also requires:
1. initialization: `sfpool_init(void* pool, size_t nmemb, size_t size)`
2. teardown: `sfpool_teardown(void* pool)`

A single pool cannot be used by multiple threads: multi-threaded
applications should create and initialize a different pool for each
running thread.

## Features

- **Portable**: Tested to run on 32 and 64 bit targets: Apple/OSX and MS/Windows, ARM and x86 as well WASM
- **Pool Allocator**: Efficiently manages small, fixed-size memory blocks using a preallocated memory pool.
- **Secure Zeroing**: Ensures all memory is zeroed out before free to protect sensitive information.
- **Reallocation Support**: Supports `realloc()` for both pool and large memory blocks, handling transitions.
- **Hashtable optimization**: Fast O(1) lookup on allocated pointers grants constant time execution.
- **Fallback Mechanism**: Falls back to `malloc()` when the pool is exhausted and continues functioning.

## Intended use case

The primary use case for 🌊 sailfish-pool is within
[Zenroom](https://zenroo.org), a small virtual machine (VM) designed
for cryptographic operations where Lua is embedded. Zenroom's main
target is WebAssembly (WASM), which [we extensively use at
work](https://forkbomb.solutions) to simplify end-to-end
encryption. This explains the 32-bit constraint of the allocator.

In Zenroom, a significant amount of small memory allocations occur,
typically involving octets for hashes, elliptic curve points,
cryptographic keys, zero-knowledge proofs, and other cryptographic
operations. These allocations frequently have sizes below 128
bytes. As cryptographic algorithms evolve, particularly with the
advent of post-quantum cryptography, the pool size will be fine-tuned
to accommodate these growing requirements.

## Usage

To use the custom memory manager in your project, include `sfpool.h`
and use the provided functions.

You can also redefine malloc/free/realloc taking advantage of the
opaque context pointers, this way you won't need to include our header
everywhere and just declare some externs:

```c
extern void *sfpool_malloc (void *restrict opaque, const size_t size);
extern bool  sfpool_free   (void *restrict opaque, void *ptr);
extern void *sfpool_realloc(void *restrict opaque, void *ptr, const size_t size);
```

Since the main use-case is being a [custom memory manager in Lua](http://www.lua.org/manual/5.3/manual.html#lua_Alloc), the primary usage example is found in [Zenroom's code src/zen_memory.c](https://github.com/dyne/Zenroom/blob/master/src/zen_memory.c).

## Testing

There is a makefile target in this repository running tests with the
address sanitizer: just type `make`.

To run these tests inside your source you can always do:

    gcc -D SFPOOL_TEST -o sfpool_test sfpool.c
    time ./sfpool_test

The test suite will allocate and deallocate memory in various patterns
to simulate different usage scenarios and assert the correctness of
the memory management.

## License

Copyright (C) 2025 Dyne.org foundation

Designed and written by Denis "[Jaromil](https://jaromil.dyne.org)" Roio

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
