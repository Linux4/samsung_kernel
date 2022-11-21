#ifndef __PWM_GPIO_BANG_H__
#define __PWM_GPIO_BANG_H__

struct pwm_gc {
	struct device *dev;
	u64 def_period;
	u64 def_duty_cycle;
	bool def_enabled;
	unsigned int gp_clk;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_suspend;
};

#define GP_CLK_M_DEFAULT		2
#define GP_CLK_N_DEFAULT		92
#define GP_CLK_D_DEFAULT		46  /* 50% duty cycle */

#define BASE_IP_CLK			1200000

#define __inp(port) ioread8(port)
#define __inpw(port) ioread16(port)
#define __inpdw(port) ioread32(port)
#define __outp(port, val) iowrite8(val, port)
#define __outpw(port, val) iowrite16(val, port)
#define __outpdw(port, val) iowrite32(val, port)

#define in_dword(addr)              (__inpdw(addr))
#define in_dword_masked(addr, mask) (__inpdw(addr) & (mask))
#define out_dword(addr, val)        __outpdw(addr, val)
#define out_dword_masked(io, mask, val, shadow) \
	((void) out_dword(io, \
	((shadow & (unsigned int)(~(mask))) \
	| ((unsigned int)((val) & (mask))))))
#define out_dword_masked_ns(io, mask, val, current_reg_content) \
	((void) out_dword(io, \
	((current_reg_content & (unsigned int)(~(mask))) \
	| ((unsigned int)((val) & (mask))))))

extern void __iomem *virt_mmss_gp1_base;
#define MSM_GCC_GPx_BASE		0x00164000

#define HWIO_CAMSS_GPx_CBCR_ADDR	((void __iomem *)(virt_mmss_gp1_base + 0x0))	//GCC_GP1_CBCR
#define HWIO_GPx_CMD_RCGR_ADDR		((void __iomem *)(virt_mmss_gp1_base + 0x4))	//GCC_GP1_CMD_RCGR
#define HWIO_GPx_CFG_RCGR_ADDR		((void __iomem *)(virt_mmss_gp1_base + 0x8))	//GCC_GP1_CFG_RCGR
#define HWIO_GPx_M_REG_ADDR		((void __iomem *)(virt_mmss_gp1_base + 0xc))	//GCC_GP1_M
#define HWIO_GPx_N_REG_ADDR		((void __iomem *)(virt_mmss_gp1_base + 0x10))	//GCC_GP1_N
#define HWIO_GPx_D_REG_ADDR		((void __iomem *)(virt_mmss_gp1_base + 0x14))	//GCC_GP1_D

#define HWIO_GP_MD_REG_RMSK		0xffffffff
#define HWIO_GP_N_REG_RMSK		0xffffffff

#define HWIO_GP_MD_REG_M_VAL_BMSK		0xff
#define HWIO_GP_MD_REG_M_VAL_SHFT		0
#define HWIO_GP_MD_REG_D_VAL_BMSK		0xff
#define HWIO_GP_MD_REG_D_VAL_SHFT		0
#define HWIO_GP_N_REG_N_VAL_BMSK		0xff
#define HWIO_GP_SRC_SEL_VAL_BMSK		0x700
#define HWIO_GP_SRC_SEL_VAL_SHFT		8
#define HWIO_GP_SRC_DIV_VAL_BMSK		0x1f
#define HWIO_GP_SRC_DIV_VAL_SHFT		0
#define HWIO_GP_MODE_VAL_BMSK			0x3000
#define HWIO_GP_MODE_VAL_SHFT			12

#define HWIO_CLK_ENABLE_VAL_BMSK		0x1
#define HWIO_CLK_ENABLE_VAL_SHFT		0
#define HWIO_UPDATE_VAL_BMSK			0x1
#define HWIO_UPDATE_VAL_SHFT			0
#define HWIO_ROOT_EN_VAL_BMSK			0x2
#define HWIO_ROOT_EN_VAL_SHFT			1

#define HWIO_GPx_CMD_RCGR_IN		\
		in_dword_masked(HWIO_GPx_CMD_RCGR_ADDR, HWIO_GP_N_REG_RMSK)
#define HWIO_GPx_CMD_RCGR_OUTM(m, v)	\
	out_dword_masked_ns(HWIO_GPx_CMD_RCGR_ADDR, m, v, HWIO_GPx_CMD_RCGR_IN)

#define HWIO_GPx_CFG_RCGR_IN		\
		in_dword_masked(HWIO_GPx_CFG_RCGR_ADDR, HWIO_GP_N_REG_RMSK)
#define HWIO_GPx_CFG_RCGR_OUTM(m, v)	\
	out_dword_masked_ns(HWIO_GPx_CFG_RCGR_ADDR, m, v, HWIO_GPx_CFG_RCGR_IN)

#define HWIO_CAMSS_GPx_CBCR_IN		\
		in_dword_masked(HWIO_CAMSS_GPx_CBCR_ADDR, HWIO_GP_N_REG_RMSK)
#define HWIO_CAMSS_GPx_CBCR_OUTM(m, v)	\
	out_dword_masked_ns(HWIO_CAMSS_GPx_CBCR_ADDR, m, v, HWIO_CAMSS_GPx_CBCR_IN)

#define HWIO_GPx_D_REG_IN		\
		in_dword_masked(HWIO_GPx_D_REG_ADDR, HWIO_GP_MD_REG_RMSK)

#define HWIO_GPx_D_REG_OUTM(m, v)\
	out_dword_masked_ns(HWIO_GPx_D_REG_ADDR, m, v, HWIO_GPx_D_REG_IN)

#define HWIO_GPx_M_REG_IN		\
		in_dword_masked(HWIO_GPx_M_REG_ADDR, HWIO_GP_MD_REG_RMSK)
#define HWIO_GPx_M_REG_OUTM(m, v)\
	out_dword_masked_ns(HWIO_GPx_M_REG_ADDR, m, v, HWIO_GPx_M_REG_IN)

#define HWIO_GPx_N_REG_IN		\
		in_dword_masked(HWIO_GPx_N_REG_ADDR, HWIO_GP_N_REG_RMSK)
#define HWIO_GPx_N_REG_OUTM(m, v)	\
	out_dword_masked_ns(HWIO_GPx_N_REG_ADDR, m, v, HWIO_GPx_N_REG_IN)

#define __msmhwio_outm(hwiosym, mask, val)  HWIO_##hwiosym##_OUTM(mask, val)
#define HWIO_OUTM(hwiosym, mask, val)	__msmhwio_outm(hwiosym, mask, val)



#endif /* __PWM_GPIO_BANG_H__ */
