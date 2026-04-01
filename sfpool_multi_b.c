/* SPDX-FileCopyrightText: 2025 Dyne.org foundation
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sfpool.h>

int sfpool_multi_a(void);

int main(void) {
  sfpool_t pool;
  void *ptr = NULL;

  if (sfpool_init(&pool, 4, sizeof(void*)) == 0) return 1;
  ptr = sfpool_malloc(&pool, 1);
  if (ptr == NULL) return 1;
  sfpool_free(&pool, ptr);
  sfpool_teardown(&pool);

  return sfpool_multi_a();
}
