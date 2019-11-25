/*
 * rbtree.c
 *
 *  Created on: Apr 11, 2019
 *      Author: WanQing
 */
#include "rbtree.h"
#include "lmem.h"
#include <math.h>
#include <stdlib.h>
#include <ldo.h>
#define qmalloc2(t,n) (t*)malloc(n*sizeof(t))
#define qmalloc(t) (t*)malloc(sizeof(t))
#define qfree free
#define rb_parent(r)   ((r)->parent)
#define rb_color(r) ((r)->color)
#define rb_is_red(r)   ((r)->color==RED)
#define rb_is_black(r)  ((r)->color==BLACK)
#define rb_set_black(r)  do { (r)->color = BLACK; } while (0)
#define rb_set_red(r)  do { (r)->color = RED; } while (0)
#define rb_set_parent(r,p)  do { (r)->parent = (p); } while (0)
#define rb_set_color(r,c)  do { (r)->color = (c); } while (0)
#define freeNode(node) do{\
		if(nfreenode<MAXFREENODE){\
			free_nodes[nfreenode++]=node;\
			if(nfreenode>highwater)\
			highwater=nfreenode;\
		}else{\
		luaM_realloc_(_S,node,sizeof(RBNode),0);\
	}}while(0)
#define MAXFREENODE 1024
#define MAXFREETREE 128
static int highwater = 0;
static RBNode *free_nodes[MAXFREENODE];
static RBTree *free_trees[MAXFREETREE];
static int nfreenode = 0;
static int nfreetree = 0;

// 创建红黑树，返回"红黑树的根"！
static RBTree* rb_create(comparef compare);

// 销毁红黑树
static void rb_destroy(lua_State *L, RBTree **tree,
		void (*destructor)(rbtype, rbtype));
static void rb_clear(lua_State *L, RBTree *t,
		void (*destructor)(rbtype, rbtype));
// 将结点插入到红黑树中。插入成功，返回true；失败返回false。
static bool rb_insert(RBTree *tree, rbtype key, rbtype val, RBNode** ptr);

// 删除结点(key为节点的值)
static void rb_remove(RBTree *tree, rbtype key);
static void rbtree_delete(RBTree *tree, RBNode *node);
// 前序遍历"红黑树"
static void rb_preorder(RBTree *tree);
// 中序遍历"红黑树"
static void rb_inorder(RBTree *tree, void (*func)(RBNode*));
// 后序遍历"红黑树"
static void rb_postorder(RBTree *tree);
static RBIter* rb_getIter(RBTree *tree);
static void rb_releaseIter(RBIter *it);
static RBNode* rb_iterNext(RBIter *it);
// (递归实现)查找"红黑树"中键值为key的节点。找到的话，返回0；否则，返回-1。
static RBNode *rb_search(RBTree *tree, rbtype key);

// 返回最小结点的值(将值保存到val中)。找到的话，返回0；否则返回-1。
static RBNode *rb_minimum(RBTree *tree);
// 返回最大结点的值(将值保存到val中)。找到的话，返回0；否则返回-1。
static RBNode *rb_maximum(RBTree *tree);
static RBNode* rb_nearest(RBTree *tree, rbtype key, rb_flag flag);
static RBNode* rb_predecessor(RBNode *x);
static RBNode* rb_successor(RBNode *x);
static RBTree *rb_clone(RBTree *tree);
void rb_insertAll(RBTree *tree, const RBTree *t, rbtype value,
		void (*failfunc)(RBNode *old, RBNode *newNode));
struct apiRB RB = { rb_create, rb_clone, rb_destroy, rb_clear, rb_insert,
		rb_insertAll, rb_remove, rbtree_delete, rb_preorder, rb_inorder,
		rb_postorder, rb_search, rb_getIter, rb_releaseIter, rb_iterNext,
		rb_minimum, rb_maximum, rb_nearest, rb_successor, rb_predecessor };

/*
 * 创建红黑树
 */
static RBTree* rb_create(comparef compare) {
	RBTree *tree;
	if (nfreetree > 0) {
		tree = free_trees[--nfreetree];
	} else
		tree = (RBTree *) luaM_realloc_(_S, NULL, 0, sizeof(RBTree));
	tree->root = NULL;
	tree->length = 0;
	tree->compare = compare;
	return tree;
}

/*
 * 前序遍历"红黑树"
 */
//static void preorder(RBNode *node) {
//	if (node != NULL) {
//		printf("%d ", node->key);
//		preorder(node->left);
//		preorder(node->right);
//	}
//}
static void rb_preorder(RBTree *tree) {
	if (!(tree && tree->root))
		return;
#ifdef QDEBUG
	preorder(tree->root);
	printf("\n");
#endif
	int size = 3 * log(tree->length + 1); //2/ln(2)=2.89
	int top = 0;
	RBNode **stack = (RBNode **) malloc(sizeof(RBNode*) * size), *node;
	stack[top++] = tree->root;
	while (top > 0) {
		node = stack[--top];
		printf("%d ", node->key);
		if (node->right)
			stack[top++] = node->right;
		if (node->left)
			stack[top++] = node->left;
	}
	free(stack);
}

static RBIter* rb_getIter(RBTree *tree) {
	RBIter *iter = qmalloc(RBIter);
	iter->t = tree;
	iter->top = 0;
	iter->node = tree->root;
	iter->stack = qmalloc2(RBNode*, 3 * log(tree->length + 1));
	return iter;
}
static void rb_releaseIter(RBIter *it) {
	free(it->stack);
	free(it);
}
/*
 * 中序遍历"红黑树"
 */
static RBNode* rb_iterNext(RBIter *it) {
	if (it->top > 0 || it->node != NULL) {
		while (it->node != NULL) {
			it->stack[it->top++] = it->node;
			it->node = it->node->left;
		}
		if (it->top > 0) {
			it->cur = it->stack[--it->top];
			it->node = it->cur->right;
		}
	} else
		return NULL;
	return it->cur;
}
static void rb_inorder(RBTree *tree, void (*func)(RBNode*)) {
	if (!(tree && tree->root))
		return;
	int size = 3 * log(tree->length + 1); //2/ln(2)=2.89
	int top = 0;
	RBNode **stack = qmalloc2(RBNode*, size), *node = tree->root;
	while (top > 0 || node != NULL) {
		while (node != NULL) {
			stack[top++] = node;
			node = node->left;
		}
		if (top > 0) {
			node = stack[--top];
			if (func) {
				func(node);
			}
			node = node->right;
		}
	}
	free(stack);
}

/*
 * 后序遍历"红黑树"
 */
static void postorder(RBNode *node) {
	if (node != NULL) {
		postorder(node->left);
		postorder(node->right);
		printf("%d ", node->key);
	}
}

static void rb_postorder(RBTree *tree) {
	if (tree && tree->root)
		postorder(tree->root);
}

/*
 * 查找"红黑树x"中键值最接近key的节点
 */
static RBNode* rb_nearest(RBTree *tree, rbtype key, rb_flag flag) {
	RBNode *x = tree->root, *below = NULL, *above = NULL;
	long res;
	while (x) {
		res = tree->compare(key, x->key);
		if (res == 0) //x->key == key
			return x;
		if (res < 0) { //key < x->key
			above = x;
			if (x->left) {
				x = x->left;
			} else
				break;
		} else {
			below = x;
			if (x->right) {
				x = x->right;
			} else
				break;
		}
	}
	switch (flag) {
	case RB_ABOVE:
		return above;
	case RB_BELOW:
		return below;
	case RB_NEAR:
		if (below && above) {
			if (abs(tree->compare(below->key, key))
					> abs(tree->compare(above->key, key))) //abs(below->key - key) > abs(above->key - key)
				return above;
			return below;
		}
		return below ? below : above;
	default:
		lua_assert(0);
	}
	return NULL;
}
/*
 * 查找"红黑树x"中键值为key的节点
 */
static RBNode *rb_search(RBTree *tree, rbtype key) {
	RBNode *x = tree->root;
	long int res;

	while (x && (res = tree->compare(x->key, key)) != 0) {
		if (res < 0) // key < x->key
			x = x->right;
		else
			x = x->left;
	}
	return x;
}

/*
 * 查找最小结点：返回tree为根结点的红黑树的最小结点。
 */

static RBNode *rb_minimum(RBTree *tree) {
	if (tree == NULL || tree->length == 0)
		return NULL;
	RBNode *node = tree->root;
	while (node->left)
		node = node->left;
	return node;
}

/*
 * 查找最大结点：返回tree为根结点的红黑树的最大结点。
 */
static RBNode *rb_maximum(RBTree *tree) {
	RBNode *node = tree->root;
	if (node == NULL)
		return NULL;
	while (node->right)
		node = node->right;
	return node;
}
static RBNode *minimum(RBNode *x) {
	while (x->left)
		x = x->left;
	return x;
}

static RBTree *rb_clone(RBTree *tree) {
	RBTree *newTree = rb_create(tree->compare);
	RBNode *iter = rb_minimum(tree);
	while (iter) {
		rb_insert(newTree, iter->key, iter->val, NULL);
		iter = rb_successor(iter);
	}
	return newTree;
}

/*
 * 找结点(x)的后继结点。即，查找"红黑树中数据值大于该结点"的"最小结点"。
 */
static RBNode* rb_successor(RBNode *x) {
	// 如果x存在右孩子，则"x的后继结点"为 "以其右孩子为根的子树的最小结点"。
	if (x->right != NULL)
		return minimum(x->right);

	// 如果x没有右孩子。则x有以下两种可能：
	// (01) x是"一个左孩子"，则"x的后继结点"为 "它的父结点"。
	// (02) x是"一个右孩子"，则查找"x的最低的父结点，并且该父结点要具有左孩子"，找到的这个"最低的父结点"就是"x的后继结点"。
	RBNode* y = x->parent;
	while ((y != NULL) && (x == y->right)) {
		x = y;
		y = y->parent;
	}
	return y;
}

static RBNode *maximum(RBNode *x) {
	while (x->right)
		x = x->right;
	return x;
}
/*
 * 找结点(x)的前驱结点。即，查找"红黑树中数据值小于该结点"的"最大结点"。
 */
static RBNode* rb_predecessor(RBNode *x) {
	// 如果x存在左孩子，则"x的前驱结点"为 "以其左孩子为根的子树的最大结点"。
	if (x->left != NULL)
		return maximum(x->left);

	// 如果x没有左孩子。则x有以下两种可能：
	// (01) x是"一个右孩子"，则"x的前驱结点"为 "它的父结点"。
	// (01) x是"一个左孩子"，则查找"x的最低的父结点，并且该父结点要具有右孩子"，找到的这个"最低的父结点"就是"x的前驱结点"。
	RBNode* y = x->parent;
	while ((y != NULL) && (x == y->left)) {
		x = y;
		y = y->parent;
	}

	return y;
}

/*
 * 对红黑树的节点(x)进行左旋转
 *
 * 左旋示意图(对节点x进行左旋)：
 *      px                              px
 *     /                               /
 *    x                               y
 *   /  \      --(左旋)-->           / \                #
 *  lx   y                          x  ry
 *     /   \                       /  \
  *    ly   ry                     lx  ly
 *
 *
 */
static void rbtree_left_rotate(RBTree *tree, RBNode *x) {
	// 设置x的右孩子为y
	RBNode *y = x->right;

	// 将 “y的左孩子” 设为 “x的右孩子”；
	// 如果y的左孩子非空，将 “x” 设为 “y的左孩子的父亲”
	x->right = y->left;
	if (y->left != NULL)
		y->left->parent = x;

	// 将 “x的父亲” 设为 “y的父亲”
	y->parent = x->parent;

	if (x->parent == NULL) {
		//tree = y;            // 如果 “x的父亲” 是空节点，则将y设为根节点
		tree->root = y;            // 如果 “x的父亲” 是空节点，则将y设为根节点
	} else {
		if (x->parent->left == x)
			x->parent->left = y;    // 如果 x是它父节点的左孩子，则将y设为“x的父节点的左孩子”
		else
			x->parent->right = y;    // 如果 x是它父节点的左孩子，则将y设为“x的父节点的左孩子”
	}

	// 将 “x” 设为 “y的左孩子”
	y->left = x;
	// 将 “x的父节点” 设为 “y”
	x->parent = y;
}

/*
 * 对红黑树的节点(y)进行右旋转
 *
 * 右旋示意图(对节点y进行左旋)：
 *            py                               py
 *           /                                /
 *          y                                x
 *         /  \      --(右旋)-->            /  \                     #
 *        x   ry                           lx   y
 *       / \                                   / \                   #
 *      lx  rx                                rx  ry
 *
 */
static void rbtree_right_rotate(RBTree *tree, RBNode *y) {
	// 设置x是当前节点的左孩子。
	RBNode *x = y->left;

	// 将 “x的右孩子” 设为 “y的左孩子”；
	// 如果"x的右孩子"不为空的话，将 “y” 设为 “x的右孩子的父亲”
	y->left = x->right;
	if (x->right != NULL)
		x->right->parent = y;

	// 将 “y的父亲” 设为 “x的父亲”
	x->parent = y->parent;

	if (y->parent == NULL) {
		//tree = x;            // 如果 “y的父亲” 是空节点，则将x设为根节点
		tree->root = x;            // 如果 “y的父亲” 是空节点，则将x设为根节点
	} else {
		if (y == y->parent->right)
			y->parent->right = x;    // 如果 y是它父节点的右孩子，则将x设为“y的父节点的右孩子”
		else
			y->parent->left = x;    // (y是它父节点的左孩子) 将x设为“x的父节点的左孩子”
	}

	// 将 “y” 设为 “x的右孩子”
	x->right = y;

	// 将 “y的父节点” 设为 “x”
	y->parent = x;
}

/*
 * 红黑树插入修正函数
 *
 * 在向红黑树中插入节点之后(失去平衡)，再调用该函数；
 * 目的是将它重新塑造成一颗红黑树。
 *
 * 参数说明：
 *     tree 红黑树的根
 *     node 插入的结点        // 对应《算法导论》中的z
 */
static void rbtree_insert_fixup(RBTree *tree, RBNode *node) {
	RBNode *parent, *gparent;

	// 若“父节点存在，并且父节点的颜色是红色”
	while ((parent = rb_parent(node)) && rb_is_red(parent)) {
		gparent = rb_parent(parent);

		//若“父节点”是“祖父节点的左孩子”
		if (parent == gparent->left) {
			// Case 1条件：叔叔节点是红色
			{
				RBNode *uncle = gparent->right;
				if (uncle && rb_is_red(uncle)) {
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			// Case 2条件：叔叔是黑色，且当前节点是右孩子
			if (parent->right == node) {
				RBNode *tmp;
				rbtree_left_rotate(tree, parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			// Case 3条件：叔叔是黑色，且当前节点是左孩子。
			rb_set_black(parent);
			rb_set_red(gparent);
			rbtree_right_rotate(tree, gparent);
		} else    //若“z的父节点”是“z的祖父节点的右孩子”
		{
			// Case 1条件：叔叔节点是红色
			{
				RBNode *uncle = gparent->left;
				if (uncle && rb_is_red(uncle)) {
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			// Case 2条件：叔叔是黑色，且当前节点是左孩子
			if (parent->left == node) {
				RBNode *tmp;
				rbtree_right_rotate(tree, parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			// Case 3条件：叔叔是黑色，且当前节点是右孩子。
			rb_set_black(parent);
			rb_set_red(gparent);
			rbtree_left_rotate(tree, gparent);
		}
	}

	// 将根节点设为黑色
	rb_set_black(tree->root);
}

/*
 * 添加节点：将节点(node)插入到红黑树中
 *
 * 参数说明：
 *     tree 红黑树的根
 *     node 插入的结点        // 对应《算法导论》中的z
 */
static void rbtree_insert(RBTree *tree, RBNode *node) {
	RBNode *y = NULL;
	RBNode *x = tree->root;
	long res;
	// 1. 将红黑树当作一颗二叉查找树，将节点添加到二叉查找树中。
	while (x != NULL) {
		y = x;
		res = tree->compare(x->key, node->key);
		if (res < 0)    //node->key < x->key
			x = x->right;
		else
			x = x->left;
	}
	rb_parent(node) = y;

	if (y != NULL) {
		res = tree->compare(y->key, node->key);
		if (res < 0)    //node->key < y->key
			y->right = node;            // 情况2：若“node所包含的值” < “y所包含的值”，则将node设为“y的左孩子”
		else
			y->left = node;           // 情况3：(“node所包含的值” >= “y所包含的值”)将node设为“y的右孩子”
	} else {
		tree->root = node;                // 情况1：若y是空节点，则将node设为根
	}

	// 2. 设置节点的颜色为红色
	node->color = RED;

	// 3. 将它重新修正为一颗二叉查找树
	rbtree_insert_fixup(tree, node);
}

/*
 * 新建结点(节点键值为key)，并将其插入到红黑树中
 *
 * 参数说明：
 *     tree 红黑树的根
 *     key 插入结点的键值
 * 返回值：
 *     RBNode *，插入成功
 *     NULL，插入失败
 */
static RBNode nilNode = { 0 };
static bool rb_insert(RBTree *tree, rbtype key, rbtype val, RBNode** ptr) {
	RBNode *node;    // 新建结点
	// 不允许插入相同键值的节点。
	// (若想允许插入相同键值的节点，注释掉下面两句话即可！)
	if ((node = rb_search(tree, key)) != NULL) {
		if (ptr)
			*ptr = node;
		return false;
	}
	if (key == 93824995082056)
		printf("debug tree\n");
	// 如果新建结点失败，则返回。
//	if ((node = create_rbtree_node(key, NULL, NULL, NULL)) == NULL)
	if (nfreenode > 0) {
		node = free_nodes[--nfreenode];
		*node = nilNode;
	} else {
		node = (RBNode*) luaM_realloc_(_S, NULL, 0, sizeof(RBNode));
	}
	node->color = BLACK; // 默认为黑色
	node->key = key;
	node->val = val;
	node->left = NULL;
	node->right = NULL;
	node->parent = NULL;
	rbtree_insert(tree, node);
	tree->length++;
	if (ptr)
		*ptr = node;
	return true;
}
void rb_insertAll(RBTree *tree, const RBTree *t, rbtype value,
		void (*failfunc)(RBNode *old, RBNode *newNode)) {
	if (t == NULL || t->length == 0)
		return;
	int size = 3 * log(t->length + 1); //2/ln(2)=2.89
	int top = 0;
	RBNode **stack = qmalloc2(RBNode*, size), *node, *node2;
	stack[top++] = t->root;
	if (value) {
		while (top > 0) {
			node = stack[--top];
			if (rb_insert(tree, node->key, value, &node2) == false && failfunc) {
				failfunc(node2, node);
			}
			if (node->right)
				stack[top++] = node->right;
			if (node->left)
				stack[top++] = node->left;
		}
	} else {
		while (top > 0) {
			node = stack[--top];
			if (rb_insert(tree, node->key, node->val, &node2) == false && failfunc) {
				failfunc(node2, node);
			}
			if (node->right)
				stack[top++] = node->right;
			if (node->left)
				stack[top++] = node->left;
		}
	}
	free(stack);
}
/*
 * 红黑树删除修正函数
 *
 * 在从红黑树中删除插入节点之后(红黑树失去平衡)，再调用该函数；
 * 目的是将它重新塑造成一颗红黑树。
 *
 * 参数说明：
 *     tree 红黑树的根
 *     node 待修正的节点
 */
static void rbtree_delete_fixup(RBTree *tree, RBNode *node, RBNode *parent) {
	RBNode *other;

	while ((!node || rb_is_black(node)) && node != tree->root) {
		if (parent->left == node) {
			other = parent->right;
			if (rb_is_red(other)) {
				// Case 1: x的兄弟w是红色的
				rb_set_black(other);
				rb_set_red(parent);
				rbtree_left_rotate(tree, parent);
				other = parent->right;
			}
			if ((!other->left || rb_is_black(other->left))
					&& (!other->right || rb_is_black(other->right))) {
				// Case 2: x的兄弟w是黑色，且w的俩个孩子也都是黑色的
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			} else {
				if (!other->right || rb_is_black(other->right)) {
					// Case 3: x的兄弟w是黑色的，并且w的左孩子是红色，右孩子为黑色。
					rb_set_black(other->left);
					rb_set_red(other);
					rbtree_right_rotate(tree, other);
					other = parent->right;
				}
				// Case 4: x的兄弟w是黑色的；并且w的右孩子是红色的，左孩子任意颜色。
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->right);
				rbtree_left_rotate(tree, parent);
				node = tree->root;
				break;
			}
		} else {
			other = parent->left;
			if (rb_is_red(other)) {
				// Case 1: x的兄弟w是红色的
				rb_set_black(other);
				rb_set_red(parent);
				rbtree_right_rotate(tree, parent);
				other = parent->left;
			}
			if ((!other->left || rb_is_black(other->left))
					&& (!other->right || rb_is_black(other->right))) {
				// Case 2: x的兄弟w是黑色，且w的俩个孩子也都是黑色的
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			} else {
				if (!other->left || rb_is_black(other->left)) {
					// Case 3: x的兄弟w是黑色的，并且w的左孩子是红色，右孩子为黑色。
					rb_set_black(other->right);
					rb_set_red(other);
					rbtree_left_rotate(tree, other);
					other = parent->left;
				}
				// Case 4: x的兄弟w是黑色的；并且w的右孩子是红色的，左孩子任意颜色。
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->left);
				rbtree_right_rotate(tree, parent);
				node = tree->root;
				break;
			}
		}
	}
	if (node)
		rb_set_black(node);
}

/*
 * 删除结点
 *
 * 参数说明：
 *     tree 红黑树的根结点
 *     node 删除的结点
 */
static void rbtree_delete(RBTree *tree, RBNode *node) {
	RBNode *child, *parent;
	int color;

	// 被删除节点的"左右孩子都不为空"的情况。
	if ((node->left != NULL) && (node->right != NULL)) {
		// 被删节点的后继节点。(称为"取代节点")
		// 用它来取代"被删节点"的位置，然后再将"被删节点"去掉。
		RBNode *replace = node;

		// 获取后继节点
		replace = replace->right;
		while (replace->left != NULL)
			replace = replace->left;

		// "node节点"不是根节点(只有根节点不存在父节点)
		if (rb_parent(node)) {
			if (rb_parent(node)->left == node)
				rb_parent(node)->left = replace;
			else
				rb_parent(node)->right = replace;
		} else
			// "node节点"是根节点，更新根节点。
			tree->root = replace;

		// child是"取代节点"的右孩子，也是需要"调整的节点"。
		// "取代节点"肯定不存在左孩子！因为它是一个后继节点。
		child = replace->right;
		parent = rb_parent(replace);
		// 保存"取代节点"的颜色
		color = rb_color(replace);

		// "被删除节点"是"它的后继节点的父节点"
		if (parent == node) {
			parent = replace;
		} else {
			// child不为空
			if (child)
				rb_set_parent(child, parent);
			parent->left = child;

			replace->right = node->right;
			rb_set_parent(node->right, replace);
		}

		replace->parent = node->parent;
		replace->color = node->color;
		replace->left = node->left;
		node->left->parent = replace;

		if (color == BLACK)
			rbtree_delete_fixup(tree, child, parent);
	} else {
		if (node->left != NULL)
			child = node->left;
		else
			child = node->right;

		parent = node->parent;
		// 保存"取代节点"的颜色
		color = node->color;

		if (child)
			child->parent = parent;

		// "node节点"不是根节点
		if (parent) {
			if (parent->left == node)
				parent->left = child;
			else
				parent->right = child;
		} else
			tree->root = child;

		if (color == BLACK)
			rbtree_delete_fixup(tree, child, parent);
	}
	freeNode(node);
	--tree->length;
}

/*
 * 删除键值为key的结点
 *
 * 参数说明：
 *     tree 红黑树的根结点
 *     key 键值
 */
static void rb_remove(RBTree *tree, rbtype key) {
	RBNode *z;
	if ((z = rb_search(tree, key)) != NULL) {
		rbtree_delete(tree, z);
	}
}

static void rb_clear(lua_State *L, RBTree *t,
		void (*destructor)(rbtype, rbtype)) {
	if (t == NULL)
		return;
	intptr_t iptr = (intptr_t) destructor;
	if (t->length) {
		int size = 3 * log(t->length + 1); //2/ln(2)=2.89
		int top = 0;
		luaD_checkstackaux(L, size, NULL, NULL);
		RBNode **stack = (RBNode **) L->top, *node;
		stack[top++] = t->root;
		while (top > 0) {
			node = stack[--top];
			if (node->right)
				stack[top++] = node->right;
			if (node->left)
				stack[top++] = node->left;
			if (destructor) {
				if (iptr & RB_FREE_FORCE) {
					if (iptr & RB_FREE_KEY) {
						qfree(node->key);
					}
					if (iptr & RB_FREE_VAL) {
						qfree(node->val);/*luaM_realloc_(_S,node->val,1,NULL);*/
					}
				} else
					destructor(node->key, node->val);
			}
			freeNode(node);
		}
		t->root = NULL;
		t->length = 0;
	}
}
/*
 * 销毁红黑树
 */
static void rb_destroy(lua_State *L, RBTree **tree_ptr,
		void (*destructor)(rbtype, rbtype)) {
	RBTree *t = *tree_ptr;
	if (t == NULL)
		return;
	rb_clear(L, t, destructor);
	if (nfreetree < MAXFREETREE) {
		free_trees[nfreetree++] = t;
	} else
		luaM_realloc_(_S, t, sizeof(RBTree), 0);
	*tree_ptr = NULL;
}
void rb_cache_clear() {
	register int i;
	for (i = 0; i < nfreetree; i++) {
		luaM_realloc_(_S, free_trees[i], sizeof(RBTree), 0);
	}
	nfreetree = 0;
	for (i = 0; i < nfreenode; i++) {
		luaM_realloc_(_S, free_nodes[i], sizeof(RBNode), 0);
	}
	nfreenode = 0;
}

