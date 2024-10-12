/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt)  "sprd-sysdump: " fmt

#include <linux/atomic.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/highuid.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/sysrq.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/regmap.h>
#ifdef CONFIG_SPRD_SIPC
#include <linux/sipc.h>
#endif
/* sysdump */
#include "sysdump.h"
#include "sysdumpdb.h"
#include "sprd_vmcoreinfo.h"

#include <linux/soc/sprd/sprd_sysdump.h>

#include <linux/kallsyms.h>
#include <asm/stacktrace.h>
#include <asm-generic/kdebug.h>
#include <linux/kdebug.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif

#include <crypto/hash.h>
#include <linux/crypto.h>
#include <crypto/sha.h>
#include <asm/sections.h>

#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif

#define CORE_STR	"CORE"
#ifndef ELF_CORE_EFLAGS
#define ELF_CORE_EFLAGS	0
#endif


#define SYSDUMP_MAGIC	"SPRD_SYSDUMP_119"
#define SYSDUMP_MAGIC_VOLUP  (0x766f7570) // v-l-u-p
#define SYSDUMP_MAGIC_VOLDN  (0X766f646e) // v-l-d-n

#define SYSDUMP_NOTE_BYTES (ALIGN(sizeof(struct elf_note), 4) +   \
			    ALIGN(sizeof(CORE_STR), 4) + \
			    ALIGN(sizeof(struct elf_prstatus), 4))

#define DUMP_REGS_SIZE min(sizeof(elf_gregset_t), sizeof(struct pt_regs))

#define ANA_RST_STATUS_OFFSET_2730 (0x1bac) /* pmic 2730 rst status register offset */
#define HWRST_STATUS_SYSDUMP  (0x200)
#define ANA_RST_STATUS_OFFSET_2721 (0xed8)  /* pmic 2721 rst status register offset */
static unsigned int pmic_reg;

/* minidump common data */
#define MINIDUMP_MAGIC	"SPRD_MINIDUMP"
#define REG_SP_INDEX	31
#define REG_PC_INDEX	32
extern void stext(void);
struct pt_regs pregs_die_g;
struct pt_regs minidump_regs_g;

struct minidump_info  minidump_info_g =	{
	.kernel_magic			=	KERNEL_MAGIC,
	.regs_info			=	{
#ifdef CONFIG_ARM
		.arch			=	ARM,
		.num			=	16,
		.size			=	sizeof(struct pt_regs)
#endif
#ifdef CONFIG_ARM64
		.arch			=	ARM64,
		.num			=	33, /*x0~x30 and SP(x31) ,PC  x30 = lr */
		.size			=	sizeof(struct pt_regs)
#endif
#ifdef CONFIG_X86_64
		.arch			=	X86_64,
		.num			=	32,
		.size			=	sizeof(struct pt_regs)
#endif
	},
	.regs_memory_info		=	{
		.per_reg_memory_size	=	256,
		.valid_reg_num	=	0,
	},
	.section_info_total		=	{
		.section_info		=	{
//			{"text", (unsigned long)_stext, (unsigned long)_etext, 0, 0, 0},
			{"data", (unsigned long)_sdata, (unsigned long)_edata, 0, 0, 0},
			{"bss", (unsigned long)__bss_start, (unsigned long)__bss_stop, 0, 0, 0},
			{"init", (unsigned long)__init_begin, (unsigned long)__init_end, 0, 0, 0},
			{"inittext", (unsigned long)_sinittext, (unsigned long)_einittext, 0, 0, 0},
			{"rodata", (unsigned long)__start_rodata, (unsigned long)__end_rodata, 0, 0,
			0},
			{"", 0, 0, 0, 0, 0},

		},
		.total_size	=	0,
		.total_num	=	0,
	},
	.compressed			=	1,
};
static int vaddr_to_paddr_flag;
static struct mutex section_mutex;
static atomic_t mutex_init_flag = ATOMIC_INIT(0);

#ifdef CONFIG_SPRD_MINI_SYSDUMP /*      minidump code start     */
static int  die_notify_flag;
static int prepare_minidump_info(struct pt_regs *regs);
static struct info_desc minidump_info_desc_g;
static int prepare_exception_info(struct pt_regs *regs,
			struct task_struct *tsk, const char *reason);
static char *ylog_buffer;
#ifdef CONFIG_SPRD_NATIVE_HANG_MONITOR
extern void get_native_hang_monitor_buffer(unsigned long *addr, unsigned long *size, unsigned long *start);
#endif
#endif /*	minidump code end	*/
typedef char note_buf_t[SYSDUMP_NOTE_BYTES];

static DEFINE_PER_CPU(note_buf_t, crash_notes_temp);
note_buf_t __percpu *crash_notes;

DEFINE_PER_CPU(struct sprd_debug_mmu_reg_t, sprd_debug_mmu_reg);

/* An ELF note in memory */
struct memelfnote {
	const char *name;
	int type;
	unsigned int datasz;
	void *data;
};
struct kaslr_info {
	uint64_t kaslr_offset;
	uint64_t kimage_voffset;
	uint64_t phys_offset;
	uint64_t vabits_actual;
};
struct minidump_info_record {
	char magic[20];
	uint64_t minidump_info_paddr;
	int minidump_info_size;
};
struct log_buf_info {
	uint64_t log_buf;
	uint32_t log_buf_len;
	uint64_t log_first_idx;
	uint64_t log_next_idx;
	uint64_t vmcoreinfo_size;
};
struct mmu_regs_info {
	uint32_t sprd_pcpu_offset;
	uint32_t cpu_id;
	uint32_t cpu_numbers;
	uint64_t paddr_mmu_regs_t;
};
struct sysdump_info {
	char magic[16];
	char time[32];
	char reason[32];
	char dump_path[128];
	int elfhdr_size;
	int mem_num;
	unsigned long dump_mem_paddr;
	int crash_key;
	struct kaslr_info sprd_kaslrinfo;
	struct minidump_info_record sprd_minidump_info;
	struct log_buf_info sprd_logbuf_info;
	struct mmu_regs_info sprd_mmuregs_info;
};

struct sysdump_extra {
	int enter_id;
	int enter_cpu;
	char reason[256];
	struct pt_regs cpu_context[CONFIG_NR_CPUS];
};

struct sysdump_config {
	int enable;
	int crashkey_only;
	int dump_modem;
	int reboot;
	char dump_path[128];
};

static struct sysdump_info *sprd_sysdump_info;
static unsigned long sprd_sysdump_info_paddr;
static unsigned long sysdump_magic_paddr;
static int sysdump_reflag;

/* global var for memory hash */
static u8 g_ktxt_hash_data[SHA1_DIGEST_SIZE];
static struct shash_desc *desc;

/* must be global to let gdb know */
struct sysdump_extra sprd_sysdump_extra = {
	.enter_id = -1,
	.enter_cpu = -1,
	.reason = {0},
};

static struct sysdump_config sysdump_conf = {
	.enable = 1,
	.crashkey_only = 0,
	.dump_modem = 1,
	.reboot = 1,
	.dump_path = "",
};

static int sprd_sysdump_init;
static int sprd_sysdump_shash_init;

int sysdump_status;
struct regmap *regmap;
static int set_sysdump_enable(int on);


void sprd_debug_check_crash_key(unsigned int code, int value)
{
/* HS03 code for P211016-02151 by yubaiwen at 20211020 start*/
	#ifdef HUAQIN_BUILD
	static unsigned int volup_p;
	static unsigned int voldown_p;
	static unsigned int loopcount;
	static unsigned long vol_pressed;

#if 0
	/* Must be deleted later */
	pr_info("Test %s:key code(%d) value(%d),(up:%d,down:%d),lpct(%d),vop(%ld)\n", __func__,
		code, value, volup_p, voldown_p, loopcount, vol_pressed);
#endif

	/*  Enter Force Upload
	 *  hold the volume down and volume up
	 *  and then press power key twice
	 */
	if (value) {
		if (code == KEY_VOLUMEUP)
			volup_p = SYSDUMP_MAGIC_VOLUP;
		if (code == KEY_VOLUMEDOWN)
			voldown_p = SYSDUMP_MAGIC_VOLDN;

		if ((volup_p == SYSDUMP_MAGIC_VOLUP) && (voldown_p == SYSDUMP_MAGIC_VOLDN)) {
			if (!vol_pressed)
				vol_pressed = jiffies;

			if (code == KEY_POWER) {
				pr_info("%s: Crash key count : %d,vol_pressed:%ld\n", __func__,
					++loopcount, vol_pressed);
				if (time_before(jiffies, vol_pressed + 5 * HZ)) {
					if (loopcount == 2)
						panic("Crash Key");
				} else {
					pr_info("%s: exceed 5s(%u) between power key and volup/voldn key\n",
						__func__, jiffies_to_msecs(jiffies - vol_pressed));
					volup_p = 0;
					voldown_p = 0;
					loopcount = 0;
					vol_pressed = 0;
				}
			}
		}
	} else {
		if (code == KEY_VOLUMEUP) {
			volup_p = 0;
			loopcount = 0;
			vol_pressed = 0;
		}
		if (code == KEY_VOLUMEDOWN) {
			voldown_p = 0;
			loopcount = 0;
			vol_pressed = 0;
		}
	}
	#endif // HUAQIN_BUILD
/* HS03 code for P211016-02151 by yubaiwen at 20211020 end*/
}

/**
 * save extend debug information of modules in minidump, such as: cm4, iram...
 *
 * @name:	the name of the modules, and the string will be a part
 *		of the file name.
 *		note: special characters can't be included in the file name,
 *		such as:'?','*','/','\','<','>',':','"','|'.
 *
 * @paddr_start:the start paddr in memory of the modules debug information
 * @paddr_end:	the end paddr in memory of the modules debug information
 *
 * Return: 0 means success, -1 means fail.
 */
int minidump_save_extend_information(const char *name, unsigned long paddr_start,
								unsigned long paddr_end)
{

	int i, j, index;
	struct section_info *extend_section;
	char str_name[SECTION_NAME_MAX];
	char tmp;

	/* check name valid first */
	if (strlen(name) > (SECTION_NAME_MAX - strlen(EXTEND_STRING) - 1)) {
		pr_err("The length of name is too long!!, add extend section fail!!\n");
		return -1;
	}
	if (!strlen(name)) {
		pr_err("The name is empty, invalid!!\n");
		return -1;
	}
	memcpy(str_name, name, strlen(name));
	for (j = 0; j < strlen(str_name); j++) {
		tmp = str_name[j];
		if (tmp == '?' || tmp == '*' || tmp == '/' || tmp == '>' || tmp == '<'
								|| tmp == '"' || tmp == '|') {
			pr_err("the name=%s include special character:%c that not supported!!\n",
					name, tmp);
			return -1;
		}
	}
	/* section_mutex only be inited once */
	if (atomic_read(&mutex_init_flag) == 0) {
		mutex_init(&section_mutex);
		atomic_inc(&mutex_init_flag);
	}

	mutex_lock(&section_mutex);
	/* check insert repeatly and acquire total seciton num before insert new section */
	for (i = 0; i < SECTION_NUM_MAX; i++) {
		if (!memcmp(str_name,
				minidump_info_g.section_info_total.section_info[i].section_name,
				strlen(str_name))) {
			mutex_unlock(&section_mutex);
			return -1;
		}
		if (!strlen(minidump_info_g.section_info_total.section_info[i].section_name))
			break;
	}
	minidump_info_g.section_info_total.total_num = i;
	index = minidump_info_g.section_info_total.total_num;

	if (index >= (SECTION_NUM_MAX - 1)) {
		pr_err("No space for new section.\n");
		mutex_unlock(&section_mutex);
		return -1;
	}
	/* add new section in the tail of section_info */
	extend_section = &minidump_info_g.section_info_total.section_info[index];
	sprintf(extend_section->section_name, "%s_%s", EXTEND_STRING, name);
	if (vaddr_to_paddr_flag == 0) {
		extend_section->section_start_vaddr = (unsigned long)__va(paddr_start);
		extend_section->section_end_vaddr = (unsigned long)__va(paddr_end);
	} else {
		extend_section->section_start_paddr = paddr_start;
		extend_section->section_end_paddr = paddr_end;
		extend_section->section_size = extend_section->section_end_paddr -
						extend_section->section_start_paddr;
		minidump_info_g.section_info_total.total_size += extend_section->section_size;
		minidump_info_g.minidump_data_size += extend_section->section_size;
	}
	minidump_info_g.section_info_total.total_num++;
	pr_emerg("%s added successfully in minidump section:paddr_start=%lx,paddr_end=%lx\n",
			name, paddr_start, paddr_end);
	mutex_unlock(&section_mutex);
	return 0;
}
EXPORT_SYMBOL(minidump_save_extend_information);

static char *storenote(struct memelfnote *men, char *bufp)
{
	struct elf_note en;

#define DUMP_WRITE(addr, nr) do {memcpy(bufp, addr, nr); bufp += nr; } while (0)

	en.n_namesz = strlen(men->name) + 1;
	en.n_descsz = men->datasz;
	en.n_type = men->type;

	DUMP_WRITE(&en, sizeof(en));
	DUMP_WRITE(men->name, en.n_namesz);

	/* XXX - cast from long long to long to avoid need for libgcc.a */
	bufp = (char *)roundup((unsigned long)bufp, 4);
	DUMP_WRITE(men->data, men->datasz);
	bufp = (char *)roundup((unsigned long)bufp, 4);

#undef DUMP_WRITE

	return bufp;
}

/*
 * fill up all the fields in prstatus from the given task struct, except
 * registers which need to be filled up separately.
 */
static void fill_prstatus(struct elf_prstatus *prstatus,
			  struct task_struct *p, long signr)
{
	prstatus->pr_info.si_signo = prstatus->pr_cursig = signr;
	prstatus->pr_sigpend = p->pending.signal.sig[0];
	prstatus->pr_sighold = p->blocked.sig[0];
	rcu_read_lock();
	prstatus->pr_ppid = task_pid_vnr(rcu_dereference(p->real_parent));
	rcu_read_unlock();
	prstatus->pr_pid = task_pid_vnr(p);
	prstatus->pr_pgrp = task_pgrp_vnr(p);
	prstatus->pr_sid = task_session_vnr(p);
	if (0 /* thread_group_leader(p) */) {
		struct task_cputime cputime;

		/*
		 * This is the record for the group leader.  It shows the
		 * group-wide total, not its individual thread total.
		 */
		/* thread_group_cputime(p, &cputime); */
		prstatus->pr_utime = ns_to_timeval(cputime.utime);
		prstatus->pr_stime = ns_to_timeval(cputime.stime);
	} else {
		prstatus->pr_utime = ns_to_timeval(p->utime);
		prstatus->pr_stime = ns_to_timeval(p->stime);
	}
	prstatus->pr_cutime = ns_to_timeval(p->signal->cutime);
	prstatus->pr_cstime = ns_to_timeval(p->signal->cstime);

}

void crash_note_save_cpu(struct pt_regs *regs, int cpu)
{
	struct elf_prstatus prstatus;
	struct memelfnote notes;

	notes.name = CORE_STR;
	notes.type = NT_PRSTATUS;
	notes.datasz = sizeof(struct elf_prstatus);
	notes.data = &prstatus;

	memset(&prstatus, 0, sizeof(struct elf_prstatus));
	fill_prstatus(&prstatus, current, 0);
	memcpy(&prstatus.pr_reg, regs, DUMP_REGS_SIZE);
	/* memcpy(&prstatus.pr_reg, regs, sizeof(struct pt_regs)); */
	storenote(&notes, (char *)per_cpu_ptr(crash_notes, cpu));
}

static void sysdump_fill_core_hdr(struct pt_regs *regs, char *bufp)
{
	struct elf_phdr *nhdr;

	/* setup ELF header */
	bufp += sizeof(struct elfhdr);

	/* setup ELF PT_NOTE program header */
	nhdr = (struct elf_phdr *)bufp;
	memset(nhdr, 0, sizeof(struct elf_phdr));
	nhdr->p_memsz = SYSDUMP_NOTE_BYTES * NR_CPUS;

	return;
}

static int __init sysdump_magic_setup(char *str)
{
	if (str != NULL)
		sscanf(&str[0], "%lx", &sysdump_magic_paddr);

	pr_info("[%s]SYSDUMP paddr from uboot: 0x%lx\n",
		 __func__, sysdump_magic_paddr);
	return 1;
}
__setup("sysdump_magic=", sysdump_magic_setup);
/* get sysdump reserve flag from uboot
 * sysdump_reflag: 1 sysdump reserved in dts
 *		   0 sysdump don't reserved in dts
 */
static int __init sysdump_reflag_setup(char *str)
{
	if (str != NULL)
		sscanf(&str[0], "%d", &sysdump_reflag);
	pr_info("[%s]sysdump reserve flag from uboot: %d\n",
		__func__, sysdump_reflag);
	return 1;
}
__setup("sysdump_re_flag=", sysdump_reflag_setup);

static unsigned long get_sprd_sysdump_info_paddr(void)
{
	struct device_node *node;
	unsigned long *magic_addr;
	unsigned long reg_phy = 0;
	int aw = 0, len = 0;

	if (sysdump_magic_paddr)
		reg_phy = sysdump_magic_paddr;
	else {
		pr_err
		    ("Not find sysdump_magic_paddr from bootargs,use sysdump node from dts\n");
		node = of_find_node_by_name(NULL, "sprd-sysdump");

		if (!node) {
			pr_err
			    ("Not find sprd-sysdump node from dts,use SPRD_SYSDUMP_MAGIC\n");
			return 0;
		} else {
			magic_addr =
			    (unsigned long *)of_get_property(node, "magic-addr",
							     &len);
			if (!magic_addr) {
				pr_err
				    ("Not find magic-addr property from sprd-sysdump node\n");
				return 0;
			} else {
				aw = of_n_addr_cells(node);
				reg_phy =
				    of_read_ulong((const __be32 *)magic_addr,
						  aw);
			}
		}
	}
	return reg_phy;
}
static int sysdump_info_init(void)
{
	/*get_sprd_sysdump_info_paddr(); */
	sprd_sysdump_info_paddr = get_sprd_sysdump_info_paddr();
	if (!sprd_sysdump_info_paddr) {
		pr_err("get sprd_sysdump_info_paddr failed.\n");
		return -1;
	}
	sprd_sysdump_info = (struct sysdump_info *)phys_to_virt(sprd_sysdump_info_paddr);

	sprd_sysdump_init = 1;

	/* can't write anything at SPRD_SYSDUMP_MAGIC before rootfs init */
	if (sysdump_reflag) {
		/*get kaslr info for arm64*/
#ifdef CONFIG_ARM64
		sprd_sysdump_info->sprd_kaslrinfo.kaslr_offset = kaslr_offset();
		sprd_sysdump_info->sprd_kaslrinfo.kimage_voffset = kimage_voffset;
		sprd_sysdump_info->sprd_kaslrinfo.phys_offset = PHYS_OFFSET;
		sprd_sysdump_info->sprd_kaslrinfo.vabits_actual = (uint64_t)VA_BITS;
		pr_emerg("[%s]vmcore info init end!\n", __func__);
#endif
		/* get log_buf info */
#ifdef CONFIG_KALLSYMS
		sprd_sysdump_info->sprd_logbuf_info.log_buf =
			__pa(kallsyms_lookup_name("log_buf"));
		sprd_sysdump_info->sprd_logbuf_info.log_first_idx =
			__pa(kallsyms_lookup_name("log_first_idx"));
		sprd_sysdump_info->sprd_logbuf_info.log_next_idx =
			__pa(kallsyms_lookup_name("log_next_idx"));
#endif
		sprd_sysdump_info->sprd_logbuf_info.log_buf_len = log_buf_len_get();
		sprd_sysdump_info->sprd_logbuf_info.vmcoreinfo_size =
			__pa(&vmcoreinfo_size);
		/* get mmu regs info */
		sprd_sysdump_info->sprd_mmuregs_info.paddr_mmu_regs_t =
			__pa(&per_cpu(sprd_debug_mmu_reg, smp_processor_id()));
		sprd_sysdump_info->sprd_mmuregs_info.cpu_id = smp_processor_id();
		sprd_sysdump_info->sprd_mmuregs_info.sprd_pcpu_offset = __per_cpu_offset[1] -
			__per_cpu_offset[0];
		sprd_sysdump_info->sprd_mmuregs_info.cpu_numbers = CONFIG_NR_CPUS;
	}
	return 0;

}
static void sysdump_prepare_info(int enter_id, const char *reason,
				 struct pt_regs *regs)
{
	struct timex txc;
	struct rtc_time tm;

	strncpy(sprd_sysdump_extra.reason,
		reason, sizeof(sprd_sysdump_extra.reason)-1);
	sprd_sysdump_extra.enter_id = enter_id;

	if (!sprd_sysdump_info_paddr)
		return;
	memcpy(sprd_sysdump_info->magic, SYSDUMP_MAGIC,
	       sizeof(sprd_sysdump_info->magic));

	if (reason != NULL && !strcmp(reason, "Crash Key"))
		sprd_sysdump_info->crash_key = 1;
	else
		sprd_sysdump_info->crash_key = 0;

	pr_info("reason: %s, sprd_sysdump_info->crash_key: %d\n",
		 reason, sprd_sysdump_info->crash_key);
	do_gettimeofday(&(txc.time));
	txc.time.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(txc.time.tv_sec, &tm);
	sprintf(sprd_sysdump_info->time, "%04d-%02d-%02d_%02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
		tm.tm_min, tm.tm_sec);

	memcpy(sprd_sysdump_info->dump_path, sysdump_conf.dump_path,
	       sizeof(sprd_sysdump_info->dump_path));

	sysdump_fill_core_hdr(regs,
			      (char *)sprd_sysdump_info +
			      sizeof(*sprd_sysdump_info));
	return;
}

DEFINE_PER_CPU(struct sprd_debug_core_t, sprd_debug_core_reg);

#ifdef CONFIG_SEC_DEBUG
void copy_to_cpu_ctx(int cpu)
{

	per_cpu(sec_debug_core_reg, cpu).x0 = per_cpu(sprd_debug_core_reg, cpu).x0;
	per_cpu(sec_debug_core_reg, cpu).x1 = per_cpu(sprd_debug_core_reg, cpu).x1;
	per_cpu(sec_debug_core_reg, cpu).x2 = per_cpu(sprd_debug_core_reg, cpu).x2;
	per_cpu(sec_debug_core_reg, cpu).x3 = per_cpu(sprd_debug_core_reg, cpu).x3;
	per_cpu(sec_debug_core_reg, cpu).x4 = per_cpu(sprd_debug_core_reg, cpu).x4;
	per_cpu(sec_debug_core_reg, cpu).x5 = per_cpu(sprd_debug_core_reg, cpu).x5;
	per_cpu(sec_debug_core_reg, cpu).x6 = per_cpu(sprd_debug_core_reg, cpu).x6;
	per_cpu(sec_debug_core_reg, cpu).x7 = per_cpu(sprd_debug_core_reg, cpu).x7;
	per_cpu(sec_debug_core_reg, cpu).x8 = per_cpu(sprd_debug_core_reg, cpu).x8;
	per_cpu(sec_debug_core_reg, cpu).x9 = per_cpu(sprd_debug_core_reg, cpu).x9;
		
	per_cpu(sec_debug_core_reg, cpu).x10 = per_cpu(sprd_debug_core_reg, cpu).x10;
	per_cpu(sec_debug_core_reg, cpu).x11 = per_cpu(sprd_debug_core_reg, cpu).x11;
	per_cpu(sec_debug_core_reg, cpu).x12 = per_cpu(sprd_debug_core_reg, cpu).x12;
	per_cpu(sec_debug_core_reg, cpu).x13 = per_cpu(sprd_debug_core_reg, cpu).x13;
	per_cpu(sec_debug_core_reg, cpu).x14 = per_cpu(sprd_debug_core_reg, cpu).x14;
	per_cpu(sec_debug_core_reg, cpu).x15 = per_cpu(sprd_debug_core_reg, cpu).x15;
	per_cpu(sec_debug_core_reg, cpu).x16 = per_cpu(sprd_debug_core_reg, cpu).x16;
	per_cpu(sec_debug_core_reg, cpu).x17 = per_cpu(sprd_debug_core_reg, cpu).x17;
	per_cpu(sec_debug_core_reg, cpu).x18 = per_cpu(sprd_debug_core_reg, cpu).x18;
	per_cpu(sec_debug_core_reg, cpu).x19 = per_cpu(sprd_debug_core_reg, cpu).x19;

	per_cpu(sec_debug_core_reg, cpu).x20 = per_cpu(sprd_debug_core_reg, cpu).x20;
	per_cpu(sec_debug_core_reg, cpu).x21 = per_cpu(sprd_debug_core_reg, cpu).x21;
	per_cpu(sec_debug_core_reg, cpu).x22 = per_cpu(sprd_debug_core_reg, cpu).x22;
	per_cpu(sec_debug_core_reg, cpu).x23 = per_cpu(sprd_debug_core_reg, cpu).x23;
	per_cpu(sec_debug_core_reg, cpu).x24 = per_cpu(sprd_debug_core_reg, cpu).x24;
	per_cpu(sec_debug_core_reg, cpu).x25 = per_cpu(sprd_debug_core_reg, cpu).x25;
	per_cpu(sec_debug_core_reg, cpu).x26 = per_cpu(sprd_debug_core_reg, cpu).x26;
	per_cpu(sec_debug_core_reg, cpu).x27 = per_cpu(sprd_debug_core_reg, cpu).x27;
	per_cpu(sec_debug_core_reg, cpu).x28 = per_cpu(sprd_debug_core_reg, cpu).x28;
	per_cpu(sec_debug_core_reg, cpu).x29 = per_cpu(sprd_debug_core_reg, cpu).x29;
	per_cpu(sec_debug_core_reg, cpu).x30 = per_cpu(sprd_debug_core_reg, cpu).x30;

	per_cpu(sec_debug_core_reg, cpu).pc = per_cpu(sprd_debug_core_reg, cpu).pc;
	per_cpu(sec_debug_core_reg, cpu).cpsr = per_cpu(sprd_debug_core_reg, cpu).pstate;
	
}

void copy_to_mmu_ctx(int cpu)
{
	per_cpu(sec_debug_mmu_reg, cpu).TTBR0_EL1 = per_cpu(sprd_debug_mmu_reg, cpu).ttbr0_el1;
	per_cpu(sec_debug_mmu_reg, cpu).TTBR1_EL1 = per_cpu(sprd_debug_mmu_reg, cpu).ttbr1_el1;
	per_cpu(sec_debug_mmu_reg, cpu).TCR_EL1 = per_cpu(sprd_debug_mmu_reg, cpu).tcr_el1;
	per_cpu(sec_debug_mmu_reg, cpu).MAIR_EL1 = per_cpu(sprd_debug_mmu_reg, cpu).mair_el1;
	per_cpu(sec_debug_mmu_reg, cpu).AMAIR_EL1 = per_cpu(sprd_debug_mmu_reg, cpu).amair_el1;
}
#endif

static inline void sprd_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sprd_debug_save_mmu_reg(&per_cpu
				(sprd_debug_mmu_reg, smp_processor_id()));
	sprd_debug_save_core_reg(&per_cpu
				 (sprd_debug_core_reg, smp_processor_id()));

#ifdef CONFIG_SEC_DEBUG
	copy_to_mmu_ctx(smp_processor_id());
	copy_to_cpu_ctx(smp_processor_id());
#endif

	pr_debug("(%s) context saved(CPU:%d)\n", __func__, smp_processor_id());
	local_irq_restore(flags);

	flush_cache_all();
}


static int sysdump_panic_event(struct notifier_block *self,
					unsigned long val,
					void *reason)
{
	struct pt_regs *pregs = NULL;
	static int enter_id;

	pr_emerg("(%s) ------ in (%d)\n", __func__, enter_id);
	bust_spinlocks(1);
	if (sprd_sysdump_init == 0) {
		sprd_sysdump_info_paddr = get_sprd_sysdump_info_paddr();
		if (!sprd_sysdump_info_paddr) {
			pr_emerg("get sprd_sysdump_info_paddr failed!!!.\n");
		} else {
			sprd_sysdump_info =
				(struct sysdump_info *)phys_to_virt(sprd_sysdump_info_paddr);
			pr_emerg("vaddr is %p, paddr is %p.\n", sprd_sysdump_info,
				(void *)sprd_sysdump_info_paddr);
		}
		crash_notes = &crash_notes_temp;
	}

	/* this should before smp_send_stop() to make sysdump_ipi enable */
	sprd_sysdump_extra.enter_cpu = smp_processor_id();

	pregs = &sprd_sysdump_extra.cpu_context[sprd_sysdump_extra.enter_cpu];
	crash_setup_regs((struct pt_regs *)pregs, NULL);

	crash_note_save_cpu(pregs, sprd_sysdump_extra.enter_cpu);
	sprd_debug_save_context();

#ifdef CONFIG_SPRD_SIPC
	if (!(reason != NULL && strstr(reason, "cpcrash")))
		smsg_senddie(SIPC_ID_LTE);
#endif

	smp_send_stop();
	mdelay(1000);

	pr_emerg("\n");
	pr_emerg("*****************************************************\n");
	pr_emerg("*                                                   *\n");
	pr_emerg("*  Sysdump enter, preparing debug info to dump ...  *\n");
	pr_emerg("*                                                   *\n");
	pr_emerg("*****************************************************\n");
	pr_emerg("\n");

	if (reason != NULL)
		sysdump_prepare_info(enter_id, reason, pregs);

#ifdef CONFIG_SPRD_MINI_SYSDUMP
	/* when track regs use pregs_die,  others use now regs*/
	if (die_notify_flag) {
		if (!user_mode(&pregs_die_g)) {
			prepare_minidump_info(&pregs_die_g);
			prepare_exception_info(&pregs_die_g, NULL, reason);
		} else {
			prepare_minidump_info(pregs);
			prepare_exception_info(pregs, NULL, reason);
		}
	} else {
		prepare_minidump_info(pregs);
		prepare_exception_info(pregs, NULL, reason);
	}
#endif
	if (sprd_sysdump_shash_init) {
		pr_emerg("KTXT VERIFY...\n");
		crypto_shash_update(desc, (u8 *)_stext, _etext-_stext);
		crypto_shash_final(desc, g_ktxt_hash_data);

		pr_emerg("KTXT [0x%lx--0x%lx]\n",
			(unsigned long)_stext, (unsigned long)_etext);
		pr_emerg("SHA1:\n");
		pr_emerg("%x %x %x %x %x\n",
			*((unsigned int *)g_ktxt_hash_data + 0),
			*((unsigned int *)g_ktxt_hash_data + 1),
			*((unsigned int *)g_ktxt_hash_data + 2),
			*((unsigned int *)g_ktxt_hash_data + 3),
			*((unsigned int *)g_ktxt_hash_data + 4));
	}

	pr_emerg("\n");
	pr_emerg("*****************************************************\n");
	pr_emerg("*                                                   *\n");
	pr_emerg("*  Try to reboot system ...                         *\n");
	pr_emerg("*                                                   *\n");
	pr_emerg("*****************************************************\n");
	pr_emerg("\n");

	flush_cache_all();
	mdelay(1000);

	bust_spinlocks(0);

#ifdef CONFIG_SPRD_DEBUG
	if (reason != NULL && strstr(reason, "Watchdog detected hard LOCKUP"))
		while (1)
			;
#endif
	return NOTIFY_DONE;
}

void sysdump_ipi(struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	if (crash_notes == NULL)
		crash_notes = &crash_notes_temp;

	/*do flush and save only in oops path */
	if (oops_in_progress) {
		memcpy((void *)&(sprd_sysdump_extra.cpu_context[cpu]),
		       (void *)regs, sizeof(struct pt_regs));
		if (!user_mode(regs)) {
			crash_note_save_cpu(regs, cpu);
			sprd_debug_save_context();
		} else {
			flush_cache_all();
		}
	}
	return;
}

static void sysdump_event(struct input_handle *handle,
	unsigned int type, unsigned int code, int value)
{
	if (type == EV_KEY && code != BTN_TOUCH)
		sprd_debug_check_crash_key(code, value);
}

static const struct input_device_id sysdump_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{},
};

static int sysdump_connect(struct input_handler *handler,
			 struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *sysdump_handle;
	int error;

	sysdump_handle = (struct input_handle *)kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!sysdump_handle)
		return -ENOMEM;

	sysdump_handle->dev = dev;
	sysdump_handle->handler = handler;
	sysdump_handle->name = "sysdump";

	error = input_register_handle(sysdump_handle);
	if (error) {
		pr_err("Failed to register input sysrq handler, error %d\n",
			error);
		goto err_free;
	}

	error = input_open_device(sysdump_handle);
	if (error) {
		pr_err("Failed to open input device, error %d\n", error);
		goto err_unregister;
	}

	return 0;

 err_unregister:
	input_unregister_handle(sysdump_handle);
 err_free:
	kfree(sysdump_handle);
	return error;
}

static void sysdump_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
}

static int sprd_sysdump_read(struct seq_file *s, void *v)
{
	seq_printf(s, "sysdump_status = %d\n", sysdump_status);
	return 0;
}

static int sprd_sysdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_sysdump_read, NULL);
}

static ssize_t sprd_sysdump_write(struct file *file, const char __user *buf,
				size_t count, loff_t *data)
{
	char sysdump_buf[SYSDUMP_PROC_BUF_LEN] = {0};

	pr_info("%s: start!!!\n", __func__);
	if (count && (count < SYSDUMP_PROC_BUF_LEN)) {
		if (copy_from_user(sysdump_buf, buf, count)) {
			pr_err("%s: copy_from_user failed!!!\n", __func__);
			return -1;
		}
		sysdump_buf[count] = '\0';

		if (!strncmp(sysdump_buf, "on", 2)) {
			pr_info("%s: enable user version sysdump!!!\n",
				__func__);
			set_sysdump_enable(1);
		} else if (!strncmp(sysdump_buf, "off", 3)) {
			pr_info("%s: disable user version sysdump!!!\n",
				__func__);
			set_sysdump_enable(0);
		} else if (!strncmp(sysdump_buf, "bug", 3)) {
			pr_info("%s  bug-on !!\n", __func__);
			BUG_ON(1);
		}
	}

	pr_info("%s: End!!!\n", __func__);
	return count;
}


static struct ctl_table sysdump_sysctl_table[] = {
	{
	 .procname = "sysdump_enable",
	 .data = &sysdump_conf.enable,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = proc_dointvec,
	 },
	{
	 .procname = "sysdump_crashkey_only",
	 .data = &sysdump_conf.crashkey_only,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = proc_dointvec,
	 },
	{
	 .procname = "sysdump_dump_modem",
	 .data = &sysdump_conf.dump_modem,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = proc_dointvec,
	 },
	{
	 .procname = "sysdump_reboot",
	 .data = &sysdump_conf.reboot,
	 .maxlen = sizeof(int),
	 .mode = 0644,
	 .proc_handler = proc_dointvec,
	 },
	{
	 .procname = "sysdump_dump_path",
	 .data = sysdump_conf.dump_path,
	 .maxlen = sizeof(sysdump_conf.dump_path),
	 .mode = 0644,
	 .proc_handler = proc_dostring,
	 },
	{}
};

static struct ctl_table sysdump_sysctl_root[] = {
	{
	 .procname = "kernel",
	 .mode = 0555,
	 .child = sysdump_sysctl_table,
	 },
	{}
};

static struct ctl_table_header *sysdump_sysctl_hdr;

static struct input_handler sysdump_handler = {
	.event = sysdump_event,
	.connect	= sysdump_connect,
	.disconnect	= sysdump_disconnect,
	.name = "sysdump_crashkey",
	.id_table	= sysdump_ids,
};

static const struct file_operations sysdump_proc_fops = {
	.owner = THIS_MODULE,
	.open = sprd_sysdump_open,
	.read = seq_read,
	.write = sprd_sysdump_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sprd_sysdump_enable_prepare(void)
{
	struct platform_device *pdev_regmap;
	struct device_node *regmap_np;

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		pr_err("of_find_compatible_node failed!!!\n");
		goto error_pmic_node;
	}

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721")) {
		pmic_reg = ANA_RST_STATUS_OFFSET_2721;
		pr_info(" detect pmic is sc2721 ,offset = 0x%x !!!\n",
			 pmic_reg);
	} else {
		pmic_reg = ANA_RST_STATUS_OFFSET_2730;
		pr_info(" detect pmic is sc2730 ,offset = 0x%x !!!\n",
			pmic_reg);
	}

	pdev_regmap = of_find_device_by_node(regmap_np);
	if (!pdev_regmap) {
		pr_err("of_find_device_by_node failed!!!\n");
		goto error_find_device;
	}

	regmap = dev_get_regmap(pdev_regmap->dev.parent, NULL);
	if (!regmap) {
		pr_err("dev_get_regmap failed!!!\n");
		goto error_find_device;
	}

	of_node_put(regmap_np);
	pr_info("%s ok\n", __func__);
	return 0;

error_find_device:
	of_node_put(regmap_np);
error_pmic_node:
	return -ENODEV;
}

static int set_sysdump_enable(int on)
{
	unsigned int val = 0;


	if (!regmap) {
		pr_err("can not %s sysdump because of regmap is NULL\n",
			on ? "enable" : "disable");
		return -1;
	}

	regmap_read(regmap, pmic_reg, &val);
	pr_info("%s: get rst mode  value is = %x\n", __func__, val);

	if (on) {
		pr_info("%s: enable sysdump!\n", __func__);
		val |= HWRST_STATUS_SYSDUMP;
		regmap_write(regmap, pmic_reg, val);
		sysdump_status = 1;
	} else {
		pr_info("%s: disable sysdump!\n", __func__);
		val &= ~(HWRST_STATUS_SYSDUMP);
		regmap_write(regmap, pmic_reg, val);
		sysdump_status = 0;
	}

	return 0;
}

static int sysdump_shash_init(void)
{
	struct crypto_shash *tfm;
	size_t desc_size;
	int ret;

	tfm = crypto_alloc_shash("sha1", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm))
		return (PTR_ERR(tfm) == -ENOENT) ? -ENOPKG : PTR_ERR(tfm);

	desc_size = crypto_shash_descsize(tfm) + sizeof(*desc);

	desc = kzalloc(desc_size, GFP_KERNEL);
	if (!desc)
		goto error_no_desc;

	desc->tfm = tfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	ret = crypto_shash_init(desc);
	if (ret < 0) {
		pr_err("crypto_shash_init fail(%d)!\n", ret);
		return ret;
	}
	sprd_sysdump_shash_init = 1;
	return 0;
error_no_desc:
	crypto_free_shash(tfm);
	return -ENOMEM;
}

#ifdef CONFIG_SPRD_MINI_SYSDUMP /*	minidump code start	*/
static int minidump_info_read(struct seq_file *s, void *v)
{
	seq_printf(s,
		    "%s:0x%lx\n"
		    "%s:0x%x\n"
		    , GET_MINIDUMP_INFO_NAME(MINIDUMP_INFO_PADDR), minidump_info_desc_g.paddr
		    , GET_MINIDUMP_INFO_NAME(MINIDUMP_INFO_SIZE), minidump_info_desc_g.size
		   );
	return 0;
}

static int minidump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, minidump_info_read, NULL);
}
static const struct file_operations minidump_proc_fops = {
		.owner = THIS_MODULE,
		.open = minidump_info_open,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
};
void prepare_minidump_reg_memory(struct pt_regs *regs)
{
	int i;
	unsigned long addr;
	mm_segment_t fs;
	if (user_mode(regs))
		for (i = 0; i < minidump_info_g.regs_info.num; i++)
			minidump_info_g.regs_memory_info.reg_paddr[i] = 0;
	fs = get_fs();
	set_fs(KERNEL_DS);
	/*	get all valid paddr every  reg refers to  */
	for (i = 0; i < minidump_info_g.regs_info.num; i++) {
#ifdef CONFIG_ARM
		addr = regs->uregs[i] - minidump_info_g.regs_memory_info.per_reg_memory_size / 2;
		pr_debug("R%d: %08lx\n", i, regs->uregs[i]);
		pr_debug("addr: %08lx\n", addr);
		if (addr < PAGE_OFFSET || addr > -256UL) {
#endif
#ifdef CONFIG_ARM64
		if (REG_SP_INDEX == i) {
			addr = regs->sp - minidump_info_g.regs_memory_info.per_reg_memory_size / 2;
			pr_debug("sp: %llx\n", regs->sp);
			pr_debug("addr: %lx\n", addr);

		} else if (REG_PC_INDEX == i) {
			addr = regs->pc - minidump_info_g.regs_memory_info.per_reg_memory_size / 2;
			pr_debug("pc: %llx\n", regs->pc);
			pr_debug("addr: %lx\n", addr);

		} else {
			addr = regs->regs[i] - minidump_info_g.regs_memory_info.per_reg_memory_size
				/ 2;
			pr_debug("R%d: %llx\n", i, regs->regs[i]);
			pr_debug("addr: %lx\n", addr);
		}
		if (addr < KIMAGE_VADDR || addr > -256UL) {

#endif
			minidump_info_g.regs_memory_info.reg_paddr[i] = 0;
			pr_debug("reg value invalid !!!\n");
		} else {
			minidump_info_g.regs_memory_info.reg_paddr[i] = __pa(addr);
			minidump_info_g.regs_memory_info.valid_reg_num++;
		}
		pr_debug("reg[%d] paddr: %lx\n",
			 i, minidump_info_g.regs_memory_info.reg_paddr[i]);
	}
	minidump_info_g.regs_memory_info.size = minidump_info_g.regs_memory_info.valid_reg_num *
					 minidump_info_g.regs_memory_info.per_reg_memory_size;
	pr_debug("size : %d\n", minidump_info_g.regs_memory_info.size);
	set_fs(fs);
	return;
}
void show_minidump_info(struct minidump_info *minidump_infop)
{
	int i;

	pr_debug("kernel_magic: %s\n ", minidump_infop->kernel_magic);
	pr_debug("---     regs_info       ---\n ");
	pr_debug("arch:              %d\n ", minidump_infop->regs_info.arch);
	pr_debug("num:               %d\n ", minidump_infop->regs_info.num);
	pr_debug("paddr:         %lx\n ", minidump_infop->regs_info.paddr);
	pr_debug("size:          %d\n ", minidump_infop->regs_info.size);
	pr_debug("---     regs_memory_info        ---\n ");
	for (i = 0; i < minidump_infop->regs_info.num; i++) {
		pr_debug("reg[%d] paddr:          %lx\n ",
			i, minidump_infop->regs_memory_info.reg_paddr[i]);
	}
	pr_debug("per_reg_memory_size:    %d\n ",
		minidump_infop->regs_memory_info.per_reg_memory_size);
	pr_debug("valid_reg_num:          %d\n ",
		minidump_infop->regs_memory_info.valid_reg_num);
	pr_debug("reg_memory_all_size:    %d\n ",
		minidump_infop->regs_memory_info.size);
	pr_debug("---     section_info_total        ---\n ");
	pr_debug("Here are %d sections, Total size : %d\n",
		minidump_infop->section_info_total.total_num,
		minidump_infop->section_info_total.total_size);
	pr_debug("total_num:        %x\n ",
		minidump_infop->section_info_total.total_num);
	pr_debug("total_size        %x\n ",
		minidump_infop->section_info_total.total_size);
	for (i = 0; i < minidump_infop->section_info_total.total_num; i++) {
		pr_debug("section_name:           %s\n ",
		minidump_infop->section_info_total.section_info[i].section_name);
		pr_debug("section_start_vaddr:    %lx\n ",
		minidump_infop->section_info_total.section_info[i].section_start_vaddr);
		pr_debug("section_end_vaddr:      %lx\n ",
		minidump_infop->section_info_total.section_info[i].section_end_vaddr);
		pr_debug("section_start_paddr:    %lx\n ",
		minidump_infop->section_info_total.section_info[i].section_start_paddr);
		pr_debug("section_end_paddr:      %lx\n ",
		minidump_infop->section_info_total.section_info[i].section_end_paddr);
		pr_debug("section_size:           %x n ",
		minidump_infop->section_info_total.section_info[i].section_size);
	}
	pr_debug("minidump_data_size:     %x\n ",
		minidump_infop->minidump_data_size);
	return;
}

/*	Here we prepare minidump all info
	| minidump_info | struct pt_regs | memory amount regs | sections | others(just like kernel
	logbuf ) |
*/
static int prepare_minidump_info(struct pt_regs *regs)
{

	if (regs != NULL) {
		/*	struct pt_regs part: save minidump_regs_g contents */
		memcpy(&minidump_regs_g, regs, sizeof(struct pt_regs));
		/*      memory amount regs part: save minidump_regs_g contents */
		prepare_minidump_reg_memory(regs);

	} else {
		pr_err("%s regs NULL .\n", __func__);
	}

	minidump_info_g.minidump_data_size =  minidump_info_g.regs_info.size +
				minidump_info_g.regs_memory_info.size +
				minidump_info_g.section_info_total.total_size;

	/*	sections part: we have got all info when init, here do nothing */
	show_minidump_info(&minidump_info_g);
	return 0;
}

static int dump_die_cb(struct notifier_block *nb, unsigned long reason, void *arg)
{
	struct die_args *die_args = arg;
	if (reason == DIE_OOPS) {
		memcpy(&pregs_die_g, die_args->regs, sizeof(pregs_die_g));
		die_notify_flag = 1;
		pr_emerg("%s save pregs_die_g ok .\n", __func__);
	}
	return NOTIFY_DONE;
}

static struct notifier_block dump_die_notifier = {
	.notifier_call = dump_die_cb
};

static void section_info_ylog_buf(void)
{
	int ret;
	long vaddr = (long)(ylog_buffer);;

	pr_info("%s in. vaddr : 0x%lx  len :0x%x\n",
		 __func__, vaddr, YLOG_BUF_SIZE);
	ret = minidump_save_extend_information("ylog_buf", __pa(vaddr),
						__pa(vaddr + YLOG_BUF_SIZE));
	if (!ret)
		pr_info("ylog_buf added to minidump section ok!!\n");
}

#ifdef CONFIG_SPRD_NATIVE_HANG_MONITOR
static void section_native_hang(void)
{
	int ret;
	unsigned long vaddr, len, start;

	get_native_hang_monitor_buffer(&vaddr, &len, &start);
	pr_debug("%s in.\n", __func__);
	ret = minidump_save_extend_information("nhang", __pa(vaddr), __pa(vaddr + len));
	if (!ret)
		pr_info("nhang added to minidump section ok!!\n");
	pr_debug("%s out.\n", __func__);
}
#endif

static void section_info_pt(void)
{
	unsigned long vaddr;
	int len;
	int ret;
	vaddr  = (unsigned long)swapper_pg_dir;

#ifdef CONFIG_ARM
		len = (unsigned long)stext - vaddr;
#endif
#ifdef CONFIG_ARM64
		len = SWAPPER_DIR_SIZE;
#endif
	ret = minidump_save_extend_information("kernel_pt", __pa(vaddr), __pa(vaddr + len));
	if (!ret)
		pr_info("kernel pt added to minidump section ok!!\n");
#ifdef CONFIG_ARM
	pr_info("pgd vaddr start: 0x%lx  paddr start: 0x%x  len :0x%x\n",
							vaddr, __pa(vaddr), len);
#endif
#ifdef CONFIG_ARM64
	pr_info("pgd vaddr start: 0x%lx  paddr start: 0x%llx  len :0x%x\n",
							vaddr, __pa(vaddr), len);
#endif
	return;
}

static int extend_section_cm4dump(void)
{
#define CM4_DUMP_IRAM "scproc"
	struct device_node *node;
	unsigned long cm4_dump_start, cm4_dump_end;
	struct resource res;
	int ret;

	node = of_find_node_by_name(NULL, CM4_DUMP_IRAM);
	if (!node) {
		pr_err("Not find %s from dts \n", CM4_DUMP_IRAM);
		return -1;
	} else {
		ret = of_address_to_resource(node, 0, &res);
		if (!ret) {
			cm4_dump_start = res.start;
			cm4_dump_end = res.end;
			pr_err("cm4_dump_start : 0x%lx cm4_dump_end :0x%lx , size : 0x%lx\n",
				cm4_dump_start, cm4_dump_end, cm4_dump_end - cm4_dump_start + 1);
		} else {
			pr_err("Not find cm4_reg property from %s node\n", CM4_DUMP_IRAM);
			return -1;
		}
	}
	minidump_save_extend_information(CM4_DUMP_IRAM, cm4_dump_start, cm4_dump_end);
	return 0;
}
/*	init section_info_all_item : name,paddr,size 	return: total_size */
static void section_extend_info_init(void)
{
	/* section info init */
	section_info_ylog_buf();
	section_info_pt();
#ifdef CONFIG_SPRD_NATIVE_HANG_MONITOR
	section_native_hang();
#endif

	extend_section_cm4dump();

	return;
}
static int ylog_buffer_open(struct inode *inode, struct file *file)
{
	pr_info("open ylog_buffer ok !\n");
	return 0;
}

static int ylog_buffer_map(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long ylog_buffer_paddr;

	if (vma->vm_end - vma->vm_start > YLOG_BUF_SIZE)
		return -EINVAL;

	ylog_buffer_paddr = virt_to_phys(ylog_buffer);
	if (remap_pfn_range(vma,
			vma->vm_start,
			ylog_buffer_paddr >> PAGE_SHIFT, /*	get pfn */
			vma->vm_end - vma->vm_start,
			vma->vm_page_prot))
		return -1;

	pr_info("mmap ylog_buffer ok !\n");
	return 0;
}

static const struct file_operations ylog_buffer_fops = {
	.owner = THIS_MODULE,
	.open = ylog_buffer_open,
	.mmap = ylog_buffer_map,
};

static struct miscdevice misc_dev_ylog = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME_YLOG,
	.fops = &ylog_buffer_fops,
};
static int ylog_buffer_init(void)
{
	int ret;

	ylog_buffer = kzalloc(YLOG_BUF_SIZE, GFP_KERNEL);
	if (ylog_buffer == NULL) {
		return -1;
	}
	pr_info("%s: ylog_buffer vaddr is %p\n", __func__, ylog_buffer);
	sprintf(ylog_buffer, "%s", "This is ylog buffer. Now , it is nothing . ");
	/*here, we can add something to head to check if data is ok */
	SetPageReserved(virt_to_page(ylog_buffer));
	ret = misc_register(&misc_dev_ylog);
	return ret;
}
static void ylog_buffer_exit(void)
{
	misc_deregister(&misc_dev_ylog);
	ClearPageReserved(virt_to_page(ylog_buffer));
	kfree(ylog_buffer);
}
int minidump_init(void)
{
	struct proc_dir_entry *minidump_info_dir;
	struct proc_dir_entry *minidump_info;

	minidump_info_dir = proc_mkdir(MINIDUMP_INFO_DIR, NULL);
	if (!minidump_info_dir)
		return -ENOMEM;
	minidump_info = proc_create(MINIDUMP_INFO_PROC, 0644, minidump_info_dir,
				&minidump_proc_fops);
	if (!minidump_info)
		return -ENOMEM;
	ylog_buffer_init();
	/*	dump_die_notifier for get infomation when die */
	if (register_die_notifier(&dump_die_notifier) != 0) {
		pr_err("register dump_die_notifyier failed.\n");
		return -1;
	}
	minidump_info_desc_g.paddr = __pa(&minidump_info_g);
	minidump_info_desc_g.size = sizeof(minidump_info_g);
	section_extend_info_init();
	pr_info("%s out.\n", __func__);
	return 0;
}
void show_exception_info(void)
{
	pr_debug("kernel_magic:             %s\n ",
		minidump_info_g.exception_info.kernel_magic);
	pr_debug("exception_serialno:  %s\n ",
		minidump_info_g.exception_info.exception_serialno);
	pr_debug("exception_kernel_version: %s\n ",
		minidump_info_g.exception_info.exception_kernel_version);
	pr_debug("exception_reboot_reason:  %s\n ",
		minidump_info_g.exception_info.exception_reboot_reason);
	pr_debug("exception_panic_reason:   %s\n ",
		minidump_info_g.exception_info.exception_panic_reason);
	pr_debug("exception_time:           %s\n ",
		minidump_info_g.exception_info.exception_time);
	pr_debug("exception_file_info:      %s\n ",
		minidump_info_g.exception_info.exception_file_info);
	pr_debug("exception_task_id:        %d\n ",
		minidump_info_g.exception_info.exception_task_id);
	pr_debug("exception_task_family:      %s\n ",
		minidump_info_g.exception_info.exception_task_family);
	pr_debug("exception_pc_symbol:      %s\n ",
		minidump_info_g.exception_info.exception_pc_symbol);
	pr_debug("exception_stack_info:     %s\n ",
		minidump_info_g.exception_info.exception_stack_info);
}
void get_file_line_info(struct pt_regs *regs)
{
	struct bug_entry *bug;
	const char *file = NULL;
	unsigned int line = 0;

	if (!regs || !is_valid_bugaddr(regs->reg_pc)) {
		snprintf(minidump_info_g.exception_info.exception_file_info,
			EXCEPTION_INFO_SIZE_SHORT, "not-bugon");
		pr_debug("no regs  or not a bugon,do nothing\n");
		return;
	}

	bug = find_bug(regs->reg_pc);
	if (!bug) {
		pr_debug("not a bugon, no  bug info ,do nothing\n");
		snprintf(minidump_info_g.exception_info.exception_file_info,
			EXCEPTION_INFO_SIZE_SHORT, "not-bugon");
		return;
	}
#ifdef CONFIG_DEBUG_BUGVERBOSE
#ifndef CONFIG_GENERIC_BUG_RELATIVE_POINTERS
	file = bug->file;
#else
	 file = (const char *)bug + bug->file_disp;
#endif
	line = bug->line;
#endif
	if (file) {
		snprintf(minidump_info_g.exception_info.exception_file_info,
			EXCEPTION_INFO_SIZE_SHORT, "[%s:%d]", file, line);
	} else {
		snprintf(minidump_info_g.exception_info.exception_file_info,
			EXCEPTION_INFO_SIZE_SHORT, "not-bugon");
		pr_debug("no file info ,do nothing\n");
	}
}
void get_exception_stack_info(struct pt_regs *regs)
{
	unsigned long stack_entries[MAX_STACK_TRACE_DEPTH];
	char symbol[96];
	int sz;
	int off, plen;
	struct stack_trace trace;
	int i;
	struct task_struct *tsk, *cur;

	cur = current;
	tsk = cur;
	if (!virt_addr_valid(tsk))
		return;

	/* Current panic user tasks */
	sz = 0;
	do {
		if (!tsk) {
			pr_err("No tsk info\n");
			break;
		}
		sz += snprintf(
			minidump_info_g.exception_info.exception_task_family + sz,
			EXCEPTION_INFO_SIZE_SHORT - sz,
			"[%s, %d]", tsk->comm, tsk->pid);
		tsk = tsk->real_parent;
	} while (tsk && (tsk->pid != 0) && (tsk->pid != 1));

	/* Grab kernel task stack trace */
	trace.nr_entries = 0;
	trace.max_entries = MAX_STACK_TRACE_DEPTH;
	trace.entries = stack_entries;
	trace.skip = 0;
	save_stack_trace_tsk(cur, &trace);
	for (i = 0; i < trace.nr_entries; i++) {
		off = strlen(minidump_info_g.exception_info.exception_stack_info);
		plen = EXCEPTION_INFO_SIZE_LONG - ALIGN(off, 8);
		if (plen > 16) {
			sz = snprintf(symbol, 96, "[<%p>] %pS\n",
				      (void *)stack_entries[i],
				      (void *)stack_entries[i]);
			if (ALIGN(sz, 8) - sz) {
				memset_io(symbol + sz - 1, ' ', ALIGN(sz, 8) - sz);
				memset_io(symbol + ALIGN(sz, 8) - 1, '\n', 1);
			}
			if (ALIGN(sz, 8) <= plen)
				memcpy(
				minidump_info_g.exception_info.exception_stack_info + ALIGN(off, 8),
				symbol, ALIGN(sz, 8));
		}
	}
	if (regs) {
		snprintf(minidump_info_g.exception_info.exception_pc_symbol,
			EXCEPTION_INFO_SIZE_SHORT, "[<%p>] %pS",
			(void *)(unsigned long)regs->reg_pc,
			(void *)(unsigned long)regs->reg_pc);
	} else {
		snprintf(minidump_info_g.exception_info.exception_pc_symbol,
			EXCEPTION_INFO_SIZE_SHORT, "[<%p>] %pS",
			(void *)(unsigned long)stack_entries[0],
			(void *)(unsigned long)stack_entries[0]);
	}
}
static int prepare_exception_info(struct pt_regs *regs,
				struct task_struct *tsk, const char *reason)
{

	struct timex txc;
	struct rtc_time tm;

	if (!tsk)
		tsk = current;
	memset(&minidump_info_g.exception_info, 0,
		sizeof(minidump_info_g.exception_info));
	memcpy(minidump_info_g.exception_info.kernel_magic, KERNEL_MAGIC, 4);

	/*	exception_kernel_version	*/
	memcpy(minidump_info_g.exception_info.exception_kernel_version,
		linux_banner,
		strlen(linux_banner));

	if (reason != NULL)
		memcpy(minidump_info_g.exception_info.exception_panic_reason,
			reason,
			strlen(reason));
	/*	exception_reboot_reason	 update in uboot */

	/*	exception_time		*/
	do_gettimeofday(&(txc.time));
	txc.time.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(txc.time.tv_sec, &tm);
	snprintf(minidump_info_g.exception_info.exception_time,
		EXCEPTION_INFO_SIZE_SHORT,
		"%04d-%02d-%02d:%02d:%02d:%02d",
		tm.tm_year + 1900,
		tm.tm_mon + 1,
		tm.tm_mday,
		tm.tm_hour,
		tm.tm_min,
		tm.tm_sec);

	/*	exception_file & exception_line		*/
	get_file_line_info(regs);
	/*	exception_task_id	*/
	minidump_info_g.exception_info.exception_task_id = tsk->pid;
	/*	exception_stack		*/
	get_exception_stack_info(regs);
	show_exception_info();
	return 0;
}

#ifdef CONFIG_SPRD_HANG_DEBUG
void prepare_dump_info_for_wdh(struct pt_regs *regs, const char *reason)
{
	/* 1    prepare minidump info */
	prepare_minidump_info(regs);
	prepare_exception_info(regs, NULL, reason);
}
EXPORT_SYMBOL(prepare_dump_info_for_wdh);
#endif /*	CONFIG_SPRD_HANG_DEBUG end*/
#endif  /*	minidump code end	*/

/* common data init */
static void section_info_per_cpu(void)
{
	int ret;
	long vaddr = (long)(__per_cpu_start) + (long)(__per_cpu_offset[0]);
	int len = (__per_cpu_offset[1] - __per_cpu_offset[0]) * CONFIG_NR_CPUS;

	ret = minidump_save_extend_information("per_cpu", __pa(vaddr), __pa(vaddr + len));
	if (!ret)
		pr_info("per_cpu added to minidump section ok!!\n");
}
static void section_info_log_buf(void)
{
	int ret;
	unsigned long vaddr = (unsigned long)(log_buf_addr_get());
	int len = log_buf_len_get();

	pr_info("%s in. vaddr : 0x%lx  len :0x%x\n",
		__func__, vaddr, len);
	ret = minidump_save_extend_information("log_buf", __pa(vaddr), __pa(vaddr + len));
	if (!ret)
		pr_info("add log_buf to minidump section ok!!\n");
}
static void minidump_addr_convert(int i)
{
	minidump_info_g.section_info_total.section_info[i].section_start_paddr =
		__pa(minidump_info_g.section_info_total.section_info[i].section_start_vaddr);
	minidump_info_g.section_info_total.section_info[i].section_end_paddr =
		__pa(minidump_info_g.section_info_total.section_info[i].section_end_vaddr);
	minidump_info_g.section_info_total.section_info[i].section_size =
		minidump_info_g.section_info_total.section_info[i].section_end_paddr -
		minidump_info_g.section_info_total.section_info[i].section_start_paddr;
}
static void minidump_info_init(void)
{
	int i;

	/* section_mutex only be inited once */
	if (atomic_read(&mutex_init_flag) == 0) {
		mutex_init(&section_mutex);
		atomic_inc(&mutex_init_flag);
	}
	/* section init */
	mutex_lock(&section_mutex);
	for (i = 0; i < SECTION_NUM_MAX; i++) {
		/* when section name is null, break */
		if (!strlen(minidump_info_g.section_info_total.section_info[i].section_name))
			break;
		minidump_addr_convert(i);
		minidump_info_g.section_info_total.total_size +=
			minidump_info_g.section_info_total.section_info[i].section_size;
	}
	minidump_info_g.section_info_total.total_num = i;
	/* vaddr to paddr flag must nearby section info init */
	vaddr_to_paddr_flag = 1;
	mutex_unlock(&section_mutex);

	/* regs init */
	minidump_info_g.regs_info.paddr = __pa(&minidump_regs_g);
	/* regs_memory_info init*/
	minidump_info_g.regs_memory_info.size = REGS_NUM_MAX *
			minidump_info_g.regs_memory_info.per_reg_memory_size;
	/* update minidump data size */
	minidump_info_g.minidump_data_size = minidump_info_g.regs_info.size +
			minidump_info_g.regs_memory_info.size +
			minidump_info_g.section_info_total.total_size;

	section_info_log_buf();
	section_info_per_cpu();
	/* add etb section */
	minidump_save_extend_information("etb_data", 0, 0);
	/* update minidump info */
	if (sysdump_reflag && sprd_sysdump_info_paddr) {
		sprd_sysdump_info->sprd_minidump_info.minidump_info_paddr = __pa(&minidump_info_g);
		sprd_sysdump_info->sprd_minidump_info.minidump_info_size = sizeof(minidump_info_g);
		memcpy(sprd_sysdump_info->sprd_minidump_info.magic, MINIDUMP_MAGIC,
									sizeof(MINIDUMP_MAGIC));
	}

}

static struct notifier_block sysdump_panic_event_nb = {
	.notifier_call	= sysdump_panic_event,
	.priority	= INT_MAX - 2,
};

static int sysdump_panic_event_init(void)
{
	crash_notes = &crash_notes_temp;
	/* register sysdump panic notifier */
	atomic_notifier_chain_register(&panic_notifier_list,
						&sysdump_panic_event_nb);
	return 0;
}
static __init int sysdump_early_init(void)
{
	int ret;
	/* register sysdump panic notifier */
	sysdump_panic_event_init();
	/* init kaslr_offset if need */
	ret = sysdump_info_init();
	if (ret)
		pr_emerg("sysdump_info init failed!!!\n");

	minidump_info_init();

	return 0;
}
early_initcall(sysdump_early_init);
static void per_cpu_funcs_register(void *info)
{
	/* save mmu regs per cpu */
	sprd_debug_save_mmu_reg(&per_cpu(sprd_debug_mmu_reg, smp_processor_id()));
}
static __init int per_cpu_funcs_init(void)
{
	int ret;

	/* mmu regs init in all other cpus */
	ret = smp_call_function(per_cpu_funcs_register, NULL, 1);
	/* mmu regs init in current cpu*/
	sprd_debug_save_mmu_reg(&per_cpu(sprd_debug_mmu_reg, smp_processor_id()));

	if (!ret)
		pr_info("per cpu funcs init ok!\n");
	return ret;
}
pure_initcall(per_cpu_funcs_init);
static int sysdump_sysctl_init(void)
{
	struct proc_dir_entry *sysdump_proc;
	sysdump_sysctl_hdr =
	    register_sysctl_table((struct ctl_table *)sysdump_sysctl_root);
	if (!sysdump_sysctl_hdr)
		return -ENOMEM;

	if (input_register_handler(&sysdump_handler))
		pr_err("regist sysdump_handler failed.\n");

	sysdump_proc = proc_create("sprd_sysdump", S_IWUSR | S_IRUSR, NULL, &sysdump_proc_fops);
	if (!sysdump_proc)
		return -ENOMEM;

	memset(g_ktxt_hash_data, 0x55, SHA1_DIGEST_SIZE);
	if (sysdump_shash_init())
		return -ENOMEM;

	sprd_sysdump_enable_prepare();
#if defined(CONFIG_SPRD_DEBUG)
	pr_info("userdebug enable sysdump in default !!!\n");
	set_sysdump_enable(1);
#endif
#ifdef CONFIG_SPRD_MINI_SYSDUMP
	minidump_init();
#endif
	return 0;
}
void sysdump_sysctl_exit(void)
{
	if (sysdump_sysctl_hdr)
		unregister_sysctl_table(sysdump_sysctl_hdr);
	input_unregister_handler(&sysdump_handler);
	remove_proc_entry("sprd_sysdump", NULL);
	if (desc) {
		if (desc->tfm)
			crypto_free_shash(desc->tfm);
		kfree(desc);
	}
#ifdef CONFIG_SPRD_MINI_SYSDUMP
	ylog_buffer_exit();
#endif
}
late_initcall_sync(sysdump_sysctl_init);
module_exit(sysdump_sysctl_exit);

MODULE_AUTHOR("Jianjun.He <jianjun.he@spreadtrum.com>");
MODULE_DESCRIPTION("kernel core dump for Spreadtrum");
MODULE_LICENSE("GPL");
