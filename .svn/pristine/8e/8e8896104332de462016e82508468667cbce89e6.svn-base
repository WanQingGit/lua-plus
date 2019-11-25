#ifndef LINKLIST_H_INCLUDED
#define LINKLIST_H_INCLUDED
#include "lobject.h"
#define list_tail(l) l->tail
#define list_head(l) cast(qlist,l)->head
#define list_data(n) (n)->data
#define list_iter(l) cast(lNode,l)
#define list_get(l,t) cast(t,list_data(l))
#define list_getTail(l,t) list_get(list_tail(l),t)
#define list_getHead(l,t) list_get(list_head(l),t)
//使it指向下一个位置，并返回指向下一个位置后的迭代器
#define list_next(l,t) (t=t->next)==list_iter(l)?NULL:t
#define list_prev(l,t) (t=t->prev)==list_iter(l)?NULL:t
#define list_empty(l) (l->head==l->tail)
#define list_notempty(l) (l->head!=l->tail)
#define LIST_NOT_FREE 0
#define LIST_FORECE_FREE 1
#define listType void*
typedef struct lnode {
	struct lnode *next;
	struct lnode *prev; //指向当前结点的上一结点
	listType data; //数据域指针
}*lNode,lnode;
//list为空时head和tail指向本身
typedef struct linklist {
	struct lnode *head;
	struct lnode *tail; //指向当前结点的上一结点
	intptr_t length; //链表list的长度
}*qlist;

struct apiList {
	//创建链表
	qlist (*create)();
	lNode (*newnode)(qlist l, listType data);
	//把data的内容插入到链表list的末尾
	lNode (*append)(qlist linklist, listType data);

	//把data的内容插入到链表的迭代器it_before的前面
	//assign指定数据data间的赋值方法
	lNode (*insert)(qlist linklist, listType data);
	void (*linkNodeToPrev)(qlist linklist, lNode data, lNode lnode);

	//删除链表list中node指向的结点
	bool (*remove)(qlist linklist, lNode node, bool del);
	bool (*del)(qlist list, listType data);
	//返回list中第index个数据的指针
	lNode (*at)(qlist linklist, int index);

	//销毁list
	void (*destroy)(qlist *list_ptr, void (*destructor)(listType));

	lNode (*push_back)(qlist l, listType data);

	lNode (*push_front)(qlist l, listType data);
	listType (*pop_back)(qlist l);
	listType (*pop_front)(qlist l);
	qlist (*addArray)(qlist l, intptr_t *data, int n);
	bool (*exist_node)(qlist list, lNode node);
	bool (*exist)(qlist list, listType data);
	void (*merge)(qlist to, qlist from);
};
//LinkList
extern struct apiList List;

#define LIST_API inline
LIST_API qlist list_create();

//把data的内容插入到链表list的末尾
//assign指定数据data间的赋值方法
LIST_API lNode list_append(qlist list, void *data);

//把data的内容插入到链表的迭代器it_before的前面
//assign指定数据data间的赋值方法
LIST_API lNode list_insert(qlist list, void *data);
LIST_API void linkNodeToPrev(qlist list, lNode data, lNode node);
LIST_API bool list_remove(qlist list, lNode node, bool del);
LIST_API bool list_existnode(qlist list, lNode node);
LIST_API bool list_exist(qlist list, listType data);
LIST_API bool list_del(qlist list, listType data);
LIST_API lNode list_at(qlist list, int index);
LIST_API lNode list_newnode(qlist list, void* data);
LIST_API void list_merge(qlist to, qlist from);
LIST_API void list_destroy(qlist *list_ptr, void (*destructor)(void*));
LIST_API listType list_pop_back(qlist l);
LIST_API listType list_pop_front(qlist l);
LIST_API qlist list_addArray(qlist l, intptr_t *data, int n);
LIST_API void list_cache_clear();
#endif // LIST_H_INCLUDED
