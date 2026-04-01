/* SPDX-FileCopyrightText: 2025 Dyne.org foundation
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sfpool.h>

int sfpool_multi_a(void) {
  sfpool_t pool;
  size_t total = sfpool_init(&pool, 4, sizeof(void*));
  if (total == 0) return 1;
  sfpool_teardown(&pool);
  return 0;
}
