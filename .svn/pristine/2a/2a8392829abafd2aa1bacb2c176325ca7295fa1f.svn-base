/*
 *  Created on: Aug 15, 2019
 *  Author: WanQing
 *  E-mail: 1109162935@qq.com
 */
#include "lobject.h"
#include "lfunc.h"
#include "lgc.h"
#include "qlist.h"
#include "lapi.h"
#include "ltable.h"
#include "lfunc.h"
#include "lstring.h"



void luaC_barrierback(lua_State *L, TValue *p, TValue *v) {
//	if (iscollectable(v) && isblack(p) && iswhite(gcvalue(v)))
//		luaC_barrierback_(L, p);
}
void luaC_objbarrier(lua_State *L, TValue *p, TValue *o) {
//	if (isblack(p) && iswhite(o))
//		luaC_barrier_(L, obj2gco(p), obj2gco(o));
}
void luaC_upvalbarrier(lua_State *L, UpVal *uv) {
//	if (iscollectable(*(uv->v)) && !(uv)->v[0] != (uv)->u.value)
//		luaC_upvalbarrier_(L, uv);
}
