#ifndef __SCI_IOMAP_H__
#define __SCI_IOMAP_H__

#ifndef __ASSEMBLER__

struct iomap_sprd {
	unsigned long vaddr;
	unsigned long paddr;
	unsigned long length;
};

extern struct iotable_sprd io_addr_sprd;
struct iotable_sprd {
	struct iomap_sprd AHB;
#define SPRD_AHB_PHYS		io_addr_sprd.AHB.paddr
#define SPRD_AHB_BASE		io_addr_sprd.AHB.vaddr
#define SPRD_AHB_SIZE		io_addr_sprd.AHB.length

/*	struct iomap_sprd HWLOCK0;
#define SPRD_HWLOCK0_PHYS	io_addr_sprd.HWLOCK0.paddr
#define SPRD_HWLOCK0_BASE	io_addr_sprd.HWLOCK0.vaddr
#define SPRD_HWLOCK0_SIZE	io_addr_sprd.HWLOCK0.length
*/
	struct iomap_sprd PUB;
#define SPRD_PUB_PHYS		io_addr_sprd.PUB.paddr
#define SPRD_PUB_BASE		io_addr_sprd.PUB.vaddr
#define SPRD_PUB_SIZE		io_addr_sprd.PUB.length

	struct iomap_sprd AONAPB;
#define SPRD_AONAPB_PHYS	io_addr_sprd.AONAPB.paddr
#define SPRD_AONAPB_BASE	io_addr_sprd.AONAPB.vaddr
#define SPRD_AONAPB_SIZE	io_addr_sprd.AONAPB.length

	struct iomap_sprd PMU;
#define SPRD_PMU_PHYS		io_addr_sprd.PMU.paddr
#define SPRD_PMU_BASE		io_addr_sprd.PMU.vaddr
#define SPRD_PMU_SIZE		io_addr_sprd.PMU.length

	struct iomap_sprd MMAHB;
#define SPRD_MMAHB_PHYS		io_addr_sprd.MMAHB.paddr
#define SPRD_MMAHB_BASE		io_addr_sprd.MMAHB.vaddr
#define SPRD_MMAHB_SIZE		io_addr_sprd.MMAHB.length

	struct iomap_sprd MMCKG;
#define SPRD_MMCKG_PHYS		io_addr_sprd.MMCKG.paddr
#define SPRD_MMCKG_BASE		io_addr_sprd.MMCKG.vaddr
#define SPRD_MMCKG_SIZE		io_addr_sprd.MMCKG.length

#if defined(CONFIG_MACH_SP9830I) || defined(CONFIG_ARCH_SCX30G2) || defined(CONFIG_ARCH_SCX35LT8)
	struct iomap_sprd CODECAHB;
#define SPRD_CODECAHB_PHYS	io_addr_sprd.CODECAHB.paddr
#define SPRD_CODECAHB_BASE	io_addr_sprd.CODECAHB.vaddr
#define SPRD_CODECAHB_SIZE      io_addr_sprd.CODECAHB.length
#endif

#if defined(CONFIG_ARCH_SCX30G3)
	struct iomap_sprd CRYPTO;
#define SPRD_CRYPTO_PHYS	io_addr_sprd.CRYPTO.paddr
#define SPRD_CRYPTO_BASE	io_addr_sprd.CRYPTO.vaddr
#define SPRD_CRYPTO_SIZE	io_addr_sprd.CRYPTO.length
#endif

	struct iomap_sprd APBREG;
#define SPRD_APBREG_PHYS	io_addr_sprd.APBREG.paddr
#define SPRD_APBREG_BASE	io_addr_sprd.APBREG.vaddr
#define SPRD_APBREG_SIZE	io_addr_sprd.APBREG.length

	struct iomap_sprd GPUAPB;
#define SPRD_GPUAPB_PHYS	io_addr_sprd.GPUAPB.paddr
#define SPRD_GPUAPB_BASE	io_addr_sprd.GPUAPB.vaddr
#define SPRD_GPUAPB_SIZE	io_addr_sprd.GPUAPB.length

	struct iomap_sprd APBCKG;
#define SPRD_APBCKG_PHYS	io_addr_sprd.APBCKG.paddr
#define SPRD_APBCKG_BASE	io_addr_sprd.APBCKG.vaddr
#define SPRD_APBCKG_SIZE	io_addr_sprd.APBCKG.length

/* seems not good to keep */
	struct iomap_sprd DMA0;
#define SPRD_DMA0_PHYS		io_addr_sprd.DMA0.paddr
#define SPRD_DMA0_BASE		io_addr_sprd.DMA0.vaddr
#define SPRD_DMA0_SIZE		io_addr_sprd.DMA0.length

	struct iomap_sprd WDG;
#define SPRD_WDG_PHYS	(io_addr_sprd.WDG.paddr)
#define SPRD_WDG_BASE	(io_addr_sprd.WDG.vaddr)
#define SPRD_WDG_SIZE	(io_addr_sprd.WDG.length)

	struct iomap_sprd HWSPINLOCK0;
#define SPRD_HWSPINLOCK0_PHYS	io_addr_sprd.HWSPINLOCK0.paddr
#define SPRD_HWSPINLOCK0_BASE	io_addr_sprd.HWSPINLOCK0.vaddr
#define SPRD_HWSPINLOCK0_SIZE	io_addr_sprd.HWSPINLOCK0.length

	struct iomap_sprd HWSPINLOCK1;
#define SPRD_HWSPINLOCK1_PHYS	io_addr_sprd.HWSPINLOCK1.paddr
#define SPRD_HWSPINLOCK1_BASE	io_addr_sprd.HWSPINLOCK1.vaddr
#define SPRD_HWSPINLOCK1_SIZE	io_addr_sprd.HWSPINLOCK1.length

/************** 64BIT private map **************/
#ifdef CONFIG_64BIT
	struct iomap_sprd APCKG;
#define SPRD_APCKG_PHYS	io_addr_sprd.APCKG.paddr
#define SPRD_APCKG_BASE	io_addr_sprd.APCKG.vaddr
#define SPRD_APCKG_SIZE	io_addr_sprd.APCKG.length

	struct iomap_sprd AONDMA;
#define SPRD_AONDMA_PHYS	io_addr_sprd.AONDMA.paddr
#define SPRD_AONDMA_BASE	io_addr_sprd.AONDMA.vaddr
#define SPRD_AONDMA_SIZE	io_addr_sprd.AONDMA.length

	struct iomap_sprd PWM;
#define SPRD_PWM_PHYS	io_addr_sprd.PWM.paddr
#define SPRD_PWM_BASE	io_addr_sprd.PWM.vaddr
#define SPRD_PWM_SIZE	io_addr_sprd.PWM.length

		struct iomap_sprd ISP;
#define SPRD_ISP_PHYS		io_addr_sprd.ISP.paddr
#define SPRD_ISP_BASE		io_addr_sprd.ISP.vaddr
#define SPRD_ISP_SIZE		io_addr_sprd.ISP.length
	
		struct iomap_sprd CSI2;
#define SPRD_CSI2_PHYS		io_addr_sprd.CSI2.paddr
#define SPRD_CSI2_BASE		io_addr_sprd.CSI2.vaddr
#define SPRD_CSI2_SIZE		io_addr_sprd.CSI2.length

		struct iomap_sprd DCAM;
#define SPRD_DCAM_PHYS		io_addr_sprd.DCAM.paddr
#define SPRD_DCAM_BASE		io_addr_sprd.DCAM.vaddr
#define SPRD_DCAM_SIZE		io_addr_sprd.DCAM.length

		struct iomap_sprd PIN;
#define SPRD_PIN_PHYS		io_addr_sprd.PIN.paddr
#define SPRD_PIN_BASE		io_addr_sprd.PIN.vaddr
#define SPRD_PIN_SIZE		io_addr_sprd.PIN.length
	
		struct iomap_sprd GPIO;
#define SPRD_GPIO_PHYS		io_addr_sprd.GPIO.paddr
#define SPRD_GPIO_BASE		io_addr_sprd.GPIO.vaddr
#define SPRD_GPIO_SIZE		io_addr_sprd.GPIO.length

		struct iomap_sprd INTC2;
#define SPRD_INTC2_PHYS		io_addr_sprd.INTC2.paddr
#define SPRD_INTC2_BASE		io_addr_sprd.INTC2.vaddr
#define SPRD_INTC2_SIZE		io_addr_sprd.INTC2.length

		struct iomap_sprd SYSCNT;
#define SPRD_SYSCNT_PHYS        io_addr_sprd.SYSCNT.paddr
#define SPRD_SYSCNT_BASE        io_addr_sprd.SYSCNT.vaddr
#define SPRD_SYSCNT_SIZE        io_addr_sprd.SYSCNT.length

		struct iomap_sprd AONCKG;
#define SPRD_AONCKG_PHYS        io_addr_sprd.AONCKG.paddr
#define SPRD_AONCKG_BASE        io_addr_sprd.AONCKG.vaddr
#define SPRD_AONCKG_SIZE        io_addr_sprd.AONCKG.length

		struct iomap_sprd CORE;
#define SPRD_CORE_PHYS          io_addr_sprd.CORE.paddr
#define SPRD_CORE_BASE          io_addr_sprd.CORE.vaddr
#define SPRD_CORE_SIZE          io_addr_sprd.CORE.length

		struct iomap_sprd GPUCKG;
#define SPRD_GPUCKG_PHYS        io_addr_sprd.GPUCKG.paddr
#define SPRD_GPUCKG_BASE        io_addr_sprd.GPUCKG.vaddr
#define SPRD_GPUCKG_SIZE        io_addr_sprd.GPUCKG.length

		struct iomap_sprd INT;
#define SPRD_INT_PHYS           io_addr_sprd.INT.paddr
#define SPRD_INT_BASE           io_addr_sprd.INT.vaddr
#define SPRD_INT_SIZE           io_addr_sprd.INT.length

		struct iomap_sprd INTC0;
#define SPRD_INTC0_PHYS         io_addr_sprd.INTC0.paddr
#define SPRD_INTC0_BASE         io_addr_sprd.INTC0.vaddr
#define SPRD_INTC0_SIZE         io_addr_sprd.INTC0.length

		struct iomap_sprd INTC1;
#define SPRD_INTC1_PHYS         io_addr_sprd.INTC1.paddr
#define SPRD_INTC1_BASE         io_addr_sprd.INTC1.vaddr
#define SPRD_INTC1_SIZE         io_addr_sprd.INTC1.length

		struct iomap_sprd INTC3;
#define SPRD_INTC3_PHYS         io_addr_sprd.INTC3.paddr
#define SPRD_INTC3_BASE         io_addr_sprd.INTC3.vaddr
#define SPRD_INTC3_SIZE         io_addr_sprd.INTC3.length

		struct iomap_sprd UIDEFUSE;
#define SPRD_UIDEFUSE_PHYS      io_addr_sprd.UIDEFUSE.paddr
#define SPRD_UIDEFUSE_BASE      io_addr_sprd.UIDEFUSE.vaddr
#define SPRD_UIDEFUSE_SIZE      io_addr_sprd.UIDEFUSE.length

		struct iomap_sprd CA7WDG;
#define SPRD_CA7WDG_PHYS        io_addr_sprd.CA7WDG.paddr
#define SPRD_CA7WDG_BASE        io_addr_sprd.CA7WDG.vaddr
#define SPRD_CA7WDG_SIZE        io_addr_sprd.CA7WDG.length

		struct iomap_sprd EIC;
#define SPRD_EIC_PHYS           io_addr_sprd.EIC.paddr
#define SPRD_EIC_BASE           io_addr_sprd.EIC.vaddr
#define SPRD_EIC_SIZE           io_addr_sprd.EIC.length

		struct iomap_sprd IPI;
#define SPRD_IPI_PHYS           io_addr_sprd.IPI.paddr
#define SPRD_IPI_BASE           io_addr_sprd.IPI.vaddr
#define SPRD_IPI_SIZE           io_addr_sprd.IPI.length

		struct iomap_sprd LPDDR2;
#define SPRD_LPDDR2_PHYS           io_addr_sprd.LPDDR2.paddr
#define SPRD_LPDDR2_BASE           io_addr_sprd.LPDDR2.vaddr
#define SPRD_LPDDR2_SIZE           io_addr_sprd.LPDDR2.length

		struct iomap_sprd UART0;
#define SPRD_UART0_PHYS           io_addr_sprd.UART0.paddr
#define SPRD_UART0_BASE           io_addr_sprd.UART0.vaddr
#define SPRD_UART0_SIZE           io_addr_sprd.UART0.length

		struct iomap_sprd UART1;
#define SPRD_UART1_PHYS           io_addr_sprd.UART1.paddr
#define SPRD_UART1_BASE           io_addr_sprd.UART1.vaddr
#define SPRD_UART1_SIZE           io_addr_sprd.UART1.length

		struct iomap_sprd UART2;
#define SPRD_UART2_PHYS           io_addr_sprd.UART2.paddr
#define SPRD_UART2_BASE           io_addr_sprd.UART2.vaddr
#define SPRD_UART2_SIZE           io_addr_sprd.UART2.length

		struct iomap_sprd UART3;
#define SPRD_UART3_PHYS           io_addr_sprd.UART3.paddr
#define SPRD_UART3_BASE           io_addr_sprd.UART3.vaddr
#define SPRD_UART3_SIZE           io_addr_sprd.UART3.length

		struct iomap_sprd UART4;
#define SPRD_UART4_PHYS           io_addr_sprd.UART4.paddr
#define SPRD_UART4_BASE           io_addr_sprd.UART4.vaddr
#define SPRD_UART4_SIZE           io_addr_sprd.UART4.length

	/* indicate base address of maps */
	unsigned long base;

/************** 32BIT private map **************/
#else /* 32BIT */
	struct iomap_sprd AONCKG;
#define SPRD_AONCKG_PHYS	io_addr_sprd.AONCKG.paddr
#define SPRD_AONCKG_BASE	io_addr_sprd.AONCKG.vaddr
#define SPRD_AONCKG_SIZE	io_addr_sprd.AONCKG.length

	struct iomap_sprd CORESIGHT;
#define SPRD_CORESIGHT_PHYS	io_addr_sprd.CORESIGHT.paddr
#define SPRD_CORESIGHT_BASE	io_addr_sprd.CORESIGHT.vaddr
#define SPRD_CORESIGHT_SIZE	io_addr_sprd.CORESIGHT.length

	struct iomap_sprd CORE;
#define SPRD_CORE_PHYS		io_addr_sprd.CORE.paddr
#define SPRD_CORE_BASE		io_addr_sprd.CORE.vaddr
#define SPRD_CORE_SIZE		io_addr_sprd.CORE.length

	struct iomap_sprd GPUCKG;
#define SPRD_GPUCKG_PHYS	io_addr_sprd.GPUCKG.paddr
#define SPRD_GPUCKG_BASE	io_addr_sprd.GPUCKG.vaddr
#define SPRD_GPUCKG_SIZE	io_addr_sprd.GPUCKG.length

	struct iomap_sprd INT;
#define SPRD_INT_PHYS		io_addr_sprd.INT.paddr
#define SPRD_INT_BASE		io_addr_sprd.INT.vaddr
#define SPRD_INT_SIZE		io_addr_sprd.INT.length

	struct iomap_sprd INTC0;
#define SPRD_INTC0_PHYS		io_addr_sprd.INTC0.paddr
#define SPRD_INTC0_BASE		io_addr_sprd.INTC0.vaddr
#define SPRD_INTC0_SIZE		io_addr_sprd.INTC0.length

	struct iomap_sprd INTC1;
#define SPRD_INTC1_PHYS		io_addr_sprd.INTC1.paddr
#define SPRD_INTC1_BASE		io_addr_sprd.INTC1.vaddr
#define SPRD_INTC1_SIZE		io_addr_sprd.INTC1.length

	struct iomap_sprd INTC2;
#define SPRD_INTC2_PHYS		io_addr_sprd.INTC2.paddr
#define SPRD_INTC2_BASE		io_addr_sprd.INTC2.vaddr
#define SPRD_INTC2_SIZE		io_addr_sprd.INTC2.length

	struct iomap_sprd INTC3;
#define SPRD_INTC3_PHYS		io_addr_sprd.INTC3.paddr
#define SPRD_INTC3_BASE		io_addr_sprd.INTC3.vaddr
#define SPRD_INTC3_SIZE		io_addr_sprd.INTC3.length

	struct iomap_sprd UIDEFUSE;
#define SPRD_UIDEFUSE_PHYS	io_addr_sprd.UIDEFUSE.paddr
#define SPRD_UIDEFUSE_BASE	io_addr_sprd.UIDEFUSE.vaddr
#define SPRD_UIDEFUSE_SIZE	io_addr_sprd.UIDEFUSE.length

	struct iomap_sprd ISP;
#define SPRD_ISP_PHYS		io_addr_sprd.ISP.paddr
#define SPRD_ISP_BASE		io_addr_sprd.ISP.vaddr
#define SPRD_ISP_SIZE		io_addr_sprd.ISP.length

	struct iomap_sprd CA7WDG;
#define SPRD_CA7WDG_PHYS	(io_addr_sprd.CA7WDG.paddr)
#define SPRD_CA7WDG_BASE	(io_addr_sprd.CA7WDG.vaddr)
#define SPRD_CA7WDG_SIZE	(io_addr_sprd.CA7WDG.length)

	struct iomap_sprd CSI2;
#define SPRD_CSI2_PHYS		io_addr_sprd.CSI2.paddr
#define SPRD_CSI2_BASE		io_addr_sprd.CSI2.vaddr
#define SPRD_CSI2_SIZE		io_addr_sprd.CSI2.length

	struct iomap_sprd EIC;
#define SPRD_EIC_PHYS		io_addr_sprd.EIC.paddr
#define SPRD_EIC_BASE		io_addr_sprd.EIC.vaddr
#define SPRD_EIC_SIZE		io_addr_sprd.EIC.length

	struct iomap_sprd IPI;
#define SPRD_IPI_PHYS		io_addr_sprd.IPI.paddr
#define SPRD_IPI_BASE		io_addr_sprd.IPI.vaddr
#define SPRD_IPI_SIZE		io_addr_sprd.IPI.length

	struct iomap_sprd DCAM;
#define SPRD_DCAM_PHYS		io_addr_sprd.DCAM.paddr
#define SPRD_DCAM_BASE		io_addr_sprd.DCAM.vaddr
#define SPRD_DCAM_SIZE		io_addr_sprd.DCAM.length

/*	struct iomap_sprd VSP;
#define SPRD_VSP_PHYS		io_addr_sprd.VSP.paddr
#define SPRD_VSP_BASE		io_addr_sprd.VSP.vaddr
#define SPRD_VSP_SIZE		io_addr_sprd.VSP.length
*/
	struct iomap_sprd SYSCNT;
#define SPRD_SYSCNT_PHYS	io_addr_sprd.SYSCNT.paddr
#define SPRD_SYSCNT_BASE	io_addr_sprd.SYSCNT.vaddr
#define SPRD_SYSCNT_SIZE	io_addr_sprd.SYSCNT.length

	struct iomap_sprd PIN;
#define SPRD_PIN_PHYS		io_addr_sprd.PIN.paddr
#define SPRD_PIN_BASE		io_addr_sprd.PIN.vaddr
#define SPRD_PIN_SIZE		io_addr_sprd.PIN.length

	struct iomap_sprd GPIO;
#define SPRD_GPIO_PHYS		io_addr_sprd.GPIO.paddr
#define SPRD_GPIO_BASE		io_addr_sprd.GPIO.vaddr
#define SPRD_GPIO_SIZE		io_addr_sprd.GPIO.length

#endif
};

#else /* __ASSEMBLER__ */

#if !defined(CONFIG_64BIT)
#define SPRD_UART1_PHYS		0x70100000
#endif

#endif /* __ASSEMBLER__ */
#if defined(CONFIG_ARCH_SCX30G2) || defined(CONFIG_MACH_SP9830I) || defined(CONFIG_ARCH_SCX35LT8) 
#define REGS_CODEC_AHB_BASE	SPRD_CODECAHB_BASE
#endif
#if defined(CONFIG_ARCH_SCX30G3)
#define REGS_CRYPTO_APB_RF_BASE	SPRD_CRYPTO_BASE
#endif

#endif /* __SCI_IOMAP_H__ */
