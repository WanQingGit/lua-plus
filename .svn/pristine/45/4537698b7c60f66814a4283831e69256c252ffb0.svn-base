/*
 * qmem_pool.c
 *
 *  Created on: Apr 15, 2019
 *      Author: WanQing
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define ALIGNMENT               8               /* must be 2^N */
#define ALIGNMENT_SHIFT         3
#define SMALL_REQUEST_THRESHOLD 512
#define NB_SMALL_SIZE_CLASSES   (SMALL_REQUEST_THRESHOLD / ALIGNMENT)
typedef unsigned char block;
typedef unsigned int uint;
/* Return the number of bytes in size class I, as a uint. */
#define INDEX2SIZE(I) (((uint)(I) + 1) << ALIGNMENT_SHIFT)
#define SIZE2INDEX(size) (((uint)(size) - 1) >> ALIGNMENT_SHIFT)
#define POOL_CHECK 1
struct pool_header {
	union {
		block *_padding;
		uint count;
	} ref; /* number of allocated blocks    */
	block *freeblock; /* pool's free list head         */
	struct pool_header *nextpool; /* next pool of this size class  */
	struct pool_header *prevpool; /* previous pool       ""        */
	uint arenaindex; /* index into arenas of base adr */
	uint szidx; /* block size class index        */
	uint nextoffset; /* bytes to virgin block         */
	uint maxnextoffset; /* largest valid nextoffset      */
};
typedef struct pool_header *poolp;
#define PTA(x)  ((poolp )((uint8_t *)&(usedpools[2*(x)]) - 2*sizeof(block *)))
#define PT(x)   PTA(x), PTA(x)
//@formatter:off
/*Initializes bidirectional linked list */
static poolp usedpools[NB_SMALL_SIZE_CLASSES * 2] = {
		PT(0),  PT(1),  PT(2),  PT(3),  PT(4),  PT(5),  PT(6),  PT(7),
		PT(8),  PT(9),  PT(10), PT(11), PT(12), PT(13), PT(14), PT(15),
		PT(16), PT(17), PT(18), PT(19), PT(20), PT(21), PT(22), PT(23),
		PT(24), PT(25), PT(26), PT(27), PT(28), PT(29), PT(30), PT(31),
		PT(32), PT(33), PT(34), PT(35), PT(36), PT(37), PT(38), PT(39),
		PT(40), PT(41), PT(42), PT(43), PT(44), PT(45), PT(46), PT(47),
		PT(48), PT(49), PT(50), PT(51), PT(52), PT(53), PT(54), PT(55),
		PT(56), PT(57), PT(58), PT(59), PT(60), PT(61), PT(62), PT(63),
};
//@formatter:on
/* Record keeping for arenas. */
typedef struct arena_object {
	/* The address of the arena, as returned by malloc.  Note that 0
	 * will never be returned by a successful malloc, and is used
	 * here to mark an arena_object that doesn't correspond to an
	 * allocated arena.
	 */
	uintptr_t address;

	/* Pool-aligned pointer to the next pool to be carved off. */
	block* pool_address;

	/* The number of available pools in the arena:  free pools + never-
	 * allocated pools.
	 */
	uint nfreepools;

	/* The total number of pools in the arena, whether or not available. */
	uint ntotalpools;

	/* Singly-linked list of available pools. */
	struct pool_header* freepools;

	/* Whenever this arena_object is not associated with an allocated
	 * arena, the nextarena member is used to link all unassociated
	 * arena_objects in the singly-linked `unused_arena_objects` list.
	 * The prevarena member is unused in this case.
	 *
	 * When this arena_object is associated with an allocated arena
	 * with at least one available pool, both members are used in the
	 * doubly-linked `usable_arenas` list, which is maintained in
	 * increasing order of `nfreepools` values.
	 *
	 * Else this arena_object is associated with an allocated arena
	 * all of whose pools are in use.  `nextarena` and `prevarena`
	 * are both meaningless in this case.
	 */
	struct arena_object* nextarena;
	struct arena_object* prevarena;
} arena_obj;

/* The head of the singly-linked, NULL-terminated list of available
 * arena_objects.
 */
static struct arena_object* unused_arena_objects = NULL;
/* The head of the doubly-linked, NULL-terminated at each end, list of
 * arena_objects associated with arenas that have pools available.
 */
static struct arena_object* usable_arenas = NULL;
/* Number of slots currently allocated in the `arenas` vector. */
static uint narenas = 0;
/* How many arena_objects do we initially allocate?
 * 16 = can allocate 16 arenas = 16 * ARENA_SIZE = 4MB before growing the
 * `arenas` vector.
 */

/* Array of objects used to track chunks of memory (arenas). */
static struct arena_object* arenas = NULL;
#define INITIAL_ARENA_OBJECTS 16
#define ARENA_SIZE              (256 << 10)     /* 256KB */
/* Number of arenas allocated that haven't been free()'d. */
static size_t narenas_currently_allocated = 0;
/* High water mark (max value ever seen) for narenas_currently_allocated. */
static size_t narenas_highwater = 0;
/* Total number of times malloc() called to allocate an arena. */
static size_t ntimes_arena_allocated = 0;
#define SYSTEM_PAGE_SIZE        (4 * 1024)
#define SYSTEM_PAGE_SIZE_MASK   (SYSTEM_PAGE_SIZE - 1)
/*
 * Size of the pools used for small blocks. Should be a power of 2,
 * between 1K and SYSTEM_PAGE_SIZE, that is: 1k, 2k, 4k.
 */
#define POOL_SIZE               SYSTEM_PAGE_SIZE        /* must be 2^N */
#define POOL_OVERHEAD sizeof(struct pool_header)
#define DUMMY_SIZE_IDX          0xffff  /* size class of newly cached pools */
#define _Sy_ALIGN_DOWN(p, a) ((void *)((uintptr_t)(p) & ~(uintptr_t)((a) - 1)))

/* Round pointer P down to the closest pool-aligned address <= P, as a poolp */
#define POOL_ADDR(P) ((poolp)_Sy_ALIGN_DOWN((P), POOL_SIZE))
#define POOL_SIZE_MASK          SYSTEM_PAGE_SIZE_MASK
static int address_in_range(void *p, poolp pool) {
	// Since address_in_range may be reading from memory which was not allocated
	// by Python, it is important that pool->arenaindex is read only once, as
	// another thread may be concurrently modifying the value without holding
	// the GIL. The following dance forces the compiler to read pool->arenaindex
	// only once.
	//By using unsigned arithmetic, the "0 <=" half of the test can be skipped.
	uint arenaindex = *((volatile uint *) &pool->arenaindex);
	return arenaindex < narenas
			&& (uintptr_t) p - arenas[arenaindex].address < ARENA_SIZE
			&& arenas[arenaindex].address != 0;
}
static arena_obj* pool_new_arena() {
	arena_obj *arenaobj;
	uint excess; /* number of bytes above pool alignment */
	void *address;

	if (unused_arena_objects == NULL) {
		uint i, j;
		uint numarenas;
		size_t nbytes;

		/* Double the number of arena objects on each allocation.
		 * Note that it's possible for `numarenas` to overflow.
		 */
		numarenas = narenas ? narenas << 1 : INITIAL_ARENA_OBJECTS;
		if (numarenas <= narenas)
			return NULL; /* overflow */
		nbytes = numarenas * sizeof(*arenas);	//sizeof(struct arena_object)
		arenaobj = realloc(arenas, nbytes);
//		S->g->gc.GCdebt += nbytes - narenas * sizeof(*arenas);
//		arenaobj = skym_alloc(S, arenas, narenas * sizeof(*arenas), nbytes);
//		if (arenaobj == NULL)
//			return NULL;
		assert(arenaobj);
		arenas = arenaobj;
		/* We might need to fix pointers that were copied.  However,
		 * new_arena only gets called when all the pages in the
		 * previous arenas are full.  Thus, there are *no* pointers
		 * into the old array. Thus, we don't have to worry about
		 * invalid pointers.  Just to be sure, some asserts:
		 */
		assert(usable_arenas == NULL);
		assert(unused_arena_objects == NULL);
		j = numarenas - 1;
		/* Put the new arenas on the unused_arena_objects list. */
		for (i = narenas; i < j; ++i) {
			arenas[i].address = 0; /* mark as unassociated */
			arenas[i].nextarena = &arenas[i + 1];
		}
		arenas[i].nextarena = NULL;
		/* Update globals. */
		unused_arena_objects = &arenas[narenas];
		narenas = numarenas;
	}

	/* Take the next available arena object off the head of the list. */
	assert(unused_arena_objects != NULL);
	arenaobj = unused_arena_objects;
	unused_arena_objects = arenaobj->nextarena;
	assert(arenaobj->address == 0);
	address = malloc(ARENA_SIZE);	//calloc
	if (address == NULL) {
		/* The allocation failed: return NULL after putting the
		 * arenaobj back.
		 */
		arenaobj->nextarena = unused_arena_objects;
		unused_arena_objects = arenaobj;
		return NULL;
	}
	arenaobj->address = (uintptr_t) address;
	++narenas_currently_allocated;
	++ntimes_arena_allocated;
	if (narenas_currently_allocated > narenas_highwater)
		narenas_highwater = narenas_currently_allocated;
	arenaobj->freepools = NULL;
	/* pool_address <- first pool-aligned address in the arena
	 nfreepools <- number of whole pools that fit after alignment */
	arenaobj->pool_address = (block*) arenaobj->address;
	arenaobj->nfreepools = ARENA_SIZE / POOL_SIZE;
	assert(POOL_SIZE * arenaobj->nfreepools == ARENA_SIZE);
	excess = (uint) (arenaobj->address & POOL_SIZE_MASK);
	if (excess != 0) {
		--arenaobj->nfreepools;
		arenaobj->pool_address += POOL_SIZE - excess;
	}
	arenaobj->ntotalpools = arenaobj->nfreepools;
	return arenaobj;
}

static int mem_malloc(/*State *S,*/void **ptr, size_t nbytes) {
	uint size;
	poolp pool;
	block *bp;
	poolp next;
#if POOL_CHECK
	if (nbytes == 0 || nbytes > SMALL_REQUEST_THRESHOLD) {
		return 0;
	}
#endif
	size = (uint) (nbytes - 1) >> ALIGNMENT_SHIFT;
	pool = usedpools[size * 2];
	if (pool != pool->nextpool) {
		++pool->ref.count;
		bp = pool->freeblock;
		assert(bp != NULL);
		if ((pool->freeblock = *(block **) bp) != NULL) {
			goto success;
		}
		/*
		 * Reached the end of the free list, try to extend it.
		 */
		else if (pool->nextoffset <= pool->maxnextoffset) {
			/* There is room for another block. */
			pool->freeblock = (block*) pool + pool->nextoffset;
			pool->nextoffset += INDEX2SIZE(size);
			*(block **) (pool->freeblock) = NULL; //使block链尾为NULL,freeblock指向的数值为零
			goto success;
		}
		/* Pool is full, unlink from used pools. */
		next = pool->nextpool;
		pool = pool->prevpool;
		next->prevpool = pool;
		pool->nextpool = next;
		goto success;
	} //if (pool != pool->nextpool)
	/* There isn't a pool of the right size class immediately
	 * available:  use a free pool.
	 */
	if (usable_arenas == NULL) {
		/* No arena has a free pool:  allocate a new arena. */
		usable_arenas = pool_new_arena(); //freepools为NULL，需初始化
		if (usable_arenas == NULL) {
			goto failed;
		}
		usable_arenas->nextarena = usable_arenas->prevarena = NULL;
	}
	assert(usable_arenas->address != 0);
	pool = usable_arenas->freepools;
	if (pool != NULL) {
		/* Unlink from cached pools. */
		usable_arenas->freepools = pool->nextpool;

		/* This arena already had the smallest nfreepools
		 * value, so decreasing nfreepools doesn't change
		 * that, and we don't need to rearrange the
		 * usable_arenas list.  However, if the arena has
		 * become wholly allocated, we need to remove its
		 * arena_object from usable_arenas.
		 */
		--usable_arenas->nfreepools;
		if (usable_arenas->nfreepools == 0) {
			/* Wholly allocated:  remove. */
			assert(usable_arenas->freepools == NULL);
			assert(
					usable_arenas->nextarena == NULL || usable_arenas->nextarena->prevarena == usable_arenas);
			usable_arenas = usable_arenas->nextarena;
			if (usable_arenas != NULL) {
				usable_arenas->prevarena = NULL;
				assert(usable_arenas->address != 0);
			}
		} else {
			/* nfreepools > 0:  it must be that freepools
			 * isn't NULL, or that we haven't yet carved
			 * off all the arena's pools for the first
			 * time.
			 */
			assert(
					usable_arenas->freepools != NULL || usable_arenas->pool_address <= (block*)usable_arenas->address + ARENA_SIZE - POOL_SIZE);
		}

		init_pool:
		/* Frontlink to used pools. */
		next = usedpools[size * 2]; /* == prev */
		pool->nextpool = next;
		pool->prevpool = next;
		next->nextpool = pool;
		next->prevpool = pool;
		pool->ref.count = 1;
		if (pool->szidx == size) {
			/* Luckily, this pool last contained blocks
			 * of the same size class, so its header
			 * and free list are already initialized.
			 */
			bp = pool->freeblock;
			assert(bp != NULL);
			pool->freeblock = *(block **) bp;
			goto success;
		}
		/*
		 * Initialize the pool header, set up the free list to
		 * contain just the second block, and return the first
		 * block.
		 */
		pool->szidx = size;
		size = INDEX2SIZE(size);
		bp = (block *) pool + POOL_OVERHEAD; //size << 1 eq size*2
		pool->nextoffset = POOL_OVERHEAD + (size << 1); //second block
		pool->maxnextoffset = POOL_SIZE - size;
		pool->freeblock = bp + size; //pool->freeblock指向的地址的4或8个字节为0
		*(block **) (pool->freeblock) = NULL;
		goto success;
	} //if (pool != NULL)
	/* if pool == NULL,说明第一次使用,Carve off a new pool. */
	assert(usable_arenas->nfreepools > 0);
	assert(usable_arenas->freepools == NULL);
	pool = (poolp) usable_arenas->pool_address;
	assert(
			(block*)pool <= (block*)usable_arenas->address + ARENA_SIZE - POOL_SIZE);
	pool->arenaindex = (uint) (usable_arenas - arenas);
	assert(&arenas[pool->arenaindex] == usable_arenas);
	pool->szidx = DUMMY_SIZE_IDX;
	usable_arenas->pool_address += POOL_SIZE;
	--usable_arenas->nfreepools;

	if (usable_arenas->nfreepools == 0) {
		assert(
				usable_arenas->nextarena == NULL || usable_arenas->nextarena->prevarena == usable_arenas);
		/* Unlink the arena:  it is completely allocated. */
		usable_arenas = usable_arenas->nextarena;
		if (usable_arenas != NULL) {
			usable_arenas->prevarena = NULL;
			assert(usable_arenas->address != 0);
		}
	}
	goto init_pool;
	success:
//	UNLOCK();
	assert(bp != NULL);
	*ptr = (void *) bp;
	return 1;
	failed: *ptr = NULL;
//	UNLOCK();
	return 0;
}
static int mem_free(void *p) {
	poolp pool;
	pool = POOL_ADDR(p);
	block *lastfree;
	poolp next, prev;
	uint size;
	if (!address_in_range(p, pool)) {
		free(p);
		return 0;
	}
	/* We allocated this address. */
//	LOCK();
	/* Link p to the start of the pool's freeblock list.  Since
	 * the pool had at least the p block outstanding, the pool
	 * wasn't empty (so it's already in a usedpools[] list, or
	 * was full and is in no list -- it's not in the freeblocks
	 * list in any case).
	 */
	assert(pool->ref.count > 0); /* else it was empty */
	*(block **) p = lastfree = pool->freeblock;
	pool->freeblock = (block *) p;
	if (!lastfree) {
		/* Pool was full, so doesn't currently live in any list:
		 * link it to the front of the appropriate usedpools[] list.
		 * This mimics LRU pool usage for new allocations and
		 * targets optimal filling when several pools contain
		 * blocks of the same size class.
		 */
		--pool->ref.count;
		assert(pool->ref.count > 0); /* else the pool is empty */
		size = pool->szidx;
		next = usedpools[size * 2];
		prev = next->prevpool;

		/* insert pool before next:   prev <-> pool <-> next */
		pool->nextpool = next;
		pool->prevpool = prev;
		next->prevpool = pool;
		prev->nextpool = pool;
		goto success;
	}
	/* freeblock wasn't NULL, so the pool wasn't full,
	 * and the pool is in a usedpools[] list.
	 */
	else if (--pool->ref.count != 0) {
		/* pool isn't empty:  leave it in usedpools */
		goto success;
	}

	struct arena_object* ao;
	uint nf; /* ao->nfreepools */
	/* Pool is now empty:  unlink from usedpools, and
	 * link to the front of freepools.  This ensures that
	 * previously freed pools will be allocated later
	 * (being not referenced, they are perhaps paged out).
	 */
	next = pool->nextpool;
	prev = pool->prevpool;
	next->prevpool = prev;
	prev->nextpool = next;
	/* Link the pool to freepools.  This is a singly-linked
	 * list, and pool->prevpool isn't used there.
	 */
	ao = &arenas[pool->arenaindex];
	pool->nextpool = ao->freepools;
	ao->freepools = pool;
	nf = ++ao->nfreepools;
	/* All the rest is arena management.  We just freed
	 * a pool, and there are 4 cases for arena mgmt:
	 * 1. If all the pools are free, return the arena to
	 *    the system free().
	 * 2. If this is the only free pool in the arena,
	 *    add the arena back to the `usable_arenas` list.
	 * 3. If the "next" arena has a smaller count of free
	 *    pools, we have to "slide this arena right" to
	 *    restore that usable_arenas is sorted in order of
	 *    nfreepools.
	 * 4. Else there's nothing more to do.
	 */
	if (nf == ao->ntotalpools) {
		/* Case 1.  First unlink ao from usable_arenas.
		 */
		assert(ao->prevarena == NULL || ao->prevarena->address != 0);
		assert(ao ->nextarena == NULL || ao->nextarena->address != 0);

		/* Fix the pointer in the prevarena, or the
		 * usable_arenas pointer.
		 */
		if (ao->prevarena == NULL) {
			usable_arenas = ao->nextarena;
			assert(usable_arenas == NULL || usable_arenas->address != 0);
		} else {
			assert(ao->prevarena->nextarena == ao);
			ao->prevarena->nextarena = ao->nextarena;
		}
		/* Fix the pointer in the nextarena. */
		if (ao->nextarena != NULL) {
			assert(ao->nextarena->prevarena == ao);
			ao->nextarena->prevarena = ao->prevarena;
		}
		/* Record that this arena_object slot is
		 * available to be reused.
		 */
		ao->nextarena = unused_arena_objects;
		unused_arena_objects = ao;

		/* Free the entire arena. */
//		skym_alloc(S, ao->address, ARENA_SIZE, 0);
		free((void*) ao->address);
//		S->g->gc.GCdebt -= ARENA_SIZE;
		ao->address = 0; /* mark unassociated */
		--narenas_currently_allocated;
		goto success;
	} else if (nf == 1) {
		/* Case 2.  Put ao at the head of
		 * usable_arenas.  Note that because
		 * ao->nfreepools was 0 before, ao isn't
		 * currently on the usable_arenas list.
		 */
		ao->nextarena = usable_arenas;
		ao->prevarena = NULL;
		if (usable_arenas)
			usable_arenas->prevarena = ao;
		usable_arenas = ao;
		assert(usable_arenas->address != 0);
		goto success;
	}
	/* If this arena is now out of order, we need to keep
	 * the list sorted.  The list is kept sorted so that
	 * the "most full" arenas are used first, which allows
	 * the nearly empty arenas to be completely freed.  In
	 * a few un-scientific tests, it seems like this
	 * approach allowed a lot more memory to be freed.
	 */
	else if (ao->nextarena == NULL || nf <= ao->nextarena->nfreepools) {
		/* Case 4.  Nothing to do. */
		goto success;
	}
	/* Case 3:  We have to move the arena towards the end
	 * of the list, because it has more free pools than
	 * the arena to its right.
	 * First unlink ao from usable_arenas.
	 */
	else if (ao->prevarena != NULL) {
		/* ao isn't at the head of the list */
		assert(ao->prevarena->nextarena == ao);
		ao->prevarena->nextarena = ao->nextarena;
	} else {
		/* ao is at the head of the list */
		assert(usable_arenas == ao);
		usable_arenas = ao->nextarena;
	}
	ao->nextarena->prevarena = ao->prevarena;

	/* Locate the new insertion point by iterating over
	 * the list, using our nextarena pointer.
	 */
	while (ao->nextarena != NULL && nf > ao->nextarena->nfreepools) {
		ao->prevarena = ao->nextarena;
		ao->nextarena = ao->nextarena->nextarena;
	}
	/* Insert ao at this point. */
	assert(ao->nextarena == NULL || ao->prevarena == ao->nextarena->prevarena);
	assert(ao->prevarena->nextarena == ao->nextarena);

	ao->prevarena->nextarena = ao;
	if (ao->nextarena != NULL) {
		ao->nextarena->prevarena = ao;
	}
	/* Verify that the swaps worked. */
	assert(ao->nextarena == NULL || nf <= ao->nextarena->nfreepools);
	assert(ao->prevarena == NULL || nf > ao->prevarena->nfreepools);
	assert(ao->nextarena == NULL || ao->nextarena->prevarena == ao);
	assert(
			(usable_arenas == ao && ao->prevarena == NULL) || ao->prevarena->nextarena == ao);
	success:
//	UNLOCK();
	return 1;
}
int mem_realloc(/*State *S,*/void **ptr, size_t nbytes) {
	void *bp;
	poolp pool;
	size_t size;
	assert(*ptr);
	pool = POOL_ADDR(*ptr);
#if POOL_CHECK
	if (!address_in_range(*ptr, pool)) {
		/* pymalloc is not managing this block.

		 If nbytes <= SMALL_REQUEST_THRESHOLD, it's tempting to try to take
		 over this block.  However, if we do, we need to copy the valid data
		 from the C-managed block to one of our blocks, and there's no
		 portable way to know how much of the memory space starting at p is
		 valid.

		 As bug 1185883 pointed out the hard way, it's possible that the
		 C-managed block is "at the end" of allocated VM space, so that a
		 memory fault can occur if we try to copy nbytes bytes starting at p.
		 Instead we punt: let C continue to manage this block. */
		return 0;
	}
#endif
	/* pymalloc is in charge of this block */
	size = INDEX2SIZE(pool->szidx);
	if (nbytes <= size) {
		/* The block is staying the same or shrinking.

		 If it's shrinking, there's a tradeoff: it costs cycles to copy the
		 block to a smaller size class, but it wastes memory not to copy it.

		 The compromise here is to copy on shrink only if at least 25% of
		 size can be shaved off. */
		if (4 * nbytes > 3 * size) {
			/* It's the same, or shrinking and new/old > 3/4. */
//			*newptr_p = ptr;
			return 1;
		}
//		size = nbytes;
	}
	mem_malloc(/*S,*/&bp, nbytes);
	if (bp != NULL) {
		memcpy(bp, *ptr, nbytes);
		mem_free(*ptr);
	}
	*ptr = bp;
	return 1;
}
