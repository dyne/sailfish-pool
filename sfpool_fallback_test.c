/* SPDX-FileCopyrightText: 2025 Dyne.org foundation
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>

#include <sfpool.h>

int main(void) {
  sfpool_t pool;
  void *pool_ptr = NULL;
  void *heap_ptr = NULL;

  assert(sfpool_init(&pool, 1, 128) == 128);

  pool_ptr = sfpool_malloc(&pool, 64);
  assert(pool_ptr != NULL);
  assert(sfpool_contains(&pool, pool_ptr) == 1);

  heap_ptr = sfpool_malloc(&pool, 64);
  assert(heap_ptr != NULL);
  assert(sfpool_contains(&pool, heap_ptr) == 0);

  heap_ptr = sfpool_realloc(&pool, heap_ptr, 256);
  assert(heap_ptr != NULL);
  assert(sfpool_contains(&pool, heap_ptr) == 0);

  sfpool_free(&pool, heap_ptr);
  sfpool_free(&pool, pool_ptr);
  sfpool_teardown(&pool);
  return 0;
}
