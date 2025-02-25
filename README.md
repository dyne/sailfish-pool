<!--
SPDX-FileCopyrightText: 2025 Dyne.org foundation
SPDX-License-Identifier: GPL-3.0-or-later
-->

# ⚡fastalloc32.c⚡

This is a custom memory manager in C, optimized for speed, safety and
privacy. It provides custom implementations of `malloc()`, `free()`,
and `realloc()` functions to be used in place of the standard C
library memory allocation functions, with the addition of a reentrant
pointer to the memory manager, created and destroyed after use.

## Features

- **Pool Allocator**: Efficiently manages small, fixed-size memory blocks using a preallocated memory pool.
- **Secure Zeroing**: Ensures all memory is zeroed out before free to protect sensitive information.
- **Reallocation Support**: Supports `realloc()` for both pool and large memory blocks, handling transitions.
- **Hashtable optimization**: Fast O(1) lookup on allocated pointers grants constant time execution.
- **Memory Alignment**: All memory allocations are aligned to improve performance on various architectures.
- **Large Allocations**: Manage large allocations when the pool is exhausted, minimizing system calls.
- **Fallback Mechanism**: Falls back to `malloc()` when the pool is exhausted and continues functioning.

## Intended use case

The primary use case for ⚡fastalloc32.c⚡ is within
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

To use the custom memory manager in your project, include
`fastalloc32.c` in your build and use the provided functions.

There is no need for a header, just declare some externs:

```c
extern void *fastalloc32_create  ();
extern void  fastalloc32_destroy (void *restrict manager);
extern void *fastalloc32_malloc  (void *restrict manager, size_t size);
extern void *fastalloc32_realloc (void *restict  manager, void *ptr, size_t size);
extern void  fastalloc32_free    (void *restrict manager, void *ptr);
```

Since the main use-case is being a [custom memory manager in Lua](http://www.lua.org/manual/5.3/manual.html#lua_Alloc), here is an example:

```c
//  ⚡fastalloc32.c⚡ for the Lua state
extern void *restrict FA32;
extern void *fastalloc32_malloc  (void *restrict mm, size_t size);
extern void *fastalloc32_realloc (void *restict  mm, void *ptr, size_t size);
extern void  fastalloc32_free    (void *restrict mm, void *ptr);
#define malloc(size)       fastalloc32_malloc  (FA32, size)
#define free(ptr)          fastalloc32_free    (FA32, ptr)
#define realloc(ptr, size) fastalloc32_realloc (FA32, ptr, size)
void *fastalloc32_for_lua(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
	if(ptr == NULL) {
		if(nsize!=0) {
			void *ret = malloc(nsize);
			if(ret) return ret;
			return NULL;
		} else
      return NULL;
	} else {
		if(nsize==0) {
			free(ptr);
			return NULL;
    }
    return realloc(ptr, nsize);
  }
}
```

To use this one should create the global pointer `*FA32` somewhere
else, close to the lua initialization, using `fastalloc32_create()`
and at the end of the execution destroy it with
`fastalloc32_destroy()`. To keep full reentrancy it is also possible
to change this function and pass the `*FA32` pointer through the "user
data" argument Lua provides (`*ud`).


## Testing

There is a makefile target in this repository running tests with the
address sanitizer: just type `make`.

To run these tests inside your source you can always do:

    gcc -D FASTALLOC32_TEST -o fastalloc32_test fastalloc32.c
    time ./fastalloc32_test

The test suite will allocate and deallocate memory in various patterns
to simulate different usage scenarios and assert the correctness of
the memory management.

## Benchmarks

⚠️ NNt yet! please help...

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
