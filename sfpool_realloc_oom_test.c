/* SPDX-FileCopyrightText: 2025 Dyne.org foundation
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>
#include <stdint.h>

#include <sfpool.h>

int main(void) {
  sfpool_t pool;
  void *ptr = NULL;
  void *grown = NULL;

  assert(sfpool_init(&pool, 4, 128) == 512);

  ptr = sfpool_malloc(&pool, 1);
  assert(ptr != NULL);
  assert(sfpool_contains(&pool, ptr) == 1);

  grown = sfpool_realloc(&pool, ptr, SIZE_MAX);
  assert(grown == NULL);
  assert(sfpool_contains(&pool, ptr) == 1);

  sfpool_free(&pool, ptr);
  sfpool_teardown(&pool);
  return 0;
}
