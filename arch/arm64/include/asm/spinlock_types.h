/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_SPINLOCK_TYPES_H
#define __ASM_SPINLOCK_TYPES_H

#if !defined(__LINUX_SPINLOCK_TYPES_H) && !defined(__ASM_SPINLOCK_H)
# error "please don't include this file directly"
#endif

#include <linux/types.h>

#define TICKET_SHIFT	16


//为了解决多个cpu一起去抢spinlock,那么谁先抢到呢?
//在linux-2.6.24之前的内核中,就是斗狠.比如cpu0已经持有了spin_lock，在其释放之前,cpu1--cpu7都来抢.在cpu0释放spinlock的瞬间,cpu1--cpu7到底谁先
//抢到,这个就不知道了，那么这就会出现饥饿的现象.

//所以,为了解决这个问题,2.6.25之后,spinlock采用了银行的叫号机制,就是来银行,先取一个号,银行柜台每服务完一个人，就报一个新的号.
//如果报的号等于自己持有的票,则取得柜台服务.


//这里的ownwer类似于柜台语音报的号,next是取票的号.
//这里的逻辑是: 谁取票先把spinlock的next暂存到本地local_next,然后把spinlock的next+1.
//所以后来的人取到的票号肯定更大;
//谁释放锁就把owner加1，如果spinlock释放后,owner正好等于某个cpu本地暂存的local_next，则这个cpu获得spinlock
typedef struct {
#ifdef __AARCH64EB__
	u16 next;
	u16 owner;
#else
	u16 owner;
	u16 next;
#endif
} __aligned(4) arch_spinlock_t;

#define __ARCH_SPIN_LOCK_UNLOCKED	{ 0 , 0 }

typedef struct {
	volatile unsigned int lock;
} arch_rwlock_t;

#define __ARCH_RW_LOCK_UNLOCKED		{ 0 }

#endif
