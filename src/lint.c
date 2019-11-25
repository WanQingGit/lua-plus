/*
 *  Created on: Aug 16, 2019
 *  Author: WanQing
 *  E-mail: 1109162935@qq.com
 */
#include "lstate.h"
#include "ltable.h"
#include "lgc.h"
#include "lapi.h"
#include "qlist.h"
#include <stdlib.h>
Object _boolTrue = { 1, { { (void*) 1 }, LUA_TBOOLEAN } }, _boolFalse = { 1, { {
		(void*) 0 }, LUA_TBOOLEAN } }, _luaO_nilobject = { 1, { { (void*) 0 },
		LUA_TNIL } };
TValue *boolTrue = &_boolTrue.ob, *boolFalse = &_boolFalse.ob, *luaO_nilobject =
		&_luaO_nilobject.ob;
//TValue *int_get(lua_State *L, lua_Integer i) {
//	return luaH_gset_int(L, G(L)->intt, i);
//}
void const_init(lua_State *L) {
#ifdef USE_INT_POOL
	Table *t = luaH_create(L, 0, 128);
	refInc(t);
	G(L)->intt = t;
	box_remove(L,O2B(t));
#else
	TValue **intt = luaM_realloc_(L, NULL, 0, sizeof(TValue*) * (32 + 256 + 1));
	for (int i = -32; i <= 256; i++) {
		TValue *io = luaC_newobjNotGC(L, LUA_TNUMINT, sizeof(TValue));
		io->value_.i = i;
		refInc(io);
		intt[i + 32] = io;
	}
	G(L)->intt = intt;
#endif
}
void const_destroy(lua_State *L) {
#ifdef USE_INT_POOL
	luaH_free_set(L, g->intt);
#else
	TValue **intt = G(L)->intt;
	for (int i = -32; i <= 256; i++) {
		refDec(L, intt[i + 32]);
	}
#endif
	free(intt);
	G(L)->intt = NULL;
}
