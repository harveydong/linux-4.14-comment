/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * NUMA memory policies for Linux.
 * Copyright 2003,2004 Andi Kleen SuSE Labs
 */
#ifndef _UAPI_LINUX_MEMPOLICY_H
#define _UAPI_LINUX_MEMPOLICY_H

#include <linux/errno.h>


/*
 * Both the MPOL_* mempolicy mode and the MPOL_F_* optional mode flags are
 * passed by the user to either set_mempolicy() or mbind() in an 'int' actual.
 * The MPOL_MODE_FLAGS macro determines the legal set of optional mode flags.
 */

/* Policies */
//系统支持的内存策略
enum {
	MPOL_DEFAULT,//默认策略，也就是应该从当前节点分配内存.如果当前节点没有空闲内存时，那从最近有空闲内存的节点分配.
	MPOL_PREFERRED, //从指定节点上分配内存。若该节点上没有空闲内存，则其他任何一个节点都可以.
	MPOL_BIND, //内存分配指定在特定的节点集(即某几个节点)中.这些节点不能提供所需要的内存时，内存分配就会失败.
	MPOL_INTERLEAVE,//内存分配要覆盖所有节点,该策略通常用于共享内存区域.分配的内存覆盖所有区域来保证不会有节点过载。
	MPOL_LOCAL,
	MPOL_MAX,	/* always last member of enum */
};

/* Flags for set_mempolicy */
#define MPOL_F_STATIC_NODES	(1 << 15)
#define MPOL_F_RELATIVE_NODES	(1 << 14)

/*
 * MPOL_MODE_FLAGS is the union of all possible optional mode flags passed to
 * either set_mempolicy() or mbind().
 */
#define MPOL_MODE_FLAGS	(MPOL_F_STATIC_NODES | MPOL_F_RELATIVE_NODES)

/* Flags for get_mempolicy */
#define MPOL_F_NODE	(1<<0)	/* return next IL mode instead of node mask */
#define MPOL_F_ADDR	(1<<1)	/* look up vma using address */
#define MPOL_F_MEMS_ALLOWED (1<<2) /* return allowed memories */

/* Flags for mbind */
#define MPOL_MF_STRICT	(1<<0)	/* Verify existing pages in the mapping */
#define MPOL_MF_MOVE	 (1<<1)	/* Move pages owned by this process to conform
				   to policy */
#define MPOL_MF_MOVE_ALL (1<<2)	/* Move every page to conform to policy */
#define MPOL_MF_LAZY	 (1<<3)	/* Modifies '_MOVE:  lazy migrate on fault */
#define MPOL_MF_INTERNAL (1<<4)	/* Internal flags start here */

#define MPOL_MF_VALID	(MPOL_MF_STRICT   | 	\
			 MPOL_MF_MOVE     | 	\
			 MPOL_MF_MOVE_ALL)

/*
 * Internal flags that share the struct mempolicy flags word with
 * "mode flags".  These flags are allocated from bit 0 up, as they
 * are never OR'ed into the mode in mempolicy API arguments.
 */
#define MPOL_F_SHARED  (1 << 0)	/* identify shared policies */
#define MPOL_F_LOCAL   (1 << 1)	/* preferred local allocation */
#define MPOL_F_MOF	(1 << 3) /* this policy wants migrate on fault */
#define MPOL_F_MORON	(1 << 4) /* Migrate On protnone Reference On Node */


#endif /* _UAPI_LINUX_MEMPOLICY_H */
