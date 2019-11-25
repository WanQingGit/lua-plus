/*
 ** $Id: lobject.h,v 2.117.1.1 2017/04/19 17:39:34 roberto Exp $
 ** Type definitions for Lua objects
 ** See Copyright Notice in lua.h
 */

#ifndef lobject_h
#define lobject_h

#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include "llimits.h"
#include "lua.h"

/*
 ** Extra tags for non-values
 */
//#define LUA_TPROTO	LUA_NUMTAGS		/* function prototypes */
#define LUA_TDEADKEY	(LUA_NUMTAGS+1)		/* removed keys in tables */

/*
 ** number of all possible tags (including LUA_TNONE but excluding DEADKEY)
 */
#define LUA_TOTALTAGS	(LUA_TPROTO + 2)

/* mark a tag as collectable */
#define ctb(t)			((t) | BIT_ISCOLLECTABLE)

/*
 ** Common type for all collectable objects
 */
typedef struct lua_TValue TValue;
typedef TValue** StkId; /* index to stack elements */
typedef struct GCObj GCObj;
typedef struct Proto Proto;
typedef struct Udata Udata;
typedef struct Table Table;
typedef struct TString TString;
typedef union Closure Closure;
typedef union Value Value;
/*
 ** Common Header for all collectable objects (in macro form, to be
 ** included in other objects)
 */
#define TValuefields	Value value_; VarType tt:8;lu_byte marked:6;unsigned collectable:2
//#define TValuefields	Value value_; int tt_
#define GCHead	TValuefields
//#define GCHead	struct GCObj *next; lu_byte tt; lu_byte marked

/*
 ** Tagged Values. This is the basic representation of values in Lua,
 ** an actual value plus a tag with its type.
 */

/*
 ** Union of all Lua values
 */
typedef union Value {
	GCObj *gc; /* collectable objects */
	void *p; /* light userdata */
	TString *s;
	Udata *u;
	Table *t;
	Closure *cl;
	int b; /* booleans */
	lua_CFunction f; /* light C functions */
	lua_Integer i; /* integer numbers */
	lua_Number n; /* float numbers */
} Value;

/*
 ** Common type has only the common header
 */
struct GCObj {
	GCHead;
};

typedef struct lua_TValue {
	TValuefields;
	unsigned pad :16;
} TValue;
typedef struct gcnode {
	struct gcnode *next;
	struct gcnode *prev; //指向当前结点的上一结点
	ssize_t gcref;
	ssize_t nref;
	TValue ob;
} GCNode;
typedef struct Object {
	ssize_t nref;
	TValue ob;
} Object;
typedef struct objnode {
	struct objnode *next;
	struct objnode *prev; //指向当前结点的上一结点
	Object *v;
} ObjNode;

typedef struct _gcprefix {
	struct gcnode *next;
	struct gcnode *prev; //指向当前结点的上一结点
	ssize_t gcref;
	ssize_t nref;
	GCObj ob[];
} GCPrefix;
typedef struct _objprefix {
	ssize_t nref;
	GCObj ob[];
} ObjPrefix;
#define O2B(o) (cast(GCPrefix*,o)-1)
#define refDec(L,o) do{\
  if (!ttisnil(o)){ \
  	Object *ob = OBJ(o); \
  	ob->nref--; \
  	ob->ob.marked--; \
  	if (ob->nref <= 0) \
  		obj_destroy(L,&ob->ob); \
  }\
}while(0)

LUAI_FUNC void obj_destroy(lua_State *L, TValue *o);

/* macro defining a nil value */
#define NILCONSTANT	{NULL}, LUA_TNIL

#define val_(o)		((o)->value_)

/* raw type tag of a TValue */
//VarType rttype(const TValue *o);
#define rttype(o)	((o)->tt)

/* tag with no variants (bits 0-3) */
#define novariant(x)	((x) & 0x0F)

/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */
#define ttype(o)	(rttype(o) & 0x3F)

/* type tag of a TValue with no variants (bits 0-3) */
#define ttnov(o)	(novariant(rttype(o)))

/* Macros to test type */
#define checktag(o,t)		(ttype(o) == (t))
#define checktype(o,t)		(ttnov(o) == (t))
#define ttisnumber(o)		checktype((o), LUA_TNUMBER)
#define ttisfloat(o)		checktag((o), LUA_TNUMFLT)
#define ttisinteger(o)		checktag((o), LUA_TNUMINT)
#define ttisnil(o)		(((o)==NULL) || checktag((o), LUA_TNIL))
#define ttisboolean(o)		checktag((o), LUA_TBOOLEAN)
#define ttislightuserdata(o)	checktag((o), LUA_TLIGHTUSERDATA)
#define ttisstring(o)		checktype((o), LUA_TSTRING)
#define ttisshrstring(o)	checktag((o), LUA_TSHRSTR)
#define ttislngstring(o)	checktag((o), LUA_TLNGSTR)
#define ttistable(o)		 (ttnov(o)==LUA_TTABLE)
#define ttisfunction(o)		checktype(o, LUA_TFUNCTION)
#define ttisclosure(o)		((rttype(o) & 0x1F) == LUA_TFUNCTION)
#define ttisCclosure(o)		checktag((o), LUA_TCCL)
#define ttisLclosure(o)		checktag((o), LUA_TLCL)
#define ttislcf(o)		checktag((o), LUA_TLCF)
#define ttisfulluserdata(o)	checktag((o), LUA_TUSERDATA)
#define ttisthread(o)		checktag((o), LUA_TTHREAD)
#define ttisdeadkey(o)		checktag((o), LUA_TDEADKEY)

/* Macros to access values */
#define ivalue(o)	check_exp(ttisinteger(o), val_(o).i)

//lua_Number fltvalue(const TValue *o);
#define fltvalue(o)	(o)->value_.n
#define nvalue(o)	check_exp(ttisnumber(o), \
	(ttisinteger(o) ? cast_num(ivalue(o)) : fltvalue(o)))
//#define gcvalue(o) o
#define gcvalue(o) cast(GCObj*,o)
//#define gcvalue(o) (o)->value_.gc
//#define gcvalue(o)	check_exp(iscollectable(o), (o)->value_.gc)
#define pvalue(o)	  (o)->value_.p
//TString *tsvalue(TValue *o);
#define tsvalue(o)	(o)->value_.s
#define uvalue(o)	  (o)->value_.u
typedef union Closure Closure;
#define clvalue(o)	check_exp(ttisclosure(o),(o)->value_.cl)
typedef struct LClosure LClosure;
//LClosure *clLvalue(TValue *o);
#define clLvalue(o)	check_exp(ttisLclosure(o), &(o)->value_.cl->l)
typedef struct CClosure CClosure;
//CClosure *clCvalue(TValue *o);
#define clCvalue(o)	check_exp(ttisCclosure(o), &(o)->value_.cl->c)
#define fvalue(o)		(o)->value_.f
//Table *hvalue(TValue *o);
#define hvalue(o)		(o)->value_.t
//#define hvalue(o)	check_exp(ttistable(o), gco2t(val_(o).gc))
#define bvalue(o)	(o)->value_.i
#define thvalue(o)	check_exp(ttisthread(o), gco2th(val_(o).gc))
/* a dead value may get the 'gc' field, but cannot access its contents */
#define deadvalue(o)	check_exp(ttisdeadkey(o), cast(void *, val_(o).gc))

#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

#define iscollectable(o)	((o)->collectable)

/* Macros for internal tests */
#define righttt(obj)		(ttype(obj) == gcvalue(obj)->tt)

#define checkliveness(L,obj) (void)0
//	lua_longassert(!iscollectable(obj) ||
//		(righttt(obj) && (L == NULL || !isdead(G(L),gcvalue(obj)))))

/* Macros to set values */
//void settt_(TValue *o, VarType t);
#define settt_(o,t)	((o)->tt=(t))

//void setfltvalue(TValue *io, lua_Number x);
#define setfltvalue(obj,x) \
  { TValue *io=(obj); val_(io).n=(x); settt_(io, LUA_TNUMFLT); }
//
#define chgfltvalue(obj,x) \
  { TValue *io=(obj); lua_assert(ttisfloat(io)); val_(io).n=(x); }
//void setivalue(TValue *io, lua_Integer x);
#define setivalue(obj,x) do{\
  TValue *io=(obj);\
  val_(io).i = x;\
  settt_(io, LUA_TNUMINT);\
}while(0)

#define chgivalue(obj,x) \
  { TValue *io=(obj); lua_assert(ttisinteger(io)); val_(io).i=(x); }
//#define setnilvalue(obj) *(obj)=luaO_nilobject
//void setnilvalue(StkId op);
#define setnilvalue(obj) do{\
	refDec(_S, *(obj));\
	*(obj) = luaO_nilobject;\
}while(0)
//void setfvalue(TValue *obj, lua_CFunction x);
#define setfvalue(obj,x) \
  { TValue *io=(obj); val_(io).f=(x); settt_(io, LUA_TLCF); }
//
#define setpvalue(obj,x) \
  { TValue *io=(obj); val_(io).p=(x); settt_(io, LUA_TLIGHTUSERDATA); }
//

#define setbvalue(L,obj,x) do{\
	TValue *_b = x ? boolTrue : boolFalse;\
	refDec(L, *obj);\
	refInc(_b);\
	*obj = _b;\
}while(0)
#define setgcovalue(L,obj,x)  { *obj = *((TValue*) x);  }

#define setuvalue(L,obj,x) do{\
  val_(obj).u = x;\
  settt_(obj, ctb(LUA_TUSERDATA));\
  checkliveness(L, obj);\
}while(0)
//
#define setthvalue(L,obj,x) \
  { *obj=(TValue*)x; }
//
#define setclLvalue(L,obj,x) setobj2s(L,obj,(TValue*)(x))
#define setclCvalue(L,obj,x) \
  { *obj=(TValue*)x; }
//void sethvalue(lua_State *L, TValue *obj, TValue *x);
#define sethvalue(L,obj,x) \
  { *(obj)=*((TValue*)(x)); }

#define setdeadvalue(obj)	settt_(obj, LUA_TDEADKEY)

/*
 ** different types of assignments, according to destination
 */

/* from stack to (same) stack */
#define setobjs2s	setobj
/* to stack (not from same stack) */

//#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
/* from table to same table */
#define setobjt2t	setobj
/* to new object */
#define setobj2n	setobj
#define setsvalue2n	setsvalue

/* to table (define it as an expression to be used in macros) */
#define setobj2t(L,o1,o2)  ((void)L, *(o1)=*(o2), checkliveness(L,(o1)))

/*
 ** {======================================================
 ** types and prototypes
 ** =======================================================
 */

/* prev,hash,next,
 ** Header for string value; string bytes follow the end of this structure
 ** (aligned according to 'UTString'; see next).
 */
typedef struct TString {
//	Value *data; VarType tt:8;
	GCHead;
	lu_byte extra; /* reserved words for short strings; "has hash" for longs */
	lu_byte info; /* length for short strings */
	unsigned int hash; //length
	size_t length; /* length for long strings */
	char val[];
} TString;

/*
 ** Ensures that address after this type is always fully aligned.
 */
typedef union UTString {
//	L_Umaxalign dummy; /* ensures maximum alignment for strings */
	TString tsv;
} UTString;

/*
 ** Get the actual string (array of bytes) from a 'TString'.
 ** (Access to 'extra' ensures that value is really a 'TString'.)
 */
#define getstr(ts)  \
  ((ts)->val)

/* get the actual string (array of bytes) from a Lua value */
#define svalue(o)       getstr(tsvalue(o))

/* get string length from 'TString *s' */
#define tsslen(s)	((s)->length)

/* get string length from 'TValue *o' */
#define vslen(o)	tsslen(tsvalue(o))

/*
 ** Header for userdata; memory area follows the end of this structure
 ** (aligned according to 'UUdata'; see next).
 */
typedef struct Udata {
	GCHead;
	lu_byte ttuv_; /* user value's tag */
	struct Table *metatable;
	size_t len; /* number of bytes */
	TValue *user_; /* user value */
} Udata;

/*
 ** Ensures that address after this type is always fully aligned.
 */
typedef union UUdata {
	L_Umaxalign dummy; /* ensures maximum alignment for 'local' udata */
	Udata uv;
} UUdata;

/*
 **  Get the address of memory block inside 'Udata'.
 ** (Access to 'ttuv_' ensures that value is really a 'Udata'.)
 */
#define getudatamem(u)  \
  check_exp(sizeof((u)->ttuv_), (cast(char*, (u)) + sizeof(UUdata)))

#define setuservalue(L,u,o) \
	{ TValue *io=(o); Udata *iu = (u); \
	  iu->user_ = io; iu->ttuv_ = rttype(io); \
	  checkliveness(L,io); }

#define getuservalue(L,u,o) \
	{ refDec(L,*o);TValue *io=(u->user_);refInc(io);*o=io;checkliveness(L,io); }

/*
 ** Description of an upvalue for function prototypes
 */
typedef struct Upvaldesc {
	TString *name; /* upvalue name (for debug information) */
	lu_byte instack; /* whether it is in stack (register) */
	lu_byte idx; /* index of upvalue (in stack or in outer function's list) */
} Upvaldesc;

/*
 ** Description of a local variable for function prototypes
 ** (used for debug information)
 */
typedef struct LocVar {
	TString *name;
	int startpc; /* first point where variable is active */
	int endpc; /* first point where variable is dead */
} LocVar;
typedef struct _module {
	GCHead;
	StkId k;
	int nconst;
	char *src;
	Proto *main;
} Module;
/*
 ** Function Prototypes
 */
struct Proto {
	GCHead;
	lu_byte numparams; /* number of fixed parameters */
	lu_byte is_vararg;
	lu_byte maxstacksize; /* number of registers needed by this function */
	int sizeupvalues; /* size of 'upvalues' */
	int sizek; /* size of 'k' */
	int ncode;
	int sizelineinfo;
	int np; /* size of 'p' */
	int nlocvars;
	int linedefined; /* debug information  */
	int lastlinedefined; /* debug information  */
	StkId k; /* constants used by the function */
	Instruction *code; /* opcodes */
	struct Proto **p; /* functions defined inside the function */
	int *lineinfo; /* map from opcodes to source lines (debug information) */
	LocVar *locvars; /* information about local variables (debug information) */
	Upvaldesc *upvalues; /* upvalue information */
	struct LClosure *cache; /* last-created closure with this prototype */
	TString *source; /* used for debug information */
	Module *module;
	GCObj *gclist;
};

/*
 ** Lua Upvalues
 */
//typedef struct lua_TValue UpVal;
typedef struct UpVal UpVal;

/*
 ** Closures
 */

#define ClosureHeader \
	GCHead; lu_byte nupvalues; struct GCObj *gclist

typedef struct CClosure {
	ClosureHeader;
	lua_CFunction f;
	TValue *upvalue[1]; /* list of upvalues */
} CClosure;

typedef struct LClosure {
	ClosureHeader;
	struct Proto *p;
	UpVal *upvals[1]; /* list of upvalues */
} LClosure;

typedef union Closure {
	CClosure c;
	LClosure l;
} Closure;

#define isLfunction(o)	ttisLclosure(o)

#define getproto(o)	(clLvalue(o)->p)

/* copy a value into a key without messing up field 'next' */
#define setnodekey(L,key,obj) \
	{ TKey *k_=(key); const TValue *io_=(obj); \
	  k_->nk.value_ = io_->value_; k_->nk.tt = io_->tt; \
	  (void)L; checkliveness(L,io_); }

typedef struct NodeMap {
	TValue *i_key;
	lua_Integer hash;
	struct NodeMap *next;
	TValue *i_val;
} NodeMap;
typedef struct NodeSet {
	TValue *i_key;
	lua_Integer hash;
	struct NodeSet *next;
} NodeSet;
typedef union {
	NodeMap *map;
	NodeSet *set;
} Node;
typedef struct RBTree RBTree;
typedef struct Entry {
	Node node;
#if USE_RBTREE
	RBTree *tree;
	lua_Integer len;
#endif
//#if __WORDSIZE==64
//	lua_Integer len :63;
//#else
//	lua_Integer len:31;
//#endif
//	unsigned rbfalg :1;
} Entry;

typedef struct Table {
	GCHead;
	lu_byte flags; /* 1<<p means tagmethod(p) is not present */
	lu_byte type;
	unsigned int sizearray; /* size of 'array' array */
	TValue **array; /* array part */
	Entry *entry; //Node *node;
	lua_Integer nodemask; //Node *lastfree; /* any free position is before this position */
	struct Table *metatable;
	GCObj *gclist; //Table *
	lua_Integer lsizenode; /* log2 of size of 'node' array */
	lua_Integer length;
	unsigned int len_array;
	unsigned int array_used;
} Table;

/*
 ** 'module' operation for hashing (size is always a power of 2)
 */
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))

#define twoto(x)	(1<<(x))
#define sizenode(t)	(t)->lsizenode

/*
 ** (address of) a fixed nil value
 */

//LUAI_DDEC TValue luaO_nilobject_,boolTrue_,boolFalse_;
extern TValue *boolTrue, *boolFalse, *luaO_nilobject;

/* size of buffer for 'luaO_utf8esc' function */
#define UTF8BUFFSZ	8

LUAI_FUNC int luaO_int2fb(unsigned int x);
LUAI_FUNC int luaO_fb2int(int x);
LUAI_FUNC int luaO_utf8esc(char *buff, unsigned long x);
LUAI_FUNC int luaO_ceillog2(unsigned int x);
LUAI_FUNC void luaO_arith(lua_State *L, int op, const TValue *p1,
		const TValue *p2, StkId res);
LUAI_FUNC size_t luaO_str2num(const char *s, TValue *o);
LUAI_FUNC int luaO_hexavalue(int c);
LUAI_FUNC void luaO_tostring(lua_State *L, StkId obj);
LUAI_FUNC const char *luaO_pushvfstring(lua_State *L, const char *fmt,
		va_list argp);
LUAI_FUNC const char *luaO_pushfstring(lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid(char *out, const char *source, size_t len);

void const_init(lua_State *L);
void const_destroy(lua_State *L);
#define refObj(o) (cast(ObjPrefix*,o) - 1)
#define getRef(o)    refObj(o)->nref
#define OBJ(o) 		cast(Object*,refObj(o))
#define refInc(o) 	do{getRef(o)++;OBJ(o)->ob.marked++;}while(0)
#define api_incr_top(L)   {L->top++; api_check(L, L->top <= L->ci->top, \
				"stack overflow");}
#define stack_push_nil(L) do{\
  *((L)->top) = luaO_nilobject;\
  api_incr_top(L);\
}while(0)
#define stack_push(L,obj) do{\
	TValue *_obj=cast(TValue*,obj);\
	lua_assert(ttisnil(L->top[0]));\
	refInc(_obj);\
	*(L->top) =cast(TValue*,obj);\
	api_incr_top(L);\
}while(0)
#define stack_pop(L) do{\
	L->top--;\
  refDec(L, *L->top);\
  *L->top = NULL;\
}while(0)
#define box_remove(L,bp) do{\
  qlist l = (qlist) (G(L)->boxs);\
  List.remove(l, (lNode) bp, 0);\
}while(0)
#ifdef LUA_OBJ_DEBUG
#define obj_remove(L,ob) \
	lua_assert(list_del((qlist) (G(L)->objs), (listType ) ob))
#else
#define obj_remove(L,ob)
#endif
#define box_append(L,ob) do{\
	qlist boxs=cast(qlist, G(L)->boxs);\
  List.linkNodeToPrev(boxs, (lNode) ob,\
  list_head(boxs));\
}while(0)

static inline void setsvalue(lua_State *L, StkId obj, TString *x) {
	if (!ttisnil(*obj))
		refDec(L, *obj);
	*obj = (TValue*) x;
	refInc(x);
	checkliveness(L, x);
}
static inline void setobj2s(lua_State *L, StkId obj1, const TValue *obj2) {
	if (!ttisnil(*obj1)) {
		refDec(L, *obj1);
	}
	*obj1 = (TValue *) (obj2);
	refInc(obj2);
}

static inline void moveobj(lua_State *L, StkId obj1, StkId obj2) {
	TValue *o1 = *obj1;
	refDec(L, o1);
	*obj1 = *obj2;
	*obj2 = NULL;
	checkliveness(L, obj1);
}
static inline void setobj(lua_State *L, StkId obj1, StkId obj2) {
	TValue *o = *obj2;
	refDec(L, *obj1);
	*obj1 = o;
	refInc(o);
}
#endif

