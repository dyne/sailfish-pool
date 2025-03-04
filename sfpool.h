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

#ifndef __SFPOOL_H__
#define __SFPOOL_H__
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
#define SECURE_ZERO // Enable secure zeroing
#define FALLBACK   // Enable fallback to system alloc
#define PROFILING // Profile most used sizes allocated

// Memory pool structure
typedef struct sfpool_t {
  uint8_t *data;
  uint8_t *free_list;
  uint32_t free_count;
  uint32_t total_blocks;
  uint32_t total_bytes;
  uint32_t block_size;
} sfpool_t;

#ifdef PROFILING
uint32_t *hits;
uint32_t hits_total;
size_t hits_bytes;
uint32_t miss_total;
size_t miss_bytes;
#define profile(pool,s) \
if(s<pool->block_size) { hits[s]++; hits_total++; hits_bytes+=s; } \
else { miss_total++; miss_bytes+=s; }
#endif

static inline void _secure_zero(void *ptr, uint32_t size) {
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

static inline bool _is_in_pool(sfpool_t *pool, const void *ptr) {
  volatile ptr_t p = (ptr_t)ptr;
  return(p >= (ptr_t)pool->data
         && p < (ptr_t)(pool->data + pool->total_bytes));
}

// Create memory manager
size_t sfpool_init(sfpool_t *pool, size_t nmemb, size_t blocksize) {
  if((blocksize & (blocksize - 1)) != 0) {
    fprintf(stderr,"SFPool blocksize must be a power of two\n");
    return -1;
  }
  size_t totalsize = nmemb * blocksize;
#if defined(__EMSCRIPTEN__)
  pool->data = (uint8_t *)malloc(totalsize);
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
    exit(EXIT_FAILURE);
  }
  // Zero out the entire pool
  _secure_zero(pool->data, totalsize);
  pool->total_bytes  = totalsize;
  pool->total_blocks = nmemb;
  pool->block_size   = blocksize;
  // Initialize the embedded free list
  pool->free_list = pool->data;
  for (volatile uint32_t i = 0; i < pool->total_blocks - 1; ++i) {
    *(uint8_t **)(pool->data + i * blocksize) =
      pool->data + (i + 1) * blocksize;
  }
  pool->free_count = pool->total_blocks;
  *(uint8_t **)
    (pool->data + (pool->total_blocks - 1) * blocksize) = NULL;
#ifdef PROFILING
  hits = calloc(blocksize+4,sizeof(uint32_t));
  miss_total = miss_bytes = 0;
  hits_total = hits_bytes = 0;
#endif
  return totalsize;
}

// Destroy memory manager
void sfpool_teardown(sfpool_t *restrict pool) {
  // Free pool memory
#if defined(__EMSCRIPTEN__)
  free(pool->data);
#elif defined(_WIN32)
  VirtualFree(pool->data, 0, MEM_RELEASE);
#else // Posix
  munmap(pool->data, pool->total_bytes);
#endif
#ifdef PROFILING
  free(hits);
  miss_total = miss_bytes = 0;
  hits_total = hits_bytes = 0;
#endif
}

// Allocate memory
void *sfpool_malloc(sfpool_t *restrict pool, const size_t size) {
  void *ptr;
  if (size <= pool->block_size
      && pool->free_list != NULL) {
    // Remove the first block from the free list
    uint8_t *block = pool->free_list;
    pool->free_list = *(uint8_t **)block;
    pool->free_count-- ;
#ifdef PROFILING
    profile(pool,size);
#endif
    return block;
  }
  // Fallback to system malloc for large allocations
  ptr = malloc(size);
  if(ptr == NULL) perror("system malloc error");
#ifdef PROFILING
  profile(pool,size);
#endif
  return ptr;
}

// Free memory
bool sfpool_free(sfpool_t *restrict pool, void *ptr) {
  if (ptr == NULL) return false; // Freeing NULL is a no-op
  if (_is_in_pool(pool,ptr)) {
    // Add the block back to the free list
    *(uint8_t **)ptr = pool->free_list;
    pool->free_list = (uint8_t *)ptr;
    pool->free_count++ ;
#ifdef SECURE_ZERO
    // Zero out the block for security
    _secure_zero(ptr, pool->block_size);
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

// Reallocate memory
void *sfpool_realloc(sfpool_t *restrict pool, void *ptr, const size_t size) {
  if (ptr == NULL) {
    return sfpool_malloc(pool, size);
  }
  if (size == 0) {
    sfpool_free(pool, ptr);
    return NULL;
  }
  if (_is_in_pool((sfpool_t*)pool,ptr)) {
    if (size <= pool->block_size) {
#ifdef PROFILING
      profile(pool,size);
#endif
      return ptr; // No need to reallocate
    } else {
      void *new_ptr = malloc(size);
      memcpy(new_ptr, ptr, pool->block_size); // Copy only BLOCK_SIZE bytes
#ifdef SECURE_ZERO
      _secure_zero(ptr, pool->block_size); // Zero out the old block
#endif
      // Add the block back to the free list
      *(uint8_t **)ptr = pool->free_list;
      pool->free_list = (uint8_t *)ptr;
      pool->free_count++ ;
#ifdef SECURE_ZERO
      // Zero out the block for security
      _secure_zero(ptr, pool->block_size);
#endif
#ifdef PROFILING
      profile(pool,size);
#endif
      return new_ptr;
    }
  } else {
#ifdef FALLBACK
    // Handle large allocations
    return realloc(ptr, size);
#ifdef PROFILING
    profile(pool,size);
#endif
#else
    return NULL;
#endif
  }
}

// Debug function to print memory manager state
void sfpool_status(sfpool_t *restrict p) {
  fprintf(stderr,"\n🌊 sfpool: %u blocks %u B each\n",
          p->total_blocks, p->block_size);
#ifdef PROFILING
  fprintf(stderr,"🌊 Misses: %u - %lu K\n",miss_total,miss_bytes/1024);
  fprintf(stderr,"🌊 Hits:   %u - %lu K\n",hits_total,hits_bytes/1024);
  // for (uint32_t i = 1; i <= p->block_size; i++) {
  //   fprintf(stdout,"%u %u\n",i,hits[i]);
  // }
#endif
}
#endif
