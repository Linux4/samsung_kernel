/*
 * Copyright (C) 2011 System S/W Group, Samsung Electronics.
 * Geunyoung Kim. <nenggun.kim@samsung.com>
 */

#ifndef __MINT_H__
#define __MINT_H__

#include <globald.h>

//#define CONFIG_DETECT_USB_DOWNLOAD_MODE //huyue
#define CONFIG_ARMV7
#define CONFIG_SPRD
#define CONFIG_MACHINE_TYPE 2014
#define CONFIG_SP8830SSD
#define DDR_LPDDR2
//#define CONFIG_FB_LCD_HX8369B_MIPI

/*
 * SPREADTRUM BIGPHONE board - SoC Configuration
 */
#define CONFIG_STAR2
#define CONFIG_SPX15
#define CONFIG_SC8830
#define CONFIG_SP8830WCN

//#define CHIP_ENDIAN_LITTLE
//#define _LITTLE_ENDIAN 1

//#define CONFIG_PIT_FILE

#ifdef CONFIG_PIT_FILE
#define CONFIG_PARAM_LFS
#endif


/*wangdali dolphin ddr add*/
#define CONFIG_CLK_PARA

#ifdef CONFIG_CLK_PARA
#define MAGIC_HEADER	0x5555AAAA
#define MAGIC_END	0xAAAA5555
#define CONFIG_PARA_VERSION 1
#define CLK_CA7_CORE    ARM_CLK_1000M
#define CLK_CA7_AXI     ARM_CLK_500M
#define CLK_CA7_DGB     ARM_CLK_200M
#define CLK_CA7_AHB     AHB_CLK_192M
#define CLK_CA7_APB     APB_CLK_64M
#define CLK_PUB_AHB     PUB_AHB_CLK_153_6M
#define CLK_AON_APB     AON_APB_CLK_128M
#define DDR_FREQ        333000000
#define DCDC_ARM	1200
#define DCDC_CORE	1100
#define CONFIG_VOL_PARA
#endif
/*add end*/


/* System */
#define CONFIG_EMMC_BOOT
//#define CONFIG_USE_VLX
#define CONFIG_CP_LOAD_IN_SBOOT
#define CALIBRATE_ENUM_MS 1000
//#define CONFIG_TRUSTZONE

/*CALIBRATION */
#define CALIBRATION_FLAG         0x897FFC00
#define CALIBRATION_FLAG_WCDMA   0x93FFEC00

/* MicroUSB */
#define CONFIG_MUIC_SPRD
//#define CONFIG_MUIC_FSA880
//#define CONFIG_MUIC_TSU6721
#define CHARGING_STATE      0x03
#define CONFIG_VF_CHECK_ENABLE

/* LCD */
//#define CONFIG_LCD_ILI9341_MINT
#define CONFIG_FB_LCD_ILI9486
#define CONFIG_LCD_RGB565
#define CONFIG_LCD_HVGA
//#define CONFIG_DISPLAY_480x800
#define CONFIG_DISPLAY_320x480
#define CONFIG_DRAW_PROGRESS

/* Log */
#define CONFIG_BOOTLOADER_LOGGING
#define LOG_LENGTH	(0x200000)
#define CONFIG_FORCED_CONTROL_KERNEL_LOG

/* board + model name definitions. */
#define CONFIG_SPRD_CHIP_NAME	"SC7715"
#define CONFIG_BOARD_NAME		"DOLPHIN"
#define CONFIG_MODEL_NAME		"GT-S6818"

/* MMU, DDR, SRAM */
#define CONFIG_PHY_SDRAM_BASE	0x80000000

#define LPDDR0_BASE_ADDR	(CONFIG_PHY_SDRAM_BASE)
#define LPDDR0_SIZE			(0x20000000)	/* 512MB */

#define SDRAM0_BASE     	(CONFIG_PHY_SDRAM_BASE)
#define SDRAM0_SIZE			(0x10000000)    /* 256MB */
#define SDRAM1_BASE			(SDRAM0_BASE + SDRAM0_SIZE)
#define SDRAM1_SIZE			(0x10000000)    /* 256MB */

//#define CP_RAM_SIZE			(0x02000000) // /*17MB */
//#define VM_RAM_SIZE			(0x00500000)
//#define LPDDR0_AP_BASE_ADDR	(CONFIG_PHY_SDRAM_BASE + CP_RAM_SIZE+ VM_RAM_SIZE)

/* AP Memory Map, huyue added by shark intro on 7/29/2013 */

#define CP_W_OFFSET             (0x10000000) //256MB
#define CP_W_SIZE               (0x04000000)  //64MB

#define CP_T_OFFSET             (0x08000000) //128MB
#define CP_T_SIZE               (0x02000000) //32MB

#define LPDDR0_AP_BASE_ADDR     (CONFIG_PHY_SDRAM_BASE)
#define AP_RAM_SIZE             (SDRAM0_SIZE - CP_T_SIZE)

#define LPDDR0_CP_BASE_ADDR     (LPDDR0_AP_BASE_ADDR + CP_W_OFFSET)
#define CP_RAM_SIZE             CP_W_SIZE

#define WCN_SIZE                (0x01000000) //16MB

#define LPDDR0_AP2_BASE_ADDR     (LPDDR0_CP_BASE_ADDR + CP_RAM_SIZE + WCN_SIZE)
#define AP2_RAM_SIZE             (SDRAM0_SIZE - CP_W_SIZE - WCN_SIZE) 

#define LPDDR0_VM_BASE_ADDR      (LPDDR0_AP_BASE_ADDR + CP_T_OFFSET + CP_T_SIZE)
#define VM_RAM_SIZE              (0x400) //1k ?       

/* AP Memory Map end. */

#if defined(CONFIG_CP_LOAD_IN_SBOOT)
#if defined(CONFIG_SC8830) //wangdali cpload
#define KERNEL_ADR      0x80008000
#define VLX_TAG_ADDR    0x82000100
#define RAMDISK_ADR     0x85500000
#define TDDSP_ADR       0x88020000
#define TDFIXNV_ADR     0x88480000
#define TDRUNTIMENV_ADR 0x884a0000
#define TDMODEM_ADR     0x88500000
#define WDSP_ADR        0x90020000
#define WFIXNV_ADR      0x90440000
#define WRUNTIMENV_ADR  0x90480000
#define WMODEM_ADR      0x90500000
#else
#define DSP_DDR_ADDR		(LPDDR0_BASE_ADDR + 0x00020000)
#define VM_DDR_ADDR			(LPDDR0_BASE_ADDR + 0x00400000)
#define FIXNV_ADR			(LPDDR0_BASE_ADDR + 0x10480000)
#define PRODUCTINFO_ADR		(LPDDR0_BASE_ADDR + 0x00490000)
#define RUNTIMENV_ADR		(LPDDR0_BASE_ADDR + 0x004a0000)
#define MODEM_DDR_ADDR 		(LPDDR0_BASE_ADDR + 0x00500000)
#define TD_RUNTIMENV_ADR (LPDDR0_BASE_ADDR + 0x084a0000)
#endif

#endif // CONFIG_CP_LOAD_IN_SBOOT


#define KERNEL_RAM_BASE		(LPDDR0_BASE_ADDR + 0x00008000)
#define BOOT_PARAM_RAM_BASE	(LPDDR0_BASE_ADDR + 0x02000100)
#define RAMDISK_RAM_BASE	(LPDDR0_BASE_ADDR + 0x05500000)
#define USBDOWN_RAM_BASE	(LPDDR0_BASE_ADDR + 0x60000000)
#define USBDOWN_RAM_SIZE	(0x8000000) /*  128MB */
//#define FIXNV_BUFFER_BASE	(USBDOWN_RAM_BASE + 0x3C00000) //yjn
#define FIXNV_BUFFER_BASE 0x904a0000

#define FB_RAM_BASE			(CONFIG_TEXT_BASE - 0x00100000)

/* MMU, SRAM */
#define CONFIG_MMU_TABLE_ADDR	(0x81600000 - 16*1024)//(0x40000000) wangdali mmu
#define SCRATCH_RAM_BASE		(0x40000000)
#define SCRATCH_RAM_SIZE		(0xC000) /* 48KB */
//#define MAGIC_RAM_BASE			(0x40007f24)
//#define MAGIC_RAM_BASE		(7 * 1024)  //huyue 7K
#define MAGIC_RAM_BASE			(0x50003000 + 0x100)  // temp. final addr = 0x50003000
#define REG_EMU_AREA			(MAGIC_RAM_BASE + 0x48)

//wangdali mmu
#define CONFIG_INC_IMG_LOGO

/* sboot memory */
#define CONFIG_PADTO_SIZE		853760
#define CONFIG_BOOTLOADER_SIZE		1572864
#define CONFIG_VM_SIZE			1966080 /* sbooti(1.5MB)+vm(384KB) */
#define CONFIG_SDRAM_BANK_N		2
#define CONFIG_STACK_SIZE		0x40000 /* 256KB */
#define __nocache_bss__  __attribute__((__section__(".bss.ncache")))


/*+++++++++++++++++ partition definition +++++++++++++++++
partition ID : 0~49 -> normal partitions(user data area)
partition ID : 50~59 -> boot0 partitions
partition ID : 60~69 -> boot1 partitions
partition ID : 70~999 -> ignore partitions(are not written by odin)
------------------ partition definition ------------------*/

#define KERNEL_PARTITION_NAME	"KERNEL"
#define RECOVERY_PARTITION_NAME	"RECOVERY"
#define PARAM_PARTITION_NAME	"PARAM"
#define FOTA_SIG_PARTITION_NAME	"FOTA_SIG"
#define SBL1_PARTITION_NAME		"SBL"
#define SBL2_PARTITION_NAME		"SBL"
#define VM_PARTITION_NAME		"VLX"
#define MODEM_PARTITION_NAME	"modem"
#define DSP_PARTITION_NAME		"DSP"
#define PARTITION_FIX_NV1		"FIXNV1"
#define PARTITION_FIX_NV2		"FIXNV2"
#define PARTITION_RUNTIME_NV1	"RUNTIMENV1"
#define PARTITION_RUNTIME_NV2	"RUNTIMENV2"
#define PARTITION_PROD_INFO1	"ProductInfo1"
#define PARTITION_PROD_INFO2	"ProductInfo2"
#define PARTITION_PROD_INFO3	"ProductInfo3"
#define UBOOT_PARTITION_NAME           "UBOOT"
#define PARTITION_NAME_MODEM         "MODEM"
#define PARTITION_NAME_WDSP         "WDSP"



#define GANG_PARTITION_ID			0x00
#define SBL1_PARTITION_ID			0x09
#define SBL2_PARTITION_ID			0x0A
#define KERNEL_PARTITION_ID			0x05
#define RECOVERY_PARTITION_ID		0x06
#define MODEM_PARTITION_ID			0x07
#define BOOT_PARTITION0_ID			0x50    /* 80 */
#define BOOT_PARTITION1_ID			0x5A    /* 90 */
#define IGNORE_PARTITION_ID_START	0x46    /* 70 *//* 70 : pit, 71 : smdhdr */
#define IGNORE_PARTITION_ID_END		0x4F    /* 79 *//* 70 : pit, 71 : smdhdr */
#define PIT_PART_ID                    70

#define KERNEL_SECTOR_BASE          (32256) //(73728)
#define RECOVERY_SECTOR_BASE        (48640) //(83968)   /* Temp */
#define PIT_SECTOR_BASE				(34)

#define SECTOR_SIZE     512
#define SECTOR_BITS     9

#define ERASE_FIRST_PARTITION		"CSC"

#define PartitionX				 	0x5980000//183,296 sector means odin_reserved
#define FLASH_CUSTOM_BINARY_INFO	(PartitionX/512)
#define NPS_ADDRESS 				(FLASH_CUSTOM_BINARY_INFO+1)
#define SUD_ADDRESS 				(NPS_ADDRESS + 1)
#define JIG_ADDRESS 				(SUD_ADDRESS + 1)

#define IGNORE_PARTITION_ID     0x46    /* 70 *//* 70 : pit, 71 : smdhdr */

#define SIGINFO_MAGIC				0x494E464F
#define CONFIG_SIGNATURE_SIZE		256

#define EMMC_SECTOR_SIZE	SECTOR_SIZE

#define FIXNV_SIZE			(2*128 * 1024)	
#define PRODUCTINFO_SIZE	(3 * 1024)
#define MODEM_SIZE		(0xA00000)

#define RUNTIMENV_SIZE		(3*128 * 1024)
#define DSP_SIZE			(0x3E0400) /* 3968K */
#define TD_RUNTIMENV_ADR_SIZE (256 * 1024)


/* CP loading */
#if defined(CONFIG_CP_LOAD_IN_SBOOT)
#define MAX_SN_LEN                      (24)
#define SP09_MAX_SN_LEN                 MAX_SN_LEN
#define SP09_MAX_STATION_NUM            (15)
#define SP09_MAX_STATION_NAME_LEN       (10)
#define SP09_SPPH_MAGIC_NUMBER          (0X53503039)    // "SP09"
#define SP09_MAX_LAST_DESCRIPTION_LEN   (32)
#define CONFIG_CP_FEATURE_NVBACKUP_IN_SBOOT
#endif // CONFIG_CP_LOAD_IN_SBOOT

#define PHYS_OFFSET_ADDR			0x80000000
#define TD_CP_OFFSET_ADDR			0x8000000	/*128*/
#define TD_CP_SDRAM_SIZE			0x1800000	/*24M*/
#define WCDMA_CP_OFFSET_ADDR		0x8000000	/*128M*/
#define WCDMA_CP_SDRAM_SIZE		    0x2800000	/*40M*/

#define CONFIG_TD_MODEM_NO_BOOT

#define WCN_CP_OFFSET_ADDR          0xa800000   /*168M*/
#define WCN_CP_SDRAM_SIZE           0x400000    /*4M*/

#define SIPC_APCP_RESET_ADDR_SIZE	0xC00	/*3K*/
#define SIPC_APCP_RESET_SIZE	    0x1000	/*4K*/

#define SIPC_WCN_APCP_START_ADDR    (PHYS_OFFSET_ADDR + WCN_CP_OFFSET_ADDR + WCN_CP_SDRAM_SIZE - SIPC_APCP_RESET_SIZE)
#define SIPC_TD_APCP_START_ADDR		(PHYS_OFFSET_ADDR + TD_CP_OFFSET_ADDR + TD_CP_SDRAM_SIZE - SIPC_APCP_RESET_SIZE)	/*0x897FF000*/
#define SIPC_WCDMA_APCP_START_ADDR	(PHYS_OFFSET_ADDR + WCDMA_CP_OFFSET_ADDR + WCDMA_CP_SDRAM_SIZE - SIPC_APCP_RESET_SIZE) /*0x93FFF000*/

// huyue sound
#define CONFIG_CMD_SOUND 1
#define CONFIG_CMD_FOR_HTC 1
#define CONFIG_SOUND_CODEC_SPRD_V3 1
#define CONFIG_SOUND_DAI_VBC_R2P0 1
/* #define CONFIG_SPRD_AUDIO_DEBUG */


/* defines */
#define REBOOT_MODE_PREFIX		0x12345670
#define REBOOT_MODE_NONE		0
#define REBOOT_MODE_DOWNLOAD		1
#define REBOOT_MODE_UPLOAD		2
#define REBOOT_MODE_CHARGING		3
#define REBOOT_MODE_RECOVERY		4
#define REBOOT_MODE_ARM11_FOTA		5
#define REBOOT_MODE_CPREBOOT		6

#define REBOOT_SET_PREFIX		0xabc00000
#define REBOOT_SET_DEBUG		0x000d0000
#define REBOOT_SET_SWSEL		0x000e0000
#define REBOOT_SET_SUD			0x000f0000

#define UPLOAD_CAUSE_INIT		(0xCAFEBABE)
#define UPLOAD_CAUSE_KERNEL_PANIC	(0x000000C8)
#define UPLOAD_CAUSE_FORCED_UPLOAD	(0x00000022)
#define UPLOAD_CAUSE_CP_ERROR_FATAL	(0x000000CC)
#define UPLOAD_CAUSE_HSIC_DISCONNECTED	(0x000000DD)
#define UPLOAD_CAUSE_USER_FAULT 	(0x0000002F)

#define KERNEL_SEC_UPLOAD_CAUSE_MASK	(0x000000FF)
#define KERNEL_SEC_UPLOAD_AUTOTEST_BIT	(31)
#define KERNEL_SEC_UPLOAD_AUTOTEST_MASK	\
	(1<<KERNEL_SEC_UPLOAD_AUTOTEST_BIT)

#define KERNEL_SEC_DEBUG_LEVEL_BIT	(29)
#define KERNEL_SEC_DEBUG_LEVEL_MASK	(3<<KERNEL_SEC_DEBUG_LEVEL_BIT)

#define KERNEL_SEC_DEBUG_LEVEL_LOW_OLD	(0x574F4C44)
#define KERNEL_SEC_DEBUG_LEVEL_MID_OLD	(0x44494D44)
#define KERNEL_SEC_DEBUG_LEVEL_HIGH_OLD	(0x47494844)
#define KERNEL_SEC_DEBUG_LEVEL_AUTO_OLD	(0x54554144)
#define KERNEL_SEC_DEBUG_LEVEL_LOW	(0x4F4C) /* LO */
#define KERNEL_SEC_DEBUG_LEVEL_MID	(0x494D) /* MI */
#define KERNEL_SEC_DEBUG_LEVEL_HIGH	(0x4948) /* HI */
#define KERNEL_SEC_DEBUG_LEVEL_AUTO	(0x5541) /* AU */

#define KERNEL_SEC_LOG_ENABLE		(0x45474f4c)
#define KERNEL_SEC_LOG_DISABLE		(0x44474f4c)

#define MAGIC_CODE_T32			(0x66262562)
#define MAGIC_CODE_DUMP			(0x66262564)

#define SWITCH_AP_UART			(0x1<<1)
#define SWITCH_CP_UART			(0x0<<1)
#define SWITCH_AP_USB			(0x1<<0)
#define SWITCH_CP_USB			(0x0<<0)



/* set uart path to modem and usb path to pda */
#define SWITCH_SEL			(SWITCH_CP_UART|SWITCH_AP_USB)

/* default boot mode. */
#define REBOOT_MODE			(REBOOT_MODE_RECOVERY)

/* booting modes. */
#define BOOT_MODE_NORMAL			0
#define BOOT_MODE_CHARGING			1
#define BOOT_MODE_DOWNLOAD			2
#define BOOT_MODE_UPLOAD			3
#define BOOT_MODE_RECOVERY			4
#define BOOT_MODE_CAL				5
#define BOOT_MODE_HOME_DOWNLOAD		6
#define BOOT_MODE_CAL_BACKUP		7
#define BOOT_MODE_SUD_DOWNLOAD		8

/* default command line. */
#define DEFAULT_PARAM_SWITCH_SEL	(SWITCH_AP_USB)
//#define DEFAULT_PARAM_REBOOT_MODE	(REBOOT_MODE_RECOVERY) //gaowu.chen
#define DEFAULT_PARAM_REBOOT_MODE	(REBOOT_MODE_NONE)

#define DEFAULT_PARAM_COMMAND_LINE	""
#define CMDLINE_DEBUG_LEVEL KERNEL_SEC_DEBUG_LEVEL_HIGH
#define CMDLINE_LOG_ENABLE KERNEL_SEC_DEBUG_LEVEL_ENABLE
#define MTDPARTS_DEFAULT " mtdparts=sprd-nand:256k(spl),2m(2ndbl),256k(tdmodem),256k(tddsp),256k(tdfixnv1),256k(tdruntimenv),256k(wmodem),256k(wdsp),256k(wfixnv1),256k(wruntimenv1),256k(prodinfo1),256k(prodinfo3),1024k(logo),1024k(fastbootlogo),10m(boot),300m(system),150m(cache),10m(recovery),256k(misc),256k(sd),512k(userdata)"
#define CMDLINE_DEFAULT " mem=512M init=/init lcd_id=ID9486 ram=512M" MTDPARTS_DEFAULT


#define JIGUSB_DOWN_FILE_NAME "jigusb_dl_flag"
#define SUD_INDEX_FILE_NAME "sud_index"
#define NPS_STATUS_FILE_NAME "nps_status"
#define NPS_STATUS_START      0x52415453
#define NPS_STATUS_COMPLETED  0x504d4f43
#define NPS_STATUS_FAIL		0x4c494146

#define DOWNLOAD_MODE_ODIN			0x10
#define DOWNLOAD_MODE_SUD			0x20
#define DOWNLOAD_MODE_NPS_START		0x41
#define DOWNLOAD_MODE_NPS_FAIL		0x42
#define DOWNLOAD_MODE_NPS			(DOWNLOAD_MODE_NPS_START & DOWNLOAD_MODE_NPS_FAIL)

#define JIGUSB_DOWNLOAD_MODE    0x4e574f44	//DOWN
#define JIGUSB_NORMAL_MODE      0x4d524f4e	//NORM

#define SUD_INDEX_1		0x31313131
#define SUD_INDEX_2		0x32323232
#define SUD_INDEX_3		0x33333333
#define SUD_INDEX_4		0x34343434
#define SUD_INDEX_5		0x35353535
#define SUD_INDEX_6		0x36363636
#define SUD_INDEX_7		0x37373737
#define SUD_INDEX_8		0x38383838
#define SUD_INDEX_9		0x39393939

/* gpio definitions. */

#ifndef __ASSEMBLY__
extern int mach_board_initialize(void);
extern int mach_board_main(void);
extern void show_partition_info(void);
extern void forced_enable_log(void);
extern char log_buf_lds[];
extern unsigned int nocached_buf_var;
#endif	/* __ASSEMBLY__ */

#endif	/* __MINT_H__ */
