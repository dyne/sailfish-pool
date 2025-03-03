# SPDX-FileCopyrightText: 2025 Dyne.org foundation
# SPDX-License-Identifier: GPL-3.0-or-later

CC ?= gcc

CFLAGS ?= -g -fsanitize=address -fsanitize=undefined						\
-fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow	\
-fsanitize=leak

# EMSCRIPTEN settings
JS_INIT_MEM :=8MB
JS_MAX_MEM := 256MB
JS_STACK_SIZE := 7MB
ld_emsdk_settings := -I ${EMSCRIPTEN}/system/include/libc
ld_emsdk_settings += -s MODULARIZE=1 -s SINGLE_FILE=1
ld_emsdk_settings += -s INITIAL_MEMORY=${JS_INIT_MEM} -s MAXIMUM_MEMORY=${JS_MAX_MEM}
ld_emsdk_settings += -s STACK_SIZE=${JS_STACK_SIZE} -s MALLOC=dlmalloc --no-heap-copy -sALLOW_MEMORY_GROWTH=1
ld_emsdk_settings += -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'
ld_emsdk_settings += -s EXPORTED_FUNCTIONS='["_malloc","_free","_calloc","_realloc","_main"]'
ld_emsdk_optimizations := -g -sUSE_SDL=0 -sEVAL_CTORS=1
cc_emsdk_optimizations := -g -sUSE_SDL=0 -sEVAL_CTORS=1
emsdk_cflags  := ${cc_emsdk_optimizations}
emsdk_ldflags := ${ld_emsdk_optimizations} ${ld_emsdk_settings}

fastalloc32_test:
	$(info Build fastalloc32 test.)
	$(CC) $(CFLAGS) -DFASTALLOC32_TEST fastalloc32.c -o fastalloc32_test

check: fastalloc32_test
	$(info Run fastalloc32 test and measure timing.)
	@time ./fastalloc32_test

lib:
	$(info Build libfastalloc32.so)
	$(CC) -O3 -fPIC -shared fastalloc32.c -o libfastalloc32.so

wasm:
	$(info Build Web Assembly target with EMSCRIPTEN and run test.)
	${EMSDK}/upstream/emscripten/emcc \
		${emsdk_cflags} -DFASTALLOC32_TEST fastalloc32.c -o fastalloc32.js ${emsdk_ldflags}
	@time	node -e "require('./fastalloc32.js')()"

clean:
	@rm -f *.o fastalloc32_test
	$(info Build clean.)
