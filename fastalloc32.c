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

// platform dependent memory allocation interfaces
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

// Configuration
#define SECURE_ZERO // Enable secure zeroing
#define FALLBACK // Enable fallback to system alloc

// Memory pool structure
typedef struct fastpool_t {
  uint8_t *data;
  uint8_t *free_list; // Pointer to the first free block
  uint32_t free_count;
  uint32_t total_blocks;
  uint32_t total_bytes;
  uint32_t block_size;
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

// Pool initialization: allocates memory for an array of nmemb
// elements of size bytes each and returns a pointer to the allocated
// memory pool
bool sfpool_init(fastpool_t *pool, size_t nmemb, size_t size) {
  if(size & (size - 1) != 0) {
    fprintf(stderr,"SFPool blocksize must be a power of two\n");
    return false;
  }
  size_t totalsize = nmemb * size;
#if defined(__EMSCRIPTEN__)
  pool->data = (uint8_t *)sys_malloc(totalsize);

#elif defined(_WIN32)
  pool->data = VirtualAlloc(NULL, totalsize,
                            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

#else // Posix
  pool->data = mmap(NULL, totalsize, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE
                    , -1, 0);
#endif
  if (pool->data == NULL) {
    fprintf(stderr, "Failed to allocate pool memory\n");
    return false;
  }

  pool->total_blocks = (uint32_t)nmemb;
  pool->block_size   = (uint32_t)size;
  pool->total_bytes  = (uint32_t)totalsize;
  // Initialize the embedded free list
  pool->free_list = pool->data;
  for (volatile uint32_t i = 0; i < pool->total_blocks - 1; ++i) {
    *(uint8_t **)(pool->data + i * size) =
      pool->data + (i + 1) * size;
  }
  pool->free_count = pool->total_blocks;
  *(uint8_t **)
    (pool->data + (pool->total_blocks - 1)
     * size) = NULL;
  return true;
}

// Destroy memory manager
void sfpool_teardown(void *restrict pool) {
  fastpool_t *p = (fastpool_t*)pool;
  // Free pool memory
#if defined(__EMSCRIPTEN__)
  free(p->data);
#elif defined(_WIN32)
  VirtualFree(p->data, 0, MEM_RELEASE);
#else // Posix
  munmap(p->data, p->total_bytes);
#endif
}

// Allocate memory
void *sfpool_malloc(void *restrict pool, const size_t size) {
  fastpool_t *restrict sfp = (fastpool_t*)pool;
  if (!sfp->free_list) return NULL; // pool is full
  if (size <= sfp->block_size) {
    // Remove the first block from the free list
    uint8_t *block = sfp->free_list;
    sfp->free_list = *(uint8_t **)block;
    sfp->free_count-- ;
    return block;
  }
  void *ptr = NULL;
#ifdef FALLBACK
  ptr = malloc(size);
  if(ptr == NULL) perror("system malloc error");
#endif
  return ptr;
}

// Free memory
bool sfpool_free(void *restrict pool, void *ptr) {
  if (!ptr) return false; // Freeing NULL is a no-op
  fastpool_t *sfp = (fastpool_t*)pool;
  if (is_in_pool(sfp,ptr)) {
    // Add the block back to the free list
    *(uint8_t **)ptr = sfp->free_list;
    sfp->free_list = (uint8_t *)ptr;
    sfp->free_count++ ;
#ifdef SECURE_ZERO
    // Zero out the block for security
    secure_zero(ptr, sfp->block_size);
#endif
    return true;
  } else {
#ifdef FALLBACK
    free(ptr);
    return true;
#else
    return false;
#endif
  }
}

#ifdef FALLBACK
// Reallocate memory
void *sfpool_realloc(void *restrict pool, void *ptr, const size_t size) {
  if (!ptr) return sfpool_malloc(pool, size);
  if (size == 0) { sfpool_free(pool, ptr); return NULL; }
  fastpool_t *sfp = (fastpool_t*) pool;
  if (is_in_pool(sfp, ptr)) {
    if (size <= sfp->block_size) {
      return ptr; // No need to reallocate
    } else {
      void *new_ptr = malloc(size);
      memcpy(new_ptr, ptr, sfp->block_size); // Copy all block bytes
#ifdef SECURE_ZERO
      secure_zero(ptr, sfp->block_size); // Zero out the old block
#endif
      sfpool_free(pool, ptr);
      return new_ptr;
    }
  } else {
    // Handle large allocations
    return realloc(ptr, size);
  }
  return NULL;
}
#endif

// Debug function to print memory manager state
void sfpool_status(void *restrict pool) {
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
#define MAX_ALLOCATION_SIZE 256
#endif

#define BLOCKSIZE 128
#define BLOCKNUM (2 * 8192) // two MiBs

int main(int argc, char **argv) {
  srand(time(NULL));
  void *pool = malloc(sizeof(fastpool_t));
  if (!pool) {
    perror("memory pool creation error");
    return 1;
  }
  assert( sfpool_init(pool, BLOCKNUM, BLOCKSIZE) );
  void *pointers[NUM_ALLOCATIONS];
  int sizes[NUM_ALLOCATIONS];

#if defined(__x86_64__) || defined(_M_X64) || defined(__ppc64__) || defined(__LP64__)
  fprintf(stderr,"Running in a 64-bit environment\n");
#else
  fprintf(stderr,"Running in a 32-bit environment\n");
#endif
  fprintf(stderr,"Testing with %u allocations\n",NUM_ALLOCATIONS);

  fprintf(stderr,"Step 1: Allocate memory\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    size_t size = rand() % MAX_ALLOCATION_SIZE + 1;
    pointers[i] = sfpool_malloc(pool, size);
    sizes[i] = size;
    assert(pointers[i] != NULL); // Ensure allocation was successful
  }
  sfpool_status(pool);

  fprintf(stderr,"Step 2: Free every other allocation\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i += 2) {
    sfpool_free(pool, pointers[i]);
    pointers[i] = NULL;
  }
  sfpool_status(pool);

  fprintf(stderr,"Step 3: Reallocate remaining memory\n");
  for (int i = 1; i < NUM_ALLOCATIONS; i += 2) {
    size_t new_size = rand() % (MAX_ALLOCATION_SIZE*4) + 1;
    pointers[i] = sfpool_realloc(pool, pointers[i], new_size);
    assert(pointers[i] != NULL); // Ensure reallocation was successful
  }
  sfpool_status(pool);

  fprintf(stderr,"Step 4: Free all memory\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    if (pointers[i] != NULL) {
      sfpool_free(pool, pointers[i]);
      pointers[i] = NULL;
    }
  }
  sfpool_status(pool);

  fprintf(stderr,"Step 5: Final check for memory leaks\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    assert(pointers[i] == NULL); // Ensure all pointers are freed
  }

  sfpool_teardown(pool);
  free(pool);
  printf("Sailfish Pool test passed successfully.\n");
  return 0;
}

#endif
