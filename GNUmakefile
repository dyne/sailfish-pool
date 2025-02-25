# SPDX-FileCopyrightText: 2025 Dyne.org foundation
# SPDX-License-Identifier: GPL-3.0-or-later

CC ?= gcc

CFLAGS ?= -g -fsanitize=address -fsanitize=undefined						\
-fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow	\
-fsanitize=leak

# emscripten settings
ld_emsdk_settings := -I ${EMSCRIPTEN}/system/include/libc
ld_emsdk_settings += -s MODULARIZE=1 -s SINGLE_FILE=1
ld_emsdk_settings += -s STACK_SIZE=65536kb -s MALLOC=dlmalloc --no-heap-copy -sALLOW_MEMORY_GROWTH=1
ld_emsdk_settings += -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'
ld_emsdk_settings += -s EXPORTED_FUNCTIONS='["_malloc","_free","_calloc","_realloc","_main"]'
ld_emsdk_optimizations := -g -sSTRICT -flto -sUSE_SDL=0 -sEVAL_CTORS=1
cc_emsdk_optimizations := -g -sSTRICT -flto -fno-rtti -fno-exceptions
emsdk_cflags  := ${cc_emsdk_optimizations}
emsdk_ldflags := ${ld_emsdk_optimizations} ${ld_emsdk_settings}

check:
	$(info Build fastalloc32 and run test.)
	@$(CC) $(CFLAGS) -DFASTALLOC32_TEST fastalloc32.c -o fastalloc32_test
	@time -f "Command executed in:\n\tUser time: %U seconds\n\tSystem time: %S seconds\n\tElapsed (real) time: %E" ./fastalloc32_test

lib:
	$(info Build libfastalloc32.so)
	@$(CC) -O3 -fPIC -shared fastalloc32.c -o libfastalloc32.so

wasm:
	$(info Build Web Assembly target with EMSCRIPTEN and run test.)
	@${EMSDK}/upstream/emscripten/emcc \
		${emsdk_cflags} -DFASTALLOC32_TEST fastalloc32.c -o fastalloc32.js ${emsdk_ldflags}
	@node -e "require('./fastalloc32.js')()"

clean:
	@rm -f *.o fastalloc32_test
	$(info Build clean.)
