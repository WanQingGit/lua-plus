/*
 ** $Id: lgc.c,v 2.215.1.2 2017/08/31 16:15:27 roberto Exp $
 ** Garbage Collector
 ** See Copyright Notice in lua.h
 */

#define lgc_c
#define LUA_CORE

#include "lprefix.h"

#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
//-------------------
#include "qlist.h"
#include "lapi.h"
#include <stdlib.h>
/*
 ** internal state for collector while inside the atomic phase. The
 ** collector should never be in this state while running regular code.
 */
#define GCSinsideatomic		(GCSpause + 1)

/*
 ** cost of sweeping one element (the size of a small object divided
 ** by some adjust for the sweep speed)
 */
#define GCSWEEPCOST	((sizeof(TString) + 4) / 4)

/* maximum number of elements to sweep in each single step */
#define GCSWEEPMAX	(cast_int((GCSTEPSIZE / GCSWEEPCOST) / 4))

/* cost of calling one finalizer */
#define GCFINALIZECOST	GCSWEEPCOST

/*
 ** macro to adjust 'stepmul': 'stepmul' is actually used like
 ** 'stepmul / STEPMULADJ' (value chosen by tests)
 */
#define STEPMULADJ		200

/*
 ** macro to adjust 'pause': 'pause' is actually used like
 ** 'pause / PAUSEADJ' (value chosen by tests)
 */
#define PAUSEADJ		100

/*
 ** 'makewhite' erases all color bits then sets only the current white
 ** bit
 */
#define maskcolors	(~(bitmask(BLACKBIT) | WHITEBITS))
#define makewhite(g,x)	(void*)0
// (x->marked = cast_byte((x->marked & maskcolors) | luaC_white(g)))

#define white2gray(x)	resetbits(x->marked, WHITEBITS)
#define black2gray(x)	resetbit(x->marked, BLACKBIT)

#define valiswhite(x)   (iscollectable(x) && iswhite(gcvalue(x)))

#define checkdeadkey(n)	lua_assert(!ttisdeadkey(gkey(n)) || ttisnil(gval(n)))

#define checkconsistency(obj)  \
  lua_longassert(!iscollectable(obj) || righttt(obj))

#define markvalue(g,o) { checkconsistency(o); \
  if (valiswhite(o)) reallymarkobject(g,gcvalue(o)); }

#define markobject(g,t)	{ if (iswhite(t)) reallymarkobject(g, obj2gco(t)); }

/*
 ** mark an object that can be NULL (either because it is really optional,
 ** or it was stripped as debug info, or inside an uncompleted structure)
 */
#define markobjectN(g,t)	{ if (t) markobject(g,t); }

static void reallymarkobject(global_State *g, GCObj *o);

/*
 ** {======================================================
 ** Generic functions
 ** =======================================================
 */

/*
 ** one after last element in a hash array
 */
#define gnodelast(h)	gnode(h, cast(size_t, sizenode(h)))

/*
 ** link collectable object 'o' into list pointed by 'p'
 */
#define linkgclist(o,p)	((o)->gclist = (p), (p) = obj2gco(o))

/*
 ** If key is not marked, mark its entry as dead. This allows key to be
 ** collected, but keeps its entry in the table.  A dead node is needed
 ** when Lua looks up for a key (it may be part of a chain) and when
 ** traversing a weak table (key might be removed from the table during
 ** traversal). Other places never manipulate dead keys, because its
 ** associated nil value is enough to signal that the entry is logically
 ** empty.
 */
#define UNREACHABLE -1
#define IS_GC(v) ((v)->tt & GC_FLAG)
typedef void (*traversefn)(TValue *v, void *arg);
typedef struct {
	struct gcnode *head;
	struct gcnode *tail; //指向当前结点的上一结点
	ssize_t length;
	ssize_t count;
	ssize_t threshold;
} GcList;
// @formatter:off
static GcList generations[NUM_GENERATIONS]={
		{(GCNode*)&generations[0],(GCNode*)&generations[0],0,0,600},
		{(GCNode*)&generations[1],(GCNode*)&generations[1],0,0,5},
		{(GCNode*)&generations[2],(GCNode*)&generations[2],0,0,5},
};
// @formatter:on
static GcList recycle = { (GCNode*) &recycle, (GCNode*) &recycle, 0, 0, 1000 };
static ObjNode objs = { &objs, &objs };
GcList *generation0 = &generations[0];
void luaC_init() {
	_G->boxs = (GCPrefix*) &generations[0];
	_G->recycle_bin = (ObjNode *) &recycle;
	_G->objs = &objs;
}

/*
 ** tells whether a key or value can be cleared from a weak
 ** table. Non-collectable objects are never removed from weak
 ** tables. Strings behave as 'values', so are never removed too. for
 ** other objects: if really collected, cannot keep them; for objects
 ** being finalized, keep them in keys, but not in values
 */
static int iscleared(global_State *g, const TValue *o) {
	if (!iscollectable(o))
		return 0;
	else if (ttisstring(o)) {
		markobject(g, tsvalue(o)); /* strings are 'values', so are never weak */
		return 0;
	}

	else
		return iswhite(gcvalue(o));
}

/*
 ** barrier that moves collector forward, that is, mark the white object
 ** being pointed by a black object. (If in sweep phase, clear the black
 ** object to white [sweep it] to avoid other barrier calls for this
 ** same object.)
 */
void luaC_barrier_(lua_State *L, GCObj *o, GCObj *v) {
	global_State *g = G(L);
	lua_assert(isblack(o) && iswhite(v) && !isdead(g, v) && !isdead(g, o));
	if (keepinvariant(g)) /* must keep invariant? */
		reallymarkobject(g, v); /* restore invariant */
	else { /* sweep phase */
		lua_assert(issweepphase(g));
		makewhite(g, o); /* mark main obj. as white to avoid other barriers */
	}
}

/*
 ** barrier that moves collector backward, that is, mark the black object
 ** pointing to a white object as gray again.
 */
void luaC_barrierback_(lua_State *L, Table *t) {
	global_State *g = G(L);
	lua_assert(isblack(t) && !isdead(g, t));
	black2gray(t); /* make table gray (again) */
	linkgclist(t, g->grayagain);
}

/*
 ** barrier for assignments to closed upvalues. Because upvalues are
 ** shared among closures, it is impossible to know the color of all
 ** closures pointing to it. So, we assume that the object being assigned
 ** must be marked.
 */
void luaC_upvalbarrier_(lua_State *L, UpVal *uv) {
	global_State *g = G(L);
	GCObj *o = gcvalue(uv->v);
	lua_assert(!upisopen(uv)); /* ensured by macro luaC_upvalbarrier */
	if (keepinvariant(g))
		markobject(g, o);
}

void luaC_fix(lua_State *L, GCObj *o) {
	refInc(o);
}
static inline void update_refs(GCPrefix *generation) {
	GCPrefix *iter = generation;
	while ((iter = (GCPrefix*) iter->next) != generation) {
		iter->gcref = iter->nref;
	}
}

void clean_const(lua_State *L) {
	ObjNode *recyle = G(L)->recycle_bin;
	ObjNode *iter = recyle->next, *next;
	while (iter != recyle) {
		next = iter->next;
		if (iter->v->nref > 0) {
			List.remove((qlist) recyle, (lNode) iter, 1);
		} else {
			Object *o = iter->v;
			TValue *v = &o->ob;
			lua_assert(o->nref == 0);
			if (v->collectable & 1) {
				v->collectable = 2;
			} else {
				lua_assert(v->collectable & 2);
				List.remove((qlist) recyle, (lNode) iter, 1);
#ifdef USE_INT_POOL
				if (v->tt == LUA_TNUMINT) {
					lua_assert(luaH_del_set(L, G(L)->intt, v, v->value_.i));
				} else
#endif
				{
					lua_assert(v->tt == LUA_TSHRSTR);
					lua_assert(luaH_del_set(L, G(L)->strt, v, cast(TString*,v)->hash));
				}
			}
		}
		iter = next;
	}
}
static inline void recycle_add(lua_State *L, TValue *o, void *ob) {
	if (o->collectable == 0) {
		GcList *recycle = (GcList *) G(L)->recycle_bin;
		if (recycle->length > recycle->threshold) {
			clean_const(L);
			recycle->count += recycle->length;
			recycle->length = 0;
		}
		List.append(cast(qlist, recycle), ob);
		o->collectable = 1;
	}
}
void obj_destroy(lua_State *L, TValue *o) {
	switch (ttype(o)) {
	case LUA_TMODULE: {
		register int i, nk;
		ObjPrefix *ob = refObj(o);
		lua_assert(ob->nref == 0);
		Module *module = (Module*) o;
		nk = module->nconst;
		StkId k = module->k;
		for (i = 0; i < nk; i++) {
			refDec(L, k[i]);
		}
		obj_remove(L, ob);
		luaM_realloc_(L, module->k, sizeof(TValue*) * nk, 0);
		luaM_realloc_(L, ob, sizeof(Module) + sizeof(ObjPrefix), 0);
		break;
	}
	case LUA_TPROTO: {
		ObjPrefix *ob = refObj(o);
		Proto* p = (Proto*) o;
		refDec(L, p->source);
		register int i;
		if (p->sizeupvalues) {
			for (i = 0; i < p->sizeupvalues; i++) {
				refDec(L, p->upvalues[i].name);
			}
			luaM_realloc_(L, p->upvalues, sizeof(Upvaldesc) * p->sizeupvalues, 0);
		}

		if (p->np) {
			for (i = 0; i < p->np; i++) {
				refDec(L, p->p[i]);
			}
			luaM_realloc_(L, p->p, sizeof(void*) * p->np, 0);
		}
		if (p->nlocvars) {
			for (i = 0; i < p->nlocvars; i++) {
				refDec(L, p->locvars[i].name);
			}
			luaM_realloc_(L, p->locvars, sizeof(LocVar) * p->nlocvars, 0);
		}
		if (p->ncode) {
			luaM_realloc_(L, p->code, sizeof(Instruction) * p->ncode, 0);
			luaM_realloc_(L, p->lineinfo, sizeof(int) * p->ncode, 0);
		}

		refDec(L, p->module);
		obj_remove(L, ob);
		luaM_realloc_(L, ob, sizeof(Proto) + sizeof(ObjPrefix), 0);
		break;
	}
	case LUA_TLCL: {
		GCPrefix *bp = O2B(o);
		lua_assert(bp->nref >= 0);
		LClosure *lc = (LClosure*) o;
		int nup = lc->nupvalues;
		refDec(L, lc->p);
		bp->nref++;
		for (int i = 0; i < nup; i++) {
			UpVal *up = lc->upvals[i];
			up->refcount--;
			refDec(L, up->v[0]);
			if (up->refcount == 0) {
				luaM_realloc_(L, up, sizeof(UpVal), 0);
			}
		}
		bp->nref--;
		if (bp->nref > 0) {
			lc->nupvalues = 0;
			lc->p = NULL;
		} else {
			box_remove(L, bp);
			luaM_realloc_(L, bp, sizeLclosure(nup) + sizeof(GCPrefix), 0);
		}
		break;
	}
	case LUA_TCCL: {
		GCPrefix *bp = O2B(o);
		lua_assert(bp->nref >= 0);
		CClosure *cc = (CClosure*) o;
		int nup = cc->nupvalues;
		bp->nref++;
		for (int i = 0; i < nup; i++) {
			refDec(L, cc->upvalue[i]);
		}
		bp->nref--;
		if (bp->nref > 0) {
			cc->nupvalues = 0;
		} else {
			box_remove(L, bp);
			luaM_realloc_(L, bp, sizeCclosure(nup) + sizeof(GCPrefix), 0);
		}
		break;
	}
	case LUA_TLIGHTUSERDATA: {
		ObjPrefix *ob = refObj(o);
		lua_assert(ob->nref == 0);
		obj_remove(L, ob);
		luaM_realloc_(L, ob, sizeof(TValue) + sizeof(ObjPrefix), 0);
		break;
	}
	case LUA_TLCF: {
		ObjPrefix *ob = refObj(o);
		lua_assert(ob->nref == 0);
		obj_remove(L, ob);
		luaM_realloc_(L, ob, sizeof(TValue) + sizeof(ObjPrefix), 0);
		break;
	}
	case LUA_TUSERDATA: {
		GCPrefix *bp = O2B(o);
		lua_assert(bp->nref >= 0);
		Udata *u = (Udata*) o;
		bp->nref++;
		if (o->collectable & 2) {
			call_gc(L, o, 1);
			o->collectable &= 1;
		}
		refDec(L, u->user_);
		refDec(L, u->metatable);
		bp->nref--;
		if (bp->nref > 0) {
			u->user_ = NULL;
			u->metatable = NULL;
		} else {
			box_remove(L, bp);
			luaM_realloc_(L, bp, sizeof(Udata) + sizeof(GCPrefix), 0);
		}
		break;
	}
	case LUA_TLNGSTR: {
		ObjPrefix *ob = refObj(o);
		lua_assert(ob->nref == 0);
		obj_remove(L, ob);
		luaM_realloc_(L, ob, sizelstring(o->value_.s->length), 0);
		break;
	}
	case LUA_TSHRSTR: {
		luaS_remove(L, (TString*) o);
//		ObjPrefix *ob = refObj(o);
//		if (ob->nref == 0) {
//			recycle_add(L, o, ob);
//		} else {
//			lua_assert(ob->nref == -1);
//			obj_remove(L, ob);
//			luaM_realloc_(L, ob,
//			sizelstring(o->value_.s->length) + sizeof(ObjPrefix), 0);
//		}
		break;
	}
	case LUA_TNUMINT: {
		ObjPrefix *ob = refObj(o);
#ifdef USE_INT_POOL
		if (ob->nref == 0) {
			recycle_add(L, o, ob);
		} else {
			lua_assert(ob->nref == -1);
			obj_remove(L, ob);
			luaM_realloc_(L, ob, sizeof(TValue) + sizeof(ObjPrefix), 0);
		}
#else
		lua_assert(ob->nref == 0);
		obj_remove(L, ob);
		luaM_realloc_(L, ob, sizeof(TValue) + sizeof(ObjPrefix), 0);
#endif
		break;
	}
	case LUA_TNUMFLT: {
		ObjPrefix *ob = refObj(o);
		lua_assert(ob->nref == 0);
		obj_remove(L, ob);
		luaM_realloc_(L, ob, sizeof(TValue) + sizeof(ObjPrefix), 0);
		break;
	}
	case LUA_TTABLE: {
		ObjPrefix *ob = refObj(o);
		lua_assert(ob->nref >= 0);
		ob->nref++;
		if (o->collectable & 2) {
			call_gc(L, o, 1);
			o->collectable = 1;
		}
		Table *t = cast(Table*, o);
		if (t->metatable)
			refDec(L, (TValue* ) t->metatable);
		luaH_free(L, t);
		break;
	}
	case LUA_TTHREAD: {
		GCPrefix *bp = O2B(o);
		lua_assert(bp->nref >= 0);
		bp->nref++;
		lua_State *ts = (lua_State*) o;
		luaF_close(ts, ts->stack);
		lua_assert(ts->openupval == NULL);
		freestack(ts);
		bp->nref--;
		if (bp->nref > 0) {
			L->openupval = NULL;
		} else {
			if (ts != G(L)->mainthread) {
				box_remove(L, bp);
				luaM_realloc_(L, bp, sizeof(lua_State) + sizeof(GCPrefix), 0);
			}
		}
		break;
	}
	case LUA_TBOOLEAN:
	case LUA_TNIL:
		printf("-_-#\n");
		break;

	default:
		break;
	}
}
void traverse(GCObj *ob, traversefn fn, void *arg) {
	register ssize_t i;
	TValue *v;
	switch (ttype(ob)) {
	case LUA_TTABLE: {
		Table *t = (Table*) ob;
		for (i = 0; i < t->sizearray; i++) {
			v = t->array[i];
			if (IS_GC(v)) {
				fn(v, arg);
			}
		}
		for (i = 0; i < t->lsizenode; i++) {
			NodeMap *node = t->entry[i].node.map;
			while (node) {
				if (IS_GC(node->i_key)) {
					fn(node->i_key, arg);
				}
				if (IS_GC(node->i_val)) {
					fn(node->i_val, arg);
				}
				node = node->next;
			}
		}
		break;
	}
	case LUA_TTHREAD: {
		lua_State *ts = (lua_State*) ob;
		StkId st = ts->stack;
		for (; st < ts->top; st++) {
			v = st[0];
			if (IS_GC(v)) {
				fn(v, arg);
			}
		}
		break;
	}
	case LUA_TLCL: {
		LClosure *lc = (LClosure *) ob;
		for (i = 0; i < lc->nupvalues; i++) {
			UpVal *uv = lc->upvals[i];
			if (uv && !upisopen(uv) && IS_GC(uv->v[0])) {
				fn(uv->v[0], arg);
			}
		}
		break;
	}
	case LUA_TCCL: {
		CClosure *cc = (CClosure*) ob;
		for (i = 0; i < cc->nupvalues; i++) {
			v = cc->upvalue[i];
			if (IS_GC(v)) {
				fn(v, arg);
			}
		}
		break;
	}
	case LUA_TUSERDATA: {
		Udata *u = (Udata *) ob;
		v = u->user_;
		if (v && IS_GC(v)) {
			fn(v, arg);
		}
		if (u->metatable)
			fn((TValue*) u->metatable, arg);
		break;
	}
	default: {
		printf("error!\n");
	}
	}
}
void reachable_func(TValue *v, GCPrefix **arr) {
	GCPrefix *reachable = arr[0];
	GCPrefix *unreachable = arr[1];
	GCPrefix *bp = O2B(v);
	if (bp->gcref == UNREACHABLE) {
		List.remove((qlist) unreachable, (lNode) bp, 0);
		List.linkNodeToPrev((qlist) reachable, (lNode) bp, list_iter(reachable));
		bp->gcref = 1;
	} else if (bp->gcref == 0) {
		bp->gcref = 1;
	} else {
		assert(bp->gcref > 0);
	}
}
void sub_ref_func(TValue *v, void *arg) {
	O2B(v)->gcref--;
}
static inline void subtract_refs(GCPrefix *generation) {
	GCPrefix *iter = (GCPrefix*) generation->next;
	for (; iter != generation; iter = (GCPrefix*) iter->next) {
		GCObj *ob = iter->ob;
		traverse(ob, sub_ref_func, NULL);
	}
}

void move_unreachable(GCPrefix* young, GCPrefix *unreachable) {
	GCNode *iter = young->next, *next;
	GCPrefix *a[2] = { young, unreachable };
	GCObj *ob;
	for (; iter != (GCNode *) young;) {
		if (iter->gcref <= 0) {
			lua_assert(iter->gcref == 0);
			iter->gcref = UNREACHABLE;
			next = iter->next;
			List.remove((qlist) young, (lNode) iter, 0);
			List.linkNodeToPrev((qlist) unreachable, (lNode) iter,
					list_head(unreachable));
		} else {
			ob = (GCObj*) &iter->ob;
			traverse(ob, (traversefn) reachable_func, a);
			next = iter->next;
		}
		iter = next;
	}
}
void move_finalizer(GCPrefix* unreachable, GCPrefix *finalizers) {
	GCNode *iter = unreachable->next, *next;
	for (; iter != (GCNode *) unreachable;) {
		next = iter->next;
		if (iter->ob.collectable & 2) {
			List.remove((qlist) unreachable, (lNode) iter, 0);
			List.linkNodeToPrev((qlist) finalizers, (lNode) iter,
					list_head(finalizers));
		}
		iter = next;
	}
}
static inline void handle_finalizers(GCPrefix *finalizers) {
	GCNode *iter = finalizers->next;
	for (; iter != (GCNode *) finalizers; iter = iter->next) {
		call_gc(_S, &iter->ob, 0);
	}
}
static inline void clean_unreachable(GCPrefix *unreachable) {
	if (unreachable->gcref) {
		GCNode *iter = unreachable->next;
		for (; iter != (GCNode *) unreachable; iter = iter->next) {
			obj_destroy(_S, &iter->ob);
		}
		generation0->length += unreachable->gcref;
	}
}
void gc_collect(int gen) {
	GCPrefix unreachable = { (GCNode*) &unreachable, (GCNode*) &unreachable, 0 };
	GCPrefix finalizers = { (GCNode*) &finalizers, (GCNode*) &finalizers, 0 };
	GcList *generation = &generations[gen], *old;
	if (gen + 1 < NUM_GENERATIONS) {
		old = &generations[gen + 1];
		old->count++;
	} else {
		old = &generations[gen];
	}
	for (int i = 0; i < gen; i++) {
		List.merge((qlist) generation, (qlist) &generations[i]);
	}
	update_refs((GCPrefix*) generation);
	subtract_refs((GCPrefix*) generation);
	move_unreachable((GCPrefix*) generation, &unreachable);
	if (generation != old) {
		List.merge((qlist) old, (qlist) generation);
	}
	move_finalizer(&unreachable, &finalizers);
	if (finalizers.gcref) {
		handle_finalizers(&finalizers);
		clean_unreachable(&finalizers);
	}
	clean_unreachable(&unreachable);
}
void generation_collect() {
	for (int i = NUM_GENERATIONS - 1; i >= 0; i--) {
		if (generations[i].count > generations[i].threshold) {
			gc_collect(i);
		}
	}
}
/*
 ** create a new collectable object (with given type and size) and link
 ** it to 'allgc' list.
 */
GCObj *luaC_newobj(lua_State *L, VarType tt, size_t sz) {
	GCPrefix *ob = cast(GCPrefix *,
			luaM_newobject(L, novariant(tt), sz+sizeof(GCPrefix)));
	GCObj *o = cast(GCObj*, ob + 1);
	o->marked = ob->nref = 0;
	o->value_.p = o;
//	o->marked = luaC_white(g);
	o->tt = (unsigned char) tt | GC_FLAG;
	o->collectable = 1;
//	o->tt = tt | BIT_ISCOLLECTABLE;
	if (generation0->length > generation0->threshold) {
		generation0->count = generation0->length;
		generation_collect();
	}
	box_append(L, ob);
	return o;
}
#define MASKN(n,p)	((~((~(unsigned long long)0)<<(n)))<<(p))
TValue *luaC_newobjNotGC(lua_State *L, VarType tt, size_t sz) {
	ObjPrefix *head = cast(ObjPrefix *,
			luaM_newobject(L, novariant(tt), sz+sizeof(ObjPrefix)));
	head->nref = 0;
	GCObj *o = cast(GCObj*, head + 1);
	o->marked = 0; // luaC_white(G(L));
	o->tt = tt & MASKN(6, 0);
	o->collectable = 0;
	o->value_.p = o;
#ifdef LUA_OBJ_DEBUG
	List.append(cast(qlist, G(L)->objs), (listType) head);
#endif
	return (TValue*) o;
}

/* }====================================================== */

/*
 ** {======================================================
 ** Mark functions
 ** =======================================================
 */

/*
 ** mark an object. Userdata, strings, and closed upvalues are visited
 ** and turned black here. Other objects are marked gray and added
 ** to appropriate list to be visited (and turned black) later. (Open
 ** upvalues are already linked in 'headuv' list.)
 */
static void reallymarkobject(global_State *g, GCObj *o) {
	reentry:
	white2gray(o);
	switch (o->tt) {
	case LUA_TSHRSTR: {
		gray2black(o);
		g->GCmemtrav += sizelstring(gco2ts(o)->length);
		break;
	}
	case LUA_TLNGSTR: {
		gray2black(o);
		g->GCmemtrav += sizelstring(gco2ts(o)->length);
		break;
	}
	case LUA_TUSERDATA: {
		TValue *uvalue = NULL;
		markobjectN(g, gco2u(o)->metatable); /* mark its metatable */
		gray2black(o);
		g->GCmemtrav += sizeudata(gco2u(o));
		getuservalue(g->mainthread, gco2u(o), &uvalue);
		if (valiswhite(uvalue)) { /* markvalue(g, &uvalue); */
			o = gcvalue(uvalue);
			goto reentry;
		}
		break;
	}
	case LUA_TLCL: {
		linkgclist(gco2lcl(o), g->gray);
		break;
	}
	case LUA_TCCL: {
		linkgclist(gco2ccl(o), g->gray);
		break;
	}
	case LUA_TTABLE: {
		linkgclist(gco2t(o), g->gray);/*t->gclist=g->gray,g->gray=t*/
		break;
	}
	case LUA_TTHREAD: {
		linkgclist(gco2th(o), g->gray);
		break;
	}
	case LUA_TPROTO: {
		linkgclist(gco2p(o), g->gray);
		break;
	}
	default:
		lua_assert(0);
		break;
	}
}

/*
 ** mark metamethods for basic types
 */
static void markmt(global_State *g) {
	int i;
	for (i = 0; i < LUA_NUMTAGS; i++)
		markobjectN(g, g->mt[i]);
}

/*
 ** mark all objects in list of being-finalized
 */
static void markbeingfnz(global_State *g) {
	GCObj *o;
	for (o = g->tobefnz; o != NULL; o = o->value_.p)
		markobject(g, o);
}

/*
 ** Mark all values stored in marked open upvalues from non-marked threads.
 ** (Values from marked threads were already marked when traversing the
 ** thread.) Remove from the list threads that no longer have upvalues and
 ** not-marked threads.
 */
static void remarkupvals(global_State *g) {
	lua_State *thread;
	lua_State **p = &g->twups;
	while ((thread = *p) != NULL) {
		lua_assert(!isblack(thread)); /* threads are never black */
		if (isgray(thread) && thread->openupval != NULL)
			p = &thread->twups; /* keep marked thread with upvalues in the list */
		else { /* thread is not marked or without upvalues */
			UpVal *uv;
			*p = thread->twups; /* remove thread from the list */
			thread->twups = thread; /* mark that it is out of list */
			for (uv = thread->openupval; uv != NULL; uv = uv->u.open.next) {
				if (uv->u.open.touched) {
					markvalue(g, uv->v[0]); /* remark upvalue's value */
					uv->u.open.touched = 0;
				}
			}
		}
	}
}

/*
 ** mark root set and reset all gray lists, to start a new collection
 */
static void restartcollection(global_State *g) {
	g->gray = g->grayagain = NULL;
	g->weak = g->allweak = g->ephemeron = NULL;
//	markobject(g, g->mainthread);
	reallymarkobject(g, obj2gco(g->mainthread));
	reallymarkobject(g, gcvalue(g->glt));
	markmt(g);
	markbeingfnz(g); /* mark any finalizing object left from previous cycle */
}

/* }====================================================== */

/*
 ** {======================================================
 ** Traverse functions
 ** =======================================================
 */

/*
 ** Traverse a table with weak values and link it to proper list. During
 ** propagate phase, keep it in 'grayagain' list, to be revisited in the
 ** atomic phase. In the atomic phase, if table has any white value,
 ** put it in 'weak' list, to be cleared.
 */
static void traverseweakvalue(global_State *g, Table *h) {
//  Node *n, *limit = gnodelast(h);
//  /* if there is array part, assume it may have white values (it is not
//     worth traversing it now just to check) */
//  int hasclears = (h->sizearray > 0);
//  for (n = gnode(h, 0); n < limit; n++) {  /* traverse hash part */
//    checkdeadkey(n);
//    if (ttisnil(gval(n)))  /* entry is empty? */
//      removeentry(n);  /* remove it */
//    else {
//      lua_assert(!ttisnil(gkey(n)));
//      markvalue(g, gkey(n));  /* mark key */
//      if (!hasclears && iscleared(g, gval(n)))  /* is there a white value? */
//        hasclears = 1;  /* table will have to be cleared */
//    }
//  }
//  if (g->gcstate == GCSpropagate)
//    linkgclist(h, g->grayagain);  /* must retraverse it in atomic phase */
//  else if (hasclears)
//    linkgclist(h, g->weak);  /* has to be cleared later */
}

/*
 ** Traverse an ephemeron table and link it to proper list. Returns true
 ** iff any object was marked during this traversal (which implies that
 ** convergence has to continue). During propagation phase, keep table
 ** in 'grayagain' list, to be visited again in the atomic phase. In
 ** the atomic phase, if table has any white->white entry, it has to
 ** be revisited during ephemeron convergence (as that key may turn
 ** black). Otherwise, if it has any white key, table has to be cleared
 ** (in the atomic phase).
 */
static int traverseephemeron(global_State *g, Table *h) {
	int marked = 0; /* true if an object is marked in this traversal */
//  int hasclears = 0;  /* true if table has white keys */
//  int hasww = 0;  /* true if table has entry "white-key -> white-value" */
//  Node *n, *limit = gnodelast(h);
//  unsigned int i;
//  /* traverse array part */
//  for (i = 0; i < h->sizearray; i++) {
//    if (valiswhite(&h->array[i])) {
//      marked = 1;
//      reallymarkobject(g, gcvalue(&h->array[i]));
//    }
//  }
//  /* traverse hash part */
//  for (n = gnode(h, 0); n < limit; n++) {
//    checkdeadkey(n);
//    if (ttisnil(gval(n)))  /* entry is empty? */
//      removeentry(n);  /* remove it */
//    else if (iscleared(g, gkey(n))) {  /* key is not marked (yet)? */
//      hasclears = 1;  /* table must be cleared */
//      if (valiswhite(gval(n)))  /* value not marked yet? */
//        hasww = 1;  /* white-white entry */
//    }
//    else if (valiswhite(gval(n))) {  /* value not marked yet? */
//      marked = 1;
//      reallymarkobject(g, gcvalue(gval(n)));  /* mark it now */
//    }
//  }
//  /* link table into proper list */
//  if (g->gcstate == GCSpropagate)
//    linkgclist(h, g->grayagain);  /* must retraverse it in atomic phase */
//  else if (hasww)  /* table has white->white entries? */
//    linkgclist(h, g->ephemeron);  /* have to propagate again */
//  else if (hasclears)  /* table has white keys? */
//    linkgclist(h, g->allweak);  /* may have to clean white keys */
	return marked;
}

static void traversestrongtable(global_State *g, Table *h) {
//  Node *n, *limit = gnodelast(h);
	register unsigned int i, len = h->sizearray;
	for (i = 0; i < len; i++) /* traverse array part */
		markvalue(g, h->array[i]);
	len = h->lsizenode;
	int isMap = h->type;
	if (h->length) {
		for (i = 0; i < len; i++) {
			NodeMap *node = h->entry[i].node.map, *prev = NULL;
			while (node) {
				NodeMap *temp = node->next;
				if (isMap) {
					if (ttisnil(node->i_val)) {
						refDec(g->mainthread, node->i_key);
						luaM_realloc_(_S, node, sizeof(NodeMap), 0);
						if (prev) {
							prev->next = temp;
						} else {
							h->entry[i].node.map = temp;
						}
						--h->length;
					} else {
						markvalue(g, node->i_key); /* mark key */
						markvalue(g, node->i_val); /* mark value */
					}
					prev = node;
				} else {
					markvalue(g, node->i_key);
				}
				node = temp;
			}
		}
	}
}

static lu_mem traversetable(global_State *g, Table *h) {
	const char *weakkey, *weakvalue;
	const TValue *mode = gfasttm(g, h->metatable, TM_MODE);
	markobjectN(g, h->metatable);
	if (mode && ttisstring(mode)
			&& /* is there a weak mode? */
			((weakkey = strchr(svalue(mode), 'k')), (weakvalue = strchr(svalue(mode),
					'v')), (weakkey || weakvalue))) { /* is really weak? */
		black2gray(h); /* keep table gray */
		if (!weakkey) /* strong keys? */
			traverseweakvalue(g, h);
		else if (!weakvalue) /* strong values? */
			traverseephemeron(g, h);
		else
			/* all weak */
			linkgclist(h, g->allweak); /* nothing to traverse now */
	} else
		/* not weak */
		traversestrongtable(g, h);
	return sizeof(Table) + sizeof(TValue*) * h->sizearray
			+ sizeof(Entry) * cast(size_t, allocsizenode(h));
}

/*
 ** Traverse a prototype. (While a prototype is being build, its
 ** arrays can be larger than needed; the extra slots are filled with
 ** NULL, so the use of 'markobjectN')
 */
static int traverseproto(global_State *g, Proto *f) {
	int i;
	if (f->cache && iswhite(f->cache))
		f->cache = NULL; /* allow cache to be collected */
	markobjectN(g, f->source);
	for (i = 0; i < f->sizek; i++) /* mark literals */
		markvalue(g, f->k[i]);
	for (i = 0; i < f->sizeupvalues; i++) /* mark upvalue names */
		markobjectN(g, f->upvalues[i].name);
	for (i = 0; i < f->np; i++) /* mark nested protos */
		markobjectN(g, f->p[i]);
	for (i = 0; i < f->nlocvars; i++) /* mark local-variable names */
		markobjectN(g, f->locvars[i].name);
	return sizeof(Proto) + sizeof(Instruction) * f->ncode
			+ sizeof(Proto *) * f->np + sizeof(TValue) * f->sizek
			+ sizeof(int) * f->sizelineinfo + sizeof(LocVar) * f->nlocvars
			+ sizeof(Upvaldesc) * f->sizeupvalues;
}

static lu_mem traverseCclosure(global_State *g, CClosure *cl) {
	int i;
	for (i = 0; i < cl->nupvalues; i++) /* mark its upvalues */
		markvalue(g, cl->upvalue[i]);
	return sizeCclosure(cl->nupvalues);
}

/*
 ** open upvalues point to values in a thread, so those values should
 ** be marked when the thread is traversed except in the atomic phase
 ** (because then the value cannot be changed by the thread and the
 ** thread may not be traversed again)
 */
static lu_mem traverseLclosure(global_State *g, LClosure *cl) {
	int i;
	markobjectN(g, cl->p); /* mark its prototype */
	for (i = 0; i < cl->nupvalues; i++) { /* mark its upvalues */
		UpVal *uv = cl->upvals[i];
		if (uv != NULL) {
			if (upisopen(uv) && g->gcstate != GCSinsideatomic)
				uv->u.open.touched = 1; /* can be marked in 'remarkupvals' */
			else
				markvalue(g, uv->v[0]);
		}
	}
	return sizeLclosure(cl->nupvalues);
}

static lu_mem traversethread(global_State *g, lua_State *th) {
	StkId o = th->stack;
	if (o == NULL)
		return 1; /* stack not completely built yet */
	lua_assert(
			g->gcstate == GCSinsideatomic || th->openupval == NULL || isintwups(th));
	for (; o < th->top; o++) { /* mark live elements in the stack */
		TValue *v = *o;
		if (v->collectable == 1)
			markvalue(g, v);
	}
	if (g->gcstate == GCSinsideatomic) { /* final traversal? */
		StkId lim = th->stack + th->stacksize; /* real end of stack */
		for (; o < lim; o++) /* clear not-marked stack slice */
			setnilvalue(o);
		/* 'remarkupvals' may have removed thread from 'twups' list */
		if (!isintwups(th) && th->openupval != NULL) {
			th->twups = g->twups; /* link it back to the list */
			g->twups = th;
		}
	} else if (g->gckind != KGC_EMERGENCY)
		luaD_shrinkstack(th); /* do not change stack in emergency cycle */
	return (sizeof(lua_State) + sizeof(TValue) * th->stacksize
			+ sizeof(CallInfo) * th->nci);
}

/*
 ** traverse one gray object, turning it to black (except for threads,
 ** which are always gray).
 */
static void propagatemark(global_State *g) {
	lu_mem size;
	GCObj *o = g->gray;
	lua_assert(isgray(o)); //gray=0,black=4
	gray2black(o);
	switch (o->tt) {
	case LUA_TTABLE: {
		Table *h = gco2t(o);
		g->gray = h->gclist; /* remove from 'gray' list */
		size = traversetable(g, h);
		break;
	}
	case LUA_TLCL: {
		LClosure *cl = gco2lcl(o);
		g->gray = cl->gclist; /* remove from 'gray' list */
		size = traverseLclosure(g, cl);
		break;
	}
	case LUA_TCCL: {
		CClosure *cl = gco2ccl(o);
		g->gray = cl->gclist; /* remove from 'gray' list */
		size = traverseCclosure(g, cl);
		break;
	}
	case LUA_TTHREAD: {
		lua_State *th = gco2th(o);
		g->gray = th->gclist; /* remove from 'gray' list */
		linkgclist(th, g->grayagain); /* insert into 'grayagain' list */
		black2gray(o);
		size = traversethread(g, th);
		break;
	}
	case LUA_TPROTO: {
		Proto *p = gco2p(o);
		g->gray = p->gclist; /* remove from 'gray' list */
		size = traverseproto(g, p);
		break;
	}
	default:
		lua_assert(0);
		return;
	}
	g->GCmemtrav += size;
}

static void propagateall(global_State *g) {
	while (g->gray)
		propagatemark(g);
}

static void convergeephemerons(global_State *g) {
	int changed;
	do {
		GCObj *w;
		GCObj *next = g->ephemeron; /* get ephemeron list */
		g->ephemeron = NULL; /* tables may return to this list when traversed */
		changed = 0;
		while ((w = next) != NULL) {
			next = gco2t(w)->gclist;
			if (traverseephemeron(g, gco2t(w))) { /* traverse marked some value? */
				propagateall(g); /* propagate changes */
				changed = 1; /* will have to revisit all ephemeron tables */
			}
		}
	} while (changed);
}

/* }====================================================== */

/*
 ** {======================================================
 ** Sweep Functions
 ** =======================================================
 */

/*
 ** clear entries with unmarked keys from all weaktables in list 'l' up
 ** to element 'f'
 */
static void clearkeys(global_State *g, GCObj *l, GCObj *f) {
//  for (; l != f; l = gco2t(l)->gclist) {
//    Table *h = gco2t(l);
//    Node *n, *limit = gnodelast(h);
//    for (n = gnode(h, 0); n < limit; n++) {
//      if (!ttisnil(gval(n)) && (iscleared(g, gkey(n)))) {
//        setnilvalue(gval(n));  /* remove value ... */
//      }
//      if (ttisnil(gval(n)))  /* is entry empty? */
//        removeentry(n);  /* remove entry from table */
//    }
//  }
}

/*
 ** clear entries with unmarked values from all weaktables in list 'l' up
 ** to element 'f'
 */
static void clearvalues(global_State *g, GCObj *l, GCObj *f) {
	for (; l != f; l = gco2t(l)->gclist) {
		Table *h = gco2t(l);
//    Node *n, *limit = gnodelast(h);
		register unsigned int i, len = h->sizearray;
		for (i = 0; i < len; i++) {
			TValue *o = h->array[i];
			if (iscleared(g, o)) /* value was collected? */
				refDec(_S, o); /* remove value */
		}
		len = h->lsizenode;
		if (h->flags && h->length) {
			for (i = 0; i < len; i++) {
				NodeMap *node = h->entry[i].node.map, *prev = NULL, *temp;
				while (node) {
					temp = node->next;
					if (!ttisnil(node->i_val) && iscleared(g, node->i_val)) {
						refDec(_S, node->i_val);
						if (prev)
							prev->next = temp;
						else
							h->entry[i].node.map = temp;
					}
					node = temp;
					prev = node;
				}
			}
		}
	}
}

void luaC_upvdeccount(lua_State *L, UpVal *uv) {
	lua_assert(uv->refcount > 0);
	uv->refcount--;
	if (uv->refcount == 0 && !upisopen(uv))
		luaM_free(L, uv);
}

static void freeLclosure(lua_State *L, LClosure *cl) {
	int i;
	for (i = 0; i < cl->nupvalues; i++) {
		UpVal *uv = cl->upvals[i];
		if (uv)
			luaC_upvdeccount(L, uv);
	}
	luaM_freemem(L, cl, sizeLclosure(cl->nupvalues));
}

static void freeobj(lua_State *L, GCObj *o) {
	switch (o->tt) {
	case LUA_TPROTO:
		luaF_freeproto(L, gco2p(o));
		break;
	case LUA_TLCL: {
		freeLclosure(L, gco2lcl(o));
		break;
	}
	case LUA_TCCL: {
		luaM_freemem(L, o, sizeCclosure(gco2ccl(o)->nupvalues));
		break;
	}
	case LUA_TTABLE:
		luaH_free(L, gco2t(o));
		break;
	case LUA_TTHREAD:
		luaE_freethread(L, gco2th(o));
		break;
	case LUA_TUSERDATA:
		luaM_freemem(L, o, sizeudata(gco2u(o)));
		break;
	case LUA_TSHRSTR:
		luaS_remove(L, gco2ts(o)); /* remove it from hash table */
		luaM_freemem(L, o, sizelstring(gco2ts(o)->length));
		break;
	case LUA_TLNGSTR: {
		luaM_freemem(L, o, sizelstring(gco2ts(o)->length));
		break;
	}
	default:
		lua_assert(0);
	}
}

#define sweepwholelist(L,p)	sweeplist(L,p,MAX_LUMEM)
static GCObj **sweeplist(lua_State *L, GCObj **p, lu_mem count);

/*
 ** sweep at most 'count' elements from a list of GCObjects erasing dead
 ** objects, where a dead object is one marked with the old (non current)
 ** white; change all non-dead objects back to white, preparing for next
 ** collection cycle. Return where to continue the traversal or NULL if
 ** list is finished.
 */
static GCObj **sweeplist(lua_State *L, GCObj **p, lu_mem count) {
	global_State *g = G(L);
	int ow = otherwhite(g);
	int white = luaC_white(g); /* current white */
	while (*p != NULL && count-- > 0) {
		GCObj *curr = *p;
		int marked = curr->marked;
		if (isdeadm(ow, marked)) { /* 为ow就需要清除,is 'curr' dead? */
			*p = curr->value_.p; /* remove 'curr' from list */
			freeobj(L, curr); /* erase 'curr' */
		} else { /* change mark to 'white' */
			curr->marked = cast_byte((marked & maskcolors) | white);
			p = &curr->value_.p; /* go to next element */
		}
	}
	return (*p == NULL) ? NULL : p;
}

/*
 ** sweep a list until a live object (or end of list)
 */
static GCObj **sweeptolive(lua_State *L, GCObj **p) {
	GCObj **old = p;
	do {
		p = sweeplist(L, p, 1);
	} while (p == old);
	return p;
}

/* }====================================================== */

/*
 ** {======================================================
 ** Finalization
 ** =======================================================
 */

/*
 ** If possible, shrink string table
 */
static void checkSizes(lua_State *L, global_State *g) {
//  if (g->gckind != KGC_EMERGENCY) {
//    l_mem olddebt = g->GCdebt;
//    if (g->strt.nuse < g->strt.size / 4)  /* string table too big? */
//      luaS_resize(L, g->strt.size / 2);  /* shrink it a little */
//    g->GCestimate += g->GCdebt - olddebt;  /* update estimate */
//  }
}

static GCObj *udata2finalize(global_State *g) {
	GCObj *o = g->tobefnz; /* get first element */
	lua_assert(tofinalize(o));
	g->tobefnz = o->value_.p; /* remove it from 'tobefnz' list */
	o->value_.p = g->allgc; /* return it to 'allgc' list */
	g->allgc = o;
	resetbit(o->marked, FINALIZEDBIT); /* object is "normal" again */
	if (issweepphase(g))
		makewhite(g, o); /* "sweep" object */
	return o;
}

static void dothecall(lua_State *L, void *ud) {
	UNUSED(ud);
	luaD_callnoyield(L, L->top - 2, 0);
}

static void GCTM(lua_State *L, int propagateerrors) {
	global_State *g = G(L);
	const TValue *tm;
	TValue v;
	setgcovalue(L, &v, udata2finalize(g));
	tm = luaT_gettmbyobj(L, &v, TM_GC);
	if (tm != NULL && ttisfunction(tm)) { /* is there a finalizer? */
		int status;
		lu_byte oldah = L->allowhook;
		int running = g->gcrunning;
		L->allowhook = 0; /* stop debug hooks during GC metamethod */
		g->gcrunning = 0; /* avoid GC steps */
		setobj2s(L, L->top, tm); /* push finalizer... */
		setobj2s(L, L->top + 1, &v); /* ... and its argument */
		L->top += 2; /* and (next line) call the finalizer */
		L->ci->callstatus |= CIST_FIN; /* will run a finalizer */
		status = luaD_pcall(L, dothecall, NULL, savestack(L, L->top - 2), 0);
		L->ci->callstatus &= ~CIST_FIN; /* not running a finalizer anymore */
		L->allowhook = oldah; /* restore hooks */
		g->gcrunning = running; /* restore state */
		if (status != LUA_OK && propagateerrors) { /* error while running __gc? */
			if (status == LUA_ERRRUN) { /* is there an error object? */
				const char *msg =
						(ttisstring(*(L->top - 1))) ? svalue(*(L->top - 1)) : "no message";
				luaO_pushfstring(L, "error in __gc metamethod (%s)", msg);
				status = LUA_ERRGCMM; /* error in __gc metamethod */
			}
			luaD_throw(L, status); /* re-throw error */
		}
	}
}
void call_gc(lua_State *L, TValue *v, int propagateerrors) {
	global_State *g = G(L);
	const TValue *tm;
	tm = luaT_gettmbyobj(L, v, TM_GC);
	if (tm != NULL && ttisfunction(tm)) { /* is there a finalizer? */
		int status;
		lu_byte oldah = L->allowhook;
		int running = g->gcrunning;
		L->allowhook = 0; /* stop debug hooks during GC metamethod */
		g->gcrunning = 0; /* avoid GC steps */
		stack_push2(L, tm); /* push finalizer... */
		stack_push2(L, v); /* ... and its argument */
		L->ci->callstatus |= CIST_FIN; /* will run a finalizer */
		status = luaD_pcall(L, dothecall, NULL, savestack(L, L->top - 2), 0);
		L->ci->callstatus &= ~CIST_FIN; /* not running a finalizer anymore */
		L->allowhook = oldah; /* restore hooks */
		g->gcrunning = running; /* restore state */
		if (status != LUA_OK && propagateerrors) { /* error while running __gc? */
			if (status == LUA_ERRRUN) { /* is there an error object? */
				const char *msg =
						(ttisstring(*(L->top - 1))) ? svalue(*(L->top - 1)) : "no message";
				luaO_pushfstring(L, "error in __gc metamethod (%s)", msg);
				status = LUA_ERRGCMM; /* error in __gc metamethod */
			}
			luaD_throw(L, status); /* re-throw error */
		}
	}
}

/*
 ** call a few (up to 'g->gcfinnum') finalizers
 */
static int runafewfinalizers(lua_State *L) {
	global_State *g = G(L);
	unsigned int i;
	lua_assert(!g->tobefnz || g->gcfinnum > 0);
	for (i = 0; g->tobefnz && i < g->gcfinnum; i++)
		GCTM(L, 1); /* call one finalizer */
	g->gcfinnum = (!g->tobefnz) ? 0 /* nothing more to finalize? */
	:
																g->gcfinnum * 2; /* else call a few more next time */
	return i;
}

/*
 ** call all pending finalizers
 */
static void callallpendingfinalizers(lua_State *L) {
	global_State *g = G(L);
	while (g->tobefnz)
		GCTM(L, 0);
}

/*
 ** find last 'next' field in list 'p' list (to add elements in its end)
 */
static GCObj **findlast(GCObj **p) {
//	while (*p != NULL)
//		p = &(*p)->value_.p;
	return p;
}

/*
 ** move all unreachable objects (or 'all' objects) that need
 ** finalization from list 'finobj' to list 'tobefnz' (to be finalized)
 */
static void separatetobefnz(global_State *g, int all) {
//	GCObj *curr;
//	GCObj **p = &g->finobj;
//	GCObj **lastnext = findlast(&g->tobefnz);
//	while ((curr = *p) != NULL) { /* traverse all finalizable objects */
//		lua_assert(tofinalize(curr));
//		if (!(iswhite(curr) || all)) /* not being collected? */
//			p = &curr->value_.p; /* don't bother with it */
//		else {
//			*p = curr->value_.p; /* remove 'curr' from 'finobj' list */
//			curr->value_.p = *lastnext; /* link at the end of 'tobefnz' list */
//			*lastnext = curr;
//			lastnext = &curr->value_.p;
//		}
//	}
}

/*
 ** if object 'o' has a finalizer, remove it from 'allgc' list (must
 ** search the list to find it) and link it in 'finobj' list.
 */
void luaC_checkfinalizer(lua_State *L, GCObj *o, Table *mt) {
	global_State *g = G(L);
	if (gfasttm(g, mt, TM_GC) == NULL) /* obj. is already marked...  or has no finalizer? */
		return; /* nothing to be done */
	else { /* move 'o' to 'finobj' list */
//		if (issweepphase(g)) {
//			makewhite(g, o); /* "sweep" object 'o' */
//			if (g->sweepgc == &o->value_.p) /* should not remove 'sweepgc' object */
//				g->sweepgc = sweeptolive(L, g->sweepgc); /* change 'sweepgc' */
//		}
		o->collectable = 3;
		/* search for pointer pointing to 'o' */
//		for (p = &g->allgc; *p != o; p = &(*p)->value_.p) { /* empty */
//		}
//		*p = o->value_.p; /* remove 'o' from 'allgc' list */
//		o->value_.p = g->finobj; /* 插入finobj节点中,link it in 'finobj' list */
//		g->finobj = o;
//		l_setbit(o->marked, FINALIZEDBIT); /* mark it as such */
	}
}

/* }====================================================== */

/*
 ** {======================================================
 ** GC control
 ** =======================================================
 */

/*
 ** Set a reasonable "time" to wait before starting a new GC cycle; cycle
 ** will start when memory use hits threshold. (Division by 'estimate'
 ** should be OK: it cannot be zero (because Lua cannot even start with
 ** less than PAUSEADJ bytes).
 */
static void setpause(global_State *g) {
	l_mem threshold, debt;
	l_mem estimate = g->GCestimate / PAUSEADJ; /* adjust 'estimate' */
	lua_assert(estimate > 0);
	threshold = (g->gcpause < MAX_LMEM / estimate) /* overflow? */
	? estimate * g->gcpause /* no overflow */
	:
		MAX_LMEM; /* overflow; truncate to maximum */
	debt = gettotalbytes(g) - threshold;
	luaE_setdebt(g, debt);
}

/*
 ** Enter first sweep phase.
 ** The call to 'sweeplist' tries to make pointer point to an object
 ** inside the list (instead of to the header), so that the real sweep do
 ** not need to skip objects created between "now" and the start of the
 ** real sweep.
 */
static void entersweep(lua_State *L) {
	global_State *g = G(L);
	g->gcstate = GCSswpallgc;
	lua_assert(g->sweepgc == NULL);
	g->sweepgc = sweeplist(L, &g->allgc, 1);
}

void luaC_freeallobjects(lua_State *L) {
	global_State *g = G(L);
	separatetobefnz(g, 1); /* separate all objects with finalizers */
	lua_assert(g->finobj == NULL);
	callallpendingfinalizers(L);
	lua_assert(g->tobefnz == NULL);
	g->currentwhite = WHITEBITS; /* this "white" makes all objects look dead */
	g->gckind = KGC_NORMAL;
	sweepwholelist(L, &g->finobj);
	sweepwholelist(L, &g->allgc);
	sweepwholelist(L, &g->fixedgc); /* collect fixed objects */
//	lua_assert(g->strt.nuse == 0);
}

static l_mem atomic(lua_State *L) {
	global_State *g = G(L);
	l_mem work;
	GCObj *origweak, *origall;
	GCObj *grayagain = g->grayagain; /* save original list */
	lua_assert(g->ephemeron == NULL && g->weak == NULL);
	lua_assert(!iswhite(g->mainthread));
	g->gcstate = GCSinsideatomic;
	g->GCmemtrav = 0; /* start counting work */
	markobject(g, L); /* mark running thread */
	/* registry and global metatables may be changed by API */
	markvalue(g, g->glt);
	if (L != g->mainthread)
		markvalue(g, g->mainthread);
//	markvalue(g, &g->l_registry);
	markmt(g); /* mark global metatables */
	/* remark occasional upvalues of (maybe) dead threads */
	remarkupvals(g);
	propagateall(g); /* propagate changes */
	work = g->GCmemtrav; /* stop counting (do not recount 'grayagain') */
	g->gray = grayagain;
	propagateall(g); /* traverse 'grayagain' list */
	g->GCmemtrav = 0; /* restart counting */
	convergeephemerons(g);
	/* at this point, all strongly accessible objects are marked. */
	/* Clear values from weak tables, before checking finalizers */
	clearvalues(g, g->weak, NULL);
	clearvalues(g, g->allweak, NULL);
	origweak = g->weak;
	origall = g->allweak;
	work += g->GCmemtrav; /* stop counting (objects being finalized) */
	separatetobefnz(g, 0); /* separate objects to be finalized */
	g->gcfinnum = 1; /* there may be objects to be finalized */
	markbeingfnz(g); /* mark objects that will be finalized */
	propagateall(g); /* remark, to propagate 'resurrection' */
	g->GCmemtrav = 0; /* restart counting */
	convergeephemerons(g);
	/* at this point, all resurrected objects are marked. */
	/* remove dead objects from weak tables */
	clearkeys(g, g->ephemeron, NULL); /* clear keys from all ephemeron tables */
	clearkeys(g, g->allweak, NULL); /* clear keys from all 'allweak' tables */
	/* clear values from resurrected weak tables */
	clearvalues(g, g->weak, origweak);
	clearvalues(g, g->allweak, origall);
//	luaS_clearcache(g);
	g->currentwhite = cast_byte(otherwhite(g)); /* flip current white */
	work += g->GCmemtrav; /* complete counting */
	return work; /* estimate of memory marked by 'atomic' */
}

static lu_mem sweepstep(lua_State *L, global_State *g, int nextstate,
		GCObj **nextlist) {
	if (g->sweepgc) {
		l_mem olddebt = g->GCdebt;
		g->sweepgc = sweeplist(L, g->sweepgc, GCSWEEPMAX);
		g->GCestimate += g->GCdebt - olddebt; /* update estimate */
		if (g->sweepgc) /* is there still something to sweep? */
			return (GCSWEEPMAX * GCSWEEPCOST);
	}
	/* else enter next state */
	g->gcstate = nextstate;
	g->sweepgc = nextlist;
	return 0;
}

static lu_mem singlestep(lua_State *L) {
	global_State *g = G(L);
	switch (g->gcstate) {
	case GCSpause: {
		g->GCmemtrav = 0;
//      g->GCmemtrav = g->strt.size * sizeof(GCObj*);
		restartcollection(g);
		g->gcstate = GCSpropagate;
		return g->GCmemtrav;
	}
	case GCSpropagate: {
		g->GCmemtrav = 0;
		lua_assert(g->gray);
		propagatemark(g);
		if (g->gray == NULL) /* no more gray objects? */
			g->gcstate = GCSatomic; /* finish propagate phase */
		return g->GCmemtrav; /* memory traversed in this step */
	}
	case GCSatomic: {
		lu_mem work;
		propagateall(g); /* make sure gray list is empty */
		work = atomic(L); /* work is what was traversed by 'atomic' */
		entersweep(L);
		g->GCestimate = gettotalbytes(g); /* first estimate */
		;
		return work;
	}
	case GCSswpallgc: { /* sweep "regular" objects */
		return sweepstep(L, g, GCSswpfinobj, &g->finobj);
	}
	case GCSswpfinobj: { /* sweep objects with finalizers */
		return sweepstep(L, g, GCSswptobefnz, &g->tobefnz);
	}
	case GCSswptobefnz: { /* sweep objects to be finalized */
		return sweepstep(L, g, GCSswpend, NULL);
	}
	case GCSswpend: { /* finish sweeps */
		makewhite(g, g->mainthread); /* sweep main thread */
//		checkSizes(L, g);
		g->gcstate = GCScallfin;
		return 0;
	}
	case GCScallfin: { /* call remaining finalizers */
		if (g->tobefnz && g->gckind != KGC_EMERGENCY) {
			int n = runafewfinalizers(L);
			return (n * GCFINALIZECOST);
		} else { /* emergency mode or no more finalizers */
			g->gcstate = GCSpause; /* finish collection */
			return 0;
		}
	}
	default:
		lua_assert(0);
		return 0;
	}
}

/*
 ** advances the garbage collector until it reaches a state allowed
 ** by 'statemask'
 */
void luaC_runtilstate(lua_State *L, int statesmask) {
	global_State *g = G(L);
	while (!testbit(statesmask, g->gcstate))
		singlestep(L);
}

/*
 ** get GC debt and convert it from Kb to 'work units' (avoid zero debt
 ** and overflows)
 */
static l_mem getdebt(global_State *g) {
	l_mem debt = g->GCdebt;
	int stepmul = g->gcstepmul;
	if (debt <= 0)
		return 0; /* minimal debt */
	else {
		debt = (debt / STEPMULADJ) + 1;
		debt = (debt < MAX_LMEM / stepmul) ? debt * stepmul : MAX_LMEM;
		return debt;
	}
}

/*
 ** performs a basic GC step when collector is running
 */
void luaC_step(lua_State *L) {
	global_State *g = G(L);
	l_mem debt = getdebt(g); /* GC deficit (be paid now) */
	if (!g->gcrunning) { /* not running? */
		luaE_setdebt(g, -GCSTEPSIZE * 10); /* avoid being called too often */
		return;
	}
	do { /* repeat until pause or enough "credit" (negative debt) */
		lu_mem work = singlestep(L); /* perform one single step */
		debt -= work;
	} while (debt > -GCSTEPSIZE && g->gcstate != GCSpause);
	if (g->gcstate == GCSpause)
		setpause(g); /* pause until next cycle */
	else {
		debt = (debt / g->gcstepmul) * STEPMULADJ; /* convert 'work units' to Kb */
		luaE_setdebt(g, debt);
		runafewfinalizers(L);
	}
}

/*
 ** Performs a full GC cycle; if 'isemergency', set a flag to avoid
 ** some operations which could change the interpreter state in some
 ** unexpected ways (running finalizers and shrinking some structures).
 ** Before running the collection, check 'keepinvariant'; if it is true,
 ** there may be some objects marked as black, so the collector has
 ** to sweep all objects to turn them back to white (as white has not
 ** changed, nothing will be collected).
 */
void luaC_fullgc(lua_State *L, int isemergency) {
	global_State *g = G(L);
	lua_assert(g->gckind == KGC_NORMAL);
	if (isemergency)
		g->gckind = KGC_EMERGENCY; /* set flag */
	if (keepinvariant(g)) { /* black objects? */
		entersweep(L); /* sweep everything to turn them back to white */
	}
	/* finish any pending sweep phase to start a new cycle */
	luaC_runtilstate(L, bitmask(GCSpause));
	luaC_runtilstate(L, ~bitmask(GCSpause)); /* start new collection */
	luaC_runtilstate(L, bitmask(GCScallfin)); /* run up to finalizers */
	/* estimate must be correct after a full GC cycle */
	lua_assert(g->GCestimate == gettotalbytes(g));
	luaC_runtilstate(L, bitmask(GCSpause)); /* finish collection */
	g->gckind = KGC_NORMAL;
	setpause(g);
}

/* }====================================================== */

