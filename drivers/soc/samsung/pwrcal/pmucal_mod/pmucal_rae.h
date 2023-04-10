#ifndef __PMUCAL_RAE_H__
#define __PMUCAL_RAE_H__

#include <asm/io.h>

/* for phy2virt address mapping */
struct p2v_map {
	phys_addr_t pa;
	void __iomem *va;
};

#define DEFINE_PHY(p) { .pa = p }

/* APIs to be supported to PMUCAL common logics */
extern int pmucal_rae_init(void);
extern int pmucal_rae_handle_seq(struct pmucal_seq *seq, unsigned int seq_size);
extern void pmucal_rae_save_seq(struct pmucal_seq *seq, unsigned int seq_size);
extern int pmucal_rae_restore_seq(struct pmucal_seq *seq, unsigned int seq_size);
extern int pmucal_rae_phy2virt(struct pmucal_seq *seq, unsigned int seq_size);

#endif
