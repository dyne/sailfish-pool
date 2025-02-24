# SPDX-FileCopyrightText: 2025 Dyne.org foundation
# SPDX-License-Identifier: GPL-3.0-or-later

CC ?= gcc

CFLAGS ?= -g -fsanitize=address -fsanitize=undefined						\
-fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow	\
-fsanitize=leak

check:
	$(info Build fastalloc32 and run test.)
	@$(CC) $(CFLAGS) -DFASTALLOC32_TEST fastalloc32.c -o fastalloc32_test
	@time -f "Command executed in:\n\tUser time: %U seconds\n\tSystem time: %S seconds\n\tElapsed (real) time: %E" ./fastalloc32_test

clean:
	@rm -f *.o fastalloc32_test
	$(info Build clean.)
