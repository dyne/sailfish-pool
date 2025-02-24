<!--
SPDX-FileCopyrightText: 2025 Dyne.org foundation
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Fastalloc32

This is a custom memory manager in C, optimized for performance and
efficiency. The memory manager provides custom implementations of
`malloc()`, `free()`, and `realloc()` functions, designed to be used
in place of the standard C library memory allocation functions.

## Features

- **Pool Allocator**: Efficiently manages small, fixed-size memory blocks using a preallocated memory pool.
- **Memory Alignment**: Ensures that all memory allocations are properly aligned to improve performance on various architectures.
- **Cache for Freed Blocks**: Implements a cache for recently freed blocks to speed up subsequent allocations and deallocations.
- **Large Allocation Handling**: Uses a separate mechanism for managing large allocations when the pool is exhausted, minimizing system calls.
- **Fallback Mechanism**: Falls back to `malloc()` for allocations when the pool is exhausted, ensuring the allocator can continue to function.
- **Reallocation Support**: Supports `realloc()` for both pool-allocated and large-allocated memory blocks, handling transitions between different allocation strategies.
- **Memory Leak Detection**: Includes a stress test to check for memory leaks and ensure the correctness of the memory manager.

## Usage

To use the custom memory manager in your project, include
`fastalloc32.c` in your build and use the provided
`fastalloc32_malloc()`, `fastalloc32_free()`, and
`fastalloc32_realloc()` functions in place of the standard `malloc()`,
`free()`, and `realloc()` functions.

There is no need for a header, just declare some externs:

```c
extern struct fastalloc32_mm;
extern void fastalloc32_init(fastalloc32_mm *manager);
extern void fastalloc32_destroy(fastalloc32_mm *manager);
extern void *fastalloc32_malloc(fastalloc32_mm *manager, size_t size);
extern void *fastalloc32_realloc(fastalloc32_mm *manager, void *ptr, size_t size);
extern void fastalloc32_free(fastalloc32_mm *manager, void *ptr);
```

## Testing

There is a makefile target in this repository running tests with the
address sanitized on, to build and run them just type `make`.

To run the tests, compile and execute `fastalloc32.c` from inside your
source you can do something like this at any time:

    gcc -DFASTALLOC32_TEST -o fastalloc32_test fastalloc32.c

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
