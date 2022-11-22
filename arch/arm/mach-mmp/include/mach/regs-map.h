/*
 * linux/arch/arm/mach-mmp/include/mach/regs-map.h
 *
 *   Common soc registers map
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_MACH_REGS_MAP_H
#define __ASM_MACH_REGS_MAP_H
#include <plat/dump_regs.h>

struct reg_map pxa_reg_map[] = {
#ifdef CONFIG_CPU_PXA988
	{0xD4050000, 0xD405004C, 0, "Main_PMU1"},
	{0xD4050100, 0xD4050100, 0, "Main_PMU2"},
	{0xD4050200, 0xD4050200, 0, "Main_PMU3"},
	{0xD4050400, 0xD4050410, 0, "Main_PMU4"},
	{0xD4051000, 0xD4051004, 0, "Main_PMU5"},
	{0xD4051020, 0xD4051028, 0, "Main_PMU6"},
	{0xD4051048, 0xD405104C, 0, "Main_PMU7"},
	{0xD4282800, 0xD4282804, 0, "Apps_PMU_1"},
	{0xD4282808, 0xD428280C, 0, "Apps_PMU_2"},
	{0xD4282810, 0xD4282820, 0, "Apps_PMU_3"},
	{0xD4282828, 0xD42828F0, 0, "Apps_PMU_4"},
	{0xD4282900, 0xD4282908, 0, "Apps_PMU_5"},
	{0xD4282920, 0xD4282928, 0, "Apps_PMU_6"},
	{0xF00E0000, 0xF00E0140, 0, "CCU"},
	{0xD403B000, 0xD403B03C, 0, "APB_ctl"},
	{0xD4015000, 0xD4015060, 0, "APB_clock"},
	{0xD4080000, 0xD40800AC, 0, "PMU_Timer"},
	{0xFFA60000, 0xFFA60018, 0, "Sleep_Timer"},
	{0xD4014000, 0xD40140B0, 0, "Timer"},
	{0xD401E000, 0xD401E32C, 0, "MFPR"},
	{0xD4282000, 0xD428214C, 0, "ICU"},
	{0xD4282C00, 0xD4282CE8, 0, "CIU"},
	{0xF0205100, 0xF020510C, 0, "DSSP0"},
	{0xF0206100, 0xF020610C, 0, "DSSP1"},
	{0xF0207100, 0xF020710C, 0, "DSSP2"},
	{0xF0208100, 0xF020810C, 0, "DSSP3"},
	{0xD4032000, 0xD4032044, 0, "USIM1"},
	{0xD4033000, 0xD4033044, 0, "USIM2"},
	{0xD4019000, 0xD4019014, 0, "GPIO1"},
	{0xD4019030, 0xD4019050, 0, "GPIO2"},
	{0xD401909C, 0xD40190A4, 0, "GPIO3"},
	{0xD4019800, 0xD401980C, 0, "GPIO_Edge"},
	{0xD401D000, 0xD401D000, 0, "APIPC"},
	{0xD4280000, 0xD42800FE, 0, "SD1"},
	{0xD4280800, 0xD42808FE, 0, "SD2"},
	{0xD4281000, 0xD42810FE, 0, "SD3"},
	{0xD4036000, 0xD403602C, 0, "UART0"},
	{0xD4017000, 0xD401702C, 0, "UART1"},
	{0xD4018000, 0xD401802C, 0, "UART1"},
	{0xD420A000, 0xD420A238, 0, "Camera"},
	{0xD420B000, 0xD420B1FC, 0, "LCD"},
	{0xD401B000, 0xD401B08C, 0, "SSP0"},
	{0xD42A0C00, 0xD42A0C8C, 0, "SSP1"},
	{0xD401C000, 0xD401C08C, 0, "SSP2"},
	{0xD4011000, 0xD401108C, 0, "TWSI0"},
	{0xD4010800, 0xD401088C, 0, "TWSI1"},
	{0xD4010000, 0xD4010024, 0, "RTC"},
	{0xD4012000, 0xD4012048, 0, "KPC"},
	{0xD4013200, 0xD4013224, 0, "DRO"},
	{0xD4000000, 0xD40003FC, 0, "DMAC"},
	{0xD4011800, 0xD4011810, 0, "1Wire"},
	{0xD4208000, 0xD42081FC, 0, "USB_OTG"},
	{0xD4207000, 0xD420707C, 0, "USB_PHY"},
	{0xC0100000, 0xC010045C, 0, "DDR_MC"},
#endif /* CONFIG_CPU_PXA988 */

#ifdef CONFIG_CPU_PXA1088
/* register map of pxa1088 */

/*
 * important registers like CIU, Main_PMU, AP_PMU, MCK4, APB_clock, ICU are
 * put ahead, they will be dumped as early as possible.
 */
	{0xD4282C00, 0xD4282CFC, 0, "CIU"},
	{0xD4050000, 0xD405004C, 0, "Main_PMU_0"},
	{0xD4050100, 0xD4050100, 0, "Main_PMU_1"},
	{0xD4050200, 0xD4050200, 0, "Main_PMU_2"},
	{0xD4050400, 0xD4050410, 0, "Main_PMU_3"},
	{0xD4051000, 0xD4051004, 0, "Main_PMU_4"},
	{0xD4051020, 0xD4051028, 0, "Main_PMU_5"},
	{0xD4051048, 0xD405104C, 0, "Main_PMU_6"},
	{0xD4052000, 0xD4052058, 0, "Main_PMU_DVC"},
	{0xD4282800, 0xD4282970, 0, "AP_PMU"},
	{0xC0100000, 0xC010003C, 0, "MCK4_00"},
	{0xC0100050, 0xC0100078, 0, "MCK4_01"},
	{0xC0100080, 0xC010009C, 0, "MCK4_02"},
	{0xC0100100, 0xC0100100, 0, "MCK4_03"},
	{0xC0100110, 0xC0100110, 0, "MCK4_04"},
	{0xC0100120, 0xC010012C, 0, "MCK4_05"},
	{0xC010013C, 0xC0100158, 0, "MCK4_06"},
	{0xC0100160, 0xC0100164, 0, "MCK4_07"},
	{0xC0100170, 0xC0100170, 0, "MCK4_08"},
	{0xC0100180, 0xC0100184, 0, "MCK4_09"},
	{0xC01001C0, 0xC01001C0, 0, "MCK4_10"},
	{0xC01001C8, 0xC01001CC, 0, "MCK4_11"},
	{0xC0100220, 0xC0100220, 0, "MCK4_12"},
	{0xC0100230, 0xC0100258, 0, "MCK4_13"},
	{0xC0100280, 0xC0100288, 0, "MCK4_14"},
	{0xC0100300, 0xC0100308, 0, "MCK4_15"},
	{0xC0100380, 0xC0100390, 0, "MCK4_16"},
	{0xC0100400, 0xC0100400, 0, "MCK4_17"},
	{0xC0100410, 0xC0100414, 0, "MCK4_18"},
	{0xC0100440, 0xC0100448, 0, "MCK4_19"},
	{0xC0100450, 0xC010045c, 0, "MCK4_20"},
	{0xD4015000, 0xD4015068, 0, "APB_clock"},
	{0xD4282000, 0xD428217C, 0, "ICU_0"},
	{0xD4282200, 0xD4282258, 0, "ICU_1"},
	{0xD4282300, 0xD428235C, 0, "ICU_2"},

/*
 * below registers are arranged according their physical address.
 * please -don't- delete the commented lines, they are kept as index
 * some regs are already dumped above;
 * some regs do not exist in spec;
 * some modules are not enabled, if read the regs, system will hang;
 * some regs are not necessary to dump at present, but may need in furture.
 */

/*	{0xC0000000, 0xC000FFFC, 0, "SPH USB PHY"}, not necessary */
/*	{0xC0010000, 0xC001FFFC, 0, "SPH USB CTL"}, not necessary */
/*	{0xC0100000, 0xC010045C, 0, "MCK4"}, already dumped above */
/*	{0xC0400000, 0xC04FFFFC, 0, "GC 1000T"}, not necessary */

/* 0xD1DF9000 ~ 0xD1DFDFFF are GIC registers */
	{0xD1DF9000, 0xD1DF9008, 0, "GIC_Distributor_00"},
	{0xD1DF9080, 0xD1DF90BC, 0, "GIC_Distributor_01"},
	{0xD1DF9100, 0xD1DF913C, 0, "GIC_Distributor_02"},
	{0xD1DF9180, 0xD1DF91BC, 0, "GIC_Distributor_03"},
	{0xD1DF9200, 0xD1DF923C, 0, "GIC_Distributor_04"},
	{0xD1DF9280, 0xD1DF92BC, 0, "GIC_Distributor_05"},
	{0xD1DF9300, 0xD1DF933C, 0, "GIC_Distributor_06"},
	{0xD1DF9380, 0xD1DF93BC, 0, "GIC_Distributor_07"},
	{0xD1DF9400, 0xD1DF95FC, 0, "GIC_Distributor_08"},
	{0xD1DF9800, 0xD1DF99FC, 0, "GIC_Distributor_09"},
	{0xD1DF9C00, 0xD1DF9C7C, 0, "GIC_Distributor_10"},
	{0xD1DF9D00, 0xD1DF9D3C, 0, "GIC_Distributor_11"},
	{0xD1DF9F00, 0xD1DF9F2C, 0, "GIC_Distributor_12"},
	{0xD1DF9FD0, 0xD1DF9FFC, 0, "GIC_Distributor_13"},
	{0xD1DFA000, 0xD1DFA028, 0, "GIC_CPU_interface_0"},
	{0xD1DFA0D0, 0xD1DFA0FC, 0, "GIC_CPU_interface_1"},
	{0xD1DFB000, 0xD1DFB030, 0, "GIC_common_base_add_0"},
	{0xD1DFB0F0, 0xD1DFB10C, 0, "GIC_common_base_add_1"},
	{0xD1DFC000, 0xD1DFC030, 0, "GIC_processor_spc_add_0"},
	{0xD1DFC0F0, 0xD1DFC10C, 0, "GIC_processor_spc_add_1"},
	{0xD1DFD000, 0xD1DFD028, 0, "GIC_Vir_CPU_interface_0"},
	{0xD1DFD0D0, 0xD1DFD0FC, 0, "GIC_Vir_CPU_interface_1"},

/* 0xD4000000 ~ 0xD41FFFFF are APB Peripheral registers */
	{0xD4000000, 0xD40003FC, 0, "DMA"},
	{0xD4010000, 0xD4010024, 0, "RTC"},
	{0xD4010800, 0xD4010858, 0, "IIC1"},
	{0xD4011000, 0xD4011058, 0, "IIC0"},
	{0xD4011800, 0xD4011810, 0, "OneWire"},
	{0xD4012000, 0xD4012048, 0, "Keypad"},
/*	{0xD4013000, 0xD40130FC, 0, "Trackball"}, not exist in spec */
	{0xD4013100, 0xD4013108, 0, "JTAG"},
	{0xD4013200, 0xD4013230, 0, "DRO"},
	{0xD4014000, 0xD40140AC, 0, "Timer0"},
/*	{0xD4015000, 0xD4015068, 0, "APB_clock"}, already dumped above */
	{0xD4016000, 0xD40160AC, 0, "Timer1"},
	{0xD4017000, 0xD401702C, 0, "UART1"},
	{0xD4018000, 0xD401802C, 0, "UART2"},
	{0xD4019000, 0xD40191A8, 0, "GPIO"},
	{0xD4019800, 0xD401980C, 0, "GPIO_Edge"},
	{0xD401A000, 0xD401A008, 0, "PWM0"},
	{0xD401A400, 0xD401A408, 0, "PWM1"},
	{0xD401A800, 0xD401A808, 0, "PWM2"},
	{0xD401AC00, 0xD401AC08, 0, "PWM3"},
	{0xD401B000, 0xD401B08C, 0, "SSP1"},
	{0xD401C000, 0xD401C08C, 0, "SSP3"},
	{0xD401D000, 0xD401D014, 0, "AP_IPC"},
	{0xD401D800, 0xD401D814, 0, "CP_IPC"},
	{0xD401E000, 0xD401E32C, 0, "MFPR"},
	{0xD401E800, 0xD401E830, 0, "IO_Power_0"},
/*	{0xD401EC00, 0xD401EC14, 0, "IO_Power_1"}, overlap with SFO */
	{0xD401EC00, 0xD401EC14, 0, "SFO"},
	{0xD401F000, 0xD401F0B0, 0, "Timer2"},
/*	{0xD4030000, 0xD403004C, 0, "TCU_0"}, not necessary */
/*	{0xD4030400, 0xD40307FC, 0, "TCU_1"}, not necessary */
/*	{0xD4031000, 0xD40318F4, 0, "XIRQ"}, not necessary */
/*	{0xD4032000, 0xD4032044, 0, "USIM1"}, not necessary */
/*	{0xD4033000, 0xD4033044, 0, "USIM2"}, not necessary */
/*	{0xD4034000, 0xD4034050, 0, "E_cip_core"}, not necessary */
/*	{0xD4034100, 0xD4034100, 0, "E_cip_data"}, not necessary */
/*	{0xD4035000, 0xD4035FFC, 0, "E_cip_ctrl"}, not necessary */
/*	{0xD4036000, 0xD403602C, 0, "GB_UART0"}, not necessary */
/*	{0xD4037000, 0xD4037058, 0, "GB_IIC"}, not necessary */
/*	{0xD4038000, 0xD403802C, 0, "GB_SCLK"}, not necessary */
/*	{0xD403B00C, 0xD403B00C, 0, "GSSP"}, inside GB Mreg */
/*	{0xD403A000, 0xD403A0AC, 0, "TimerCP"}, not necessary */
/*	{0xD403B000, 0xD403B03C, 0, "APB_CP_clock_ctrl"}, not necessary */
/*	{0xD403C000, 0xD403C014, 0, "GB_IPC"}, not necessary */
/*	{0xD403D000, 0xD403D01C, 0, "GB_R_IPC_0"}, not necessary */
/*	{0xD403D100, 0xD403D11C, 0, "GB_R_IPC_1"}, not necessary */
/*	{0xD403D200, 0xD403D21C, 0, "GB_R_IPC_2"}, not necessary */
/*	{0xD403D300, 0xD403D31C, 0, "GB_R_IPC_3"}, not necessary */
/*	{0xD4050000, 0xD405104C, 0, "Main PMU"}, already dumped above */
/*	{0xD4052000, 0xD4052058, 0, "Main_PMU_DVC"}, already dumped above */
/*	{0xD4060000, 0xD406FFFC, 0, "PMU_SCK"}, not exist in spec */
	{0xD4070000, 0xD4070020, 0, "APB_Aux"},
	{0xD4080000, 0xD40800AC, 0, "PMU_Timer"},
	{0xD4090000, 0xD409000C, 0, "APB_spare_0"},
	{0xD4090100, 0xD409010C, 0, "APB_spare_1"},
/*	{0xD4100000, 0xD41FFFFC, 0, "CoreSight"}, not necessary */
	{0xD4101000, 0xD4101020, 0, "GenericCounter"},

/* 0xD4200000 ~ 0xD4284FFF are AXI Peripheral registers */
	{0xD4200200, 0xD4200260, 0, "AXI_Fab_0"},
	{0xD4200408, 0xD42004CC, 0, "AXI_Fab_1"},
/*	{0xD4201000, 0xD42014C8, 0, "GEU"}, not necessary */
	{0xD4207000, 0xD4207064, 0, "USB_utmi_phy"},
	{0xD4207100, 0xD4207134, 0, "USB_utmi_old"},
	{0xD4208000, 0xD42081FC, 0, "USB_device"},
	{0xD420A000, 0xD420A23C, 0, "CCIC"},
	{0xD420B000, 0xD420B1F4, 0, "LCD"},
	{0xD420B800, 0xD420B9EC, 0, "DSI"},
/*	{0xD420D000, 0xD420DFFC, 0, "7542_Video_Decoder"}, not necessary */
/*	{0xD420E000, 0xD420EFFC, 0, "DTE(DDR Test)"}, not exist in spec */
/*	{0xD420F000, 0xD420FE38, 0, "ISPDMA"}, not necessary */
/*	{0xD4240000, 0xD427FFFC, 0, "DXO DMA config"}, not exist in spec */
	{0xD4280000, 0xD428006C, 0, "SDH1_0"},
	{0xD42800E0, 0xD42800E0, 0, "SDH1_1"},
	{0xD42800FC, 0xD428011C, 0, "SDH1_2"},
	{0xD4280800, 0xD428086C, 0, "SDH2_0"},
	{0xD42808E0, 0xD42808E0, 0, "SDH2_1"},
	{0xD42808FC, 0xD428091C, 0, "SDH2_2"},
	{0xD4281000, 0xD428106C, 0, "SDH3_0"},
	{0xD42810E0, 0xD42810E0, 0, "SDH3_1"},
	{0xD42810FC, 0xD428111C, 0, "SDH3_2"},
/*	{0xD4282000, 0xD42823FC, 0, "ICU"}, already dumped above */
/*	{0xD4282800, 0xD4282970, 0, "AP PMU"}, already dumped above */
/*	{0xD4282C00, 0xD42820FC, 0, "CIU"},  already dumped above */
/*	{0xD4283000, 0xD428307C, 0, "NAND"}, if read, will hang */
	{0xD4283800, 0xD4283840, 0, "SMC"},
/*	{0xD4284000, 0xD42840C0, 0, "DTC"}, if read, will hang */
/*	{0xD4290000, 0xD4290B78, 0, "HSI"}, if read, will hang */
	{0xD42A0C00, 0xD42A0C8C, 0, "SSP3"},
#endif /* CONFIG_CPU_PXA1088 */

	{0         , 0         , 0, NULL   }
};

#endif /* __ASM_MACH_REG_MAP_H
# */
