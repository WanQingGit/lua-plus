/*
 ** $Id: lmem.c,v 1.91.1.1 2017/04/19 17:20:42 roberto Exp $
 ** Interface to Memory Manager
 ** See Copyright Notice in lua.h
 */

#define lmem_c
#define LUA_CORE

#include "lprefix.h"

#include <stddef.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "stdlib.h"
#include "qlist.h"
#include "ltable.h"

/*
 ** About the realloc function:
 ** void * frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
 ** ('osize' is the old size, 'nsize' is the new size)
 **
 ** * frealloc(ud, NULL, x, s) creates a new block of size 's' (no
 ** matter 'x').
 **
 ** * frealloc(ud, p, x, 0) frees the block 'p'
 ** (in this specific case, frealloc must return NULL);
 ** particularly, frealloc(ud, NULL, 0, 0) does nothing
 ** (which is equivalent to free(NULL) in ISO C)
 **
 ** frealloc returns NULL if it cannot create or reallocate the area
 ** (any reallocation to an equal or smaller size cannot fail!)
 */

#define MINSIZEARRAY	4

void *luaM_growaux_(lua_State *L, void *block, int *size, size_t size_elems,
		int limit, const char *what) {
	void *newblock;
	int newsize;
	int oldsize = *size;
	if (oldsize >= limit / 2) { /* cannot double it? */
		if (oldsize >= limit) /* cannot grow even a little? */
			luaG_runerror(L, "too many %s (limit is %d)", what, limit);
		newsize = limit; /* still have at least one free place */
	} else {
		newsize = (oldsize) * 2;
		if (newsize < MINSIZEARRAY)
			newsize = MINSIZEARRAY; /* minimum size */
	}
	newblock = luaM_reallocv(L, block, oldsize, newsize, size_elems);
	*size = newsize; /* update only when everything else is OK */
	return newblock;
}

l_noret luaM_toobig(lua_State *L) {
	luaG_runerror(L, "memory allocation error: block too big");
}
#ifndef USE_POOL

/*
 ** generic allocation routine.
 */
void *luaM_realloc_(lua_State *L, void *block, size_t osize, size_t nsize) {
	void *newblock;
	global_State *g = G(L);
	lua_assert((osize == 0) == (block == NULL));
	g->GCdebt += nsize - osize;
#if defined(HARDMEMTESTS)
  if (nsize > realosize && g->gcrunning)
    luaC_fullgc(L, 1);  /* force a GC whenever possible */
#endif
	if (nsize == 0) {
		if (block)
			free(block);
		return NULL;
	} else {
		newblock = realloc(block, nsize);
		if (newblock == NULL) {
			lua_assert(nsize > osize); /* cannot fail when shrinking a block */
			if (g->version) { /* is state fully built? */
				luaC_fullgc(L, 1); /* try to free some memory... */
				newblock = realloc(block, nsize); /* try again */
			}
			if (newblock == NULL)
				luaD_throw(L, LUA_ERRMEM);
		}
	}
	return newblock;
}
#else
#include "mem_pool.c"

void *luaM_realloc_(lua_State *L, void *ptr, size_t osize, size_t nsize) {
	void* n = NULL;
	assert((osize==0)==(ptr==NULL));
	G(L)->GCdebt += nsize - osize;
	if (nsize == 0) {
		if (osize) {
			mem_free(/*S, */ptr);
		}
		return NULL;
	} else { //nsize != 0
		if (osize) {
			if (osize > SMALL_REQUEST_THRESHOLD) {
				if (nsize > SMALL_REQUEST_THRESHOLD) {
					n = realloc(ptr, nsize);
				} else {
					mem_malloc(&n, nsize);
					if (n)
						memcpy(n, ptr, nsize);
					free(ptr);
				}
			} else { //osize <= SMALL_REQUEST_THRESHOLD
				if (nsize > SMALL_REQUEST_THRESHOLD) {
					n = malloc(nsize);
					memcpy(n, ptr, osize);
				} else { //nsize <= SMALL_REQUEST_THRESHOLD
					if (SIZE2INDEX(nsize) != SIZE2INDEX(osize)) {
						mem_malloc(&n, nsize);
						memcpy(n, ptr, osize > nsize ? nsize : osize);
					} else
						return ptr;
				}
				if (n) {
					if (!mem_free(ptr)) {
						free(ptr);
					}
				}
				return n;
			}
		} else { //osize=0
			if (nsize > SMALL_REQUEST_THRESHOLD) {
				n = malloc(nsize);
			} else {
				mem_malloc(&n, nsize);
			}
		}
		return n;
	}
}
#endif

void luaM_destroy() {
	list_cache_clear();
	table_clear_cache();
#ifdef USE_POOL
	if (arenas)
		free(arenas);
	assert(narenas_currently_allocated == 0);
#endif
	free(O2B(_S));
	_S = NULL;
	_G = NULL;
}

