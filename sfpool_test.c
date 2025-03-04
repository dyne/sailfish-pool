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

#if defined(FALLBACK)
#include <assert.h>
#include <time.h>

#ifndef NUM_ALLOCATIONS
#define NUM_ALLOCATIONS 80000
#endif

#ifndef MAX_ALLOCATION_SIZE
#define MAX_ALLOCATION_SIZE 256
#endif

int main(int argc, char **argv) {
  srand(time(NULL));
  fprintf(stderr,"Size of sfpool_t: %lu\n",sizeof(sfpool_t));
  void *pool = malloc(sizeof(sfpool_t));
  sfpool_init(pool, atoi(argv[1]), atoi(argv[2]));

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

#endif
