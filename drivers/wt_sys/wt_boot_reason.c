/**
 * @file wt_boot_reason.c
 *
 * Add for wt boot reason feature
 *
 * Copyright (C) 2020 Wingtech.Co.Ltd. All rights reserved.
 *
 * =============================================================================
 *                             EDIT HISTORY
 *
 * when         who          what, where, why
 * ----------   --------     ---------------------------------------------------
 * 2020/03/13   wanghui      Initial Revision
 *
 * =============================================================================
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/kallsyms.h>
#include <soc/qcom/subsystem_restart.h>
#include <wt_sys/wt_sys_config.h>
#include <wt_sys/wt_boot_reason.h>

/* please must align in 8 bytes */
struct wt_boot_reason_header {
	uint32_t magic;
	uint32_t initial;
	uint64_t log_addr;
	uint32_t log_size;
	uint32_t log_crc;
	uint32_t boot_reason;
	uint8_t  boot_reason_str[32];
	uint8_t  reserved[4];
}__attribute__((aligned(8)));

struct bootreason_t {
	uint32_t reason_num;
	const char *reason_str;
};

// if subsystem changed, it need modify
static struct bootreason_t wt_subsystem_reason_map[] = {
	{RESET_MAGIC_MODEM,     "modem"},
	{RESET_MAGIC_ADSP,      "adsp"},
	{RESET_MAGIC_CDSP,      "cdsp"},
	{RESET_MAGIC_WLAN,      "wlan"},
	{RESET_MAGIC_VENUS,     "venus"},
	{RESET_MAGIC_WCNSS,     "wcnss"},
	{RESET_MAGIC_AXXX_ZAP,  "a610_zap"},
	{RESET_MAGIC_NPU,       "npu"},
	{RESET_MAGIC_IPA_FWS,   "ipa_fws"},
	{RESET_MAGIC_SPSS,      "spss"},
	{RESET_MAGIC_APSS,      "apss"},
	{RESET_MAGIC_CVPSS,     "cvpss"},
	{RESET_MAGIC_SLPI,      "slpi"},
	{RESET_MAGIC_IPA_UC,    "ipa_uc"},
	{RESET_MAGIC_ESOC,      "esoc0"}
};

#define WT_BOOT_REASON_MAGIC      0x4D524257  /* W B R M */
#define WT_BOOT_REASON_MSG        "[wt_boot_reason]: "
#define XBL_RAMDUMP_DISPLAY_INFO  // if not please remove it
static void *wt_reset_reason_addr = NULL;
static void *tz_reset_reason_addr = NULL;

#define WT_MAX_SSR_REASON_LEN 256U
static int wt_btreason_oops = 0;
static char wt_btreason_subsys_failure_log[WT_MAX_SSR_REASON_LEN];

static DEFINE_SPINLOCK(wt_brlog_lock);
static char *wt_brlog_ptr = NULL;
static struct wt_boot_reason_header *wt_brlog_head = NULL;

int wt_btreason_log_save(const char *fmt, ...)
{
	char buffer[256] = {0};
	static char *logbuf_ptr = NULL;
	va_list args;
	size_t nbytes;
	unsigned long flags;

	spin_lock_irqsave(&wt_brlog_lock, flags);
	if (!wt_brlog_ptr || !wt_brlog_head) {
		spin_unlock_irqrestore(&wt_brlog_lock, flags);
		return -1;
	}
	if(!logbuf_ptr) {
		logbuf_ptr = wt_brlog_ptr;
	}

	va_start(args, fmt);
	nbytes = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (wt_brlog_head->magic != WT_BOOT_REASON_MAGIC) {
		spin_unlock_irqrestore(&wt_brlog_lock, flags);
		return -1;
	}
	if ((wt_brlog_head->log_size + strlen(buffer) + sizeof(struct wt_boot_reason_header)) \
			> WT_BOOT_REASON_SIZE) {
		wt_brlog_head->log_size = 0;
		logbuf_ptr = wt_brlog_ptr;
	}
#ifdef XBL_RAMDUMP_DISPLAY_INFO
	if (buffer[strlen(buffer) - 1] == '\n') {
		buffer[strlen(buffer)] = '\r';
	}
#endif
	wt_brlog_head->log_size += strlen(buffer);
	memcpy(logbuf_ptr, buffer, strlen(buffer));
	wt_brlog_head->log_crc = (uint32_t)((wt_brlog_head->magic | wt_brlog_head->log_addr | wt_brlog_head->log_addr >> 32) ^ wt_brlog_head->log_size);
	logbuf_ptr += strlen(buffer);
	spin_unlock_irqrestore(&wt_brlog_lock, flags);
	return 0;
}

static void wt_btreason_log_save_symbol(const char *fmt, unsigned long address)
{
		char buffer[KSYM_SYMBOL_LEN];

		sprint_symbol(buffer, address);

		wt_btreason_log_save(fmt, buffer);
}

void wt_brlog_save_pc_lr_value(uint64_t pc, uint64_t lr)
{
	if (wt_btreason_oops == 1) {
		wt_btreason_log_save_symbol("PC is at %s,", pc);
		wt_btreason_log_save(" [<%016llx>] \n", pc);
		wt_btreason_log_save_symbol("LR is at %s,", lr);
		wt_btreason_log_save(" [<%016llx>] \n", lr);
		wt_btreason_oops = 0;
	}
}

void wt_btreason_set_oops(int panic_oops)
{
	wt_btreason_oops = panic_oops;
}

static void wt_uint32_to_4char(uint32_t a)
{
	int i;
	char c[5] = {0};
	for (i = 0; i < 4; ++i) {
		c[i] = (char)a;
		a = a >> 8;
	}
	printk(WT_BOOT_REASON_MSG"wt_btreason magic:%s\n", c);
}

static void wt_btreason_print_magic(uint32_t a)
{
	if (a < 128) {
		printk(WT_BOOT_REASON_MSG"tz reset magic: %d\n", a);
	} else {
		wt_uint32_to_4char(a);
	}
}

static void wt_btreason_raw_writel(uint32_t value, void *p_addr)
{
	wt_btreason_print_magic(value);
	if (NULL == p_addr)
		return;
	__raw_writel(value, p_addr);
	return;
}

#if 0
static unsigned long wt_btreason_raw_readl(void *p_addr)
{
	uint32_t return_value = 0;
	if (p_addr == NULL)
		return return_value;
	return_value = __raw_readl(p_addr);
	//wt_btreason_print_magic(return_value);
	return return_value;
}
#endif

void wt_btreason_set_reset_magic(uint32_t magic_number)
{
	static int set_flag = 0;

	if (!set_flag) {
		wt_btreason_raw_writel(magic_number, wt_reset_reason_addr);
		mb();
		if (magic_number != RESET_MAGIC_INIT)
			set_flag = 1;
	}
}

void wt_btreason_clear_reset_magic(void)
{
	wt_btreason_raw_writel(RESET_MAGIC_INIT, wt_reset_reason_addr);
	mb();
}

void wt_btreason_set_subsystem_magic(const char *name, int restart_level)
{
	int i, magic_flag = 0;

	if (restart_level == RESET_SOC) {
		for (i = 0; i < sizeof(wt_subsystem_reason_map) / sizeof(struct bootreason_t); ++i) {
			if (!strcmp(name, wt_subsystem_reason_map[i].reason_str)) {
				wt_btreason_set_reset_magic(wt_subsystem_reason_map[i].reason_num);
				magic_flag = 1;
				break;
			}
		}
		if (!magic_flag) {
			wt_btreason_set_reset_magic(RESET_MAGIC_SUBSYSTEM);
		}
		wt_btreason_log_save("%s subsystem failure reason: %s.\n", name, wt_btreason_subsys_failure_log);
	}
}

void wt_btreason_subsystem_failure_log(char *reason, size_t size)
{
	strlcpy(wt_btreason_subsys_failure_log, reason, min(size, (size_t)WT_MAX_SSR_REASON_LEN));
}

static int wt_btreason_panic_handler(struct notifier_block *this, unsigned long events, void *ptr)
{
#ifdef CONFIG_PREEMPT
	preempt_count_add(NMI_MASK);
#endif
	wt_btreason_set_reset_magic(RESET_MAGIC_PANIC);
#ifdef CONFIG_PREEMPT
	preempt_count_sub(NMI_MASK);
#endif
	return NOTIFY_DONE;
}

static struct notifier_block wt_btreason_panic_event_nb = {
	.notifier_call = wt_btreason_panic_handler,
	.priority = INT_MAX,
};

static int wt_btreason_register_panic_notifier(void)
{
	return atomic_notifier_chain_register(&panic_notifier_list, &wt_btreason_panic_event_nb);
}

static __init int wt_boot_reason_cmdline(char *cmdline)
{
	printk(KERN_ERR"[wt_boot_reason]:%s\n", cmdline);
	return 0;
}
__setup("androidboot.wt_boot_reason=", wt_boot_reason_cmdline);

static int wt_reset_reason_address_get(void)
{
	struct device_node *np;
	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-wt_reset_reason_addr");
	if (!np) {
		printk(KERN_ERR WT_BOOT_REASON_MSG"unable to find DT imem wt_reset_reason_addr node\n");
		return -1;
	}
	wt_reset_reason_addr = of_iomap(np, 0);
	if (!wt_reset_reason_addr) {
		printk(KERN_ERR WT_BOOT_REASON_MSG"unable to map imem wt_reset_reason_addr offset\n");
		return -1;
	}
	return 0;
}

static int tz_reset_reason_address_get(void)
{
	struct device_node *np;
	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-tz_reset_reason_addr");
	if (!np) {
		printk(KERN_ERR WT_BOOT_REASON_MSG"unable to find DT imem tz_reset_reason_addr node\n");
		return -1;
	}
	tz_reset_reason_addr = of_iomap(np, 0);
	if (!tz_reset_reason_addr) {
		printk(KERN_ERR WT_BOOT_REASON_MSG"unable to map imem tz_reset_reason_addr offset\n");
		return -1;
	}
	return 0;
}

static int wt_boot_reason_init_memory(void)
{
	int ret = 0;
	unsigned long flags;
	unsigned long wt_brlog_addr = 0;
	unsigned long wt_brlog_size = 0;
	void *head_addr = NULL;
	struct device_node *wt_brlog_mem_dts_node = NULL;
	const uint32_t *wt_brlog_mem_dts_basep = NULL;

	wt_brlog_mem_dts_node = of_find_compatible_node(NULL, NULL, "wt_brlog_mem");
	if (wt_brlog_mem_dts_node == 0) {
		printk(KERN_ERR"of_find_compatible_node error!\n");
		return -1;
	}
	wt_brlog_mem_dts_basep = of_get_address(wt_brlog_mem_dts_node, 0, (u64 *)&wt_brlog_size, NULL);
	wt_brlog_addr = (unsigned long)of_translate_address(wt_brlog_mem_dts_node, wt_brlog_mem_dts_basep);
	//printk(KERN_ERR"wt_brlog_addr:0x%lx wt_brlog_size:0x%lx\n", wt_brlog_addr, wt_brlog_size);

	if (wt_brlog_addr == 0 || wt_brlog_size > WT_BOOT_REASON_SIZE) {
		printk(KERN_ERR"wt_brlog_addr error!\n");
		return -1;
	}

	#ifdef CONFIG_ARM
		head_addr = (void *)ioremap_nocache(wt_brlog_addr, wt_brlog_size);
	#else
		head_addr = (void *)ioremap_wc(wt_brlog_addr, wt_brlog_size);
	#endif

	if (head_addr == NULL) {
		printk(KERN_ERR"wt_brlog_addr ioremap error!\n");
		return -1;
	}
	wt_brlog_head = (struct wt_boot_reason_header *)head_addr;
	if (wt_brlog_head->magic != WT_BOOT_REASON_MAGIC) {
		printk(KERN_ERR"wt boot reason magic not match!\n");
		return -1;
	}
	if (wt_brlog_head->log_crc == (uint32_t)((wt_brlog_head->magic | wt_brlog_head->log_addr | wt_brlog_head->log_addr >> 32) ^ wt_brlog_head->log_size)) {
		spin_lock_irqsave(&wt_brlog_lock, flags);
		wt_brlog_ptr = (char *)wt_brlog_head + sizeof(struct wt_boot_reason_header);
		spin_unlock_irqrestore(&wt_brlog_lock, flags);
	}

	ret = wt_reset_reason_address_get();
	if (ret)
		return -1;
	wt_btreason_clear_reset_magic();
	ret = tz_reset_reason_address_get();
	if (ret)
		return -1;
	return ret;
}

static int __init wt_boot_reason_init(void)
{
	int ret = 0;

	ret = wt_boot_reason_init_memory();
	if (ret) {
		printk(KERN_ERR"wt_boot_reason_init_memory error!\n");
		return -1;
	}
	ret = wt_btreason_register_panic_notifier();
	return ret;
}

module_init(wt_boot_reason_init);
MODULE_LICENSE("GPL");
