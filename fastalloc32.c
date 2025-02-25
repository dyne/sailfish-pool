
/* SPDX-FileCopyrightText: 2025 Dyne.org foundation
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Fast memory allocator for 32 bit 'puters
 *
 * Copyright (C) 2025 Dyne.org foundation
 * designed, written and maintained by Denis Roio <jaromil@dyne.org>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

// #define POOL_SIZE 1024
// #define BLOCK_SIZE 32
// #define ALIGNMENT 8

#define POOL_SIZE (1024*4*128)
#define BLOCK_SIZE 128
#define ALIGNMENT 4

typedef struct Pool {
  unsigned char *data;
  unsigned char **free_list;
  size_t free_count;
  size_t total_blocks;
} Pool;

typedef struct LargeAllocation {
  void *ptr;
  size_t size;
  struct LargeAllocation *next;
} LargeAllocation;

typedef struct fastalloc32_mm {
  Pool pool;
  LargeAllocation *large_allocations;
} fastalloc32_mm;

static inline size_t align_up(size_t size, size_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

static inline void secure_zero(void *ptr, size_t size) {
    volatile unsigned char *p = ptr;
    while (size--) *p++ = 0;
}

static inline fastalloc32_mm *arg_manager(void *mm) {
  return (fastalloc32_mm*)mm;
}

static inline void pool_init(Pool *pool, size_t initial_size) {
  pool->data = (unsigned char *)malloc(initial_size);
  pool->total_blocks = initial_size / BLOCK_SIZE;
  pool->free_list = (unsigned char **)malloc
    (pool->total_blocks * sizeof(unsigned char *));
  pool->free_count = pool->total_blocks;
  for (size_t i = 0; i < pool->total_blocks; ++i) {
    pool->free_list[i] = &pool->data[i * BLOCK_SIZE];
  }
}

static inline void *pool_alloc(Pool *pool) {
  if (pool->free_count == 0) {
    return NULL; // Pool exhausted
  }
  return pool->free_list[--pool->free_count];
}

static inline void pool_free(Pool *pool, void *ptr) {
  pool->free_list[pool->free_count++] = (unsigned char *)ptr;
}

void *fastalloc32_create() {
  fastalloc32_mm *manager = malloc(sizeof(fastalloc32_mm));
  pool_init(&manager->pool, POOL_SIZE);
  manager->large_allocations = NULL;
  return (void*)manager;
}

void fastalloc32_destroy(void *mm) {
  fastalloc32_mm *manager = arg_manager(mm);;
  // Free large allocations
  LargeAllocation *current = manager->large_allocations;
  while (current) {
    LargeAllocation *next = current->next;
    secure_zero(current->ptr, current->size);
    free(current->ptr);
    free(current);
    current = next;
  }

  // Free pool memory
  free(manager->pool.data);
  free(manager->pool.free_list);
  // Free context
  free(manager);
}

void *fastalloc32_malloc(void *mm, size_t size) {
  fastalloc32_mm *manager = arg_manager(mm);;
  size = align_up(size, ALIGNMENT);
  if (size <= BLOCK_SIZE) {
    void *ptr = pool_alloc(&manager->pool);
    if (ptr != NULL) {
      return ptr;
    }
  }

  void *ptr = malloc(size);
  if (ptr == NULL) {
    return NULL; // malloc failed
  }

  LargeAllocation *large_alloc =
    (LargeAllocation *)malloc(sizeof(LargeAllocation));
  if (large_alloc == NULL) {
    free(ptr);
    return NULL; // malloc failed
  }

  large_alloc->ptr = ptr;
  large_alloc->size = size;
  large_alloc->next = manager->large_allocations;
  manager->large_allocations = large_alloc;

  return ptr;
}

void fastalloc32_free(void *mm, void *ptr) {
  fastalloc32_mm *manager = arg_manager(mm);;
  if (ptr >= (void *)manager->pool.data
      && ptr < (void *)(manager->pool.data
                        + manager->pool.total_blocks * BLOCK_SIZE)) {
    pool_free(&manager->pool, ptr);
  } else {
    LargeAllocation **curr = &manager->large_allocations;
    while (*curr) {
      if ((*curr)->ptr == ptr) {
        LargeAllocation *to_free = *curr;
        *curr = (*curr)->next;
        secure_zero(to_free->ptr, to_free->size);
        free(to_free->ptr);
        free(to_free);
        return;
      }
      curr = &(*curr)->next;
    }
    // If we reach here, ptr was not allocated by this memory manager
    free(ptr);
  }
}

void *fastalloc32_realloc(void *mm, void *ptr, size_t size) {
  fastalloc32_mm *manager = arg_manager(mm);;
  size = align_up(size, ALIGNMENT);
  if (ptr == NULL) {
    return fastalloc32_malloc(manager, size);
  }

  if (size == 0) {
    fastalloc32_free(manager, ptr);
    return NULL;
  }

  if (ptr >= (void *)manager->pool.data
      && ptr < (void *)(manager->pool.data
                        + manager->pool.total_blocks * BLOCK_SIZE)) {
    if (size <= BLOCK_SIZE) {
      return ptr; // No need to reallocate
    } else {
      void *new_ptr = fastalloc32_malloc(manager, size);
      if (new_ptr) {
        memcpy(new_ptr, ptr, BLOCK_SIZE);
        pool_free(&manager->pool, ptr);
      }
      return new_ptr;
    }
  } else {
    LargeAllocation **curr = &manager->large_allocations;
    while (*curr) {
      if ((*curr)->ptr == ptr) {
        void *new_ptr = realloc(ptr, size);
        if (new_ptr == NULL) {
          return NULL; // realloc failed
        }

        (*curr)->ptr = new_ptr;
        (*curr)->size = size;
        return new_ptr;
      }
      curr = &(*curr)->next;
    }

    // Handle realloc for memory allocated outside the manager's control
    return realloc(ptr, size);
  }
}

#if defined(FASTALLOC32_TEST)
#include <assert.h>
#include <time.h>

#define NUM_ALLOCATIONS 20000
#define MAX_ALLOCATION_SIZE 1024

int main(int argc, char **argv) {
  srand(time(NULL));
  fastalloc32_mm *manager = fastalloc32_create();

  void *pointers[NUM_ALLOCATIONS];

  fprintf(stderr,"Step 1: Allocate memory\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    size_t size = rand() % MAX_ALLOCATION_SIZE + 1;
    pointers[i] = fastalloc32_malloc(manager, size);
    assert(pointers[i] != NULL); // Ensure allocation was successful
  }

  fprintf(stderr,"Step 2: Free every other allocation\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i += 2) {
    fastalloc32_free(manager, pointers[i]);
    pointers[i] = NULL;
  }

  fprintf(stderr,"Step 3: Reallocate remaining memory\n");
  for (int i = 1; i < NUM_ALLOCATIONS; i += 2) {
    size_t new_size = rand() % MAX_ALLOCATION_SIZE + 1;
    pointers[i] = fastalloc32_realloc(manager, pointers[i], new_size);
    assert(pointers[i] != NULL); // Ensure reallocation was successful
  }

  fprintf(stderr,"Step 4: Free all memory\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    if (pointers[i] != NULL) {
      fastalloc32_free(manager, pointers[i]);
      pointers[i] = NULL;
    }
  }

  fprintf(stderr,"Step 5: Final check for memory leaks\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    assert(pointers[i] == NULL); // Ensure all pointers are freed
  }

  fastalloc32_destroy(manager);
  printf("Fastalloc32 test passed successfully.\n");
  return 0;
}

#endif
