/*
 * Based on arch/arm/include/asm/tlbbatch.h
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
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
#ifndef __ASM_ARM64_TLBBATCH_H
#define __ASM_ARM64_TLBBATCH_H

#ifdef CONFIG_SMP
/*
 * arch_tlbflush_unmap_batch has cpumask to remember CPU numbers that run a
 * thread with the mm_struct that include this arch_tlbflush_unmap_batch. See
 * arch/x86/include/asm/tlbbatch.h for the referece. But it is not possible
 * in arm64 because cpumask of mm_struct is not updated in arm64. It is because
 * tlb flush against an address or A ASID in arm64 architecturally broadcasts to
 * all other remote cpus. It is also not necessary since posting IPI to all
 * other CPUs is not that expecive with limited number of multi-cores(up to 8
 * cores).
 */
struct arch_tlbflush_unmap_batch { };
#endif

#endif
