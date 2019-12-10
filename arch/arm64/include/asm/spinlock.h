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
#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <asm/lse.h>
#include <asm/spinlock_types.h>
#include <asm/processor.h>

/*
 * Spinlock implementation.
 *
 * The memory barriers are implicit with the load-acquire and store-release
 * instructions.
 */

#define arch_spin_lock_flags(lock, flags) arch_spin_lock(lock)
/********************************************/
/*spinlock的成员如下:高位是next号,owner是叫号.
 |next  | owner
*/
/********************************************/
static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned int tmp;
	arch_spinlock_t lockval, newval;

	asm volatile(
	/* Atomically increment the next ticket. */
	ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
"	prfm	pstl1strm, %3\n" //将lock变量读取到cache,增加访问速度.%3=*lock;
"1:	ldaxr	%w0, %3\n" //,w0=localval,将lock的值赋值给lockval; 取得next当前的值，这个就是local票号.保存到localval
"	add	%w1, %w0, %w5\n"//把localval中的next加1，w5=1<<16;然后赋值给newval;
"	stxr	%w2, %w1, %3\n"//把新的newval赋值给lock->lock;同时将是否设置成功结果存放到tmp中
"	cbnz	%w2, 1b\n",//如果tmp不为0，则跳到标号1执行.
	/* LSE atomics */
"	mov	%w2, %w5\n" //w5=1<<16; w2= tmp;
"	ldadda	%w2, %w0, %3\n" //
	__nops(3)
	)

	/* Did we get the lock? */
"	eor	%w1, %w0, %w0, ror #16\n" //判断next是否等于owner
"	cbz	%w1, 3f\n" //进入临界区
	/*
	 * No: spin on the owner. Send a local event to avoid missing an
	 * unlock before the exclusive load.
	 */
"	sevl\n"
"2:	wfe\n" //自选等待
"	ldaxrh	%w2, %4\n" //tmp=lock->owner,获取当前owner值，存放在tmp中.
"	eor	%w1, %w2, %w0, lsr #16\n"//判断next是否等于owner.
"	cbnz	%w1, 2b\n" //如果不等于，则跳到标号2自选,否者进入临界区
	/* We got the lock. Critical section starts here. */
"3:"
	: "=&r" (lockval), "=&r" (newval), "=&r" (tmp), "+Q" (*lock)
	: "Q" (lock->owner), "I" (1 << TICKET_SHIFT)
	: "memory");
}

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned int tmp;
	arch_spinlock_t lockval;

	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"	prfm	pstl1strm, %2\n"
	"1:	ldaxr	%w0, %2\n"
	"	eor	%w1, %w0, %w0, ror #16\n"
	"	cbnz	%w1, 2f\n"
	"	add	%w0, %w0, %3\n"
	"	stxr	%w1, %w0, %2\n"
	"	cbnz	%w1, 1b\n"
	"2:",
	/* LSE atomics */
	"	ldr	%w0, %2\n"
	"	eor	%w1, %w0, %w0, ror #16\n"
	"	cbnz	%w1, 1f\n"
	"	add	%w1, %w0, %3\n"
	"	casa	%w0, %w1, %2\n"
	"	sub	%w1, %w1, %3\n"
	"	eor	%w1, %w1, %w0\n"
	"1:")
	: "=&r" (lockval), "=&r" (tmp), "+Q" (*lock)
	: "I" (1 << TICKET_SHIFT)
	: "memory");

	return !tmp;
}

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	unsigned long tmp;

	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"	ldrh	%w1, %0\n"
	"	add	%w1, %w1, #1\n"
	"	stlrh	%w1, %0",
	/* LSE atomics */
	"	mov	%w1, #1\n"
	"	staddlh	%w1, %0\n"
	__nops(1))
	: "=Q" (lock->owner), "=&r" (tmp)
	:
	: "memory");
}

static inline int arch_spin_value_unlocked(arch_spinlock_t lock)
{
	return lock.owner == lock.next;
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	/*
	 * Ensure prior spin_lock operations to other locks have completed
	 * on this CPU before we test whether "lock" is locked.
	 */
	smp_mb(); /* ^^^ */
	return !arch_spin_value_unlocked(READ_ONCE(*lock));
}

static inline int arch_spin_is_contended(arch_spinlock_t *lock)
{
	arch_spinlock_t lockval = READ_ONCE(*lock);
	return (lockval.next - lockval.owner) > 1;
}
#define arch_spin_is_contended	arch_spin_is_contended

/*
 * Write lock implementation.
 *
 * Write locks set bit 31. Unlocking, is done by writing 0 since the lock is
 * exclusively held.
 *
 * The memory barriers are implicit with the load-acquire and store-release
 * instructions.
 */

static inline void arch_write_lock(arch_rwlock_t *rw)
{
	unsigned int tmp;

	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"	sevl\n"
	"1:	wfe\n"
	"2:	ldaxr	%w0, %1\n"
	"	cbnz	%w0, 1b\n"
	"	stxr	%w0, %w2, %1\n"
	"	cbnz	%w0, 2b\n"
	__nops(1),
	/* LSE atomics */
	"1:	mov	%w0, wzr\n"
	"2:	casa	%w0, %w2, %1\n"
	"	cbz	%w0, 3f\n"
	"	ldxr	%w0, %1\n"
	"	cbz	%w0, 2b\n"
	"	wfe\n"
	"	b	1b\n"
	"3:")
	: "=&r" (tmp), "+Q" (rw->lock)
	: "r" (0x80000000)
	: "memory");
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	unsigned int tmp;

	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"1:	ldaxr	%w0, %1\n"
	"	cbnz	%w0, 2f\n"
	"	stxr	%w0, %w2, %1\n"
	"	cbnz	%w0, 1b\n"
	"2:",
	/* LSE atomics */
	"	mov	%w0, wzr\n"
	"	casa	%w0, %w2, %1\n"
	__nops(2))
	: "=&r" (tmp), "+Q" (rw->lock)
	: "r" (0x80000000)
	: "memory");

	return !tmp;
}

static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	asm volatile(ARM64_LSE_ATOMIC_INSN(
	"	stlr	wzr, %0",
	"	swpl	wzr, wzr, %0")
	: "=Q" (rw->lock) :: "memory");
}

/* write_can_lock - would write_trylock() succeed? */
#define arch_write_can_lock(x)		((x)->lock == 0)

/*
 * Read lock implementation.
 *
 * It exclusively loads the lock value, increments it and stores the new value
 * back if positive and the CPU still exclusively owns the location. If the
 * value is negative, the lock is already held.
 *
 * During unlocking there may be multiple active read locks but no write lock.
 *
 * The memory barriers are implicit with the load-acquire and store-release
 * instructions.
 *
 * Note that in UNDEFINED cases, such as unlocking a lock twice, the LL/SC
 * and LSE implementations may exhibit different behaviour (although this
 * will have no effect on lockdep).
 */
static inline void arch_read_lock(arch_rwlock_t *rw)
{
	unsigned int tmp, tmp2;

	asm volatile(
	"	sevl\n"
	ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"1:	wfe\n"
	"2:	ldaxr	%w0, %2\n"
	"	add	%w0, %w0, #1\n"
	"	tbnz	%w0, #31, 1b\n"
	"	stxr	%w1, %w0, %2\n"
	"	cbnz	%w1, 2b\n"
	__nops(1),
	/* LSE atomics */
	"1:	wfe\n"
	"2:	ldxr	%w0, %2\n"
	"	adds	%w1, %w0, #1\n"
	"	tbnz	%w1, #31, 1b\n"
	"	casa	%w0, %w1, %2\n"
	"	sbc	%w0, %w1, %w0\n"
	"	cbnz	%w0, 2b")
	: "=&r" (tmp), "=&r" (tmp2), "+Q" (rw->lock)
	:
	: "cc", "memory");
}

static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	unsigned int tmp, tmp2;

	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"1:	ldxr	%w0, %2\n"
	"	sub	%w0, %w0, #1\n"
	"	stlxr	%w1, %w0, %2\n"
	"	cbnz	%w1, 1b",
	/* LSE atomics */
	"	movn	%w0, #0\n"
	"	staddl	%w0, %2\n"
	__nops(2))
	: "=&r" (tmp), "=&r" (tmp2), "+Q" (rw->lock)
	:
	: "memory");
}

static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	unsigned int tmp, tmp2;

	asm volatile(ARM64_LSE_ATOMIC_INSN(
	/* LL/SC */
	"	mov	%w1, #1\n"
	"1:	ldaxr	%w0, %2\n"
	"	add	%w0, %w0, #1\n"
	"	tbnz	%w0, #31, 2f\n"
	"	stxr	%w1, %w0, %2\n"
	"	cbnz	%w1, 1b\n"
	"2:",
	/* LSE atomics */
	"	ldr	%w0, %2\n"
	"	adds	%w1, %w0, #1\n"
	"	tbnz	%w1, #31, 1f\n"
	"	casa	%w0, %w1, %2\n"
	"	sbc	%w1, %w1, %w0\n"
	__nops(1)
	"1:")
	: "=&r" (tmp), "=&r" (tmp2), "+Q" (rw->lock)
	:
	: "cc", "memory");

	return !tmp2;
}

/* read_can_lock - would read_trylock() succeed? */
#define arch_read_can_lock(x)		((x)->lock < 0x80000000)

#define arch_read_lock_flags(lock, flags) arch_read_lock(lock)
#define arch_write_lock_flags(lock, flags) arch_write_lock(lock)

#define arch_spin_relax(lock)	cpu_relax()
#define arch_read_relax(lock)	cpu_relax()
#define arch_write_relax(lock)	cpu_relax()

/* See include/linux/spinlock.h */
#define smp_mb__after_spinlock()	smp_mb()

#endif /* __ASM_SPINLOCK_H */
