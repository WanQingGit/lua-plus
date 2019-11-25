/*
 *  Created on: Jul 27 2019
 *  Author: WanQing
 *  E-mail: 1109162935@qq.com
 */
//@formatter:off
static void *opcode_targets[OP_EXTRAARG+1] = {
  &&TARGET_OP_MOVE			,
  &&TARGET_OP_LOADK		  ,
  &&TARGET_OP_LOADKX		,
  &&TARGET_OP_LOADBOOL  ,
  &&TARGET_OP_LOADNIL	  ,
  &&TARGET_OP_GETUPVAL  ,
  &&TARGET_OP_GETTABUP  ,
  &&TARGET_OP_GETTABLE  ,
  &&TARGET_OP_SETTABUP  ,
  &&TARGET_OP_SETUPVAL  ,
  &&TARGET_OP_SETTABLE  ,
  &&TARGET_OP_NEWTABLE  ,
  &&TARGET_OP_SELF      ,
  &&TARGET_OP_ADD       ,
  &&TARGET_OP_SUB       ,
  &&TARGET_OP_MUL       ,
  &&TARGET_OP_MOD       ,
  &&TARGET_OP_POW       ,
  &&TARGET_OP_DIV       ,
  &&TARGET_OP_IDIV      ,
  &&TARGET_OP_BAND      ,
  &&TARGET_OP_BOR       ,
  &&TARGET_OP_BXOR      ,
  &&TARGET_OP_SHL       ,
  &&TARGET_OP_SHR       ,
  &&TARGET_OP_UNM       ,
  &&TARGET_OP_BNOT      ,
  &&TARGET_OP_NOT       ,
  &&TARGET_OP_LEN       ,
  &&TARGET_OP_CONCAT    ,
  &&TARGET_OP_JMP       ,
  &&TARGET_OP_EQ        ,
  &&TARGET_OP_LT        ,
  &&TARGET_OP_LE        ,
  &&TARGET_OP_TEST      ,
  &&TARGET_OP_TESTSET   ,
  &&TARGET_OP_CALL      ,
  &&TARGET_OP_TAILCALL  ,
  &&TARGET_OP_RETURN    ,
  &&TARGET_OP_FORLOOP   ,
  &&TARGET_OP_FORPREP   ,
  &&TARGET_OP_TFORCALL  ,
  &&TARGET_OP_TFORLOOP  ,
  &&TARGET_OP_SETLIST   ,
  &&TARGET_OP_CLOSURE   ,
  &&TARGET_OP_VARARG    ,
  &&TARGET_OP_EXTRAARG  ,
};
//@formatter:on
