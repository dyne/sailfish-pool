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
#else // lucky shot on POSIX
// defined(__unix__) || defined(__linux__) || defined(__APPLE__) ||  defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <unistd.h> // for geteuid to switch protected memory map
#include <sys/resource.h>
#include <sys/mman.h>
#endif

// Configuration
#define SECURE_ZERO // Enable secure zeroing
#define FALLBACK   // Enable fallback to system alloc
#define PROFILING // Profile most used sizes allocated

#if defined(__x86_64__) || defined(_M_X64) || defined(__ppc64__) || defined(__LP64__)
#define ptr_t uint64_t
#define ptr_align 8
#define struct_align 16
#else
#define ptr_t uint32_t
#define ptr_align 4
#define struct_align 8
#endif

// Memory pool structure
typedef struct __attribute__((aligned(struct_align))) sfpool_t {
  uint8_t *buffer; // raw
  uint8_t *data; // aligned
  uint8_t *free_list;
  uint32_t free_count;
  uint32_t total_blocks;
  uint32_t total_bytes;
  uint32_t block_size;
#ifdef PROFILING
  uint32_t *hits;
  uint32_t hits_total;
  size_t   hits_bytes;
  uint32_t miss_total;
  size_t   miss_bytes;
  size_t   alloc_total;
#endif
} sfpool_t;


#if !defined(__MUSL__)
static_assert(sizeof(ptr_t) == sizeof(void*), "Unknown memory pointer size detected");
#endif
static inline bool _is_in_pool(sfpool_t *pool, const void *ptr) {
  volatile ptr_t p = (ptr_t)ptr;
  return(p >= (ptr_t)pool->data
         && p < (ptr_t)(pool->data + pool->total_bytes));
}

/**
 * @defgroup sfutil Internal Utilities
 * @{
 */

/**
 * @brief Zeroes out a block of memory.
 *
 * This function sets a block of memory to zero using 32-bit writes for efficiency.
 *
 * @param ptr Pointer to the memory block to zero out.
 * @param size Size of the memory block in bytes.
 */
void  sfutil_zero(void *ptr, uint32_t size) {
  register uint32_t *p = (uint32_t*)ptr; // use 32bit pointer
  register uint32_t s = (size>>2); // divide counter by 4
  while (s--) *p++ = 0x0; // hit the road jack
}

/**
 * @brief Aligns a pointer to the nearest boundary.
 *
 * This function aligns the given pointer to the nearest boundary specified by `ptr_align`.
 *
 * @param ptr Pointer to align.
 * @return Aligned pointer.
 */
void *sfutil_memalign(const void* ptr) {
    register ptr_t mask = ptr_align - 1;
    ptr_t aligned = ((ptr_t)ptr + mask) & ~mask;
    return (void*)aligned;
}

/**
 * @brief Allocates memory securely.
 *
 * This function allocates memory securely, ensuring it is aligned and locked (if supported by the platform).
 *
 * @param size Size of the memory block to allocate.
 * @return Pointer to the allocated memory block, or NULL on failure.
 */
void *sfutil_secalloc(size_t size) {
	// add bytes to every allocation to support alignment
	void *res = NULL;
#if defined(__EMSCRIPTEN__)
	res = (uint8_t *)malloc(size+ptr_align);
#elif defined(_WIN32)
	res = VirtualAlloc(NULL, size+ptr_align,
					   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif defined(__APPLE__)
	int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
	res = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
	mlock(res, size);
#else // assume POSIX
	int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
	struct rlimit rl;
	if (getrlimit(RLIMIT_MEMLOCK, &rl) == 0)
		if(size<=rl.rlim_cur) flags |= MAP_LOCKED;
	res = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
#endif
	return res;
}

/**
 * @brief Frees memory allocated securely.
 *
 * This function frees memory that was allocated using `sfutil_secalloc`.
 *
 * @param ptr Pointer to the memory block to free.
 * @param size Size of the memory block in bytes.
 */
void sfutil_secfree(void *ptr, size_t size) {
#if defined(__EMSCRIPTEN__)
	free(ptr);
#elif defined(_WIN32)
	VirtualFree(ptr, 0, MEM_RELEASE);
#else // Posix
	munmap(ptr, size +ptr_align);
#endif
}

/** @} */ // End of sfutil group


/**
 * @defgroup sfpool High-Level API
 * @{
 */

/**
 * @brief Initializes a memory pool.
 *
 * This function initializes a memory pool with a specified number of blocks and block size.
 * The block size must be a power of two.
 *
 * @param pool Pointer to the memory pool structure to initialize.
 * @param nmemb Number of blocks in the pool.
 * @param blocksize Size of each block in bytes.
 * @return Total size of the memory pool in bytes, or 0 on failure.
 */
size_t sfpool_init(sfpool_t *pool, size_t nmemb, size_t blocksize) {
  if((blocksize & (blocksize - 1)) != 0) return 0;
  // SFPool blocksize must be a power of two
  size_t totalsize = nmemb * blocksize;
  pool->buffer = sfutil_secalloc(totalsize);
  if (pool->buffer == NULL) return 0;
  // Failed to allocate pool memory
  pool->data   = sfutil_memalign(pool->buffer);
  if (pool->data == NULL) return 0;
  // Failed to allocate pool memory
  pool->total_bytes  = totalsize;
  pool->total_blocks = nmemb;
  pool->block_size   = blocksize;
  // Initialize the embedded free list
  pool->free_list = pool->data;
  register uint32_t i, bi;
  for (i = 0; i < pool->total_blocks - 1; ++i) {
    bi = i*blocksize;
    *(uint8_t **)(pool->data + bi) =
      pool->data + bi + blocksize;
  }
  pool->free_count = pool->total_blocks;
  *(uint8_t **)
    (pool->data + (pool->total_blocks - 1) * blocksize) = NULL;
#ifdef PROFILING
  pool->miss_total = pool->miss_bytes = 0;
  pool->hits_total = pool->hits_bytes = 0;
  pool->alloc_total = 0;
#endif
  return totalsize;
}


/**
 * @brief Tears down a memory pool.
 *
 * This function releases all resources associated with the memory pool.
 *
 * @param pool Pointer to the memory pool structure to tear down.
 */
void sfpool_teardown(sfpool_t *restrict pool) {
  // Free pool memory
  sfutil_secfree(pool->buffer, pool->total_bytes);
#ifdef PROFILING
  pool->miss_total = pool->miss_bytes = 0;
  pool->hits_total = pool->hits_bytes = 0;
  pool->alloc_total = 0;
#endif
}

/**
 * @brief Allocates memory from the pool.
 *
 * This function allocates memory from the pool if the requested size is within the block size.
 * Otherwise, it falls back to system malloc.
 *
 * @param opaque Pointer to the memory pool structure.
 * @param size Size of the memory block to allocate.
 * @return Pointer to the allocated memory block, or NULL on failure.
 */
void *sfpool_malloc(void *restrict opaque, const size_t size) {
  sfpool_t *pool = (sfpool_t*)opaque;
  void *ptr;
  if (size <= pool->block_size
      && pool->free_list != NULL) {
#ifdef PROFILING
    pool->hits_total++;
    pool->hits_bytes+=size;
    pool->alloc_total+=size;
#endif
    // Remove the first block from the free list
    uint8_t *block = pool->free_list;
    pool->free_list = *(uint8_t **)block;
    pool->free_count-- ;
    return block;
  }
  // Fallback to system malloc for large allocations
  ptr = malloc(size);
  if(ptr == NULL) perror("system malloc error");
#ifdef PROFILING
  pool->miss_total++;
  pool->miss_bytes+=size;
  pool->alloc_total+=size;
#endif
  return ptr;
}


/**
 * @brief Frees memory allocated from the pool.
 *
 * This function frees memory that was allocated from the pool. If the memory was not allocated
 * from the pool, it falls back to system free (if enabled).
 *
 * @param opaque Pointer to the memory pool structure.
 * @param ptr Pointer to the memory block to free.
 */
void sfpool_free(void *restrict opaque, void *ptr) {
  sfpool_t *pool = (sfpool_t*)opaque;
  if (ptr == NULL) return; // Freeing NULL is a no-op
  if (_is_in_pool(pool,ptr)) {
    // Add the block back to the free list
    *(uint8_t **)ptr = pool->free_list;
    pool->free_list = (uint8_t *)ptr;
    pool->free_count++ ;
#ifdef SECURE_ZERO
    // Zero out the block for security
    sfutil_zero(ptr, pool->block_size);
#endif
    return;
  } else {
#ifdef FALLBACK
    free(ptr);
#endif
  }
}


/**
 * @brief Reallocates memory from the pool.
 *
 * This function reallocates memory from the pool. If the new size is larger than the block size,
 * it allocates new memory using system malloc and copies the old data.
 *
 * @param opaque Pointer to the memory pool structure.
 * @param ptr Pointer to the memory block to reallocate.
 * @param size New size of the memory block.
 * @return Pointer to the reallocated memory block, or NULL on failure.
 */
void *sfpool_realloc(void *restrict opaque, void *ptr, const size_t size) {
  sfpool_t *pool = (sfpool_t*)opaque;
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
      pool->hits_total++;
      pool->hits_bytes+=size;
      pool->alloc_total+=size;
#endif
      return ptr; // No need to reallocate
    } else {
      void *new_ptr = malloc(size);
      memcpy(new_ptr, ptr, pool->block_size); // Copy only BLOCK_SIZE bytes
#ifdef SECURE_ZERO
      sfutil_zero(ptr, pool->block_size); // Zero out the old block
#endif
      // Add the block back to the free list
      *(uint8_t **)ptr = pool->free_list;
      pool->free_list = (uint8_t *)ptr;
      pool->free_count++ ;
#ifdef SECURE_ZERO
      // Zero out the block for security
      sfutil_zero(ptr, pool->block_size);
#endif
#ifdef PROFILING
  pool->miss_total++;
  pool->miss_bytes+=size;
  pool->alloc_total+=size;
#endif
      return new_ptr;
    }
  } else {
#ifdef FALLBACK
    // Handle large allocations
    return realloc(ptr, size);
#ifdef PROFILING
    pool->miss_total++;
    pool->miss_bytes+=size;
    pool->alloc_total+=size;
#endif
#else
    return NULL;
#endif
  }
}

/**
 * @brief Checks if a pointer is within the memory pool.
 *
 * This function checks if the given pointer is within the memory pool.
 *
 * @param opaque Pointer to the memory pool structure.
 * @param ptr Pointer to check.
 * @return 1 if the pointer is within the pool, 0 otherwise.
 */
int sfpool_contains(void *restrict opaque, const void *ptr) {
  sfpool_t *pool = (sfpool_t*)opaque;
  int res = 0;
  if( _is_in_pool(pool,ptr) ) res = 1;
  return res;
}


/**
 * @brief Prints the status of the memory pool.
 *
 * This function prints the current status of the memory pool, including the number of blocks,
 * block size, and profiling information (if enabled).
 *
 * @param p Pointer to the memory pool structure.
 */
void sfpool_status(sfpool_t *restrict p) {
  fprintf(stderr,"\n🌊 sfpool: %u blocks %u B each\n",
          p->total_blocks, p->block_size);
#ifdef PROFILING
  fprintf(stderr,"🌊 Total:  %lu K\n",
          p->alloc_total/1024);
  fprintf(stderr,"🌊 Misses: %lu K (%u calls)\n",p->miss_bytes/1024,p->miss_total);
  fprintf(stderr,"🌊 Hits:   %lu K (%u calls)\n",p->hits_bytes/1024,p->hits_total);
#endif
}

/** @} */ // End of sfpool group

#endif
