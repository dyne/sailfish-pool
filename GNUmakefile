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

sfpool_test:
	$(info Build sfpool test.)
	$(CC) $(CFLAGS) -DSFPOOL_TEST sfpool.c -o sfpool_test

check: sfpool_test
	$(info Run sfpool test and measure timing.)
	@time ./sfpool_test

lib:
	$(info Build libsfpool.so)
	$(CC) -O3 -fPIC -shared sfpool.c -o libsfpool.so

wasm:
	$(info Build Web Assembly target with EMSCRIPTEN and run test.)
	${EMSDK}/upstream/emscripten/emcc \
		${emsdk_cflags} -DSFPOOL_TEST sfpool.c -o sfpool.js ${emsdk_ldflags}
	@time	node -e "require('./sfpool.js')()"

clean:
	@rm -f *.o sfpool_test
	$(info Build clean.)
