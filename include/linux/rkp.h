#ifndef _RKP_H
#define _RKP_H

#ifndef __ASSEMBLY__
#ifndef LINKER_SCRIPT
#include <linux/uh.h>
/* uH_RKP Command ID */
enum __RKP_CMD_ID{
	RKP_START = 0x01,
	RKP_DEFERRED_START = 0x02,
	RKP_WRITE_PGT1 = 0x03,
	RKP_WRITE_PGT2 = 0x04,
	RKP_WRITE_PGT3 = 0x05,
	RKP_EMULT_TTBR0 = 0x06,
	RKP_EMULT_TTBR1 = 0x07,
	RKP_EMULT_DORESUME = 0x08,
	RKP_FREE_PGD = 0x09,
	RKP_NEW_PGD = 0x0A,
	RKP_KASLR_MEM = 0x0B,
	RKP_FIMC_VERIFY = 0x0C,
	/* CFP cmds */
	RKP_JOPP_INIT = 0x0D,
	RKP_ROPP_INIT = 0x0E,
	RKP_ROPP_SAVE = 0x0F,
	RKP_ROPP_RELOAD = 0x10,
	/* RKP robuffer cmds*/
	RKP_RKP_ROBUFFER_ALLOC = 0x11,
	RKP_RKP_ROBUFFER_FREE = 0x12,
	RKP_GET_RO_BITMAP = 0x13,
	RKP_GET_DBL_BITMAP = 0x14,
	RKP_GET_RKP_GET_BUFFER_BITMAP = 0x15,
	/* dynamic load */
	RKP_DYNAMIC_LOAD = 0x20,
	RKP_MODULE_LOAD = 0x21,
	RKP_BFP_LOAD = 0x22,
	/* and KDP cmds */
	RKP_KDP_X40 = 0x40,
	RKP_KDP_X41 = 0x41,
	RKP_KDP_X42 = 0x42,
	RKP_KDP_X43 = 0x43,
	RKP_KDP_X44 = 0x44,
	RKP_KDP_X45 = 0x45,
	RKP_KDP_X46 = 0x46,
	RKP_KDP_X47 = 0x47,
	RKP_KDP_X48 = 0x48,
	RKP_KDP_X49 = 0x49,
	RKP_KDP_X4A = 0x4A,
	RKP_KDP_X4B = 0x4B,
	RKP_KDP_X4C = 0x4C,
	RKP_KDP_X4D = 0x4D,
	RKP_KDP_X4E = 0x4E,
	RKP_KDP_X4F = 0x4F,
	RKP_KDP_X50 = 0x50,
	RKP_KDP_X51 = 0x51,
	RKP_KDP_X52 = 0x52,
	RKP_KDP_X53 = 0x53,
	RKP_KDP_X54 = 0x54,
	RKP_KDP_X55 = 0x55,
	RKP_KDP_X56 = 0x56,
	RKP_KDP_X60 = 0x60,
#ifdef CONFIG_RKP_TEST
	CMD_ID_TEST_GET_PAR = 0x81,
	CMD_ID_TEST_GET_RO = 0x83,
	CMD_ID_TEST_GET_VA_XN,
	CMD_ID_TEST_GET_VMM_INFO,
#endif
};

#ifdef CONFIG_RKP_TEST
#define RKP_INIT_MAGIC		0x5afe0002
#else
#define RKP_INIT_MAGIC		0x5afe0001
#endif

#define RKP_FIMC_FAIL		0x10
#define RKP_FIMC_SUCCESS	0xa5

#define CRED_JAR_RO		"cred_jar_ro"
#define TSEC_JAR		"tsec_jar"
#define VFSMNT_JAR		"vfsmnt_cache"

#define SPARSE_UNIT_BIT (30)
#define SPARSE_UNIT_SIZE (1<<SPARSE_UNIT_BIT)

#define FIMC_LIB_OFFSET_VA		(VMALLOC_START + 0xF6000000 - 0x8000000)
#define FIMC_LIB_START_VA		(FIMC_LIB_OFFSET_VA + 0x04000000)
#define FIMC_VRA_LIB_SIZE		(0x80000)
#define FIMC_DDK_LIB_SIZE		(0x400000)
#define FIMC_RTA_LIB_SIZE		(0x300000)
#define FIMC_LIB_SIZE	

#define RKP_MODULE_PXN_CLEAR	0x1
#define RKP_MODULE_PXN_SET		0x2

#define RKP_BPF_JIT_LOAD		0x0
#define RKP_BPF_JIT_FREE		0x1

struct rkp_init { //copy from uh (app/rkp/rkp.h)
	u32 magic;
	u64 vmalloc_start;
	u64 vmalloc_end;
	u64 init_mm_pgd;
	u64 id_map_pgd;
	u64 zero_pg_addr;
	u64 rkp_pgt_bitmap;
	u64 rkp_dbl_bitmap;
	u32 rkp_bitmap_size;
	u32 no_fimc_verify;
	u64 fimc_phys_addr;
	u64 _text;
	u64 _etext;
	u64 extra_memory_addr;
	u32 extra_memory_size;
	u64 physmap_addr; //not used. what is this for?
	u64 _srodata;
	u64 _erodata;
	u32 large_memory;
	u64 tramp_pgd;
	u64 tramp_valias;
};

struct module_info {
	u64 base_va;
	u64 vm_size;
	u64 core_base_va;
	u64 core_text_size;
	u64 core_ro_size;
	u64 init_base_va;
	u64 init_text_size;
};

#ifdef CONFIG_RKP_CFP_ROPP_SYSREGKEY
struct ropp_init {
	u64 ropp_magic;
	u64 ropp_master_key;
	u64 ti_rrk;
	u64 ts_stack;
	u64 ts_tasks;
	u64 ts_pid;
	u64 ts_thread_group;
	u64 ts_comm;
	u64 ts_thread;
	u64 cpu_fp;
};
#endif

typedef struct sparse_bitmap_for_kernel {
	u64 start_addr;
	u64 end_addr;
	u64 maxn;
	char **map;
} sparse_bitmap_for_kernel_t;

#ifdef CONFIG_RKP_KDP
typedef struct kdp_init_struct {
	u32 credSize;
	u32 sp_size;
	u32 pgd_mm;
	u32 uid_cred;
	u32 euid_cred;
	u32 gid_cred;
	u32 egid_cred;
	u32 bp_pgd_cred;
	u32 bp_task_cred;
	u32 type_cred;
	u32 security_cred;
	u32 usage_cred;
	u32 cred_task;
	u32 mm_task;
	u32 pid_task;
	u32 rp_task;
	u32 comm_task;
	u32 bp_cred_secptr;
	u32 task_threadinfo;
	u64 verifiedbootstate;
	struct {
		u64 empty;
		u64 ss_initialized_va;
	} selinux;
} kdp_init_t;
#endif  /* CONFIG_RKP_KDP */

#ifdef CONFIG_RKP_NS_PROT
typedef struct ns_param {
	u32 ns_buff_size;
	u32 ns_size;
	u32 bp_offset;
	u32 sb_offset;
	u32 flag_offset;
	u32 data_offset;
}ns_param_t;

#define rkp_ns_fill_params(nsparam,buff_size,size,bp,sb,flag,data)	\
do {						\
	nsparam.ns_buff_size = (u64)buff_size;		\
	nsparam.ns_size  = (u64)size;		\
	nsparam.bp_offset = (u64)bp;		\
	nsparam.sb_offset = (u64)sb;		\
	nsparam.flag_offset = (u64)flag;		\
	nsparam.data_offset = (u64)data;		\
} while(0)
#endif

extern sparse_bitmap_for_kernel_t* rkp_s_bitmap_ro;
extern sparse_bitmap_for_kernel_t* rkp_s_bitmap_dbl;
extern sparse_bitmap_for_kernel_t* rkp_s_bitmap_buffer;

#ifdef CONFIG_KNOX_KAP
extern int boot_mode_security;
#endif

#ifdef CONFIG_RKP_KDP
extern int rkp_cred_enable;
#endif

typedef struct rkp_init rkp_init_t;
extern u8 rkp_started;

#ifdef CONFIG_RKP_DMAP_PROT
static inline void dmap_prot(u64 addr,u64 order,u64 val)
{
	if(rkp_cred_enable)
		uh_call(UH_APP_RKP, RKP_KDP_X4A, order, val, 0, 0);
}
#endif

static inline u64 uh_call_static(u64 app_id, u64 cmd_id, u64 arg1){
	register u64 ret __asm__("x0") = app_id;
	register u64 cmd __asm__("x1") = cmd_id;
	register u64 arg __asm__("x2") = arg1;

	__asm__ volatile (
		"smc	0\n"
		: "+r"(ret), "+r"(cmd), "+r"(arg)
		::"memory"
	);

	return ret;
}

// void *rkp_ro_alloc(void);
static inline void *rkp_ro_alloc(void){
	u64 addr = 0xDEAD;
	uh_call_static(UH_APP_RKP, RKP_RKP_ROBUFFER_ALLOC, (u64)&addr);
	if(!addr)
		return 0;
	if (addr == 0xDEAD) {
		pr_info("rkp : %llx\n", (u64)&addr);
		BUG_ON(1);
	}
	return (void *)__phys_to_virt(addr);
}

static inline void rkp_ro_free(void *free_addr){
	uh_call_static(UH_APP_RKP, RKP_RKP_ROBUFFER_FREE, (u64)free_addr);
}


static inline void rkp_deferred_init(void){
	uh_call(UH_APP_RKP, RKP_DEFERRED_START, 0, 0, 0, 0);
}

extern s64 memstart_addr;
static inline void rkp_robuffer_init(void)
{
	uh_call(UH_APP_RKP, RKP_GET_RKP_GET_BUFFER_BITMAP, (u64)&rkp_s_bitmap_buffer, (u64)memstart_addr, 0, 0);
}

static inline u8 rkp_check_bitmap(u64 pa, sparse_bitmap_for_kernel_t *kernel_bitmap, u8 overflow_ret, u8 uninitialized_ret){
	u8 val;
	u64 offset, map_loc, bit_offset;
	char *map;

	if(!kernel_bitmap || !kernel_bitmap->map)
		return uninitialized_ret;

	offset = pa - kernel_bitmap->start_addr;
	map_loc = ((offset % SPARSE_UNIT_SIZE) / PAGE_SIZE) >> 3;
	bit_offset = ((offset % SPARSE_UNIT_SIZE) / PAGE_SIZE) % 8;

	if(kernel_bitmap->maxn <= (offset >> SPARSE_UNIT_BIT)) 
		return overflow_ret;

	map = kernel_bitmap->map[(offset >> SPARSE_UNIT_BIT)];
	if(!map)
		return uninitialized_ret;

	val = ((u8)map[map_loc] >> bit_offset) & ((u8)1);
	return val;
}

static inline unsigned int is_rkp_ro_page(u64 va){
	return rkp_check_bitmap(__pa(va), rkp_s_bitmap_buffer, 0, 0);
}

static inline u8 rkp_is_pg_protected(u64 va){
	return rkp_check_bitmap(__pa(va), rkp_s_bitmap_ro, 1, 0);
}

static inline u8 rkp_is_pg_dbl_mapped(u64 pa){
	return rkp_check_bitmap(pa, rkp_s_bitmap_dbl, 0, 0);
}
#endif // LINKER_SCRIPT
#endif //__ASSEMBLY__
#endif //_RKP_H
