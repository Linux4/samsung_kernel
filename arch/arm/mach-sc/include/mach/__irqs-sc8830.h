/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_SPRD_IRQS_SCX35_H
#define __ASM_ARCH_SPRD_IRQS_SCX35_H

#ifndef __ASM_ARCH_SCI_IRQS_H
#error  "Don't include this file directly, include <mach/irqs.h>"
#endif

#define IRQ_GIC_START			(32)
#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ARCH_SCX30G)
#define NR_SCI_PHY_IRQS			(IRQ_GIC_START + 127)
#else
#define NR_SCI_PHY_IRQS			(IRQ_GIC_START + 125)
#endif

#define SCI_IRQ(_X_)			(IRQ_GIC_START + (_X_))
#define SCI_EXT_IRQ(_X_)		(NR_SCI_PHY_IRQS + (_X_))

#define IRQ_SPECIAL_LATCH		SCI_IRQ(0)
#define IRQ_SOFT_TRIGGED0_INT		SCI_IRQ(1)
#define IRQ_SER0_INT			SCI_IRQ(2)
#define IRQ_SER1_INT			SCI_IRQ(3)
#define IRQ_SER2_INT			SCI_IRQ(4)
#define IRQ_SER3_INT			SCI_IRQ(5)
#define IRQ_SER4_INT			SCI_IRQ(6)
#define IRQ_SPI0_INT			SCI_IRQ(7)
#define IRQ_SPI1_INT			SCI_IRQ(8)
#define IRQ_SPI2_INT			SCI_IRQ(9)
#define IRQ_SIM0_INT 			SCI_IRQ(10)
#define IRQ_I2C0_INT 			SCI_IRQ(11)
#define IRQ_I2C1_INT 			SCI_IRQ(12)
#define IRQ_I2C2_INT 			SCI_IRQ(13)
#define IRQ_I2C3_INT 			SCI_IRQ(14)
#define IRQ_I2C4_INT 			SCI_IRQ(15)
#define IRQ_IIS0_INT			SCI_IRQ(16)
#define IRQ_IIS1_INT			SCI_IRQ(17)
#define IRQ_IIS2_INT			SCI_IRQ(18)
#define IRQ_IIS3_INT			SCI_IRQ(19)
#define IRQ_REQ_AUD_INT			SCI_IRQ(20)
#define IRQ_REQ_AUD_VBC_AFIFO_INT			SCI_IRQ(21)
#define IRQ_REQ_AUD_VBC_DA_INT			SCI_IRQ(22)
#define IRQ_REQ_AUD_VBC_AD01_INT			SCI_IRQ(23)
#define IRQ_REQ_AUD_VBC_AD23_INT			SCI_IRQ(24)
#define IRQ_ADI_INT			SCI_IRQ(25)
#define IRQ_THM_INT			SCI_IRQ(26)
#define IRQ_FM_INT			SCI_IRQ(27)
#define IRQ_AONTMR0_INT			SCI_IRQ(28)
#define IRQ_APTMR0_INT			SCI_IRQ(29)
#define IRQ_AONSYST_INT			SCI_IRQ(30)
#define IRQ_APSYST_INT			SCI_IRQ(31)

#define IRQ_AONI2C_INT			SCI_IRQ(34)
#define IRQ_GPIO_INT			SCI_IRQ(35)
#define IRQ_KPD_INT			SCI_IRQ(36)
#define IRQ_EIC_INT			SCI_IRQ(37)
#define IRQ_ANA_INT			SCI_IRQ(38)
#define IRQ_GPU_INT			SCI_IRQ(39)
#define IRQ_CSI_INT0			SCI_IRQ(40)
#define IRQ_CSI_INT1			SCI_IRQ(41)
#define IRQ_JPG_INT			SCI_IRQ(42)
#define IRQ_VSP_INT			SCI_IRQ(43)
#define IRQ_ISP_INT			SCI_IRQ(44)
#define IRQ_DCAM_INT			SCI_IRQ(45)
#define IRQ_DISPC0_INT			SCI_IRQ(46)
#define IRQ_DISPC1_INT			SCI_IRQ(47)
#define IRQ_DSI0_INT			SCI_IRQ(48)
#define IRQ_DSI1_INT			SCI_IRQ(49)
#define IRQ_DMA_INT			SCI_IRQ(50)
#define IRQ_GSP_INT			SCI_IRQ(51)
#define IRQ_GPS_INT			SCI_IRQ(52)
#define IRQ_GPS_RTCEXP_INT			SCI_IRQ(53)
#define IRQ_GPS_WAKEUP_INT			SCI_IRQ(54)
#define IRQ_USBD_INT			SCI_IRQ(55)
#define IRQ_NFC_INT			SCI_IRQ(56)
#define IRQ_SDIO0_INT			SCI_IRQ(57)
#define IRQ_SDIO1_INT			SCI_IRQ(58)
#define IRQ_SDIO2_INT			SCI_IRQ(59)
#define IRQ_EMMC_INT			SCI_IRQ(60)
#define IRQ_BM0_INT			SCI_IRQ(61)
#define IRQ_BM1_INT			SCI_IRQ(62)
#define IRQ_BM2_INT			SCI_IRQ(63)
#define IRQ_DRM_INT			SCI_IRQ(66)
#define IRQ_CP0_DSP_INT			SCI_IRQ(67)
#define IRQ_CP0_MCU0_INT			SCI_IRQ(68)
#define IRQ_CP0_MCU1_INT			SCI_IRQ(69)
#define IRQ_CP1_DSP_INT			SCI_IRQ(70)
#define IRQ_CP1_MCU0_INT			SCI_IRQ(71)
#define IRQ_CP1_MCU1_INT			SCI_IRQ(72)
#define IRQ_CP2_INT0_INT			SCI_IRQ(73)
#define IRQ_CP2_INT1_INT			SCI_IRQ(74)
#define IRQ_CP0_DSP_FIQ_INT			SCI_IRQ(75)
#define IRQ_CP0_MCU_FIQ0_INT			SCI_IRQ(76)
#define IRQ_CP0_MCU_FIQ1_INT			SCI_IRQ(77)
#define IRQ_CP1_MCU_FIQ_INT			SCI_IRQ(78)
#if 0
#define IRQ_NFC_INT			SCI_IRQ(79)
#define IRQ_NFC_INT			SCI_IRQ(80)
#define IRQ_NFC_INT			SCI_IRQ(81)
#define IRQ_NFC_INT			SCI_IRQ(82)
#endif
#define IRQ_CP0_WDG_INT			SCI_IRQ(83)
#define IRQ_CP1_WDG_INT			SCI_IRQ(84)
#define IRQ_CP2_WDG_INT			SCI_IRQ(85)
#define IRQ_AXI_BM_PUB_INT		SCI_IRQ(86)
#if 0
#define IRQ_NFC_INT			SCI_IRQ(86)
#define IRQ_NFC_INT			SCI_IRQ(87)
#define IRQ_NFC_INT			SCI_IRQ(88)
#define IRQ_NFC_INT			SCI_IRQ(89)
#define IRQ_NFC_INT			SCI_IRQ(90)
#define IRQ_NFC_INT			SCI_IRQ(91)
#endif
#define IRQ_NPMUIRQ0_INT			SCI_IRQ(92)
#define IRQ_NPMUIRQ1_INT			SCI_IRQ(93)
#define IRQ_NPMUIRQ2_INT			SCI_IRQ(94)
#define IRQ_NPMUIRQ3_INT			SCI_IRQ(95)
#define IRQ_CA7COM0_INT			SCI_IRQ(98)
#define IRQ_CA7COM1_INT			SCI_IRQ(99)
#define IRQ_CA7COM2_INT			SCI_IRQ(100)
#define IRQ_CA7COM3_INT			SCI_IRQ(101)
#define IRQ_NCNTV0_INT			SCI_IRQ(102)
#define IRQ_NCNTV1_INT			SCI_IRQ(103)
#define IRQ_NCNTV2_INT			SCI_IRQ(104)
#define IRQ_NCNTV3_INT			SCI_IRQ(105)
#define IRQ_NCNTHP0_INT			SCI_IRQ(106)
#define IRQ_NCNTHP1_INT			SCI_IRQ(107)
#define IRQ_NCNTHP2_INT			SCI_IRQ(108)
#define IRQ_NCNTHP3_INT			SCI_IRQ(109)
#define IRQ_NCNTPN0_INT			SCI_IRQ(110)
#define IRQ_NCNTPN1_INT			SCI_IRQ(111)
#define IRQ_NCNTPN2_INT			SCI_IRQ(112)
#define IRQ_NCNTPN3_INT			SCI_IRQ(113)
#define IRQ_NCNTPS0_INT			SCI_IRQ(114)
#define IRQ_NCNTPS1_INT			SCI_IRQ(115)
#define IRQ_NCNTPS2_INT			SCI_IRQ(116)
#define IRQ_NCNTPS3_INT			SCI_IRQ(117)
#define IRQ_APTMR1_INT			SCI_IRQ(118)
#define IRQ_APTMR2_INT			SCI_IRQ(119)
#define IRQ_APTMR3_INT			SCI_IRQ(120)
#define IRQ_APTMR4_INT			SCI_IRQ(121)
#define IRQ_AVS_INT				SCI_IRQ(122)
#define IRQ_APWDG_INT			SCI_IRQ(123)
#define IRQ_CA7WDG_INT			SCI_IRQ(124)
#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ARCH_SCX30G)
#define IRQ_ZIPDEC_INT			SCI_IRQ(125)
#define IRQ_ZIPENC_INT			SCI_IRQ(126)
#endif

#define IRQ_SIPC_CPW			IRQ_CP0_MCU0_INT
#ifdef CONFIG_ARCH_SCX30G
#define IRQ_SIPC_CPT 			IRQ_CP0_MCU0_INT
#else
#define IRQ_SIPC_CPT			IRQ_CP1_MCU0_INT
#endif
#define IRQ_SIPC_WCN			IRQ_CP2_INT0_INT


/* translate gic irq number(user using ) to intc number */
#define SCI_GET_INTC_IRQ(_IRQ_NUM_)	((_IRQ_NUM_) - IRQ_GIC_START)
#define SCI_INTC_IRQ_BIT(_IRQ_NUM_)	((SCI_GET_INTC_IRQ(_IRQ_NUM_)<IRQ_GIC_START)	? \
					(1<<SCI_GET_INTC_IRQ(_IRQ_NUM_))		: \
					(1<<(SCI_GET_INTC_IRQ(_IRQ_NUM_)-IRQ_GIC_START)))

/* analog die interrupt number */
#define IRQ_ANA_ADC_INT			SCI_EXT_IRQ(0)
#if defined(CONFIG_ARCH_SCX15)
#define IRQ_ANA_GPIO_INT		-1
#else
#define IRQ_ANA_GPIO_INT		SCI_EXT_IRQ(1)
#endif
#define IRQ_ANA_RTC_INT			SCI_EXT_IRQ(2)
#define IRQ_ANA_WDG_INT			SCI_EXT_IRQ(3)
#define IRQ_ANA_FGU_INT			SCI_EXT_IRQ(4)
#define IRQ_ANA_EIC_INT			SCI_EXT_IRQ(5)
#define IRQ_ANA_AUD_HEAD_BUTTON_INT		SCI_EXT_IRQ(6)
#define IRQ_ANA_AUD_PROTECT_INT			SCI_EXT_IRQ(7)
#if defined(CONFIG_ARCH_SCX15)
#define IRQ_ANA_CAL_INT			SCI_EXT_IRQ(8)
#define IRQ_ANA_TPC_INT			SCI_EXT_IRQ(9)
#define NR_ANA_IRQS			(10)
#else
#define IRQ_ANA_THM_OTP_INT			SCI_EXT_IRQ(9)
#define IRQ_ANA_DCD_OTP_INT			SCI_EXT_IRQ(10)/*bit9 is reserved in adie intc*/
#define NR_ANA_IRQS			(11)
#endif

#define IRQ_ANA_INT_START		IRQ_ANA_ADC_INT

#if defined(CONFIG_ARCH_SCX15)
#define GPIO_IRQ_START			SCI_EXT_IRQ(10)
#define NR_GPIO_IRQS	( 320 )
#else
/* sc8830 gpio&eic pin interrupt number, total is 320, which is bigger than 256 */
#define GPIO_IRQ_START			SCI_EXT_IRQ(11)
#define NR_GPIO_IRQS	( 320 )
#endif

#if (defined(CONFIG_EIRQ_NUM) && CONFIG_EIRQ_NUM)
#ifndef CONFIG_SPARSE_IRQ
/* For external irq region desc request */
#define __NR_IRQS			(NR_SCI_PHY_IRQS + NR_ANA_IRQS + NR_GPIO_IRQS)
#define NR_IRQS				(__NR_IRQS + CONFIG_EIRQ_NUM)
#else
#error 'Dont support CONFIG_EIRQ_NUM & CONFIG_SPARSE_IRQ coexist'
#endif /* CONFIG_SPARSE_IRQ */
#else
#define NR_IRQS				(NR_SCI_PHY_IRQS + NR_ANA_IRQS + NR_GPIO_IRQS)
#endif /* CONFIG_EIRQ_NUM */
#define FIQ_START	(0)
#define NR_FIQS		(NR_SCI_PHY_IRQS)


/* redefined some MACROS for module code*/
#define IRQ_TIMER1_INT			IRQ_AONTMR0_INT
#define IRQ_ANA_AUD_INT			IRQ_ANA_AUD_PROTECT_INT
#endif
