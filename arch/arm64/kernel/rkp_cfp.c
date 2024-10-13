/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors: 	James Gleeson <jagleeso@gmail.com>
 *		Wenbo Shen <wenbo.s@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kthread.h>
#include <linux/rkp_cfp.h>
#include <linux/random.h>

/*#if defined(CONFIG_BPF) && defined(CONFIG_RKP_CFP_JOPP) */
/*#error "Cannot enable CONFIG_BPF when CONFIG_RKP_CFP_JOPP is on x(see CONFIG_RKP_CFP_JOPP for details))"*/
/*#endif*/


#ifdef CONFIG_RKP_CFP_ROPP_SYSREGKEY
/*
 * use fixed key for debugging purpose
 * Hypervisor will generate and load the key
*/
unsigned long ropp_master_key = 0x55555555;

#ifdef SYSREG_DEBUG
unsigned long ropp_thread_key = 0x33333333;
#endif

#endif

void ropp_change_key(struct task_struct *p)
{
	unsigned long new_key = 0x0;
#ifdef CONFIG_RKP_CFP_ROPP_HYPKEY
#elif defined (CONFIG_RKP_CFP_ROPP_SYSREGKEY)
	unsigned long enc_key=0x0, mask = 0x0;

	new_key = get_random_long();
	asm volatile(
			"mrs %2, DAIF\n\t"
			"msr DAIFset, #0x3\n\t"
			"mrs %1, "STR(RRMK)"\n\t"
			"eor %1, %0, %1\n\t"
			"mov %0, #0x0\n\t"
			"msr DAIF, %2"
			: "=r" (new_key), "=r" (enc_key), "=r" (mask));

	task_thread_info(p)->rrk = enc_key;

#ifdef SYSREG_DEBUG
	task_thread_info(p)->rrk = ropp_thread_key ^ ropp_master_key;
#endif

#elif defined (CONFIG_RKP_CFP_ROPP_RANDKEY)
	asm volatile("mrs %0, cntpct_el0" : "=r" (new_key));
	task_thread_info(p)->rrk = new_key;
#elif defined (CONFIG_RKP_CFP_ROPP_FIXKEY)
	task_thread_info(p)->rrk = 0x33333333;
#elif defined (CONFIG_RKP_CFP_ROPP_ZEROKEY)
	new_key = 0x0;
	task_thread_info(p)->rrk = new_key;
#else
	#error "Please choose one ROPP key scheme"
#endif
}

/*
 * should not leak mk or rrk to memory
 */
unsigned long ropp_enable_backtrace(unsigned long where, struct task_struct *tsk)
{

#ifdef CONFIG_RKP_CFP_ROPP_SYSREGKEY
	register unsigned long mask asm("x2") =0x0;
	register unsigned long tmp asm("x3") =0x0;
	register unsigned long rrk asm("x4") =0x0;
	register unsigned long enc asm("x5") = task_thread_info(tsk)->rrk;

	asm volatile ( 
		"mrs %0, DAIF\n\t"
		"msr DAIFset, #0x3\n\t"
		"mrs %1, "STR(RRMK)"\n\t"
		"eor %2, %3, %1\n\t"
		"eor %2, %4, %2\n\t"
		"mov %1, #0x0\n\t"
		"msr DAIF, %0"
		: "=r" (mask), "=r" (tmp), "=r" (rrk)
		: "r" (enc), "r" (where));

	return rrk;
#else //CONFIG_RKP_CFP_ROPP_HYPKEY
	return where ^ (task_thread_info(tsk)->rrk);
#endif //CONFIG_RKP_CFP_ROPP_HYPKEY
}
