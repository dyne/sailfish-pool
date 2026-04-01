/* SPDX-FileCopyrightText: 2025 Dyne.org foundation
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <assert.h>
#include <stdint.h>

#include <sfpool.h>

int main(void) {
  uint8_t buffer[7] = {1, 2, 3, 4, 5, 6, 7};

  sfutil_zero(buffer, sizeof(buffer));

  for (size_t i = 0; i < sizeof(buffer); ++i) {
    assert(buffer[i] == 0);
  }

  return 0;
}
