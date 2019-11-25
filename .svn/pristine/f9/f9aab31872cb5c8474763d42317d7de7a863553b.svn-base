#include <qlist.h>
#include "lmem.h"
#include <assert.h>
#define LinkNodeToTail(list, data, node) LinkNodeToPrev(list, data, node->next)
#define MAXFREELIST 64
#define MAXFREENODE MAXFREELIST*4
#define list_freeNode(node)  \
		if (numfreenode < MAXFREENODE) { \
			free_node[numfreenode++] = node; \
			} else	\
			luaM_realloc_(_S, node, sizeof(*node), 0);
static qlist free_list[MAXFREELIST];
static lNode free_node[MAXFREENODE];
static int numfreelist = 0;
static int numfreenode = 0;


/***
 函数功能：初始化链表,数据域所占内存的大小由data_size给出
 ***/
LIST_API qlist list_create() {
	qlist l;
	if (numfreelist > 0) {
		numfreelist--;
		l = free_list[numfreelist];
	} else {
		l = (qlist) luaM_realloc_(_S, NULL, 0, sizeof(struct linklist));
	}
	l->length = 0;
	l->head = list_iter(l);
	l->tail = list_iter(l);
	return l;
}
/*
 * 函数功能：把data的内容插入到链表list的末尾
 * LinkNodeToPrev(list, node, list->head);
 */
LIST_API lNode list_append(qlist list, void *data) {
	lNode node = list_newnode(list, data);
	lNode tail = list_tail(list);
	tail->next = node;
	node->prev = tail;
	node->next = cast(lNode, list);
	list_tail(list) = node;
	++list->length;
	return node;
}
/*
 * 函数功能：把data的内容插入到链表list的迭代器it_before的前面
 */
LIST_API lNode list_insert(qlist list, void *data) {
	lNode node = list_newnode(list, data);
	linkNodeToPrev(list, node, list_head(list));
//	node->prev = NULL;
	return node;
}
/***
 函数功能：把data连接到node之前
 ***/
LIST_API void linkNodeToPrev(qlist list, lNode data, lNode node) {
	lNode prev_node = node->prev;
	prev_node->next = data;
	node->prev = data;
	data->next = node;
	data->prev = prev_node;
	++list->length;
}
/***
 函数功能：从list中，移除node结点，但并不free
 注意，并不free结点，只是把结点从链中分离
 ***/
LIST_API bool list_remove(qlist list, lNode node, bool del) {
	if (node == list_iter(list))
		return false;    //不移除头结点
	lNode next_node = node->next;
	lNode prev_node = node->prev;
	//使结点node从list中分离
	next_node->prev = prev_node;
	prev_node->next = next_node;
	if (del) {
		list_freeNode(node);
	}
	//分享后，list的长度减1
	--list->length;
	return true;
}
LIST_API bool list_existnode(qlist list, lNode node) {
	lNode iter = list_iter(list);
	while (list_next(list, iter)) {
		if (iter == node)
			return true;
	}
	return false;
}
LIST_API bool list_exist(qlist list, listType data) {
	lNode iter = list_iter(list);
	while (list_next(list, iter)) {
		if (iter->data == data)
		return true;
	}
	return false;
}
LIST_API bool list_del(qlist list, listType data) {
	lNode iter = list_iter(list);
	while (list_next(list, iter)) {
		if (iter->data == data) {
			return list_remove(list, iter,1);
		}
	}
	return false;
}
/***
 函数功能：返回第index个结点的指针
 ***/
LIST_API lNode list_at(qlist list, int index) {
	lNode node = NULL;
	int len = list->length;
	assert(index < len && index >= -len);
	if (index < 0)
		index += len;
	int i = 0;
	//如果index比长度的一半小，则从前往后找
	if (index <= len >> 1) {
		//设置node初值
		node = list_head(list);
		for (i = 0; i < index; ++i) {
			node = node->next;    //向后移一个结点
		}
	}
	//否则从后往前找
	else {
		node = cast(lNode, list);    //设置node初值
		for (i = list->length; i > index; --i) {
			node = node->prev;    //向前移一个结点
		}
	}
	return node;
}

LIST_API lNode list_newnode(qlist l, void* data) {
	lNode node;
	if (numfreenode > 0) {
		node = free_node[--numfreenode];
	} else {
		node = (lNode) luaM_realloc_(_S, NULL, 0, sizeof(struct lnode));
	}
	node->data = data;
	return node;
}
LIST_API void list_merge(qlist to, qlist from) {
	if (from->tail != list_iter(from)) {
		if (to == from)
			return;
		to->head->prev = from->tail;
		from->tail->next = to->head;
		to->head = from->head;
		to->head->prev = list_iter(to);
		from->tail = list_iter(from);
		from->head = list_iter(from);
	}
	to->length += from->length;
	from->length = 0;
}

/**
 * 函数功能：销毁链表list
 */
LIST_API void list_destroy(qlist *list_ptr, void (*destructor)(void*)) {
	qlist l = *list_ptr;
	if (l == NULL)
		return;
	if (l->length) {
		lNode tmp = NULL; //用于保存被free的结点的下一个结点
		lNode node = list_head(l);
		while (l->length--) {
			assert(node != list_iter(l));
			tmp = node->next;
			if (destructor && node->data) {
				if (destructor == LIST_FORECE_FREE)
					free(node->data);
				else
					destructor(node->data);
			}
			list_freeNode(node);
			node = tmp;
		}
	}
	if (numfreelist < MAXFREELIST) {
		free_list[numfreelist] = l;
		numfreelist++;
	} else {
		luaM_realloc_(_S, l, sizeof(*l), 0);
	}
	*list_ptr = NULL;
}

LIST_API listType list_pop_back(qlist l) {
	lNode n = list_tail(l);
	listType data=n->data;
	if (list_remove(l, n,1)) {
		return data;
	}
	return NULL;
}
LIST_API listType list_pop_front(qlist l) {
	lNode n = list_head(l);
	if (list_remove(l, n,0)) {
		listType data=n->data;
		list_freeNode(n);
		return data;
	}
	return NULL;
}
LIST_API qlist list_addArray(qlist l, intptr_t *data, int n) {
	for (int i = 0; i < n; i++) {
		list_append(l, cast(void*, data[i]));
	}
	return l;
}
LIST_API void list_cache_clear() {
	for (int i = 0; i < numfreelist; i++) {
		luaM_realloc_(_S, free_list[i], sizeof(struct linklist), 0);
	}
	numfreelist = 0;
	for (int i = 0; i < numfreenode; i++) {
		luaM_realloc_(_S, free_node[i], sizeof(lnode), 0);
	}
	numfreenode = 0;
}
struct apiList List = { list_create, list_newnode, list_append, list_insert,
		linkNodeToPrev, list_remove, list_del, list_at, list_destroy, list_append,
		list_insert, list_pop_back, list_pop_front, list_addArray, list_existnode,
		list_exist, list_merge }; //链表
