/*
 ** $Id: lstring.c,v 2.56.1.1 2017/04/19 17:20:42 roberto Exp $
 ** String table (keeps all strings handled by Lua)
 ** See Copyright Notice in lua.h
 */

#define lstring_c
#define LUA_CORE

#include "lprefix.h"

#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lapi.h"
#include "qlist.h"
#define MEMERRMSG       "not enough memory"

/*
 ** Lua will use at most ~(2^LUAI_HASHLIMIT) bytes from a string to
 ** compute its hash
 */
#if !defined(LUAI_HASHLIMIT)
#define LUAI_HASHLIMIT		5
#endif

typedef struct NodeStr {
	struct NodeStr *prev;
	struct NodeStr *next;
	ssize_t nref;
	TString ts[];
} NodeStr;
typedef struct SEntry {
	NodeStr *node;
} SEntry;
/*
 ** equality for long strings
 */
int luaS_eqlngstr(TString *a, TString *b) {
	size_t len = a->length;
	lua_assert(a->tt == LUA_TLNGSTR && b->tt == LUA_TLNGSTR);
	return (a == b) || /* same instance or... */
	((len == b->length) && /* equal length and ... */
	(memcmp(getstr(a), getstr(b), len) == 0)); /* equal contents */
}

unsigned int luaS_hash(const char *str, size_t l, unsigned int seed) {
	unsigned int h = seed ^ cast(unsigned int, l); //Times33
	size_t step = (l >> LUAI_HASHLIMIT) + 1;
	for (; l >= step; l -= step)
		h ^= ((h << 5) + (h >> 2) + cast_byte(str[l - 1]));
	return h;
}

unsigned int luaS_hashlongstr(TString *ts) {
	lua_assert(ts->tt == LUA_TLNGSTR);
	if (ts->extra == 0) { /* no hash? */
		ts->hash = luaS_hash(getstr(ts), ts->length, _G->seed);
		ts->extra = 1; /* now it has its hash */
	}
	return ts->hash;
}

/*
 ** resizes the string table
 */

void luaS_resize(lua_State *L, lua_Integer size) {
	Table *t = G(L)->strt;
	lua_Integer oldsize = t->lsizenode, pos, newsize = 64;
	for (; newsize < size && newsize > 0; newsize <<= 1)
		;
	if (newsize < 0)
		luaD_throw(L, LUA_ERRMEM);
	if (newsize != oldsize) {
		register SEntry *oldt = (SEntry*) t->entry, *newt, *entry;
		newt = luaM_newvector(L, newsize, SEntry);
		t->entry = (Entry*) newt;
		memset(newt, 0, newsize * sizeof(SEntry));
		t->lsizenode = newsize;
		lua_Integer mod = newsize - 1;
		t->nodemask = mod;
		if (t->length) {
			NodeStr *node, *next;
			for (register ssize_t i = 0; i < oldsize; i++) {
				node = oldt[i].node;
				while (node) {
					pos = node->ts->hash & mod;
					next = node->next;
					entry = &newt[pos];
					node->next = entry->node;
					if (node->next) {
						node->next->prev = node;
					}
					node->prev = cast(NodeStr*, cast(intptr_t,entry)|1);
					entry->node = node;
					node = next;
				}
			}
		}
		if (oldsize)
			luaM_freearray(L, oldt, oldsize);
	}
}

/*
 ** Clear API string cache. (Entries cannot be empty, so fill them with
 ** a non-collectable string.)
 */
void luaS_clearcache(global_State *g) {
}

/*
 ** Initialize the string table and the string cache
 */
void luaS_init(lua_State *L) {
	global_State *g = G(L);
	if (g->strt == NULL) {
		Table *strt = (Table*) luaM_realloc_(L, NULL, 0, sizeof(Table));
		memset(strt, 0, sizeof(Table));
		strt->tt = LUA_TTABLE;
		g->strt = strt;
		luaS_resize(L, MINSTRTABSIZE);
		/* pre-create memory-error message */
		g->memerrmsg = luaS_newliteral(L, MEMERRMSG);
		refInc(g->memerrmsg);/* it should never be collected */
	}
}

/*
 ** creates a new string object
 */
static TString *createstrobj(lua_State *L, size_t l, int tag) {
	TString *ts;
	size_t totalsize = sizelstring(l);
	; /* total size of TString object */
	ts = (TString*) luaC_newobjNotGC(L, tag, totalsize);
	ts->info = ts->extra = 0;
	getstr(ts)[l] = '\0'; /* ending 0 */
	return ts;
}

TString *luaS_createlngstrobj(lua_State *L, size_t l) {
	TString *ts = createstrobj(L, l, LUA_TLNGSTR);
	ts->info = ts->extra = 0;
//	ts->hash = cast(unsigned int, -1);
	ts->length = l;
	return ts;
}

static TString* internshrstr(lua_State *L, const char *str, int len) {
	Table *t = G(L)->strt;
	if (t->length > t->lsizenode) {
		luaS_resize(L, t->lsizenode * 2);
	}
	lua_Integer hash = luaS_hash(str, len, G(L)->seed);
	size_t pos = t->nodemask & hash;
	SEntry *entry = (SEntry*) t->entry + pos;
	NodeStr *node = entry->node;
	while (node) {
		if (node->ts->hash == hash
				&& memcmp(str, cast(NodeStr*,node)->ts->val, len) == 0) {
			return cast(NodeStr*,node)->ts;
		}
		node = node->next;
	}
	node = cast(NodeStr*, luaM_realloc_(L, NULL, 0, sizesstring(len)));
	node->nref = 0;
	TString *ts = node->ts;
	ts->marked = node->nref = 0;
	ts->value_.p = ts;
	ts->tt = LUA_TSHRSTR;
	ts->hash = hash;
	memcpy(getstr(ts), str, len * sizeof(char));
	ts->val[len] = '\0';
	ts->length = len;
	ts->info = ts->extra = 0;
	++t->length;
	NodeStr *next = entry->node;
	node->next = next;
	node->prev = cast(NodeStr*, cast(intptr_t,entry) | 1);
	if (next) {
		next->prev = node;
	}
	entry->node = node;
	return ts;
}
inline void luaS_destroy(lua_State *L) {
	Table *t = G(L)->strt;
	lua_assert(t->length == 0);
	luaM_realloc_(L, t->entry, t->lsizenode * sizeof(SEntry), 0);
	luaM_realloc_(L, t, sizeof(Table), 0);
}
void luaS_remove(lua_State *L, TString *ts) {
	SEntry *entry;
	NodeStr *node = cast(NodeStr*, ts) - 1;
	lua_assert(node->nref == 0);
	intptr_t prev = cast(intptr_t, node->prev);
	if (prev & 1) {
		entry = cast(SEntry*, prev ^ 1);
		entry->node = node->next;
	} else {
		NodeStr *next = node->next;
		cast(NodeStr*,prev)->next = next;
		if (next)
			next->prev = cast(NodeStr*, prev);
	}
	luaM_realloc_(L, node, sizesstring(ts->length), 0);
	G(L)->strt->length--;
}

/*
 ** checks whether short string exists and reuses it or creates a new one
 */
//static TString *internshrstr(lua_State *L, const char *str, size_t l) {
//	TString *ts;
//	global_State *g = G(L);
//	NodeSet *res;
//	if (luaH_gset_str(L, g->strt, str, l, &res)) {
//		ts = res->i_key->value_.s;
////		ts->collectable = 0;
//		return ts;
//	}
//	GCPrefix *ob = cast(GCPrefix *, sizelstring(l)+sizeof(GCPrefix));
//	ts = cast(TString*, ob + 1);
//	ts->marked = ob->nref = 0;
//	ts->value_.p = ts;
//	ts->tt = LUA_TSHRSTR;
//	ts->hash = res->hash;
//	memcpy(getstr(ts), str, l * sizeof(char));
//	ts->val[l] = '\0';
//	ts->length = l;
//	res->i_key = cast(TValue*, ts);
//	return ts;
//}
/*
 ** new string (with explicit length)
 */
TString *luaS_newlstr(lua_State *L, const char *str, size_t l) {
	if (l <= LUAI_MAXSHORTLEN) /* short string? */
		return internshrstr(L, str, l);
	else {
		TString *ts;
		if (l >= (MAX_SIZE - sizeof(TString)) / sizeof(char))
			luaM_toobig(L);
		ts = luaS_createlngstrobj(L, l);
		memcpy(getstr(ts), str, l * sizeof(char));
		return ts;
	}
}

/*
 ** Create or reuse a zero-terminated string, first checking in the
 ** cache (using the string address as a key). The cache can contain
 ** only zero-terminated strings, so it is safe to use 'strcmp' to
 ** check hits.
 */
TString *luaS_new(lua_State *L, const char *str) {
	return luaS_newlstr(L, str, strlen(str));
}

Udata *luaS_newudata(lua_State *L, size_t s) {
	Udata *u;
	GCObj *o;
	if (s > MAX_SIZE - sizeof(Udata))
		luaM_toobig(L);
	o = luaC_newobj(L, LUA_TUSERDATA, sizeludata(s));
	u = gco2u(o);
	u->len = s;
	u->metatable = NULL;
	setuservalue(L, u, luaO_nilobject);
	return u;
}

