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

#ifndef _PX_UFUNC_HOOK_INTERNAL_H_
#define _PX_UFUNC_HOOK_INTERNAL_H_

#include "ufunc_hook.h"

#define SYM_HASH_SIZE     300
#define MODULE_HASH_SIZE  300
#define BKPT_HASH_SIZE    300

enum FUNC_CODE
{
	FCODE_UNKNOWN = 1,
	FCODE_EXE_ENTRY = 2,
	FCODE_EXE_EXIT = 3,
	FCODE_IGNORE_BP = 4,
	FCODE_THREAD_FUNC_ENTRY = 5,
	FCODE_THREAD_FUNC_EXIT = 6,
	FCODE_DLOPEN = 7,
	FCODE_USER_HOOK = 8,

	FCODE_HOOKED_FUNC_EXIT = 9,
	FCODE_DISPLACED_STEPPING_RETURN  = 10,

	FCODE_MAX
};


/* This structure represents a symbol in the module */
struct module_symbol
{
	struct hlist_node link;

	char *       name;                  /* symbol name */
	unsigned int address;               /* symbol virtual address */
	unsigned int info;                  /* st_info value of the symbol */
};

/* This structure represents a symbol which need relocation */
struct rel_symbol
{
	struct hlist_node link;

	char *       name;                  /* symbol name */
	unsigned int rel_offset;            /* relocation offset */
};

/* This structure represents an elf module loaded in the memory */
struct elf_module
{
	//struct hlist_node link;

	struct elf_module * exe;            /* point to the executable module */
	struct elf_module * dl;             /* point to the dynamic linker */
	struct elf_module * libc;           /* point to the libc.so */
	bool         is_exe;                /* 1 means executable file, 0 means shared library */
	bool         is_dl;                 /* 1 means it is the dynamic linker, 0 means no */
	bool         is_libc;               /* 1 means it is libc.so, 0 means no */
	char *       name;                  /* module name */
	char *       dl_name;               /* dynamic linker name */
	struct task_struct *task;           /* which task loads the module */
	unsigned int address;               /* load adress of this module */
	unsigned int size;                  /* size of the module */
	unsigned int shstr_table;           /* section header string table */
	unsigned int str_table;             /* string table address */
	unsigned int sym_table;             /* symbol table address */
	unsigned int sym_num;               /* number of the symbols in the symbol table */
	unsigned int str_table_size;        /* size of the string table */
	unsigned int entry;                 /* module entry point */
	unsigned int rel_plt;               /* address of the rel.plt section */
	unsigned int rel_plt_num;           /* number of the entries in the rel.plt section */
	unsigned int rdebug;                /* r_debug structure address */
	unsigned int l_map;                 /* link_map structure address */
	unsigned int got;                   /* the GOT address */

	/* the following fields contains the information about symbol hash tables, please refer to ELF spec */
	unsigned int num_buckets;           /* number of the buckets */
	unsigned int num_chains;            /* number of the chains */
	unsigned int buckets;               /* address of buckets */
	unsigned int chains;                /* address of chains */

	//bool         is_hooked;
	//bool         hook_func_defined;     /* one of the hooked function is defined in this module */

	//unsigned int undef_syms_num;                  /* number of the symbols in the undef_syms field */
	//struct hlist_head undef_syms[SYM_HASH_SIZE];    /* undefined symbols in the module */

	//unsigned int rel_syms_num;                    /* number of the symbols stored in the rel_syms field */
	//struct hlist_head rel_syms[SYM_HASH_SIZE];      /* symbols in relocation section with R_ARM_JUMP_SLOT type, only for .so */
};

struct module_info
{
	struct hlist_node link;
	struct elf_module *module;
};

struct displaced_desc;

typedef int (*pre_displaced_handler_t)(struct pt_regs * regs, struct displaced_desc * dsc);
typedef int (*post_displaced_handler_t)(struct pt_regs * regs, struct displaced_desc * dsc);

#if 0
union bkpt_handler
{
	ufunc_hook_entry_handler_t entry_handler;       /* callback function to call when this breakpoint is hit */
	ufunc_hook_exit_handler_t  exit_handler;        /* callback function to call when the original instruction is executed */
} ;
#endif

/* This structure represents a breakpoint */
struct uhook_breakpoint
{
	struct hlist_node link;

	const char * func_name;                         /* symbol function if user specified */
	unsigned int address;                           /* virtual address of the breakpoint */
	enum FUNC_CODE func_code;                       /* the function code */
	unsigned int orig_inst;                         /* original instruction */
	unsigned int orig_inst_size;                    /* original instruction size */
	unsigned int displaced_inst_addr;               /* the start address of the displaced instruction */
	unsigned int displaced_inst_size;               /* total size of the displaced instructions */

	ufunc_hook_pre_handler_t    pre_handler;
	ufunc_hook_post_handler_t   post_handler;
	ufunc_hook_return_handler_t return_handler;
};

/* This structure represents an area containing several displaced instructions */
struct displaced_inst_area
{
	struct list_head link;

	unsigned int address;                           /* start virtual address */
	unsigned int size;                              /* size of the area */
	unsigned int slot_size;                         /* size of each slot */
	long * bitmap;
	unsigned int bitmap_size;                       /* bitmap size in bytes */

	spinlock_t lock;                                /* lock for displaced instruction area operation */

};

/* This structure represents a process containing the breakpoints */
struct uhook_process
{
	struct hlist_node link;

	struct task_struct * task;                                /* pointer to the thread group task */
	struct module_info * exe_info;                            /* pointer to the executable module itself, not the shared libraries */
	struct hlist_head module_info_hash[MODULE_HASH_SIZE];     /* current modules loaded by the process */

	struct hlist_head bkpt_list[BKPT_HASH_SIZE];              /* breakpoint hash list */
	struct list_head  hook_list;                              /* user function hooks list */
	//struct list_head  dia_list;                               /* area list which contains the displaced instructions */

	struct displaced_inst_area * dia;
};

enum fh_step
{
	FUNC_ENTRY,                           /* function is calling */
	FUNC_EXIT,                            /* function is returned */
};

struct displaced_inst_s
{
	unsigned int inst;                    /* the displaced instrution */
	int          size;                    /* the displaced instruction size, in byte */
};

struct displaced_desc
{
	//unsigned int backup_regs[18];            /* backup value for registers */
	struct pt_regs orig_regs;
	unsigned int rd;

	struct displaced_inst_s displaced_insts[10];
	//unsigned int displaced_insts[10];
	//u16 thumb16_displaced_insts[10];
	//u32 thumb32_displaced_insts[10];
	unsigned int displaced_insts_num;
	unsigned int displaced_insts_addr;

	unsigned int orig_inst;                 /* original instruction */
	unsigned int orig_inst_size;            /* size of the original instruction */
	unsigned int orig_inst_addr;            /* address of the original instruction */

	unsigned int dia_slot_addr;             /* address of the displaced instructions */
	unsigned int dia_slot_end_addr;         /* end address of the displaced instrutions */

	pre_displaced_handler_t   pre_handler;
	post_displaced_handler_t  post_handler;
};

/* This structure represents a thread in the hooked process */
struct uhook_thread
{
	struct hlist_node         link;
	//unsigned int             fcode;
	enum   fh_step            fstep;           /* next step we will handle for the function */
	struct task_struct      * task;            /* which task belongs to */
	struct uhook_process    * proc;            /* which hooked process the thread belongs to */
	struct uhook_breakpoint * active_bkpt;     /* active breakpoint of the thread */
	struct displaced_desc     dsc;             /* information stored for the displace stepping */
	struct pt_regs            entry_regs;      /* the saved registers when calling the function */
	unsigned long long        entry_ts;        /* the saved timestamp when calling the function */
};

#define R_ARM_JUMP_SLOT 22

struct link_map
{
    uintptr_t l_addr;
    char * l_name;
    uintptr_t l_ld;
    struct link_map * l_next;
    struct link_map * l_prev;
};

struct r_debug
{
    int32_t r_version;
    struct link_map * r_map;
    void (*r_brk)(void);
    int32_t r_state;
    uintptr_t r_ldbase;
};

#if 0
struct replaced_inst
{
	struct hlist_node    link;
	pid_t                tgid;              /* the process where the instruction locates */
	unsigned int         pc;                /* where the instruction is replaced */
	int                  inst_size;         /* the replaced instruction size */
	unsigned int         inst;              /* the replaced instruction */
	//int                  bp_inst_size;      /* the breakpoint instruction size */
	//unsigned int         bp_inst;           /* the breakpoint instruction */
};
#endif

extern int raw_register_user_func_hook(struct ufunc_hook *hook);
extern int raw_unregister_user_func_hook(struct ufunc_hook *hook);

#define TP_BP_ARM_MASK                       0xffff0000
#define TP_BP_THUMB_MASK                     0xde80

#define TP_BP_ARM_ENTRY_FLAG                 0x8000
#define TP_BP_THUMB_ENTRY_FLAG               0x40

#endif /* _PX_UFUNC_HOOK_INTERNAL_H_ */
