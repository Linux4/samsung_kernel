#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <linux/stat.h>

enum sprd_debug_type{
	SCHE,
	MEM,
	COMM,
	TIMER,
	IRQ,
	IO,
	CPU,
	MISC,
	TYPE_COUNT
};

struct sprd_debug_dir {
	char *name;
	struct dentry *debugfs_entry;
};

struct sprd_debug_dir sprd_debug_dir_table[TYPE_COUNT] = {
	{.name = "sche", .debugfs_entry = NULL},
	{.name = "mem", .debugfs_entry = NULL},
	{.name = "comm", .debugfs_entry = NULL},
	{.name = "timer", .debugfs_entry = NULL},
	{.name = "irq", .debugfs_entry = NULL},
	{.name = "io", .debugfs_entry = NULL},
	{.name = "cpu", .debugfs_entry = NULL},
	{.name = "misc", .debugfs_entry = NULL},
};

struct sprd_debug_entry {
	enum sprd_debug_type type;
	char *name;
	mode_t mode;
	struct file_operations *fop;
};

#define NOD(TYPE, NAME, MODE, FOP) {  	\
	.type = TYPE,			\
	.name = NAME,			\
	.mode = MODE,			\
	.fop = FOP}

#ifdef CONFIG_PHYMEM_INFO
extern struct file_operations phymem_dist_proc_fops;
extern struct file_operations phymem_pages_proc_fops;
extern struct file_operations phymem_map_proc_fops;
#endif

#ifdef CONFIG_USRPROCMEM_INFO
extern struct file_operations user_process_meminfo_fops;
#endif

#ifdef CONFIG_SPRD_DEBUG_SCHED_LOG
extern struct file_operations scheinfo_proc_fops;
extern struct file_operations scheinfo_num_proc_fops;
extern struct file_operations irqinfo_proc_fops;
extern struct file_operations irqinfo_num_proc_fops;
extern struct file_operations last_regs_info_proc_fops;
extern struct file_operations userstack_info_proc_fops;
#endif

#ifdef CONFIG_SPRD_CPU_RATE
extern struct file_operations proc_cpu_usage_fops;
#endif

static struct sprd_debug_entry sprd_debug_array[] = {
#ifdef CONFIG_PHYMEM_INFO
	NOD(MEM,   "phymemdist", 0, &phymem_dist_proc_fops),
	NOD(MEM,   "phymempages", 0, &phymem_pages_proc_fops),
	NOD(MEM,   "phymemmap", 0, &phymem_map_proc_fops),
#endif

#ifdef CONFIG_USRPROCMEM_INFO
	NOD(MEM,   "userprocmem", 0, &user_process_meminfo_fops),
	NOD(MEM,   "userthreadstack", 0, &userstack_info_proc_fops),
#endif

#ifdef CONFIG_SPRD_DEBUG_SCHED_LOG
	NOD(SCHE,  "schedlog", 0444 , &scheinfo_proc_fops),
	NOD(SCHE,  "schedlog_num", 0, &scheinfo_num_proc_fops),
	NOD(IRQ,   "irqlog", 0, &irqinfo_proc_fops),
	NOD(IRQ,   "irqlog_num", 0, &irqinfo_num_proc_fops),
	NOD(IO,    "last_regs", 0, &last_regs_info_proc_fops),
#endif

#ifdef CONFIG_SPRD_CPU_RATE
	NOD(CPU,    "cpu_usage", 0444, &proc_cpu_usage_fops),
#endif
};

static struct dentry *debugfs_sprd_debug_root;

static void _sprd_entry_create(struct sprd_debug_entry *psprd_debug)
{
	debugfs_create_file(psprd_debug->name, psprd_debug->mode,
					sprd_debug_dir_table[psprd_debug->type].debugfs_entry, NULL, psprd_debug->fop);
}

static void _sprd_debug_init(void)
{
	int i;
	int num = sizeof(sprd_debug_array) / sizeof(struct sprd_debug_entry);

	for (i = 0; i < num; i++)
		_sprd_entry_create(&sprd_debug_array[i]);

}

static void _sprd_dir_init(void)
{
	int i;

	debugfs_sprd_debug_root = debugfs_create_dir("sprd_debug", NULL);
	for (i = 0; i < TYPE_COUNT; i++)
		sprd_debug_dir_table[i].debugfs_entry =  debugfs_create_dir(sprd_debug_dir_table[i].name, debugfs_sprd_debug_root);
}

static int _sprd_debug_module_init(void)
{
	_sprd_dir_init();
	_sprd_debug_init();
	return 0;
}

module_init(_sprd_debug_module_init);

