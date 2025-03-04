/* SPDX-FileCopyrightText: 2025 Dyne.org foundation
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2025 Dyne.org foundation
 * designed, written and maintained by Denis Roio <jaromil@dyne.org>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Affero General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 *
 */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>

// sailfish pool
#include <sfpool.h>

static sfpool_t *SFP;

// Custom memory allocation function
void *custom_lua_mem(void *ud, void *ptr, size_t osize, size_t nsize);

#if defined(MEM_SYSTEM)
#define lua_malloc(size)       malloc(size)
#define lua_realloc(ptr, size) realloc(ptr,size)
#define lua_free(ptr)          free(ptr)
#elif defined(MEM_SFPOOL)
#define lua_malloc(size)       sfpool_malloc(SFP,size)
#define lua_realloc(ptr, size) sfpool_realloc(SFP,ptr,size)
#define lua_free(ptr)          sfpool_free(SFP,ptr)
#endif

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <lua_script_path> blocks blocksize\n", argv[0]);
        return 1;
    }
    const char* script_path = argv[1];
#if defined(MEM_SFPOOL)
    SFP = malloc(sizeof(sfpool_t));
    sfpool_init(SFP, atoi(argv[2]),atoi(argv[3]));
#endif
    lua_State* L = lua_newstate(custom_lua_mem, NULL);
    if (!L) {
        fprintf(stderr, "Failed to initialize Lua state.\n");
        return 1;
    }
    // set _U=true for tests, see https://www.lua.org/tests/
    lua_pushboolean(L,1);
    lua_setglobal(L,"_U");
    // Open standard Lua libraries
    luaL_openlibs(L);
    // Load and execute the Lua script
    int status = luaL_loadfile(L, script_path);
    if (status == LUA_OK) {
        status = lua_pcall(L, 0, LUA_MULTRET, 0); // Execute the script
    }
    if (status != LUA_OK) {
        // Print error message from Lua stack
        fprintf(stderr, "Error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); // Remove error message from stack
        lua_close(L);
        return 1;
    }
    // Clean up and exit
    lua_close(L);
#if defined(MEM_SFPOOL)
    sfpool_status(SFP);
    sfpool_teardown(SFP);
    free(SFP);
#endif
    return 0;
}

void *custom_lua_mem(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
	if(ptr == NULL) {
		if(nsize!=0) {
			void *ret = lua_malloc(nsize);
			if(ret) return ret;
			fprintf(stderr,"Malloc out of memory, requested %lu B\n", nsize);
			return NULL;
		} else return NULL;
	} else {
		if(nsize==0) {
			lua_free(ptr);
			return NULL;
    }
    return lua_realloc(ptr, nsize);
  }
}
