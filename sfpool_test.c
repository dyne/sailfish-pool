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

#include <sfpool.h>

#include <assert.h>
#include <time.h>

#ifndef NUM_ALLOCATIONS
#define NUM_ALLOCATIONS 80000
#endif

#ifndef MAX_ALLOCATION_SIZE
#define MAX_ALLOCATION_SIZE 256
#endif

static void test_free_list_relink(sfpool_t *pool) {
  if (pool->total_blocks < 3) return;

  void *first = sfpool_malloc(pool, 1);
  void *second = sfpool_malloc(pool, 1);
  void *reused = NULL;
  void *next = NULL;

  assert(first != NULL);
  assert(second != NULL);
  assert(sfpool_contains(pool, first) == 1);
  assert(sfpool_contains(pool, second) == 1);

  sfpool_free(pool, first);

  reused = sfpool_malloc(pool, 1);
  next = sfpool_malloc(pool, 1);

  assert(reused == first);
  assert(next != NULL);
  assert(sfpool_contains(pool, next) == 1);

  sfpool_free(pool, reused);
  sfpool_free(pool, second);
  sfpool_free(pool, next);
}

static void test_init_validation(void) {
  sfpool_t invalid_pool;
  sfpool_t valid_pool;
  size_t valid_blocksize = sizeof(void*);
  size_t invalid_blocksize = valid_blocksize >> 1;

  assert(sfpool_init(NULL, 4, valid_blocksize) == 0);

  assert(sfpool_init(&invalid_pool, 0, valid_blocksize) == 0);
  assert(invalid_pool.buffer == NULL);
  assert(invalid_pool.data == NULL);
  assert(invalid_pool.total_blocks == 0);
  assert(invalid_pool.block_size == 0);

  assert(sfpool_init(&invalid_pool, 4, 1) == 0);
  assert(sfpool_init(&invalid_pool, 4, 2) == 0);
  if (invalid_blocksize >= 4) {
    assert(sfpool_init(&invalid_pool, 4, invalid_blocksize) == 0);
  }
  assert(invalid_pool.buffer == NULL);

  assert(sfpool_init(&invalid_pool, SIZE_MAX, valid_blocksize) == 0);
  assert(invalid_pool.buffer == NULL);

  assert(sfpool_init(&valid_pool, 4, valid_blocksize) == 4 * valid_blocksize);
  assert(valid_pool.buffer != NULL);
  assert(valid_pool.total_blocks == 4);
  assert(valid_pool.block_size == valid_blocksize);
  sfpool_teardown(&valid_pool);
}

int main(int argc, char **argv) {
  srand(time(NULL));
  fprintf(stderr,"Size of sfpool_t: %lu\n",sizeof(sfpool_t));
  void *pool = malloc(sizeof(sfpool_t));
  int blocknum = (argc > 1) ? atoi(argv[1]) : 0;
  if(!blocknum) blocknum = 1024;
  int blocksize = (argc > 2) ? atoi(argv[2]) : 0;
  if(!blocksize) blocksize = 256;

  test_init_validation();
  sfpool_init(pool, blocknum, blocksize);
  test_free_list_relink(pool);

  void *pointers[NUM_ALLOCATIONS];

#if defined(__x86_64__) || defined(_M_X64) || defined(__ppc64__) || defined(__LP64__)
  fprintf(stdout,"Running in a 64-bit environment\n");
#else
  fprintf(stdout,"Running in a 32-bit environment\n");
#endif
  fprintf(stdout,"Testing with %u allocations\n",NUM_ALLOCATIONS);

  fprintf(stdout,"Step 1: Allocate memory\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    size_t size = rand() % MAX_ALLOCATION_SIZE + 1;
    pointers[i] = sfpool_malloc(pool, size);
    assert(pointers[i] != NULL); // Ensure allocation was successful
  }
  // sfpool_status(pool);

  fprintf(stdout,"Step 2: Free every other allocation\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i += 2) {
    sfpool_free(pool, pointers[i]);
    pointers[i] = NULL;
  }
  // sfpool_status(pool);

  fprintf(stdout,"Step 3: Reallocate remaining memory\n");
  for (int i = 1; i < NUM_ALLOCATIONS; i += 2) {
    size_t new_size = rand() % (MAX_ALLOCATION_SIZE*4) + 1;
    pointers[i] = sfpool_realloc(pool, pointers[i], new_size);
    assert(pointers[i] != NULL); // Ensure reallocation was successful
  }
  // sfpool_status(pool);

  fprintf(stdout,"Step 4: Free all memory\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    if (pointers[i] != NULL) {
      sfpool_free(pool, pointers[i]);
      pointers[i] = NULL;
    }
  }
  sfpool_status(pool);

  fprintf(stdout,"Step 5: Final check for memory leaks\n");
  for (int i = 0; i < NUM_ALLOCATIONS; i++) {
    assert(pointers[i] == NULL); // Ensure all pointers are freed
  }

  sfpool_teardown(pool);
  free(pool);
  fprintf(stdout,"Sailfish Pool test passed successfully.\n");
  return 0;
}
