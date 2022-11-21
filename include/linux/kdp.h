#ifndef _KDP_H
#define _KDP_H

#ifndef __ASSEMBLY__
#ifndef LINKER_SCRIPT
#include <linux/rkp_entry.h>
#ifdef CONFIG_KDP_NS
#include <linux/mount.h>
#endif

#ifdef CONFIG_RKP_KDP
#define CRED_JAR_RO		"cred_jar_ro"
#define TSEC_JAR		"tsec_jar"
#define VFSMNT_JAR		"vfsmnt_cache"

#define rocred_uc_read(x) atomic_read(x->use_cnt)
#define rocred_uc_inc(x)  atomic_inc(x->use_cnt)
#define rocred_uc_dec_and_test(x) atomic_dec_and_test(x->use_cnt)
#define rocred_uc_inc_not_zero(x) atomic_inc_not_zero(x->use_cnt)
#define rocred_uc_set(x,v) atomic_set(x->use_cnt,v)

extern int rkp_cred_enable;
extern char __rkp_ro_start[], __rkp_ro_end[];
extern struct cred init_cred;
extern struct task_security_struct init_sec;
extern int security_integrity_current(void);


#define RKP_RO_AREA __attribute__((section (".rkp.prot.page")))

typedef struct kdp_init
{
	u32 credSize;
	u32 cred_task;
	u32 mm_task;
	u32 uid_cred;
	u32 euid_cred;
	u32 gid_cred;
	u32 egid_cred;
	u32 bp_pgd_cred;
	u32 bp_task_cred;
	u32 type_cred;
	u32 security_cred;
	u32 pid_task;
	u32 rp_task;
	u32 comm_task;
	u32 pgd_mm;
	u32 usage_cred;
	u32 task_threadinfo;
	u32 sp_size;
	u32 bp_cred_secptr;
} kdp_init_t;

/*Check whether the address belong to Cred Area*/
static inline u8 rkp_ro_page(unsigned long addr)
{
		if(!rkp_cred_enable)
				return (u8)0;
		if((addr == ((unsigned long)&init_cred)) || 
						(addr == ((unsigned long)&init_sec)))
				return (u8)1;
		else
				return rkp_is_pg_protected(addr);
}
#else
#define RKP_RO_AREA   
#define security_integrity_current()  0
#endif  /* CONFIG_RKP_KDP */

#endif /* LINKER_SCRIPT */
#endif /*__ASSEMBLY__*/
#endif /*_KDP_H*/