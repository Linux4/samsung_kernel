/*
** (C) Copyright 2011 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/cpufreq.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/mqueue.h>
#include <linux/ipc_namespace.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/traps.h>
#include <asm/cacheflush.h>

#include "ufunc_hook.h"
#include "ufunc_hook_internal.h"
#include "common.h"

//static LIST_HEAD(ufhook_hook);
static DEFINE_SPINLOCK(uhook_lock);
static bool uhooks_initialized = false;

#define TID_HASH_SIZE 100
static struct hlist_head uhook_thread_hash[TID_HASH_SIZE];

#define PID_HASH_SIZE 100
static struct hlist_head uhook_process_hash[PID_HASH_SIZE];

//static DEFINE_MUTEX(uhook_mutex);

extern int arm_displace_inst(struct pt_regs * regs, struct displaced_desc * dsc);
extern int thumb_displace_inst(struct pt_regs * regs, struct displaced_desc * dsc);

static REGISTER_UNDEF_FUNC   register_undef_func;
static UNREGISTER_UNDEF_FUNC unregister_undef_func;

static int lookup_symbol_in_proc_modules(struct task_struct * task, const char *sym_name, Elf32_Sym *sym, struct elf_module **module);
static struct uhook_process * add_uhook_process(struct task_struct * task);

static ACCESS_PROCESS_VM_FUNC access_process_vm_func;

#define PX_BUG() do { printk(KERN_ALERT "PX_BUG: %s %d\n", __FILE__, __LINE__);} while(0);

/* 
 * read data from the specific process virtual memory
 */
int read_proc_vm(struct task_struct * task, unsigned long address, void * data, int size)
{
	int ret;

	if (task == NULL)
	{
		return -EINVAL;
	}

	if (task == current)
	{
		int nleft;

		nleft = copy_from_user(data, (void *)address, size);

		return size - nleft;
	}
	else
	{
		if (access_process_vm_func == NULL)
		{
			return -EFAULT;
		}

		ret = access_process_vm_func(task, address, data, size, 0);
		
		return ret;
	}
}

/* 
 * write data to the specific process virtual memory
 */
int write_proc_vm(struct task_struct * task, unsigned long address, void * data, int size)
{
#if 1
	int ret;

	if (task == NULL)
	{
		return -EINVAL;
	}

	if (access_process_vm_func == NULL)
	{
		return -EFAULT;
	}

	ret = access_process_vm_func(task, address, data, size, 1);
	
	return ret;
#else
	write_opcode(task, address, data, size);
#endif
}

static unsigned int find_rdebug(struct task_struct *task, unsigned int address)
{
	Elf32_Ehdr elf_hdr;
	Elf32_Phdr phy_hdr;
	Elf32_Dyn  dyn;
	int i;
	unsigned int offset;


	if (read_proc_vm(task, address, &elf_hdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr))
		return INVALID_ADDRESS;

	for (i=0; i<elf_hdr.e_phnum; i++)
	{
		unsigned int addr;

		addr = address + elf_hdr.e_phoff + i * sizeof(Elf32_Phdr);

		read_proc_vm(task, addr, &phy_hdr, sizeof(Elf32_Phdr));

		if (phy_hdr.p_type == PT_DYNAMIC)
			break;
	}

	if (i >= elf_hdr.e_phnum)
		return INVALID_ADDRESS;

	offset = 0;

	while (offset < phy_hdr.p_memsz)
	{
		if (read_proc_vm(task, phy_hdr.p_vaddr + offset, &dyn, sizeof(Elf32_Dyn)) != sizeof(Elf32_Dyn))
		{
			return INVALID_ADDRESS;
		}

		if (dyn.d_tag == DT_DEBUG)
		{
			break;
		}

		offset += sizeof(Elf32_Dyn);
	}

	if (offset >= phy_hdr.p_memsz)
		return INVALID_ADDRESS;

	return dyn.d_un.d_ptr;
}

static unsigned int bkpt_addr_hash_func(unsigned int address)
{
	return address % BKPT_HASH_SIZE;
}

static unsigned int module_addr_hash_func(unsigned int address)
{
	return address % MODULE_HASH_SIZE;
}

static unsigned int module_hash_func(struct elf_module *module)
{
	return module_addr_hash_func(module->address);
}

static bool is_arm_address(unsigned int address)
{
	if (address & 0x1)
		return false;
	else
		return true;
}

static bool is_thumb_address(unsigned int address)
{
	if (address & 0x1)
		return true;
	else
		return false;
}

static int free_module(struct elf_module *module)
{
	if (module == NULL)
		return -EINVAL;

	if (module->name != NULL)
	{
		kfree(module->name);
		module->name = NULL;
	}

	if (module->dl_name != NULL)
	{
		kfree(module->dl_name);
		module->dl_name = NULL;
	}

	return 0;
}

#if 0
static int sym_hash_func(const char *sym_name)
{
	return sym_name[0] % SYM_HASH_SIZE;
}
#endif

/* check the address is in the range of the module */
static bool in_module(struct elf_module *module, unsigned int address)
{
	if (module == NULL)
		return false;

	if ((address >= module->address) && (address < module->address + module->size))
		return true;

	return false;
}

static int pid_hash_func(pid_t pid)
{
	return pid % PID_HASH_SIZE;
}

static struct uhook_process * find_uhook_process(struct task_struct * task)
{
	struct hlist_head    * head;
	struct hlist_node    * node;
	struct uhook_process * proc;

	unsigned int pid_key;

	if (task == NULL)
		return NULL;

	pid_key = pid_hash_func(task->tgid);
	head = &uhook_process_hash[pid_key];

	if (hlist_empty(head))
		return NULL;

	hlist_for_each_entry(proc, node, head, link)
	{
		if ((proc->task->tgid == task->tgid) && (proc->task->group_leader == task->group_leader))
		{
			return proc;
		}
	}

	return NULL;
}

static struct module_info * find_module_from_addr(struct task_struct * task, unsigned int address)
{
	unsigned int module_key;

	struct hlist_head    * head;
	struct hlist_node    * node;
	struct uhook_process * proc;
	struct module_info   * mi;

	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		return NULL;
	}

	module_key = module_addr_hash_func(address);

	head = &proc->module_info_hash[module_key];

	if (hlist_empty(head))
		return NULL;

	hlist_for_each_entry(mi, node, head, link)
	{
		if (mi->module->address == address)
		{
			return mi;
		}
	}

	return NULL;
}


static struct module_info * find_elf_module(struct elf_module *module)
{
	if ((module == NULL) || (module->task == NULL))
		return NULL;

	return find_module_from_addr(module->task, module->address);
}

static int add_elf_module(struct elf_module *module)
{
	unsigned int module_key;

	struct uhook_process * proc;
	struct module_info   * mi;

	struct hlist_head *head;

	if (module == NULL)
		return -EINVAL;

	if (module->task == NULL)
		return -EINVAL;

	module_key = module_hash_func(module);

	proc = find_uhook_process(module->task);

	if (proc == NULL)
	{
		return -EFAULT;
	}

	if (find_elf_module(module) != NULL)
	{
		return -EALREADY;
	}

	mi = kzalloc(sizeof(struct module_info), GFP_ATOMIC);

	if (mi == NULL)
	{
		return -ENOMEM;
	}

	mi->module = module;

	head = &proc->module_info_hash[module_key];

	hlist_add_head(&mi->link, head);

	if (mi->module->is_exe)
	{
		proc->exe_info = mi;
	}

	return 0;
}

static int remove_uhook_module(struct module_info *mi)
{
	unsigned int module_key;
	struct uhook_process * proc;
	struct module_info   * m;
	struct hlist_node *node, *tmp;
	struct hlist_head *head;

	if ((mi == NULL) || (mi->module == NULL))
		return -EINVAL;

	proc = find_uhook_process(mi->module->task);

	if (proc == NULL)
	{
		return -EINVAL;
	}

	module_key = module_hash_func(mi->module);
	head = &proc->module_info_hash[module_key];

	hlist_for_each_entry_safe(m, node, tmp, head, link)
	{
		if (m == mi)
		{
			free_module(mi->module);
			kfree(mi->module);
			hlist_del(&mi->link);
			kfree(mi);
			return 0;
		}
	}

	return -EINVAL;
}

static unsigned int get_arm_bp_inst(enum FUNC_CODE func_code)
{
	return (TP_BP_ARM_MASK | func_code);
}

static unsigned int get_thumb_bp_inst(enum FUNC_CODE func_code)
{
	return (TP_BP_THUMB_MASK | func_code);
}

static unsigned int get_bp_inst(enum FUNC_CODE func_code, unsigned int address)
{
	if (is_arm_address(address))
		return get_arm_bp_inst(func_code);
	else
		return get_thumb_bp_inst(func_code);
}


static bool is_arm_bp_inst(unsigned int inst)
{
	if ((inst & TP_BP_ARM_MASK) == TP_BP_ARM_MASK)
		return true;

	return false;
}

static bool is_thumb_bp_inst(unsigned int inst)
{
	if ((inst & TP_BP_THUMB_MASK) == TP_BP_THUMB_MASK)
		return true;

	return false;
}

static bool is_bp_inst(unsigned int inst, unsigned address)
{
	if (is_arm_address(address))
		return is_arm_bp_inst(inst);
	else
		return is_thumb_bp_inst(inst);
}

int get_inst_size(struct task_struct *task, unsigned int address)
{
	unsigned int data;

	if (!is_arm_address(address))
	{
		/* Thumb instruction */
		data = 0;

		read_proc_vm(task, address & ~0x1, &data, 2);

		if (((data & 0xe000) == 0xe000) && ((data & 0x1800) != 0))
		{
			/* it is a Thumb 32 bit instruction */
			return THUMB32_INST_SIZE;
		}
		else
		{
			/* it is a Thumb 16 bit instruction */
			return THUMB16_INST_SIZE;
		}
	}
	else
	{
		/* ARM instruction */
		return ARM_INST_SIZE;
	}

}

static int read_inst_at(struct task_struct *task, unsigned int address, unsigned int *inst, int *inst_size)
{
	unsigned int data;
	int size;

	if (task == NULL)
		return -EINVAL;

	data = 0;
	size = get_inst_size(task, address);

	if (read_proc_vm(task, address & ~0x1, &data, size) != size)
		return -EFAULT;

	if (inst != NULL)
	{
		*inst = 0;
		memcpy(inst, &data, size);
	}

	if (inst_size != NULL)
	{
		*inst_size = 0;
		memcpy(inst_size, &size, sizeof(size));
	}

	return 0;
}

static struct uhook_breakpoint * find_uhook_breakpoint(struct task_struct * task, unsigned int address)
{
	unsigned int key;
	struct uhook_process * proc;
	struct uhook_breakpoint * bkpt = NULL;
	struct hlist_node *n;
	struct hlist_head *head;

	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		return NULL;
	}

	key = bkpt_addr_hash_func(address);

	if (hlist_empty(&proc->bkpt_list[key]))
	{
		return NULL;
	}

	head = &proc->bkpt_list[key];

	hlist_for_each_entry(bkpt, n, head, link)
	{
		if (bkpt->address == address)
		{
			return bkpt;
		}
	}

	return NULL;
}

static int restore_inst_at(struct task_struct *task, unsigned int address, unsigned int * inst, unsigned int * inst_size)
{
	struct uhook_breakpoint * bkpt;
	unsigned int orig_inst;
	unsigned int orig_inst_size;

	bkpt = find_uhook_breakpoint(task, address);

	if (bkpt == NULL)
	{
		return -EINVAL;
	}

	orig_inst      = bkpt->orig_inst;
	orig_inst_size = bkpt->orig_inst_size;

	if (write_proc_vm(task, address & ~0x1, &orig_inst, orig_inst_size) != orig_inst_size)
	{
		return -EFAULT;
	}

	if (inst != NULL)
	{
		*inst = 0;
		memcpy(inst, &orig_inst, orig_inst_size);
	}

	if (inst_size != NULL)
	{
		*inst_size = 0;
		memcpy(inst_size, &orig_inst_size, sizeof(orig_inst_size));
	}

	return 0;
}


static int remove_uhook_breakpoint(struct uhook_process * proc, unsigned int address, bool restore_inst)
{
	struct uhook_breakpoint * bkpt;

	bkpt = find_uhook_breakpoint(proc->task, address);

	if (bkpt == NULL)
	{
		return -EINVAL;
	}

	if (restore_inst)
	{
		restore_inst_at(proc->task, address, NULL, NULL);
	}

	hlist_del(&bkpt->link);
	kfree(bkpt);

	return 0;
}

static struct uhook_breakpoint * copy_uhook_breakpoint_from_parent(struct uhook_process * proc, struct uhook_breakpoint * bkpt)
{
	int key;
	unsigned bkpt_inst;
	struct uhook_breakpoint * new_bkpt;

	if ((proc == NULL) || (bkpt == NULL))
		return NULL;

	bkpt_inst = get_bp_inst(bkpt->func_code, bkpt->address);

	new_bkpt = kzalloc(sizeof(struct uhook_breakpoint), GFP_ATOMIC);

	if (new_bkpt == NULL)
	{
		return NULL;
	}

	if (is_thumb_address(bkpt->address))
	{
		if (write_proc_vm(proc->task, bkpt->address & ~0x1, &bkpt_inst, THUMB16_INST_SIZE) != THUMB16_INST_SIZE)
		{
			return NULL;
		}
	}
	else
	{
		if (write_proc_vm(proc->task, bkpt->address, &bkpt_inst, ARM_INST_SIZE) != ARM_INST_SIZE)
		{
			return NULL;
		}
	}

	/* initialize the uhook_breakpoint structure */
	new_bkpt->func_name      = bkpt->func_name;
	new_bkpt->func_code      = bkpt->func_code;
	new_bkpt->address        = bkpt->address;
	new_bkpt->orig_inst      = bkpt->orig_inst;
	new_bkpt->orig_inst_size = bkpt->orig_inst_size;

	/* TODO */
	new_bkpt->displaced_inst_addr = bkpt->displaced_inst_addr;
	new_bkpt->displaced_inst_size = bkpt->displaced_inst_size;
	new_bkpt->pre_handler         = bkpt->pre_handler;
	new_bkpt->post_handler        = bkpt->post_handler;
	new_bkpt->return_handler      = bkpt->return_handler;

	/* insert the breakpoint into the process breakpoint list */
	key = bkpt_addr_hash_func(bkpt->address);

	hlist_add_head(&new_bkpt->link, &proc->bkpt_list[key]);

	return new_bkpt;
}

static int add_uhook_breakpoint(struct uhook_process      * proc,
                                const char                * func_name,
                                unsigned int                address,
                                enum FUNC_CODE              func_code,
                                ufunc_hook_pre_handler_t    pre_handler,
                                ufunc_hook_post_handler_t   post_handler,
                                ufunc_hook_return_handler_t return_handler,
				struct uhook_breakpoint   ** bkpt)
{
	unsigned int orig_inst;
	unsigned int orig_inst_size = 0;
	unsigned int bkpt_inst;

	unsigned int key;

	struct uhook_breakpoint * new_bkpt;

	if ((proc == NULL) || (proc->task == NULL))
	{
		return -EINVAL;
	}

	/* first check if a breakpoint is already set */
	if (read_inst_at(proc->task, address, &orig_inst, &orig_inst_size) == 0)
	{
		if (is_bp_inst(orig_inst, address))
		{
			return -EALREADY;
		}
	}

	bkpt_inst = get_bp_inst(func_code, address);

	new_bkpt = kzalloc(sizeof(struct uhook_breakpoint), GFP_ATOMIC);

	if (new_bkpt == NULL)
	{
		return -ENOMEM;
	}


	if (is_thumb_address(address))
	{
		if (write_proc_vm(proc->task, address & ~0x1, &bkpt_inst, THUMB16_INST_SIZE) != THUMB16_INST_SIZE)
		{
			return -EFAULT;
		}
	}
	else
	{
		if (write_proc_vm(proc->task, address, &bkpt_inst, ARM_INST_SIZE) != ARM_INST_SIZE)
		{
			return -EFAULT;
		}
	}

	/* initialize the uhook_breakpoint structure */
	new_bkpt->func_name      = func_name;
	new_bkpt->func_code      = func_code;
	new_bkpt->address        = address;
	new_bkpt->orig_inst      = orig_inst;
	new_bkpt->orig_inst_size = orig_inst_size;

	/* TODO */
	new_bkpt->displaced_inst_addr = 0;
	new_bkpt->displaced_inst_size = 0;
	new_bkpt->pre_handler         = pre_handler;
	new_bkpt->post_handler        = post_handler;
	new_bkpt->return_handler      = return_handler;

	/* insert the breakpoint into the process breakpoint list */
	key = bkpt_addr_hash_func(address);

	hlist_add_head(&new_bkpt->link, &proc->bkpt_list[key]);

	if (bkpt != NULL)
	{
		*bkpt = new_bkpt;
	}

	return 0;
}

static unsigned int hook_func_in_uhook_process(struct uhook_process * proc, struct ufunc_hook * hook)
{
	unsigned int address;

	Elf32_Sym sym;
	struct elf_module *m;
	const char * func_name;

	if ( (proc == NULL) || (hook == NULL) || (hook->hook_func == NULL))
		PX_BUG();

	func_name = hook->hook_func;

	if (lookup_symbol_in_proc_modules(proc->task, func_name, &sym, &m) == 0)
	{
		int ret;

		address = m->address + sym.st_value;
		
		ret = add_uhook_breakpoint(proc,
		                           func_name,
					   address,
					   FCODE_USER_HOOK,
					   hook->pre_handler,
					   hook->post_handler,
					   hook->return_handler,
					   NULL);

		if (ret != 0)
			return INVALID_ADDRESS;

		return address;
	}
	else
	{
		return INVALID_ADDRESS;
	}

}


#if 0
unsigned int elf_hash(const unsigned char *name)
{
	unsigned long h = 0, g;

	while (*name)
	{
		h = (h << 4) + *name++;
		if (g = (h & 0xf0000000))
			h ^= g >> 24;
		h &= ~g;
	}
	return h;
}

#else
unsigned int elf_hash(const unsigned char *name_arg)
{
	const unsigned char *name = (const unsigned char *) name_arg;
	unsigned long int hash = 0;
	if (*name != '\0')
	{
		hash = *name++;
		if (*name != '\0')
		{
			hash = (hash << 4) + *name++;
			if (*name != '\0')
			{
				hash = (hash << 4) + *name++;
				if (*name != '\0')
				{
					hash = (hash << 4) + *name++;
					if (*name != '\0')
					{
						hash = (hash << 4) + *name++;
						while (*name != '\0')
						{
							unsigned long int hi;
							hash = (hash << 4) + *name++;
							hi = hash & 0xf0000000;

							hash ^= hi;
							hash ^= hi >> 24;
						}
					}
				}
			}
		}
	}

	return hash;
}
#endif

/*
 * look up a symbol in the specified module
 * parameters:
 *                module     [in]   the specified module
 *                sym_name   [in]   which symbol to find
 *                sym        [out]  address of space to hold the symbol structure
 * return:
 *                return zero if the symbol is found, otherwise return non-zero value
 */
static int lookup_symbol_in_module(struct elf_module *module, const char *sym_name, Elf32_Sym * sym)
{
	bool found = false;
	int ret = -EINVAL;
	unsigned int key;
	Elf32_Sym s;
	//Elf32_Sym *result = NULL;

	char * str_data = NULL;
	//char * sym_data = NULL;
	
	unsigned int n;
	unsigned int m;
	unsigned int buf_size;
	
	if ((module == NULL) || (sym_name == NULL) || (sym == NULL))
		return -EINVAL;
	
	buf_size = strlen(sym_name) + 1;

	key = elf_hash(sym_name);

	n = key % (module->num_buckets);

	m = module->buckets + n * sizeof(unsigned int);

	n = 0;
	if (read_proc_vm(module->task, m, &n, sizeof(unsigned int)) != sizeof(unsigned int))
	{
		ret = -EFAULT;
		goto end;
	}

	str_data = kzalloc(buf_size, GFP_ATOMIC);

	if (str_data == NULL)
	{
		ret = -ENOMEM;
		goto end;
	}
	
	while (n != 0)
	{
		read_proc_vm(module->task, module->sym_table + n * sizeof(Elf32_Sym), &s, sizeof(Elf32_Sym));

		if ((s.st_shndx != 0) && ((ELF32_ST_BIND(s.st_info) == STB_GLOBAL) || (ELF32_ST_BIND(s.st_info) == STB_WEAK)))
		{
			read_proc_vm(module->task, module->str_table + s.st_name, str_data, buf_size);

			if (memcmp(str_data, sym_name, buf_size) == 0)
			{
				switch (ELF32_ST_BIND(s.st_info))
				{
				case STB_GLOBAL:
					found = true;
					memcpy(sym, &s, sizeof(Elf32_Sym));
					goto end;

				case STB_WEAK:
					found = true;
					memcpy(sym, &s, sizeof(Elf32_Sym));
					break;
				}
			}
		}

		m = module->chains + n * sizeof(unsigned int);

		n = 0;
		read_proc_vm(module->task, m, &n, sizeof(unsigned int));
	}

end:
	if (str_data != NULL)
		kfree(str_data);

	if (found)
		return 0;
	else
		return ret;
}

/* 
 * look up a symbol in the modules which belong to the specific process
 */
static int lookup_symbol_in_proc_modules(struct task_struct * task, const char *sym_name, Elf32_Sym *sym, struct elf_module **module)
{
	int i;
	struct uhook_process *proc;
	struct module_info *mi;
	struct hlist_node *node;
	struct hlist_head *head;
	//Elf32_Sym *s;

	if ((sym == NULL) || (module == NULL))
		return -EINVAL;

	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		return -EINVAL;
	}

	for (i=0; i<MODULE_HASH_SIZE; i++)
	{
		head = &proc->module_info_hash[i];

		if (hlist_empty(head))
			continue;

		hlist_for_each_entry(mi, node, head, link)
		{
			if (lookup_symbol_in_module(mi->module, sym_name, sym) == 0)
			{
				if (module != NULL)
					*module = mi->module;

				//if (sym != NULL)
				//	*sym = s;

				return 0;
			}
		}
	}

	return -EINVAL;
}

static bool fill_elf_module(struct task_struct * task,
                            const char         * module_name,
                            unsigned int         module_addr,
                            unsigned int         dynamic,
                            struct elf_module  * exe,
                            struct elf_module  * module)
{
	int i;
	bool sym_found;
	unsigned int address = 0;
	unsigned int dyn_addr = 0;
	unsigned int offset;
	unsigned int minaddr = (unsigned int)(-1);
	unsigned int maxaddr = 0;
	bool dyn_found = false;

	struct r_debug debug_info;

	Elf32_Ehdr elf_hdr;
	Elf32_Phdr phy_hdr;
	Elf32_Dyn  dyn;
	Elf32_Phdr dyn_section;
	Elf32_Sym  sym;

	if (module == NULL)
	{
		return false;
	}

	memset(module, 0, sizeof(struct elf_module));

	if (read_proc_vm(task, module_addr, &elf_hdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr))
	{
		return false;
	}

	if (memcmp(elf_hdr.e_ident, ELFMAG, SELFMAG) != 0)
	{
		return false;
	}

	/* check if it is an exe or an so */
	if (elf_hdr.e_type == ET_EXEC)
	{
		module->is_exe = true;
	}
	else if (elf_hdr.e_type == ET_DYN)
	{
		module->is_exe = false;
	}
	else
	{
		return false;
	}

	module->task    = task;
	module->address = module_addr;
	module->entry   = elf_hdr.e_entry;
	//module->is_dl   = is_dynamic_linker;
	module->exe     = exe;

	module->name = kzalloc(strlen(module_name) + 1, GFP_ATOMIC);

	if (module->name == NULL)
	{
		return false;
	}

	strcpy(module->name, module_name);

	for (i=0; i<elf_hdr.e_phnum; i++)
	{
		address = module->address + elf_hdr.e_phoff + i * sizeof(Elf32_Phdr);

		if (read_proc_vm(task, address, &phy_hdr, sizeof(Elf32_Phdr)) != sizeof(Elf32_Phdr))
			continue;

		if (phy_hdr.p_type != PT_LOAD)
			continue;

		if (maxaddr < phy_hdr.p_vaddr + phy_hdr.p_memsz)
		{
			maxaddr = phy_hdr.p_vaddr + phy_hdr.p_memsz;

		}

		if (minaddr > phy_hdr.p_vaddr)
			minaddr = phy_hdr.p_vaddr;
	}

	minaddr = ALIGN_DOWN(minaddr, PAGE_SIZE);
	maxaddr = ALIGN_UP(maxaddr, PAGE_SIZE);

	module->size = maxaddr - minaddr;

	/* find the dynamic section */

	dyn_found = false;

	for (i=0; i<elf_hdr.e_phnum; i++)
	{
		address = module->address + elf_hdr.e_phoff + i * sizeof(Elf32_Phdr);

		if (read_proc_vm(task, address, &phy_hdr, sizeof(Elf32_Phdr)) != sizeof(Elf32_Phdr))
			continue;

		switch (phy_hdr.p_type)
		{
		case PT_DYNAMIC:
			dyn_found = true;
			dyn_addr = address;
			break;

		case PT_INTERP:
			if (module->is_exe)
			{
				module->dl_name = kzalloc(phy_hdr.p_memsz + 1, GFP_ATOMIC);

				if (module->dl_name == NULL)
				{
					continue;
				}

				if (module->dl_name != NULL)
				{
					read_proc_vm(task, phy_hdr.p_vaddr, module->dl_name, phy_hdr.p_memsz);
				}
			}

			break;

		default:
			break;
		}
	}

	if (!dyn_found)
	{
		return false;
	}


	read_proc_vm(task, dyn_addr, &dyn_section, sizeof(Elf32_Phdr));

	offset = 0;

	if (dynamic == 0)
	{
		if (module->is_exe)
			dynamic = dyn_section.p_vaddr;
		else
			dynamic = module->address + dyn_section.p_vaddr;
	}

	/* find the symbol table and string table */
	while (offset < dyn_section.p_memsz)
	{
		address = dynamic + offset;

		read_proc_vm(task, address, &dyn, sizeof(Elf32_Dyn));

		switch (dyn.d_tag)
		{
		case DT_NEEDED:
			break;

		case DT_SYMTAB:
			module->sym_table = in_module(module, dyn.d_un.d_ptr) ? dyn.d_un.d_ptr : module->address + dyn.d_un.d_ptr;
			break;

		case DT_STRTAB:
			module->str_table = in_module(module, dyn.d_un.d_ptr) ? dyn.d_un.d_ptr : module->address + dyn.d_un.d_ptr;
			break;

		case DT_STRSZ:
		
			module->str_table_size = dyn.d_un.d_ptr;
			
			break;

		case DT_HASH:
			address = in_module(module, dyn.d_un.d_ptr) ? dyn.d_un.d_ptr : module->address + dyn.d_un.d_ptr;

			read_proc_vm(task, address, &(module->num_buckets), sizeof(unsigned int));
			read_proc_vm(task, address + 4, &(module->num_chains), sizeof(unsigned int));

			module->sym_num = module->num_chains;

			module->buckets = address + 8;
			module->chains  = address + 8 + sizeof(unsigned int) * module->num_buckets;

			break;

		case DT_JMPREL:
			module->rel_plt = in_module(module, dyn.d_un.d_ptr) ? dyn.d_un.d_ptr : module->address + dyn.d_un.d_ptr;
			break;

		case DT_PLTRELSZ:
			module->rel_plt_num = dyn.d_un.d_ptr / sizeof(Elf32_Rel);
			break;

		case DT_DEBUG:
			module->rdebug = dyn.d_un.d_ptr;

			if (module->rdebug != 0)
			{
				read_proc_vm(task, module->rdebug, &debug_info, sizeof(struct r_debug));
				module->l_map = (unsigned int)debug_info.r_map;
			}
			else
			{
				module->l_map = 0;
			}

			break;

		case DT_PLTGOT:
			module->got =  in_module(module, dyn.d_un.d_ptr) ? dyn.d_un.d_ptr : module->address + dyn.d_un.d_ptr;
			break;

		default:
			break;
		}

		offset += sizeof(Elf32_Dyn);
	}

	/* check if this module is a dynamic linker */
	if (    (module->exe != NULL)
	     && (module->exe != exe)
	     && (module->exe->dl_name != NULL)
	     && (strcmp(module->exe->dl_name, module_name) == 0))
	{
		module->is_dl = true;
		module->exe->dl = module;
	}
	else
	{
		module->is_dl = false;
	}

	/* check if this module is a libc.so */
	if ((module->exe->libc == NULL) && (lookup_symbol_in_module(module, "pthread_create", &sym) == 0))
	{
		sym_found = true;
	}
	else
	{
		sym_found = false;
	}

	if (    sym_found
	     && (ELF32_ST_BIND(sym.st_info) == STB_GLOBAL)
	     && (ELF32_ST_TYPE(sym.st_info) == STT_FUNC)
	     && (sym.st_shndx != SHN_UNDEF))
	{
		module->is_libc = true;
		module->exe->libc = module;
	}
	else
	{
		module->is_libc = false;
	}
	
	return true;
}

static int alloc_displaced_inst_area(struct uhook_process * proc, unsigned int size)
{
	unsigned long prot;
	unsigned long flags;
	unsigned long pgoff;
	unsigned long address;

	struct displaced_inst_area * dia;

	prot  = PROT_EXEC;
	flags = MAP_PRIVATE | MAP_ANONYMOUS;
	pgoff = 0;

	if ((proc->task == NULL) || (proc->task->mm == NULL))
		return -EINVAL;

	down_write(&proc->task->mm->mmap_sem);
	#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0))
	address = do_mmap(NULL, 0, size, prot, flags, pgoff);
	#else
	address = do_mmap_pgoff(NULL, 0, size, prot, flags, pgoff);
	#endif
        up_write(&proc->task->mm->mmap_sem);

	dia = kzalloc(sizeof(struct displaced_inst_area), GFP_ATOMIC);

	if (dia == NULL)
	{
		return -ENOMEM;
	}

	dia->address = address;
	dia->size    = size;
	dia->slot_size = 8 * sizeof(unsigned int);
	dia->bitmap_size = 1024;

	dia->bitmap = kzalloc(dia->bitmap_size, GFP_ATOMIC);

	if (dia->bitmap == NULL)
	{
		return -ENOMEM;
	}

	spin_lock_init(&dia->lock);

	proc->dia = dia;

	//list_add(&dia->link, &proc->dia_list);

	return 0;
}

static int free_displaced_inst_area(struct uhook_process * proc)
{
	if ((proc == NULL) || (proc->task == NULL) || (proc->dia == NULL) || (proc->task->mm == NULL))
	{
		return -EINVAL;
	}

	down_write(&proc->task->mm->mmap_sem);	
	do_munmap(proc->task->mm, proc->dia->address, proc->dia->size);
	up_write(&proc->task->mm->mmap_sem);

	if (proc->dia->bitmap != NULL)
	{
		kfree(proc->dia->bitmap);
		proc->dia->bitmap = NULL;
	}

	if (proc->dia != NULL)
	{
		kfree(proc->dia);
		proc->dia = NULL;
	}

	return 0;
}

static int remove_uhook_breakpoints_in_process(struct uhook_process * proc, bool restore_inst)
{
	int i;
	struct uhook_breakpoint * bkpt;
	struct hlist_node * bkpt_node, * bkpt_tmp;
	struct hlist_head * bkpt_head;

	if (proc == NULL)
		return -EINVAL;

	/* remove all breakpoints for this process */
	for (i=0; i< BKPT_HASH_SIZE; i++)
	{
		bkpt_head = &proc->bkpt_list[i];

		if (hlist_empty(bkpt_head))
		{
			continue;
		}

		hlist_for_each_entry_safe(bkpt, bkpt_node, bkpt_tmp, bkpt_head, link)
		{
			remove_uhook_breakpoint(proc, bkpt->address, restore_inst);
		}
	}

	return 0;
}

static int remove_uhook_modules_in_process(struct uhook_process * proc)
{
	int i;

	struct module_info   * mi;

	struct hlist_node * mod_node, * mod_tmp;
	struct hlist_head * mod_head;

	if (proc == NULL)
		return -EINVAL;

	for (i=0; i<MODULE_HASH_SIZE; i++)
	{
		mod_head = &proc->module_info_hash[i];

		if (hlist_empty(mod_head))
			continue;

		hlist_for_each_entry_safe(mi, mod_node, mod_tmp, mod_head, link)
		{
			remove_uhook_module(mi);
		}
	}

	return 0;
}

static unsigned int tid_hash_func(unsigned int data)
{
	return data % TID_HASH_SIZE;
}

static struct uhook_thread * add_uhook_thread(struct task_struct *task)
{
	unsigned int key;
	struct hlist_head *h;
	struct uhook_thread *thread;
	struct uhook_process *proc;

	if (task == NULL)
	{
		return NULL;
	}

	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		proc = add_uhook_process(task);
		
		if (proc == NULL)
			return NULL;
	}

	thread = kzalloc(sizeof(struct uhook_thread), GFP_ATOMIC);

	if (thread == NULL)
	{
		return NULL;
	}

	//thread->fcode = FCODE_UNKNOWN;
	thread->fstep    = FUNC_ENTRY;
	thread->task     = task;
	thread->proc     = proc;
	thread->entry_ts = 0;

	memset(&thread->entry_regs, 0, sizeof(struct pt_regs));
	memset(&thread->dsc, 0, sizeof(struct displaced_desc));

	key = tid_hash_func(thread->task->pid);

	h = &uhook_thread_hash[key];

	hlist_add_head(&thread->link, h);
	
	return thread;
}

static struct uhook_thread * find_uhook_thread(struct task_struct * task)
{
	unsigned int key;

	struct uhook_thread  * thread = NULL;
	struct hlist_node *n;
	struct hlist_head *head;

	key = tid_hash_func(task->pid);

	head = &uhook_thread_hash[key];

	if (hlist_empty(&uhook_thread_hash[key]))
	{
		return NULL;
	}

	hlist_for_each_entry(thread, n, head, link)
	{
		if ((thread->task == task) && (thread->task->pid == task->pid))
		{
			return thread;
		}
	}

	return NULL;
}

static int remove_uhook_thread(pid_t pid)
{
	//int i;
	int key;
	struct uhook_thread *thread;
	struct hlist_node *n, *tmp;
	struct hlist_head *head;

	key = tid_hash_func(pid);

	head = &uhook_thread_hash[key];

	if (hlist_empty(&uhook_thread_hash[key]))
		return 0;

	hlist_for_each_entry_safe(thread, n, tmp, head, link)
	{
		if (thread->task->pid == pid)
		{
			hlist_del(&thread->link);
			kfree(thread);
			
			return 0;
		}
	}

	return 0;
}

static int clear_uhook_thread_list(void)
{
	int i;
	struct hlist_head * head;
	struct hlist_node * node, * temp;
	struct uhook_thread * thread;

	for (i=0; i<TID_HASH_SIZE; i++)
	{
		head = &uhook_thread_hash[i];

		if (hlist_empty(head))
			continue;

		hlist_for_each_entry_safe(thread, node, temp, head, link)
		{
			hlist_del(&thread->link);
			
			kfree(thread);
		}
	}

	return 0;
}

static int remove_uhook_thread_in_process(struct uhook_process * proc)
{
	int i;
	struct uhook_thread *thread;
	struct hlist_node *n, *tmp;

	if (proc == NULL)
		return -EINVAL;

	for (i=0; i<TID_HASH_SIZE; i++)
	{
		struct hlist_head *head = &uhook_thread_hash[i];

		if (hlist_empty(&uhook_thread_hash[i]))
			continue;

		hlist_for_each_entry_safe(thread, n, tmp, head, link)
		{
			if (thread->task->tgid == proc->task->tgid)
			{
				hlist_del(&thread->link);
				kfree(thread);
			}
		}
	}

	return 0;
}

static int remove_uhook_process(struct uhook_process ** process, bool restore_inst)
{
	struct uhook_process * proc;
	
	if ((process == NULL) || (*process == NULL))
		return -EINVAL;

	proc = *process;

	free_displaced_inst_area(proc);

	remove_uhook_breakpoints_in_process(proc, restore_inst);

	remove_uhook_modules_in_process(proc);

	remove_uhook_thread_in_process(proc);

	proc->task = NULL;

	hlist_del(&proc->link);

	kfree(proc);

	return 0;
}

static int remove_uhook_process_by_task(struct task_struct * task, bool restore_inst)
{
	struct uhook_process * proc;

	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		return -EINVAL;
	}
	
	remove_uhook_process(&proc, restore_inst);

	return 0;
}

static int clear_uhook_process_list(void)
{
	int i;
	struct hlist_head * head;
	struct hlist_node * node, * temp;
	struct uhook_process * proc;

	for (i=0; i<PID_HASH_SIZE; i++)
	{
		head = &uhook_process_hash[i];

		if (hlist_empty(head))
			continue;

		hlist_for_each_entry_safe(proc, node, temp, head, link)
		{
			remove_uhook_process(&proc, true);
		}
	}

	return 0;
}

static struct uhook_process * add_uhook_process(struct task_struct * task)
{
	unsigned int pid_key;
	struct uhook_process *proc;

	if (task == NULL)
		return NULL;

	pid_key = pid_hash_func(task->tgid);

	if (find_uhook_process(task) != NULL)
	{
		//remove_uhook_process_by_task(task);
		return NULL;
	}

	proc = kzalloc(sizeof(struct uhook_process), GFP_ATOMIC);

	if (proc == NULL)
	{
		return NULL;
	}
	
	proc->task = task;

	INIT_LIST_HEAD(&proc->hook_list);
	//INIT_LIST_HEAD(&proc->dia_list);
	hlist_add_head(&proc->link, &uhook_process_hash[pid_key]);

	//alloc_displaced_inst_area(proc, 1024);
	return proc;
}

static int handle_func_entry(struct uhook_breakpoint * bkpt,
                             struct uhook_thread     * thread,
                             struct pt_regs          * regs,
                             unsigned long long        ts)
{
	unsigned int pc;

	ufunc_hook_pre_handler_t pre_handler;

	if ((bkpt == NULL) || (regs == NULL) || (thread == NULL))
	{
		return -EINVAL;
	}

	pc = regs->ARM_pc;

	/* if it is in thumb mode, set the lowest bit of the pc */
	if (thumb_mode(regs))
		pc |= 0x1;

	thread->entry_ts   = ts;
	thread->entry_regs = *regs;

	pre_handler = bkpt->pre_handler;
	if (pre_handler != NULL)
		//pre_handler(thread->task, thread->dsc.orig_inst, thread->dsc.orig_inst_size, regs, ts);
		pre_handler(thread->task, bkpt->orig_inst, bkpt->orig_inst_size, regs, ts);

	if (bkpt->return_handler != NULL)
	{
		/* add breakpoint at the function return address */
		if (find_uhook_breakpoint(thread->task, regs->ARM_lr) == NULL)
		{
			enum FUNC_CODE fcode;

			if (bkpt->func_code == FCODE_THREAD_FUNC_ENTRY)
				fcode = FCODE_THREAD_FUNC_EXIT;
			else
				fcode = FCODE_HOOKED_FUNC_EXIT;

			add_uhook_breakpoint(thread->proc,
			                     NULL,
					     regs->ARM_lr,
					     fcode,
					     NULL,
					     NULL,
					     bkpt->return_handler,
					     NULL);
		}
	}

	/* function call is complete */
	thread->fstep = FUNC_EXIT;
	
	return 0;
}

static int handle_func_return(struct uhook_breakpoint * bkpt,
                              struct uhook_thread     * thread,
                              struct pt_regs          * regs,
                              unsigned long long        ts)

{
	unsigned int pc;

	ufunc_hook_return_handler_t return_handler;

	if ((bkpt == NULL) || (regs == NULL) || (thread == NULL))
		return -EINVAL;

	pc = regs->ARM_pc;

	/* if it is in thumb mode, set the lowest bit of the pc */
	if (thumb_mode(regs))
		pc |= 0x1;

	return_handler = bkpt->return_handler;

	if (return_handler != NULL)
		return_handler(thread->task, bkpt->orig_inst, bkpt->orig_inst_size, regs, ts, &thread->entry_regs, thread->entry_ts);

	/* function return is complete */
	thread->fstep = FUNC_ENTRY;
	
	return 0;
}

static int handle_func(struct uhook_breakpoint * bkpt,
                       struct uhook_thread     * thread,
                       struct pt_regs          * regs,
                       unsigned long long        ts,
		       enum FUNC_CODE            func_code)
{
	int ret = -EINVAL;

	if ((thread == NULL) || (bkpt == NULL) || (regs == NULL))
	{
		return -EINVAL;
	}

#if 1
	if ((func_code == FCODE_HOOKED_FUNC_EXIT) || (func_code == FCODE_THREAD_FUNC_EXIT))
		ret = handle_func_return(bkpt, thread, regs, ts);
	else
		ret = handle_func_entry(bkpt, thread, regs, ts);
#else
	switch (thread->fstep)
	{
	case FUNC_ENTRY:
		ret = handle_func_entry(bkpt, thread, regs, ts);
		break;

	case FUNC_EXIT:
		ret = handle_func_return(bkpt, thread, regs, ts);
		break;

	default:
		ret = -EINVAL;
	}
#endif

	return ret;
}

static bool in_ignore_so_list(struct task_struct *task, unsigned int address, bool is_lr)
{
	struct uhook_process *proc;

	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		return false;
	}

	if ((proc->exe_info != NULL) && (proc->exe_info->module != NULL) && (proc->exe_info->module->dl != NULL))
	{
		if (in_module(proc->exe_info->module->dl, address))
		{
			return true;
		}
	}

	if ((proc->exe_info != NULL) && (proc->exe_info->module != NULL) && (proc->exe_info->module->libc != NULL))
	{
		if (in_module(proc->exe_info->module->libc, address))
		{
			return true;
		}
	}

	return false;
}

#if 0
/* ignore the breakpoint at the specific address */
static int ignore_breakpoint_at(struct task_struct *task, unsigned int address, struct pt_regs *regs, struct uhook_thread *thread)
{
	unsigned int first_inst;
	unsigned int first_inst_size;
	struct pt_regs cur_regs;

	/* restore the instruction at the address */
	restore_inst_at(task, address, &first_inst, &first_inst_size);

	/* backup the next instruction at the address */
	/* add breakpoint at the next instruction */

	memcpy(&cur_regs, regs, sizeof(struct pt_regs));

	if (thumb_mode(regs))
	{
		cur_regs.ARM_pc |= 0x1;
	}

	return 0;
}
#endif

#if 0
static int handle_ignore_breakpoint(enum FUNC_CODE func_code, struct task_struct *task, struct pt_regs *regs, struct uhook_thread *thread)
{
	unsigned int pc;
	int ret;

	pc = regs->ARM_pc;

	if (thumb_mode(regs))
		pc |= 0x1;

	if (func_code != FCODE_IGNORE_BP)
	{
		ret = ignore_breakpoint_at(task, pc, regs, thread);
	}

	return ret;
}
#endif

static int get_func_code_from_bp_inst(unsigned int inst, bool thumb_mode)
{
	int data;

	if (thumb_mode)
	{
		data = inst & ~TP_BP_THUMB_ENTRY_FLAG;
		return (data & ~TP_BP_THUMB_MASK);
	}
	else
	{
		data = inst & ~TP_BP_ARM_ENTRY_FLAG;
		return (data & ~TP_BP_ARM_MASK);
	}
}

#if 0
static bool is_entry_bp(unsigned int bp_inst, bool is_thumb)
{
	if (is_thumb)
	{
		return bp_inst & TP_BP_THUMB_ENTRY_FLAG;
	}
	else
	{
		return bp_inst & TP_BP_ARM_ENTRY_FLAG;
	}
}

static bool is_exit_bp(unsigned int bp_inst, bool is_thumb)
{
	return !is_entry_bp(bp_inst, is_thumb);
}
#endif

static bool should_ignore_breakpoint(struct task_struct *task, struct pt_regs *regs, unsigned int inst, enum FUNC_CODE func_code, struct uhook_thread *thread)
{
	/* if the breakpoint is for the thread function exit we should not ignore this breakpoint */
	if (func_code == FCODE_THREAD_FUNC_EXIT)
	{
		return false;
	}

	if (func_code == FCODE_IGNORE_BP)
	{
		return true;
	}

	if (func_code == FCODE_HOOKED_FUNC_EXIT)
	{
		if (thread->fstep != FUNC_EXIT)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	/*
	 * ignore breakpoint if both pc and lr is in ignore so list
	 * for example pthread_create() will call pthread_mutex_lock()
	 * we need to ignore this pthread_mutex_lock()
	 */
#if 1
	if (in_ignore_so_list(task, regs->ARM_lr, true) && in_ignore_so_list(task, regs->ARM_pc, false))
	//if (in_ignore_so_list(task, entry_regs->ARM_lr))
	{
		return true;
	}
#endif

	return false;
}

static int add_all_modules_in_task(struct task_struct * task)
{
	struct link_map * map;
	struct elf_module * exe;
	struct elf_module * module;
	struct uhook_process * proc;

	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		return -EFAULT;
	}
	
	remove_uhook_modules_in_process(proc);

	/* create an exe module */
	exe = kzalloc(sizeof(struct elf_module), GFP_ATOMIC);

	if (exe == NULL)
	{
		return -ENOMEM;
	}

	if (fill_elf_module(task, task->comm, 0x8000, 0, exe, exe))
	{
		if (add_elf_module(exe) != 0)
		{
			return -EFAULT;
		}
	}
	else
	{
		return -EFAULT;
	}

	map = (struct link_map *)exe->l_map;

	/* hook the functions in the shared libraries */
	while (map != NULL)
	{
		if (map->l_addr == 0)
		{
			map = map->l_next;
			continue;
		}

		module = kzalloc(sizeof(struct elf_module), GFP_ATOMIC);

		if (module == NULL)
		{
			map = map->l_next;
			continue;
		}

		if (fill_elf_module(task, map->l_name, map->l_addr, map->l_ld, exe, module))
		{
			add_elf_module(module);
			//update_module_got(module);
		}
		else
		{
			kfree(module);
		}

		map = map->l_next;
	}

	return 0;
}


static int dlopen_entry_cb(struct task_struct * task,
                           unsigned int         orig_inst,
                           int                  orig_inst_size,
                           struct pt_regs     * entry_regs,
                           unsigned long long   entry_ts)
{
	return 0;
}

static int dlopen_return_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * exit_regs,
                            unsigned long long   exit_ts,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	//unsigned long flags;
	struct uhook_process * proc;
	struct ufunc_hook    * hook;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
	{
		return -EINVAL;
	}

	if (exit_regs->ARM_r0 == 0)
	{
		/* dlopen fails */
		return 0;
	}

	add_all_modules_in_task(task);

	/* hook the functions in the shared libraries */
	proc = find_uhook_process(task);

	if (proc != NULL)
	{
		list_for_each_entry(hook, &proc->hook_list, link)
		{
			if (hook->hook_func != NULL)
			{
				hook_func_in_uhook_process(proc, hook);
			}
		}
	}

	return 0;
}

static int exe_entry_cb(struct task_struct *task, struct pt_regs *regs, unsigned int inst)
{
	int ret = 0;
	//unsigned long flags;
	struct uhook_process * proc;
	struct ufunc_hook    * hook;

	//spin_lock_irqsave(&uhook_lock, flags);

	add_all_modules_in_task(task);

	/* hook the functions in the shared libraries */
	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		//spin_unlock_irqrestore(&uhook_lock, flags);
		ret = -EFAULT;

		goto end;
	}

	list_for_each_entry(hook, &proc->hook_list, link)
	{
		if (hook->hook_func != NULL)
		{
			hook_func_in_uhook_process(proc, hook);
		}
	}

	hook = kzalloc(sizeof(struct ufunc_hook), GFP_ATOMIC);

	if (hook == NULL)
	{
		//spin_unlock_irqrestore(&uhook_lock, flags);
		ret = -ENOMEM;
		goto end;
	}

	init_user_func_hook(hook);

	hook->tgid  = task->tgid;

	hook->hook_func      = "dlopen";
	hook->pre_handler    = &dlopen_entry_cb;
	hook->return_handler = &dlopen_return_cb;

	raw_register_user_func_hook(hook);

end:
	return ret;
}


static unsigned int find_free_dia_slot_address(struct uhook_process * proc)
{
	int slot;
	//unsigned long flags;
	struct displaced_inst_area * dia = proc->dia;

	if ((dia == NULL) || (dia->bitmap == NULL))
	{
		return INVALID_ADDRESS;
	}

	//spin_lock_irqsave(&dia->lock, flags);

	slot = find_first_zero_bit(dia->bitmap, dia->bitmap_size);
	
	set_bit(slot, dia->bitmap);
	//spin_unlock_irqrestore(&dia->lock, flags);

	return dia->address + slot * dia->slot_size;
}

static int free_dia_slot_address(struct displaced_inst_area * dia, unsigned int address)
{
	int slot;
	//unsigned long flags;

	slot = (address - dia->address) / dia->slot_size;

	//spin_lock_irqsave(&dia->lock, flags);
	clear_bit(slot, dia->bitmap);
	//spin_unlock_irqrestore(&dia->lock, flags);

	return 0;
}

static int write_one_displaced_inst(void * inst, int inst_size, struct task_struct * task, unsigned int address)
{
	if (write_proc_vm(task, address, inst, inst_size) != inst_size)
	{
		return -EFAULT;
	}

	return 0;
}

static int add_displaced_stepping_return_bkpt(struct task_struct * task, unsigned int address)
{
	unsigned int bp_inst;

	bp_inst = get_bp_inst(FCODE_DISPLACED_STEPPING_RETURN, address);

	if (is_thumb_address(address))
	{
		if (write_proc_vm(task, address & ~0x1, &bp_inst, THUMB16_INST_SIZE) != THUMB16_INST_SIZE)
		{
			return -EFAULT;
		}
	}
	else
	{
		if (write_proc_vm(task, address, &bp_inst, ARM_INST_SIZE) != ARM_INST_SIZE)
		{
			return -EFAULT;
		}
	}

	return 0;
}

static int displace_inst(unsigned int orig_inst, unsigned int orig_inst_size, struct pt_regs * regs, struct uhook_thread * thread)
{
	int i;
	bool is_thumb;
	struct uhook_process  * proc;
	struct displaced_desc * dsc;

	unsigned int trap_addr;
	unsigned int slot_addr;
	unsigned int address;
	//unsigned int size;

	trap_addr = regs->ARM_pc;
	proc = thread->proc;
	dsc  = &thread->dsc;

	dsc->orig_inst      = orig_inst;
	dsc->orig_inst_addr = trap_addr;
	dsc->orig_inst_size = orig_inst_size;

	memcpy(&dsc->orig_regs, regs, sizeof(struct pt_regs));

	is_thumb = thumb_mode(regs);

	/* get the displaced instructions */
	if (!is_thumb)
	{
		arm_displace_inst(regs, dsc);
	}
	else
	{
		thumb_displace_inst(regs, dsc);
	}

	if (proc->dia == NULL)
	{
		if (alloc_displaced_inst_area(proc, 1024) != 0)
			return -EFAULT;
	}

	/* find the free slot */
	slot_addr = find_free_dia_slot_address(proc);

	thread->dsc.dia_slot_addr = slot_addr;

	if (dsc->pre_handler != NULL)
		dsc->pre_handler(regs, dsc);

	/* write the displaced instructions */
	address = slot_addr;

	for (i=0; i<thread->dsc.displaced_insts_num; i++)
	{
		struct displaced_inst_s * di;
		di = &thread->dsc.displaced_insts[i];
		
		write_one_displaced_inst(&di->inst, di->size, current, address);
		address += di->size;

	}

	/* add an breakpoint at the end of the displaced instructions */
	add_displaced_stepping_return_bkpt(current, address + is_thumb);

	thread->dsc.dia_slot_end_addr = address;

	/* set pc to the first displaced stepping instruction */
	regs->ARM_pc = slot_addr;
	
	return 0;
}

static unsigned int find_exe_entry(struct task_struct *task, unsigned int address)
{
	Elf32_Ehdr elf_hdr;

	if (read_proc_vm(task, address, &elf_hdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr))
		return INVALID_ADDRESS;

	return elf_hdr.e_entry;
}

static int notify_process_create(struct task_struct *task, struct task_struct *parent_task, unsigned long long ts)
{
	//unsigned long flags;
	unsigned int address;
	unsigned int entry;
	struct uhook_process * proc;
	struct ufunc_hook * hook;

	//spin_lock_irqsave(&uhook_lock, flags);

	proc = add_uhook_process(task);

	//spin_unlock_irqrestore(&uhook_lock, flags);

	if (proc == NULL)
	{
		return -EFAULT;
	}

	//alloc_displaced_inst_area(proc, 1024);

	address = find_rdebug(task, 0x8000);

	if (address != INVALID_ADDRESS)
	{
		//spin_lock_irqsave(&uhook_lock, flags);

		add_all_modules_in_task(task);

		/* hook the functions in the shared libraries */
		proc = find_uhook_process(task);

		if (proc != NULL)
		{
			list_for_each_entry(hook, &proc->hook_list, link)
			{
				if (hook->hook_func != NULL)
				{
					hook_func_in_uhook_process(proc, hook);
				}
			}
		}

		//spin_unlock_irqrestore(&uhook_lock, flags);
	}
	
	//if (proc == NULL)
	//	return 0;
	//else
	{
		entry = find_exe_entry(task, 0x8000);
		
		add_uhook_breakpoint(proc, NULL, entry, FCODE_EXE_ENTRY, NULL, NULL, NULL, NULL);
	}

	return 0;
}

static int trap_handler(struct pt_regs *regs, unsigned int inst)
{
	struct uhook_thread * thread;
	struct uhook_breakpoint * bkpt;
	unsigned long long ts;
	bool ignore_bp = true;
	int func_code;
	unsigned int address;

	ts = get_timestamp();

	if (thumb_mode(regs))
	{
		address = regs->ARM_pc | 0x1;
	}
	else
	{
		address = regs->ARM_pc;
	}

	func_code = get_func_code_from_bp_inst(inst, thumb_mode(regs));

	if (func_code == FCODE_EXE_ENTRY)
	{
		unsigned int inst1;
		unsigned int inst_size;

		exe_entry_cb(current, regs, inst);

		restore_inst_at(current, address, &inst1, &inst_size);
		
		return 0;
	}

	thread = find_uhook_thread(current);

	if (thread == NULL)
	{
		thread = add_uhook_thread(current);

		if (thread == NULL)
		{
			PX_BUG();
			return 0;
		}
	}

	if (func_code == FCODE_DISPLACED_STEPPING_RETURN)
	{
		int ret = 0;
		int data = 0;

		if (thread->dsc.post_handler != NULL)
			ret = thread->dsc.post_handler(regs, &thread->dsc);
		//else
		//	regs->ARM_pc = thread->dsc.orig_regs.ARM_pc;

		bkpt = thread->active_bkpt;

		if (bkpt == NULL)
		{
			return 0;
		}

		free_dia_slot_address(thread->proc->dia, thread->dsc.dia_slot_addr);

		if (regs->ARM_pc == thread->dsc.dia_slot_end_addr)
		{
			regs->ARM_pc = (bkpt->address + bkpt->orig_inst_size) & ~0x1;
		}

		read_proc_vm(current, regs->ARM_pc, &data, 4);

		if (bkpt->post_handler != NULL)
			bkpt->post_handler(current, bkpt->orig_inst, bkpt->orig_inst_size, regs, ts, &thread->entry_regs, thread->entry_ts);

		return 0;
	}

	bkpt = find_uhook_breakpoint(current, address);

#if 0
	if (bkpt == NULL)
	{
		printk("--- %d: tgid = %d %s pc = 0x%lx, inst = 0x%x, parent tgid = %d %s\n", __LINE__, current->tgid, current->group_leader->comm, regs->ARM_pc, inst, current->parent->tgid, current->parent->comm);
		return 0;
	}
#else
	if (bkpt == NULL)
	{
		struct uhook_breakpoint *parent_bkpt;
		struct uhook_process * proc;
		
		parent_bkpt = find_uhook_breakpoint(current->parent, address);

		if (parent_bkpt != NULL)
		{
			proc = find_uhook_process(current);

			if (proc == NULL)
			{
				proc = add_uhook_process(current);

				if (proc == NULL)
				{
					return 0;
				}

				//alloc_displaced_inst_area(proc, 1024);
			}

			bkpt = copy_uhook_breakpoint_from_parent(proc, parent_bkpt);

			if (bkpt == NULL)
			{
				return 0;
			}

		}
		else
		{
			printk("--- %d: tgid = %d %s pc = 0x%lx, inst = 0x%x, parent tgid = %d %s\n", __LINE__, current->tgid, current->group_leader->comm, regs->ARM_pc, inst, current->parent->tgid, current->parent->comm);
			return 0;
		}

	}
#endif

	thread->active_bkpt = bkpt;

	ignore_bp = should_ignore_breakpoint(current, regs, inst, func_code, thread);

	if (ignore_bp)
	{
		displace_inst(bkpt->orig_inst, bkpt->orig_inst_size, regs, thread);
		
		return 0;
	}

	handle_func(bkpt, thread, regs, ts, func_code);

	displace_inst(bkpt->orig_inst, bkpt->orig_inst_size, regs, thread);

	return 0;
}

static int tp_trap_handler(struct pt_regs *regs, unsigned int inst)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);

	ret = trap_handler(regs, inst);

	local_irq_restore(flags);

	return ret;
}



static struct undef_hook arm_func_entry_hook = {
	.instr_mask     = 0xffff0000,
	.instr_val      = TP_BP_ARM_MASK,
	.cpsr_mask      = MODE_MASK | PSR_T_BIT,
	.cpsr_val       = USR_MODE,
	.fn             = tp_trap_handler,
};

static struct undef_hook thumb_func_entry_hook = {
	.instr_mask     = 0xfff0,
	.instr_val      = TP_BP_THUMB_MASK,
	.cpsr_mask      = MODE_MASK | PSR_T_BIT,
	.cpsr_val       = USR_MODE | PSR_T_BIT,
	.fn             = tp_trap_handler,
};

int init_uhooks(void)
{
	/* for ARM instructions */
	register_undef_func(&arm_func_entry_hook);

	/* for THUMB instructions */
	register_undef_func(&thumb_func_entry_hook);

	uhooks_initialized = true;

	return 0;
}

int uninit_uhooks(void)
{
	unsigned long flags;

	//spin_lock_irqsave(&uhook_lock, flags);
	uhooks_initialized = false;
	
	local_irq_save(flags);

	clear_uhook_process_list();
	
	clear_uhook_thread_list();

	//spin_unlock_irqrestore(&uhook_lock, flags);
	local_irq_restore(flags);

	/* for ARM instructions */
	unregister_undef_func(&arm_func_entry_hook);

	/* for THUMB instructions */
	unregister_undef_func(&thumb_func_entry_hook);

	return 0;
}

/* The fields of structure hook will be initialized to default value */
int init_user_func_hook(struct ufunc_hook *hook)
{
	//INIT_LIST_HEAD(&hook->node);

	hook->flag           = 0;
	hook->hook_func      = NULL;
	hook->hook_addr      = INVALID_ADDRESS;
	hook->pre_handler    = NULL;
	hook->post_handler   = NULL;
	hook->return_handler = NULL;

	return 0;
}

int raw_register_user_func_hook(struct ufunc_hook * hook)
{
	int ret = 0;
	struct task_struct   * task;
	struct uhook_process * proc;
	unsigned long flags;

	if (hook == NULL)
	{
		return -EINVAL;
	}

	/* if both hook_func and hooed_addr are specified */
	if ((hook->hook_func != NULL) && (hook->hook_addr != INVALID_ADDRESS))
	{
		return -EINVAL;
	}

	/* if neither hook_func nor hooed_addr is specified */
	if ((hook->hook_func == NULL) && (hook->hook_addr == INVALID_ADDRESS))
	{
		return -EINVAL;
	}

	if (!uhooks_initialized)
	{
		return -EINVAL;
	}

	local_irq_save(flags);

	task = px_find_task_by_pid(hook->tgid);
	
	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		ret = -EINVAL;
		goto end;
	}

	list_add(&hook->link, &proc->hook_list);

	/* if hook function is specified by user */
	if (hook->hook_func != NULL)
	{
		unsigned int address;

		address = hook_func_in_uhook_process(proc, hook);

		if (address == INVALID_ADDRESS)
		{
			ret = -EINVAL;
		}

		return ret;
	}

	/* if hook address is specified by user */
	if (hook->hook_addr != INVALID_ADDRESS)
	{
		enum FUNC_CODE fcode;
		
		if (hook->flag & UFUNC_HOOK_FLAG_THREAD_FUNC)
			fcode = FCODE_THREAD_FUNC_ENTRY;
		else
			fcode = FCODE_USER_HOOK;

		ret = add_uhook_breakpoint(proc,
		                           NULL,
		                           hook->hook_addr,
		                           fcode,
		                           hook->pre_handler,
		                           hook->post_handler,
		                           hook->return_handler,
					   NULL);
	}

end:
	local_irq_restore(flags);

	return ret;
}

int raw_unregister_user_func_hook(struct ufunc_hook *hook)
{
	int ret = 0;
	unsigned long flags;
	struct task_struct   * task;
	struct uhook_process * proc;

	local_irq_save(flags);

	task = px_find_task_by_pid(hook->tgid);

	proc = find_uhook_process(task);

	if (proc == NULL)
	{
		ret = -EINVAL;
		goto end;
	}

	remove_uhook_breakpoint(proc, hook->hook_addr, true);
	list_del(&hook->link);

end:
	local_irq_restore(flags);

	return ret;
}

/* register the user function hook */
int register_user_func_hook(struct ufunc_hook * hook)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&uhook_lock, flags);

	ret = raw_register_user_func_hook(hook);

	spin_unlock_irqrestore(&uhook_lock, flags);

	return ret;
}

/* unregister the user function hook */
int unregister_user_func_hook(struct ufunc_hook *hook)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&uhook_lock, flags);

	ret = raw_unregister_user_func_hook(hook);

	spin_unlock_irqrestore(&uhook_lock, flags);

	return ret;
}



int uhook_notify_fork(struct task_struct * task, struct task_struct * parent, unsigned long long ts)
{
	unsigned long flags;
	struct uhook_process * proc;

	local_irq_save(flags);

	proc = find_uhook_process(parent);
	
	if (proc != NULL)
	{
		//free_displaced_inst_area(proc);
		//return -EFAULT;
	}
	
	notify_process_create(task, parent, ts);
	
	local_irq_restore(flags);
	
	return 0;
}

int uhook_notify_vfork(struct task_struct * task, struct task_struct * parent, unsigned long long ts)
{
	unsigned long flags;
	struct uhook_process * proc;
	
	//spin_lock_irqsave(&uhook_lock, flags);
	local_irq_save(flags);
	
	proc = find_uhook_process(parent);
	
	if (proc != NULL)
	{
		//free_displaced_inst_area(proc);
		//return -EFAULT;
	}
	
	notify_process_create(task, parent, ts);
	
	//spin_unlock_irqrestore(&uhook_lock, flags);
	local_irq_restore(flags);
	
	return 0;
}

int uhook_notify_clone(struct task_struct * task, struct task_struct * parent, unsigned long long ts, unsigned int clone_flags)
{
	unsigned long flags;
	struct uhook_process * proc;
	
	/* if a thread is cloned instead of a process, do nothing */
	if ((clone_flags & CLONE_THREAD))
		return 0;

	local_irq_save(flags);
	//spin_lock_irqsave(&uhook_lock, flags);

	proc = find_uhook_process(parent);
	
	if (proc != NULL)
	{
		//free_displaced_inst_area(proc);
	}
	
	notify_process_create(task, parent, ts);
	
	//spin_unlock_irqrestore(&uhook_lock, flags);
	local_irq_restore(flags);
	
	return 0;
}

int uhook_notify_exec(struct task_struct * task, struct task_struct * parent, unsigned long long ts)
{
	unsigned long flags;
	struct uhook_process * proc;
	
	//spin_lock_irqsave(&uhook_lock, flags);
	local_irq_save(flags);

	proc = find_uhook_process(task);
	
	if (proc != NULL)
	{
		remove_uhook_process(&proc, false);
		//free_displaced_inst_area(proc);
	}
	
	//spin_unlock_irqrestore(&uhook_lock, flags);
	
	notify_process_create(task, parent, ts);
	
	local_irq_restore(flags);

	return 0;
}

int uhook_notify_thread_exit(struct task_struct *task, int ret_code, unsigned long long ts)
{
	unsigned long flags;

	//spin_lock_irqsave(&uhook_lock, flags);
	local_irq_save(flags);

	/* 
	 * if uhook_notify_thread_exit() is called in the user mode, for example by pthread_exit()
	 * we can't remove uhook_thread, because it will be still used
	 */
	if (!user_mode(task_pt_regs(task)))
		remove_uhook_thread(task->pid);
	
	//spin_unlock_irqrestore(&uhook_lock, flags);
	local_irq_restore(flags);

	return 0;
}

int uhook_notify_thread_group_exit(struct task_struct *task, int ret_code, unsigned long long ts)
{
	unsigned long flags;

	//spin_lock_irqsave(&uhook_lock, flags);
	local_irq_save(flags);

	remove_uhook_process_by_task(task, false);

	//spin_unlock_irqrestore(&uhook_lock, flags);
	local_irq_restore(flags);

	return 0;
}

void set_address_register_undef_hook(unsigned int address)
{
	register_undef_func = (REGISTER_UNDEF_FUNC)address;
}

void set_address_unregister_undef_hook(unsigned int address)
{
	unregister_undef_func = (UNREGISTER_UNDEF_FUNC)address;
}

void set_address_access_process_vm(unsigned int address)
{
	access_process_vm_func = (ACCESS_PROCESS_VM_FUNC)address;
}



