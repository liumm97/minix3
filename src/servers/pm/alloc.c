/* This file is concerned with allocating and freeing arbitrary-size blocks of
 * physical memory on behalf of the FORK and EXEC system calls.  The key data
 * structure used is the hole table, which maintains a list of holes in memory.
 * It is kept sorted in order of increasing memory address. The addresses
 * it contains refers to physical memory, starting at absolute address 0
 * (i.e., they are not relative to the start of PM).  During system
 * initialization, that part of memory containing the interrupt vectors,
 * kernel, and PM are "allocated" to mark them as not available and to
 * remove them from the hole list.
 *
 * The entry points into this file are:
 *   alloc_mem:	allocate a given sized chunk of memory
 *   free_mem:	release a previously allocated chunk of memory
 *   mem_init:	initialize the tables when PM start up
 *   max_hole:	returns the largest hole currently available
 */

#include "pm.h"
#include <minix/com.h>
#include <minix/callnr.h>
#include <signal.h>
#include <stdlib.h>
#include "mproc.h"
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"

#define NR_HOLES  (2*NR_PROCS)	/* max # entries in hole table */
#define NIL_HOLE (struct hole *) 0

PRIVATE struct hole {
  struct hole *h_next;		/* pointer to next entry on the list */
  phys_clicks h_base;		/* where does the hole begin? */
  phys_clicks h_len;		/* how big is the hole? */
} hole[NR_HOLES];

PRIVATE struct hole *hole_head;	/* pointer to first hole */
PRIVATE struct hole *free_slots;/* ptr to list of unused table slots */
#define swap_base ((phys_clicks) -1)

FORWARD _PROTOTYPE( void del_slot, (struct hole *prev_ptr, struct hole *hp) );
FORWARD _PROTOTYPE( void merge, (struct hole *hp)			    );
#define swap_out()	(0)

/*===========================================================================*
 *				alloc_mem				     *
 *===========================================================================*/
PUBLIC phys_clicks alloc_mem(clicks)
phys_clicks clicks;		/* amount of memory requested */
{
/* Allocate a block of memory from the free list using first fit. The block
 * consists of a sequence of contiguous bytes, whose length in clicks is
 * given by 'clicks'.  A pointer to the block is returned.  The block is
 * always on a click boundary.  This procedure is called when memory is
 * needed for FORK or EXEC.  Swap other processes out if needed.
 */
  register struct hole *hp, *prev_ptr;
  phys_clicks old_base;

  do {
        prev_ptr = NIL_HOLE;
	hp = hole_head;
	while (hp != NIL_HOLE && hp->h_base < swap_base) {
		if (hp->h_len >= clicks) {
			/* We found a hole that is big enough.  Use it. */
			old_base = hp->h_base;	/* remember where it started */
			hp->h_base += clicks;	/* bite a piece off */
			hp->h_len -= clicks;	/* ditto */

			/* Delete the hole if used up completely. */
			if (hp->h_len == 0) del_slot(prev_ptr, hp);

			/* Return the start address of the acquired block. */
			return(old_base);
		}

		prev_ptr = hp;
		hp = hp->h_next;
	}
  } while (swap_out());		/* try to swap some other process out */
  return(NO_MEM);
}

/*===========================================================================*
 *				free_mem				     *
 *===========================================================================*/
// 释放内存信息 进入未使用内存节点链表里 
// 在这过程中 ，看看左右节点能否合并
PUBLIC void free_mem(base, clicks)
phys_clicks base;		/* base address of block to free */
phys_clicks clicks;		/* number of clicks to free */
{
/* Return a block of free memory to the hole list.  The parameters tell where
 * the block starts in physical memory and how big it is.  The block is added
 * to the hole list.  If it is contiguous with an existing hole on either end,
 * it is merged with the hole or holes.
 */
  register struct hole *hp, *new_ptr, *prev_ptr;

  if (clicks == 0) return;
  // 取出一个空slot
  if ( (new_ptr = free_slots) == NIL_HOLE) 
  	panic(__FILE__,"hole table full", NO_NUM);
  new_ptr->h_base = base;
  new_ptr->h_len = clicks;
  free_slots = new_ptr->h_next;
  hp = hole_head;

  /* If this block's address is numerically less than the lowest hole currently
   * available, or if no holes are currently available, put this hole on the
   * front of the hole list.
   */
  // 内存最小 ，插到头部
  if (hp == NIL_HOLE || base <= hp->h_base) {
	/* Block to be freed goes on front of the hole list. */
	new_ptr->h_next = hp;
	hole_head = new_ptr;
	merge(new_ptr);
	return;
  }

  /* Block to be returned does not go on front of hole list. */
  // 找到合适的位置 ，插入节点
  prev_ptr = NIL_HOLE;
  while (hp != NIL_HOLE && base > hp->h_base) {
	prev_ptr = hp;
	hp = hp->h_next;
  }

  /* We found where it goes.  Insert block after 'prev_ptr'. */
  new_ptr->h_next = prev_ptr->h_next;
  prev_ptr->h_next = new_ptr;
  // 从 上一个列表节点检测能否合并 能合并节点不超过三个
  merge(prev_ptr);		/* sequence is 'prev_ptr', 'new_ptr', 'hp' */
}

/*===========================================================================*
 *				del_slot				     *
 *===========================================================================*/
// 从内存空洞列表中删除一个slot
PRIVATE void del_slot(prev_ptr, hp)
/* pointer to hole entry just ahead of 'hp' */
register struct hole *prev_ptr;
/* pointer to hole entry to be removed */
register struct hole *hp;	
{
/* Remove an entry from the hole list.  This procedure is called when a
 * request to allocate memory removes a hole in its entirety, thus reducing
 * the numbers of holes in memory, and requiring the elimination of one
 * entry in the hole list.
 */
  if (hp == hole_head)
	hole_head = hp->h_next;
  else
	prev_ptr->h_next = hp->h_next;

  hp->h_next = free_slots;
  free_slots = hp;
}

/*===========================================================================*
 *				merge					     *
 *===========================================================================*/
// 开一看空洞内存块能不能合并
// 因为 hp 是 插入节点的上一个节点 ,所以一次检测下两个节点
PRIVATE void merge(hp)
register struct hole *hp;	/* ptr to hole to merge with its successors */
{
/* Check for contiguous holes and merge any found.  Contiguous holes can occur
 * when a block of memory is freed, and it happens to abut another hole on
 * either or both ends.  The pointer 'hp' points to the first of a series of
 * three holes that can potentially all be merged together.
 */
  register struct hole *next_ptr;

  /* If 'hp' points to the last hole, no merging is possible.  If it does not,
   * try to absorb its successor into it and free the successor's table entry.
   */
  if ( (next_ptr = hp->h_next) == NIL_HOLE) return;
  if (hp->h_base + hp->h_len == next_ptr->h_base) {
	hp->h_len += next_ptr->h_len;	/* first one gets second one's mem */
	del_slot(hp, next_ptr);
  } else {
	hp = next_ptr;
  }

  /* If 'hp' now points to the last hole, return; otherwise, try to absorb its
   * successor into it.
   */
  if ( (next_ptr = hp->h_next) == NIL_HOLE) return;
  if (hp->h_base + hp->h_len == next_ptr->h_base) {
	hp->h_len += next_ptr->h_len;
	del_slot(hp, next_ptr);
  }
}

/*===========================================================================*
 *				mem_init				     *
 *===========================================================================*/
PUBLIC void mem_init(chunks, free)
struct memory *chunks;		/* list of free memory chunks */
phys_clicks *free;		/* memory size summaries */
{
/* Initialize hole lists.  There are two lists: 'hole_head' points to a linked
 * list of all the holes (unused memory) in the system; 'free_slots' points to
 * a linked list of table entries that are not in use.  Initially, the former
 * list has one entry for each chunk of physical memory, and the second
 * list links together the remaining table slots.  As memory becomes more
 * fragmented in the course of time (i.e., the initial big holes break up into
 * smaller holes), new table slots are needed to represent them.  These slots
 * are taken from the list headed by 'free_slots'.
 */
  int i;
  register struct hole *hp;

  /* Put all holes on the free list. */
  // 设计思想 
  // 假装chunks 里的内存 都是已使用的 
  // 然后一次一次释放一个chunks ,形成未使用内存链表
  // 链表里的节点都是编译时创建的 不是动态创建的 所以需要free_slots 指向未使用的slot
  for (hp = &hole[0]; hp < &hole[NR_HOLES]; hp++) hp->h_next = hp + 1;
  hole[NR_HOLES-1].h_next = NIL_HOLE;
  // 指向空闲内存列表
  hole_head = NIL_HOLE;
  // 指向未使用sotts 列表 ，回收内存时直接取 防止创建元素
  free_slots = &hole[0];

  /* Use the chunks of physical memory to allocate holes. */
  *free = 0;
  for (i=NR_MEMS-1; i>=0; i--) {
  	if (chunks[i].size > 0) {
		free_mem(chunks[i].base, chunks[i].size);
		*free += chunks[i].size;
	}
  }
}
