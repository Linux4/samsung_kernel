#ifndef __REGS_ADDR_H
#define __REGS_ADDR_H

enum {
	REGS_ADDR_MPMU,
	REGS_ADDR_APMU,
	REGS_ADDR_APBC,
	REGS_ADDR_CIU,
	REGS_ADDR_DMCU,
	REGS_ADDR_ICU,
	REGS_ADDR_GIC_DIST,
	REGS_ADDR_DCIU,
	REGS_ADDR_MAX,
};

void regs_addr_iomap(void);
phys_addr_t regs_addr_get_pa(unsigned int id);
void __iomem *regs_addr_get_va(unsigned int id);

#endif
