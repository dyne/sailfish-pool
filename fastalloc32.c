/* SPDX-FileCopyrightText: 2025 Dyne.org foundation
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2025 Dyne.org foundation
 * designed, written and maintained by Denis Roio <jaromil@dyne.org>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

// Configuration
#define BLOCK_SIZE 128
static_assert((BLOCK_SIZE & (BLOCK_SIZE - 1)) == 0, "BLOCK_SIZE must be a power of two");
#define POOL_SIZE (2 * 8192 * BLOCK_SIZE) // two MiBs
#define SECURE_ZERO // Enable secure zeroing
#define FALLBACK // Enable fallback to system alloc

// Memory pool structure
typedef struct fastpool_t {
  uint8_t *data;
  uint8_t *free_list; // Pointer to the first free block
  uint32_t free_count;
  uint32_t total_blocks;
  uint32_t total_bytes;
} fastpool_t;

static inline void secure_zero(void *ptr, uint32_t size) {
  volatile uint32_t *p = (uint32_t*)ptr; // use 32bit pointer
  volatile uint32_t s = (size>>2); // divide counter by 4
  while (s--) *p++ = 0x0; // hit the road jack
}


#if defined(__x86_64__) || defined(_M_X64) || defined(__ppc64__) || defined(__LP64__)
#define ptr_t uint64_t
#else
#define ptr_t uint32_t
#endif
static_assert(sizeof(ptr_t) == sizeof(void*), "Unknown memory pointer size detected");

static inline bool is_in_pool(fastpool_t *pool, const void *ptr) {
  volatile ptr_t p = (ptr_t)ptr;
  return(p >= (ptr_t)pool->data
         && p < (ptr_t)(pool->data + pool->total_bytes));
}

#define sys_malloc malloc
#define sys_free   free

// Pool initialization
static inline void pool_init(fastpool_t *pool, uint32_t bytesize) {
#if defined(__EMSCRIPTEN__)
  pool->data = (uint8_t *)sys_malloc(bytesize);

#elif defined(_WIN32)
  pool->data = VirtualAlloc(NULL, bytesize,
                            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

#else // Posix
  pool->data = mmap(NULL, bytesize, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE
                    , -1, 0);
#endif
  if (pool->data == NULL) {
    fprintf(stderr, "Failed to allocate pool memory\n");
    exit(EXIT_FAILURE);
  }

  pool->total_bytes = bytesize;
  pool->total_blocks = bytesize / BLOCK_SIZE;
  // Initialize the embedded free list
  pool->free_list = pool->data;
  for (volatile uint32_t i = 0; i < pool->total_blocks - 1; ++i) {
    *(uint8_t **)(pool->data + i * BLOCK_SIZE) =
      pool->data + (i + 1) * BLOCK_SIZE;
  }
  pool->free_count = pool->total_blocks;
  *(uint8_t **)
    (pool->data + (pool->total_blocks - 1)
     * BLOCK_SIZE) = NULL;
}

// Pool allocation
static inline void *pool_alloc(fastpool_t *pool) {
  if (pool->free_list == NULL) {
    return NULL; // Pool exhausted
  }

  // Remove the first block from the free list
  uint8_t *block = pool->free_list;
  pool->free_list = *(uint8_t **)block;
  pool->free_count-- ;
  return block;
}

// Pool deallocation
static inline void pool_free(fastpool_t *pool, void *ptr) {
  // Add the block back to the free list
  *(uint8_t **)ptr = pool->free_list;
  pool->free_list = (uint8_t *)ptr;
  pool->free_count++ ;
#ifdef SECURE_ZERO
  // Zero out the block for security
  secure_zero(ptr, BLOCK_SIZE);
#endif
}

// Create memory manager
void *fastalloc32_create() {
  fastpool_t *pool = (fastpool_t*) sys_malloc(sizeof(fastpool_t));
  if (pool == NULL) {
    perror("memory pool creation error");
    return NULL;
  }

  pool_init(pool, POOL_SIZE);
  fprintf(stderr,"⚡fastalloc32.c initalized\n");
  fprintf(stderr,"void* pointer size: %lu\n",sizeof(void*));
  return (void *)pool;
}

// Destroy memory manager
void fastalloc32_destroy(void *restrict pool) {
  fastpool_t *p = (fastpool_t*)pool;
  // Free pool memory
#if defined(__EMSCRIPTEN__)
  sys_free(p->data);
#elif defined(_WIN32)
  VirtualFree(p->data, 0, MEM_RELEASE);
#else // Posix
  munmap(p->data, p->total_bytes);
#endif
  sys_free(pool);
}

// Allocate memory
void *fastalloc32_malloc(void *restrict pool, const size_t size) {
  fastpool_t *restrict manager = (fastpool_t*)pool;
  void *ptr;
  if (size <= BLOCK_SIZE) {
    ptr = pool_alloc(pool);
    if (ptr) return ptr;
  }
  // Fallback to system malloc for large allocations
  ptr = sys_malloc(size);
  if(ptr == NULL) perror("system malloc error");
  return ptr;
}

// Free memory
bool fastalloc32_free(void *restrict pool, void *ptr) {
  fastpool_t *p = (fastpool_t*)pool;
  if (ptr == NULL) return false; // Freeing NULL is a no-op
  if (is_in_pool(p,ptr)) {
    pool_free(p, ptr);
    return true;
  } else {
#ifdef FALLBACK
    sys_free(ptr);
    return true;
#else
    return false;
#endif
  }
}

// Reallocate memory
void *fastalloc32_realloc(void *restrict pool, void *ptr, const size_t size) {
  if (ptr == NULL) {
    return fastalloc32_malloc(pool, size);
  }

  if (size == 0) {
    fastalloc32_free(pool, ptr);
    return NULL;
  }
  if (is_in_pool((fastpool_t*)pool,ptr)) {
    if (size <= BLOCK_SIZE) {
      return ptr; // No need to reallocate
    } else {
      void *new_ptr = sys_malloc(size);
      memcpy(new_ptr, ptr, BLOCK_SIZE); // Copy only BLOCK_SIZE bytes
#ifdef SECURE_ZERO
      secure_zero(ptr, BLOCK_SIZE); // Zero out the old block
#endif
      pool_free(pool, ptr);
      return new_ptr;
    }
  } else {
#ifdef FALLBACK
    // Handle large allocations
    return realloc(ptr, size);
#else
    return NULL;
#endif
  }
}

// Debug function to print memory manager state
void fastalloc32_status(void *restrict pool) {
  fastpool_t *p = (fastpool_t*)pool;
  fprintf(stderr,"⚡fastpool32 \t %u \t allocations managed\n",
          p->total_blocks - p->free_count);
}

#if defined(FASTALLOC32_TEST) && defined(FALLBACK)
#include <assert.h>
#include <time.h>

#ifndef NUM_ALLOCATIONS
#define NUM_ALLOCATIONS 20000
#endif

#ifndef MAX_ALLOCATION_SIZE
#define MAX_ALLOCATION_SIZE BLOCK_SIZE*2
#endif

int main(int argc, char **argv) {
  srand(time(NULL));
  void *pool = fastalloc32_create();

  void *pointers[NUM_ALLOCATIONS];
  int sizes[NUM_ALLOCATIONS];
  uint32_t in_pool = 0;

#if defined(__x86_64__) || defined(_M_X64) || defined(__ppc64__) || defined(__LP64__)
  fprintf(stderr,"Running in a 64-bit environment\n");
#else
  fprintf(stderr,"Running in a 32-bit environment\n");
#endif
  fprintf(stderr,"Testing with %u allocations\n",NUM_ALLOCATIONS);

  fprintf(stderr,"Step 1: Allocate memory\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    size_t size = rand() % MAX_ALLOCATION_SIZE + 1;
    pointers[i] = fastalloc32_malloc(pool, size);
    sizes[i] = size;
    if(size<=BLOCK_SIZE) in_pool++;
    assert(pointers[i] != NULL); // Ensure allocation was successful
  }
  fastalloc32_status(pool);

  assert(((fastpool_t*)pool)->total_blocks
         - ((fastpool_t*)pool)->free_count <= in_pool);
  fprintf(stderr,"Step 2: Free every other allocation\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i += 2) {
    fastalloc32_free(pool, pointers[i]);
    pointers[i] = NULL;
  }
  fastalloc32_status(pool);

  fprintf(stderr,"Step 3: Reallocate remaining memory\n");
  for (int i = 1; i < NUM_ALLOCATIONS; i += 2) {
    size_t new_size = rand() % (MAX_ALLOCATION_SIZE*4) + 1;
    pointers[i] = fastalloc32_realloc(pool, pointers[i], new_size);
    assert(pointers[i] != NULL); // Ensure reallocation was successful
  }
  fastalloc32_status(pool);

  fprintf(stderr,"Step 4: Free all memory\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    if (pointers[i] != NULL) {
      fastalloc32_free(pool, pointers[i]);
      pointers[i] = NULL;
    }
  }
  fastalloc32_status(pool);

  fprintf(stderr,"Step 5: Final check for memory leaks\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    assert(pointers[i] == NULL); // Ensure all pointers are freed
  }

  fastalloc32_destroy(pool);
  printf("Fastalloc32 test passed successfully.\n");
  return 0;
}

#endif
