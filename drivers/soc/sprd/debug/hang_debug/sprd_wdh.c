/*copyright (C) 2020 unisoc Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* it's hardware watchdog interrupt handler code for Phoenix II */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/smpboot.h>
#include <linux/kernel.h>
#include <linux/stacktrace.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <asm/stacktrace.h>
#include <asm/io.h>
#include <asm/traps.h>
#include <linux/sprd_sip_svc.h>
#ifdef CONFIG_ARM
#include <asm/cacheflush.h>
#else
#include "../sysdump/sysdump64.h"
#endif

#if defined(CONFIG_SEC_DEBUG)
#include <linux/sec_debug.h>
#endif

#undef pr_fmt
#define pr_fmt(fmt) "sprd_wdh: " fmt

#define MAX_CALLBACK_LEVEL  16
#define SPRD_PRINT_BUF_LEN  524288
#define SPRD_STACK_SIZE 256

enum hand_dump_phase{
	SPRD_HANG_DUMP_ENTER = 1,
	SPRD_HANG_DUMP_CPU_REGS,
	SPRD_HANG_DUMP_STACK_DATA,
	SPRD_HANG_DUMP_CALL_STACK,
	SPRD_HANG_DUMP_GICC_REGS,
	SPRD_HANG_DUMP_SYSDUMP,
	SPRD_HANG_DUMP_GICD_REGS,
	SPRD_HANG_DUMP_END,
};

static int wdh_step[NR_CPUS];
static unsigned char wdh_cpu_state[NR_CPUS];
static raw_spinlock_t sprd_wdh_prlock;
static raw_spinlock_t sprd_wdh_wclock;
static atomic_t sprd_enter_wdh;
extern void sysdump_ipi(struct pt_regs *regs);
extern unsigned long gic_get_gicd_base(void);
extern unsigned int cpu_feed_mask;
extern unsigned int cpu_feed_bitmap;

char sprd_log_buf[SPRD_PRINT_BUF_LEN];
static int log_buf_pos;
static int log_length;
static struct gicd_data *gicd_regs;
static struct gicc_data *gicc_regs;

struct gicc_data{
	u32 gicc_ctlr;	//nonsecure
	u32 gicc_pmr;	//nonsecure
	u32 gicc_bpr;	//nonsecure
	u32 gicc_rpr;	//nonsecure
	u32 gicc_hppir; //nonsecure
	u32 gicc_abpr;	//nonsecure
	u32 gicc_aiar;	//nonsecure
	u32 gicc_ahppir;	//nonsecure
	u32 gicc_statusr;	//nonsecure
	u32 gicc_iidr;	//nonsecure
};

struct gicd_data {
	u32 gicd_ctlr;     //nonsecure
	u32 gicd_typer;    //nonsecure
	u32 gicd_statusr;	//nonsecure
	u32 gicd_igroup[7];	//secure
	u32 gicd_isenabler[7];	//nonsecure
	u32 gicd_ispendr[7];	//nonsecure
	u32 gicd_igrpmodr[7];	//secure
};

enum smcc_regs_id{
	CPU_STATUS		= 0x00000000,
	CPU_PC,
	CPU_CPSR,
	CPU_SP_EL0,
	CPU_SP_EL1,
	GICD_IGROUP_0		= 0x00010000,
	GICD_IGROUP_1,
	GICD_IGROUP_2,
	GICD_IGROUP_3,
	GICD_IGROUP_4,
	GICD_IGROUP_5,
	GICD_IGROUP_6,
	GICD_IGRPMODR_0,
	GICD_IGRPMODR_1,
	GICD_IGRPMODR_2,
	GICD_IGRPMODR_3,
	GICD_IGRPMODR_4,
	GICD_IGRPMODR_5,
	GICD_IGRPMODR_6,
};

struct pt_regs cpu_context[NR_CPUS];
static struct sprd_sip_svc_handle *svc_handle;
#ifdef CONFIG_SPRD_HANG_DEBUG_UART
extern void sprd_hangd_console_write(const char *s, unsigned int count);
#endif

#ifdef CONFIG_ARM64
#define sprd_virt_addr_valid(kaddr) ((((void *)(kaddr) >= (void *)PAGE_OFFSET && \
					(void *)(kaddr) < (void *)high_memory) || \
					((void *)(kaddr) >= (void *)KIMAGE_VADDR && \
					(void *)(kaddr) < (void *)_end)) && \
					pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#else
#define sprd_virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET && \
					(void *)(kaddr) < (void *)high_memory && \
					pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#endif

static void sprd_write_print_buf(char *source, int size)
{
	if (log_buf_pos + size <= SPRD_PRINT_BUF_LEN) {
		memcpy(sprd_log_buf + log_buf_pos, source, size);
		log_buf_pos += size;
	} else {
		memcpy(sprd_log_buf + log_buf_pos, source, (SPRD_PRINT_BUF_LEN - log_buf_pos));
		log_buf_pos = log_buf_pos + size - SPRD_PRINT_BUF_LEN;
		memcpy(sprd_log_buf, source, log_buf_pos);
	}

#if defined(CONFIG_SEC_DEBUG)
	sec_log_buf_write(source, size);
#endif
#ifdef CONFIG_SPRD_HANG_DEBUG_UART
	sprd_hangd_console_write(source, size);
#endif
	log_length += size;
}

static char wdh_tmp_buf[256];
static int g_first = 0;
void sprd_hang_debug_printf(const char *fmt, ...)
{
	int cpu = smp_processor_id();
	u64 boottime_us = ktime_get_boot_fast_ns();
	int size;
	u64 sec, usec;

	va_list args;

//#ifdef ZP_DEBUG
    if(g_first == 0){
		g_first = 1;

		raw_spin_lock(&sprd_wdh_prlock);

		do_div(boottime_us, NSEC_PER_USEC);

		sec = boottime_us;
		usec = do_div(sec, USEC_PER_SEC);

		/* add timestamp header */
		strcpy(wdh_tmp_buf, "[H ");
		sprintf(wdh_tmp_buf + strlen(wdh_tmp_buf), "%d", (int)sec);
		strcat(wdh_tmp_buf + strlen(wdh_tmp_buf), ".");
		sprintf(wdh_tmp_buf + strlen(wdh_tmp_buf), "%06d", (int)usec);
		strcat(wdh_tmp_buf + strlen(wdh_tmp_buf), "] c:");
	} else {
		strcpy(wdh_tmp_buf, " c");
	}
//#endif
	/* add cpu num header */
	//strcpy(wdh_tmp_buf, "c");
	sprintf(wdh_tmp_buf + strlen(wdh_tmp_buf), "%d:", cpu);
	strcat(wdh_tmp_buf + strlen(wdh_tmp_buf), " ");

	size = strlen(wdh_tmp_buf);

	va_start(args, fmt);
	size += vsprintf(wdh_tmp_buf + size, fmt, args);
	va_end(args);

	sprd_write_print_buf(wdh_tmp_buf, size);

	raw_spin_unlock(&sprd_wdh_prlock);
}


static int set_panic_debug_entry(unsigned long func_addr, unsigned long pgd)
{
#ifdef CONFIG_SPRD_SIP_FW
	int ret;
	ret = svc_handle->dbg_ops.set_hang_hdl(func_addr, pgd);
	if (ret) {
		pr_err("failed to set panic debug entry\n");
		return -EPERM;
	}
#endif
	pr_emerg("func_addr = 0x%lx, pgd = 0x%lx\n", func_addr, pgd);
	return 0;
}

__weak void prepare_dump_info_for_wdh(struct pt_regs *regs, const char *reason)
{
	sprd_hang_debug_printf("Not defined func %s, for %p, %s\n", __func__, regs, reason);
}

static unsigned long smcc_get_regs(enum smcc_regs_id id)
{
	uintptr_t val = 0;
#ifdef CONFIG_SPRD_SIP_FW
	int ret;

	ret = svc_handle->dbg_ops.get_hang_ctx(id, &val);
	if (ret) {
		sprd_hang_debug_printf("%s: failed to get cpu statue\n", __func__);
		return EPERM;
	}
#endif
	return val;
}
static unsigned short smcc_get_cpu_state(void)
{
	uintptr_t val = 0;
#ifdef CONFIG_SPRD_SIP_FW
	int ret;
	ret = svc_handle->dbg_ops.get_hang_ctx(CPU_STATUS, &val);
	if (ret) {
		pr_err("failed to get cpu statue\n");
		return EPERM;
	}
#endif
	return val;
}

static int is_el3_ret_last_cpu(int cpu, unsigned short n)
{
	int rc = 0, tmp = 0, rt = 0;
	int i;

	raw_spin_lock(&sprd_wdh_wclock);
	wdh_cpu_state[cpu] = 1;

	/* is all cpus come back? */
	for (i = 0; i < NR_CPUS; i++) {
		if ((n >> (i * 2)) & 0x2)
			rc++;
		tmp += wdh_cpu_state[i];
	}
	raw_spin_unlock(&sprd_wdh_wclock);

	/* is atf checks cpus finished? */
	while (n != 0) {
		n &= (n-1);
		rt++;
	}

	return (rt >= num_online_cpus() ? 1 : 0) && (rc == tmp) ? 1 : 0;
}

static void print_step(int cpu)
{
	sprd_hang_debug_printf("wdh_step = %d\n", wdh_step[cpu]);
}

static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;
	struct vm_struct *vaddr;

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < KIMAGE_VADDR || addr > -256UL)
		return;

	if (addr > VMALLOC_START && addr < VMALLOC_END) {
		vaddr = find_vm_area_no_wait((const void *)addr);
		if (!vaddr || ((vaddr->flags & VM_IOREMAP) == VM_IOREMAP))
			return;
	}

	sprd_hang_debug_printf("\nRegistor:%s: %#lx:\n", name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;


	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		sprd_hang_debug_printf("%04lx: ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u64	data;
			if (probe_kernel_address(p, data)) {
				sprd_hang_debug_printf(" ********");
			} else {
				sprd_hang_debug_printf(" %08x ", data);
			}
			++p;
		}
		sprd_hang_debug_printf("\n");
	}
}

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	mm_segment_t fs;
	unsigned int i;

	fs = get_fs();
	set_fs(KERNEL_DS);
	show_data(regs->pc - nbytes, nbytes * 2, "PC");
	show_data(regs->regs[30] - nbytes, nbytes * 2, "LR");
	show_data(regs->sp - nbytes, nbytes * 2, "SP");
	for (i = 0; i < 30; i++) {
		char name[4];
		snprintf(name, sizeof(name), "X%u", i);
		show_data(regs->regs[i] - nbytes, nbytes * 2, name);
	}
	set_fs(fs);
}

static void cpu_regs_value_dump(int cpu)
{
	struct pt_regs *pregs = &cpu_context[cpu];

#ifdef CONFIG_ARM64
	int i;
	sprd_hang_debug_printf("pc : %016llx, lr : %016llx, pstate : %016llx, sp_el0 : %016llx, sp_el1 : %016llx\n",
		      pregs->pc, pregs->regs[30], pregs->pstate, smcc_get_regs(CPU_SP_EL0), smcc_get_regs(CPU_SP_EL1));

	if (sprd_virt_addr_valid(pregs->pc))
		sprd_hang_debug_printf("pc :(%ps)\n", (void *)pregs->pc);
	sprd_hang_debug_printf("sp : %016llx\n", pregs->sp);

	for (i = 29; i >= 0; ) {
		sprd_hang_debug_printf("x%-2d: %016llx x%-2d: %016llx\n", i, pregs->regs[i], i - 1,
			pregs->regs[i - 1]);
		i -= 2;
	}
	if (!user_mode(pregs))
	{
	        show_extra_register_data(pregs, 128);
	}
#else
	sprd_hang_debug_printf("pc  : %08lx, lr : %08lx, cpsr : %08lx, sp_usr : %08lx, sp_svc : %08lx\n",
		      pregs->ARM_pc, pregs->ARM_lr, pregs->ARM_cpsr, smcc_get_regs(CPU_SP_EL0), smcc_get_regs(CPU_SP_EL1));
	if (sprd_virt_addr_valid(pregs->ARM_pc))
		sprd_hang_debug_printf("pc :(%ps)\n", (void *)pregs->ARM_pc);
	sprd_hang_debug_printf("sp  : %08lx, ip : %08lx, fp : %08lx\n",
		      pregs->ARM_sp, pregs->ARM_ip, pregs->ARM_fp);
	sprd_hang_debug_printf("r10 : %08lx, r9 : %08lx, r8 : %08lx\n",
		      pregs->ARM_r10, pregs->ARM_r9, pregs->ARM_r8);
	sprd_hang_debug_printf("r7  : %08lx, r6 : %08lx, r5 : %08lx\n",
		      pregs->ARM_r7, pregs->ARM_r6, pregs->ARM_r5);
	sprd_hang_debug_printf("r4  : %08lx, r3 : %08lx, r2 : %08lx\n",
		      pregs->ARM_r4, pregs->ARM_r3, pregs->ARM_r2);
	sprd_hang_debug_printf("r1  : %08lx, r0 : %08lx\n",
			  pregs->ARM_r1, pregs->ARM_r0);
#endif

	wdh_step[cpu] = SPRD_HANG_DUMP_CPU_REGS;
	print_step(cpu);
}

static void cpu_stack_data_dump(int cpu)
{
	int i, j;
	unsigned long sp, pc;
	unsigned int *p;
	unsigned int data;
	struct pt_regs *pregs = &cpu_context[cpu];
	mm_segment_t fs;
	char str[sizeof(" 12345678") * 8 + 1];

	/* maybe deadlock here? */
	fs = get_fs();
	set_fs(KERNEL_DS);

	if (wdh_step[cpu] == SPRD_HANG_DUMP_CPU_REGS) {
#ifdef CONFIG_ARM64
		sp = pregs->sp - (SPRD_STACK_SIZE >> 1);
		pc = pregs->pc;
#else
		sp = pregs->ARM_sp;
		pc = pregs->ARM_pc;
#endif
		if (sp & 3) {
			sprd_hang_debug_printf("%s sp unaligned %08lx\n", __func__, sp);
			return;
		}
		if (!sprd_virt_addr_valid(pc)) {
			sprd_hang_debug_printf("%s: It's not in valid kernel space!\n", __func__);
			return;
		}
#ifdef CONFIG_VMAP_STACK
		if (!((sp >= VMALLOC_START) && (sp < VMALLOC_END))) {
			sprd_hang_debug_printf("%s sp out of kernel addr space %08lx\n", __func__, sp);
			return;
		}
		if (!(((sp + SPRD_STACK_SIZE) >= VMALLOC_START) && ((sp + SPRD_STACK_SIZE) < VMALLOC_END))) {
			sprd_hang_debug_printf("%s sp top out of kernel addr space %08lx\n",
				__func__, (sp + SPRD_STACK_SIZE));
			return;
		}
#else
		if (!((sp >= (PAGE_OFFSET + THREAD_SIZE)) && sprd_virt_addr_valid(sp))) {
			sprd_hang_debug_printf("%s sp out of kernel addr space %08lx\n", __func__, sp);
			return;
		}
		if (!(((sp + SPRD_STACK_SIZE) >= (PAGE_OFFSET + THREAD_SIZE)) && sprd_virt_addr_valid(sp))) {
			sprd_hang_debug_printf("%s sp top out of kernel addr space %08lx\n",
				      __func__, (sp + SPRD_STACK_SIZE));
			return;
		}
#endif
		/* we always set 256 Bytes as stack size here */
		p = (unsigned int *)sp;
		sprd_hang_debug_printf("sp : [%lx ---- %lx]\n",
			sp, (sp + SPRD_STACK_SIZE));

		for (i = 0; i < (SPRD_STACK_SIZE >> 5); i++) {
			memset(str, ' ', sizeof(str));
			str[sizeof(str) - 1] = '\0';

			for (j = 0; j < 8; j++) {
				if (!__get_user(data, p)) {
					sprintf(str + j * 9, " %08x", data);
				} else {
					sprintf(str + j * 9, " ********");
				}
				++p;
			}
			sprd_hang_debug_printf("%04lx:%s\n", (unsigned long)(p - 8) & 0xffff, str);
		}
		flush_cache_all();
		wdh_step[cpu] = SPRD_HANG_DUMP_STACK_DATA;
	} else {
		wdh_step[cpu] = -SPRD_HANG_DUMP_STACK_DATA;
	}

	print_step(cpu);
}

static void get_gicc_regs(struct gicc_data *c)
{
#ifdef CONFIG_ARM64
	c->gicc_ctlr = (u32)read_sysreg_s(SYS_ICC_CTLR_EL1);
	c->gicc_pmr = (u32)read_sysreg_s(SYS_ICC_PMR_EL1);
	c->gicc_bpr = (u32)read_sysreg_s(SYS_ICC_BPR1_EL1);
	c->gicc_rpr = (u32)read_sysreg_s(SYS_ICC_RPR_EL1);
	c->gicc_hppir = (u32)read_sysreg_s(SYS_ICC_HPPIR1_EL1);
#else
	c->gicc_ctlr = (u32)read_sysreg(ICC_CTLR);
	c->gicc_pmr = (u32)read_sysreg(ICC_PMR);
	c->gicc_bpr = (u32)read_sysreg(ICC_BPR1);
	c->gicc_rpr = (u32)read_sysreg(ICC_RPR);
	c->gicc_hppir = (u32)read_sysreg(ICC_HPPIR1);
#endif

#if 0   //memory-mapped, do not find offset of gicc
	c->gicc_abpr = 0;
	c->gicc_aiar = 0;
	c->gicc_ahppir = 0;
	c->gicc_statusr = 0;
	c->gicc_iidr = 0;
#endif
	isb();
}

static int get_gicd_regs(struct gicd_data *d)
{
	int i;
	void __iomem *base = (void __iomem *)gic_get_gicd_base();

	if (base) {
		d->gicd_ctlr = readl_relaxed(base + GICD_CTLR);
		d->gicd_typer = readl_relaxed(base + GICD_TYPER);
		d->gicd_statusr = readl_relaxed(base + GICD_STATUSR);

		for (i = 0; i < 7; i++) {
			d->gicd_isenabler[i] = readl_relaxed(base + GICD_ISENABLER + i * 4);
			d->gicd_ispendr[i] = readl_relaxed(base + GICD_ISPENDR + i * 4);
		}
		return 0;
	}
	return -1;
}

static void gicc_regs_value_dump(int cpu)
{
	if (!gicc_regs) {
		sprd_hang_debug_printf("gicc_regs allocation failed\n");
		return;
	}

	get_gicc_regs(gicc_regs);

	sprd_hang_debug_printf("gicc_ctlr  : %08x, gicc_pmr : %08x, gicc_bpr : %08x\n",
		     gicc_regs->gicc_ctlr, gicc_regs->gicc_pmr, gicc_regs->gicc_bpr);
	sprd_hang_debug_printf("gicc_rpr  : %08x, gicc_hppir0 : %08x, gicc_abpr : %08x\n",
		     gicc_regs->gicc_rpr, gicc_regs->gicc_hppir, gicc_regs->gicc_abpr);
	sprd_hang_debug_printf("gicc_aiar  : %08x, gicc_ahppir : %08x, gicc_statusr : %08x\n",
		     gicc_regs->gicc_aiar, gicc_regs->gicc_ahppir, gicc_regs->gicc_statusr);
	sprd_hang_debug_printf("gicc_iidr  : %08x\n", gicc_regs->gicc_iidr);

	wdh_step[cpu] = SPRD_HANG_DUMP_GICC_REGS;
	print_step(cpu);
}

static void gicd_regs_value_dump(int cpu)
{
	int i, ret;

	if (!gicd_regs) {
		sprd_hang_debug_printf("gicd_regs allocation failed\n");
		return;
	}
	for (i = 0; i < 7; i++) {
		gicd_regs->gicd_igroup[i] = (u32)smcc_get_regs(GICD_IGROUP_0 + i);
		gicd_regs->gicd_igrpmodr[i] = (u32)smcc_get_regs(GICD_IGRPMODR_0 + i);
	}

	ret = get_gicd_regs(gicd_regs);
	if (!ret) {
		sprd_hang_debug_printf("--- show the gicd regs --- \n");
		sprd_hang_debug_printf("gicd_ctlr: %08x, gicd_typer : %08x, gicd_statusr : %08x\n",
			gicd_regs->gicd_ctlr, gicd_regs->gicd_typer, gicd_regs->gicd_statusr);
		sprd_hang_debug_printf("gicd_igroup: %08x, %08x, %08x, %08x, %08x, %08x, %08x\n",
			gicd_regs->gicd_igroup[0], gicd_regs->gicd_igroup[1], gicd_regs->gicd_igroup[2],
			gicd_regs->gicd_igroup[3], gicd_regs->gicd_igroup[4], gicd_regs->gicd_igroup[5],
			gicd_regs->gicd_igroup[6]);

		sprd_hang_debug_printf("gicd_isenabler: %08x, %08x, %08x, %08x, %08x, %08x, %08x\n",
			gicd_regs->gicd_isenabler[0], gicd_regs->gicd_isenabler[1], gicd_regs->gicd_isenabler[2],
			gicd_regs->gicd_isenabler[3], gicd_regs->gicd_isenabler[4], gicd_regs->gicd_isenabler[5],
			gicd_regs->gicd_isenabler[6]);

		sprd_hang_debug_printf("gicd_ispendr: %08x, %08x, %08x, %08x, %08x, %08x, %08x\n",
			gicd_regs->gicd_ispendr[0], gicd_regs->gicd_ispendr[1], gicd_regs->gicd_ispendr[2],
			gicd_regs->gicd_ispendr[3], gicd_regs->gicd_ispendr[4], gicd_regs->gicd_ispendr[5],
			gicd_regs->gicd_ispendr[6]);

		sprd_hang_debug_printf("gicd_igrpmodr: %08x, %08x, %08x, %08x, %08x, %08x, %08x\n",
			gicd_regs->gicd_igrpmodr[0], gicd_regs->gicd_igrpmodr[1], gicd_regs->gicd_igrpmodr[2],
			gicd_regs->gicd_igrpmodr[3], gicd_regs->gicd_igrpmodr[4], gicd_regs->gicd_igrpmodr[5],
			gicd_regs->gicd_igrpmodr[6]);
	} else {
		wdh_step[cpu] = -SPRD_HANG_DUMP_GICD_REGS;
	}

	wdh_step[cpu] = SPRD_HANG_DUMP_GICD_REGS;
	print_step(cpu);
}

static void sprd_unwind_backtrace_dump(int cpu)
{
	int i;
	struct stackframe frame;
	struct pt_regs *pregs = &cpu_context[cpu];
	unsigned long sp;

#ifdef CONFIG_ARM64
	frame.fp = pregs->regs[29]; //x29
	frame.pc = pregs->pc;
	sp = pregs->sp;
#else
	frame.fp = pregs->ARM_fp;
	frame.sp = pregs->ARM_sp;
	frame.lr = pregs->ARM_lr;
	frame.pc = pregs->ARM_pc;
	sp = pregs->ARM_sp;
#endif

#ifdef CONFIG_VMAP_STACK
	if (!((sp >= VMALLOC_START) && (sp < VMALLOC_END))) {
		sprd_hang_debug_printf("%s sp out of kernel addr space %08lx\n", __func__, sp);
		return;
	}
#else
	if (!sprd_virt_addr_valid(sp)) {
		sprd_hang_debug_printf("invalid sp[%lx]\n", sp);
		return;
	}
#endif
	if (!sprd_virt_addr_valid(frame.pc)) {
		sprd_hang_debug_printf("invalid pc\n");
		return;
	}
	sprd_hang_debug_printf("callstack:\n");
	sprd_hang_debug_printf("[<%08lx>] (%ps)\n", frame.pc, (void *)frame.pc);

	for (i = 0; i < MAX_CALLBACK_LEVEL; i++) {
		int urc;
#ifdef CONFIG_ARM64
		urc = unwind_frame(NULL, &frame);
#else
		urc = unwind_frame(&frame);
#endif
		if (urc < 0)
			break;

		if (!sprd_virt_addr_valid(frame.pc)) {
			sprd_hang_debug_printf("i=%d, sprd_virt_addr_valid fail\n", i);
			break;
		}

		sprd_hang_debug_printf("[<%08lx>] (%ps)\n", frame.pc, (void *)frame.pc);
	}

	wdh_step[cpu] = SPRD_HANG_DUMP_CALL_STACK;
	print_step(cpu);
}

asmlinkage __visible void wdh_atf_entry(struct pt_regs *data)
{
	int cpu, cpu_num;
	struct pt_regs *pregs;
	unsigned short cpu_state;
	int is_last_cpu;
	int wait_last_cnt = 5;

	cpu = smp_processor_id();
	wdh_step[cpu] = SPRD_HANG_DUMP_ENTER;
	sprd_hang_debug_printf("---wdh enter!---\n");

	sprd_hang_debug_printf("cpu_feed_mask:0x%04x cpu_feed_bitmap:0x%04x\n",
			       (unsigned int)cpu_feed_mask,
			       (unsigned int)cpu_feed_bitmap);

	oops_in_progress = 1;
	pregs = &cpu_context[cpu];
	*pregs = *data;
#ifdef CONFIG_ARM64
	pregs->pc = (unsigned long)smcc_get_regs(CPU_PC);
	pregs->pstate = (unsigned long)smcc_get_regs(CPU_CPSR);
#else
	pregs->ARM_pc = (unsigned long)smcc_get_regs(CPU_PC);
	pregs->ARM_cpsr = (unsigned long)smcc_get_regs(CPU_CPSR);
#endif

	cpu_state = smcc_get_cpu_state();
	is_last_cpu = is_el3_ret_last_cpu(cpu, cpu_state);

	cpu_regs_value_dump(cpu);
	cpu_stack_data_dump(cpu);
	sprd_unwind_backtrace_dump(cpu);
#if IS_ENABLED(CONFIG_SEC_USER_RESET_DEBUG)
	sec_debug_store_extc_idx(false);
#endif
	gicc_regs_value_dump(cpu);

	sprd_hang_debug_printf("cpu_state = 0x%04x\n", (unsigned int)cpu_state);

	if (atomic_xchg(&sprd_enter_wdh, 1)) {
		sprd_hang_debug_printf("%s: goto panic idle\n", __func__);
		sysdump_ipi(pregs);
		wdh_step[cpu] = SPRD_HANG_DUMP_SYSDUMP;
		flush_cache_all();
		while (1)
			cpu_relax();
	}
	wdh_step[cpu] = SPRD_HANG_DUMP_SYSDUMP;
	gicd_regs_value_dump(cpu);

	/* wait for other cpu finised */
	while (wait_last_cnt-- && !is_last_cpu) {
		mdelay(50);
		cpu_state = smcc_get_cpu_state();
		is_last_cpu = is_el3_ret_last_cpu(cpu, cpu_state);
	}
	sprd_hang_debug_printf("wait last cpu %s\n", is_last_cpu ? "ok" : "failed");

	if (!is_last_cpu)
		sprd_hang_debug_printf("cpu_state = 0x%04x\n", (unsigned int)cpu_state);
	else {
		for_each_online_cpu(cpu_num) {
			wait_last_cnt = 10;
			while (wait_last_cnt--) {
				if (wdh_step[cpu_num] && (wdh_step[cpu_num] < SPRD_HANG_DUMP_SYSDUMP))
					mdelay(50);
				else
					break;
			}
		}
	}

	prepare_dump_info_for_wdh(pregs, "wdt fiq assert");

	wdh_step[cpu] = SPRD_HANG_DUMP_END;
	print_step(cpu);

	sysdump_ipi(pregs);

	mdelay(50);

#if IS_ENABLED(CONFIG_SEC_DEBUG)
	sec_debug_prepare_for_wdog_bark_reset();
#endif

	do_kernel_restart("panic");
}

asmlinkage void el1_entry_for_wdh(void);

static int sprd_wdh_atf_init(void)
{
	int ret;

	raw_spin_lock_init(&sprd_wdh_prlock);
	raw_spin_lock_init(&sprd_wdh_wclock);
	atomic_set(&sprd_enter_wdh, 0);

	svc_handle = sprd_sip_svc_get_handle();
	if (!svc_handle) {
		pr_err("failed to get svc handle\n");
		return -ENODEV;
	}

	ret = set_panic_debug_entry((unsigned long)el1_entry_for_wdh,
#ifdef CONFIG_ARM64
		read_sysreg(ttbr1_el1)
#else
		0
#endif
	);

	if (ret)
		pr_emerg("init ATF el1_entry_for_wdh error[%d]\n", ret);

	gicc_regs = kmalloc(sizeof(struct gicc_data), GFP_KERNEL);
	gicd_regs = kmalloc(sizeof(struct gicd_data), GFP_KERNEL);

	return 0;
}

late_initcall(sprd_wdh_atf_init);
