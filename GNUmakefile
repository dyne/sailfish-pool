# SPDX-FileCopyrightText: 2025 Dyne.org foundation
# SPDX-License-Identifier: GPL-3.0-or-later

CC ?= gcc

CFLAGS ?= -Wall -Wextra -g -fsanitize=address -fsanitize=undefined	\
-fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow			\
-fsanitize=leak

LUASRC=lua-5.4.7
LUAURL=https://www.lua.org

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
	$(CC) $(CFLAGS) -I. sfpool_test.c -o sfpool_test

check: sfpool_test
	$(info Run sfpool test and measure timing.)
	@time ./sfpool_test 1024 128; sync
	@time ./sfpool_test 2048 128; sync
	@time ./sfpool_test 4096 128; sync
	@time ./sfpool_test 1024 256; sync
	@time ./sfpool_test 2048 256; sync
	@time ./sfpool_test 4096 256; sync
	@time ./sfpool_test 1024 512; sync
	@time ./sfpool_test 2048 512; sync
	@time ./sfpool_test 4096 512; sync

LUA_MEM_TEST ?= MEM_SFPOOL

check-lua: test_lua.c
	$(info Build a Lua interpreter using sfpool as memory manager.)
	@[ -r ${LUASRC}.tar.gz ] || \
	 curl -L ${LUAURL}/ftp/${LUASRC}.tar.gz -o ${LUASRC}.tar.gz
	@[ -d ${LUASRC} ] || tar xf ${LUASRC}.tar.gz
	@[ -r ${LUASRC}/src/liblua.a ] || $(MAKE) -C ${LUASRC}
	$(CC) -O2 -I. -I${LUASRC}/src test_lua.c -o test_lua \
		${LUASRC}/src/liblua.a -lm -D${LUA_MEM_TEST}
	@[ -r ${LUASRC}-tests.tar.gz ] || \
   curl -L ${LUAURL}/tests/${LUASRC}-tests.tar.gz -o ${LUASRC}-tests.tar.gz
	@[ -d ${LUASRC}-tests ] || tar xf ${LUASRC}-tests.tar.gz
	@cd ${LUASRC}-tests && time ../test_lua all.lua 1024 128; sync
	@cd ${LUASRC}-tests && time ../test_lua all.lua 1024 256; sync
	@cd ${LUASRC}-tests && time ../test_lua all.lua 1024 512; sync
	@cd ${LUASRC}-tests && time ../test_lua all.lua 1024 1024; sync


wasm:
	$(info Build Web Assembly target with EMSCRIPTEN and run test.)
	${EMSDK}/upstream/emscripten/emcc \
		${emsdk_cflags} sfpool_test.c -I. -o sfpool.js ${emsdk_ldflags}
	@time	node -e "require('./sfpool.js')()"

clean:
	@rm -f *.o sfpool_test test_lua
	$(info Build clean.)
