
#include <config.h>
#include <common.h>
#include <param.h>
#include <serial.h>
#include <time.h>
#include <display.h>
#include <mmc.h>
#include <ddi.h>
#include <drawimg.h>
#include <asm/arch/cpu.h>
//#include <asm/arch/smc.h>

void sprd_reset_reboot_mode(void)
{
	printf("%s\n", __func__);
	writel(0, INFORM3);
}

//#define D2200_DIALOG_PMU //huyue


#define HWRST_STATUS_RECOVERY 		(0x20)
#define HWRST_STATUS_NORMAL 		(0X40)
#define HWRST_STATUS_ALARM 		(0X50)
#define HWRST_STATUS_SLEEP 		(0X60)
#define HWRST_STATUS_FASTBOOT 		(0X30)
#define HWRST_STATUS_SPECIAL 		(0x70)
#define HWRST_STATUS_PANIC 		(0X80)
#define HWRST_STATUS_CALIBRATION 	(0x90)
#define HWRST_STATUS_AUTODLOADER 	(0xa0)
#define HWRST_STATUS_NORMAL2            (0Xf0)
#define UNKNOW_REBOOT_MODE 		0x77665508

#define LOW 	(0x4c)
#define MID 	(0x4d)
#define HIGH 	(0x48)

#ifdef D2200_DIALOG_PMU
unsigned char d2200_get_reboot_mode_reg(void);
void 	d2200_set_reboot_mode_reg(unsigned rst_mode);
void 	d2200_clear_reboot_mode_reg(void);
int 	d2200_check_poweron_cause_bywatchdog(void);
#endif

void sprd_check_reboot_mode(void)
{
	int inform3 = readl(INFORM3);
	int magc_ram_base= readl(MAGIC_RAM_BASE);
    unsigned char rst_mode= 0;	
	
	printf("%s: INFORM3 = %x ... \n", __func__, inform3);
	printf("%s: magc_ram_base = %x ...\n ", __func__, magc_ram_base);

	/*
	  Data in INFORM registers within IRAM is not getting retained 
	  after normal reset. So, GP_ID_1 register used to retain data 
	*/
		
#ifdef D2200_DIALOG_PMU
	rst_mode = d2200_get_reboot_mode_reg();
	d2200_clear_reboot_mode_reg();
#endif

	if ((inform3 & 0xfffffff0) == REBOOT_MODE_PREFIX
			&& cur_param_data_int(PARAM_REBOOT_MODE) != (inform3 & 0xf)) {
		printf("setting REBOOT_MODE to %d\n", (inform3 & 0xf));
		set_param_data(PARAM_REBOOT_MODE, (inform3 & 0xf), NULL, 1);
	} else if ((inform3 & 0xffff0000) ==
			(REBOOT_SET_PREFIX | REBOOT_SET_DEBUG)) {
		printf("setting DEBUG_LEVEL to 0x%x\n", (inform3 & 0xffff));
		set_param_data(PARAM_DEBUG_LEVEL, (inform3 & 0xffff), NULL, 1);
#if defined(DW_BUILD_ERROR)
	} else if ((inform3 & 0xffff0000) ==
			(REBOOT_SET_PREFIX | REBOOT_SET_SWSEL)) {
		printf("setting SWITCH_SEL to %d\n", (inform3 & 0xffff));
		set_param_data(PARAM_SWITCH_SEL, (inform3 & 0xffff), NULL, 1);
#endif		
	} else if ((inform3 & 0xffff0000) ==
			(REBOOT_SET_PREFIX | REBOOT_SET_SUD)) {
		printf("setting SUD to %d\n", (inform3 & 0xffff));
		set_param_data(PARAM_SUD_MODE, inform3, NULL, 1);
	} else if (readl(MAGIC_RAM_BASE) == MAGIC_CODE_DUMP) {
		printf("setting REBOOT_MODE to %d\n", (REBOOT_MODE_UPLOAD));
		set_param_data(PARAM_REBOOT_MODE, (REBOOT_MODE_UPLOAD), NULL, 1);
		printf("upload mode skip\n", __func__);
		return;
	} else {
		printf("rst_mode==%x\n",rst_mode);
#if 0 // TODO: check to enable the below codes.
#ifdef D2200_DIALOG_PMU
		if(d2200_check_poweron_cause_bywatchdog()){
#else
		if(hw_watchdog_rst_pending()){
#endif
			printf("hw watchdog rst int pending\n");
			if(rst_mode == HWRST_STATUS_RECOVERY) {
				set_param_data(PARAM_REBOOT_MODE, REBOOT_MODE_RECOVERY, NULL, 1);
				writel(0xffffffff, INFORM3);
			}
	//			return RECOVERY_MODE;
			else if (rst_mode == LOW) {
				set_param_data(PARAM_DEBUG_LEVEL, KERNEL_SEC_DEBUG_LEVEL_LOW, NULL, 1);
				writel(0x12345678, INFORM2);										// Since inform registers are not getting retained while reset, so setting inform2 here, instead of kernel : Nitin
			}
			else if (rst_mode == MID) {
				set_param_data(PARAM_DEBUG_LEVEL, KERNEL_SEC_DEBUG_LEVEL_MID, NULL, 1);
	                        writel(0x12345678, INFORM2);
	                }
			else if (rst_mode == HIGH) {
	                        set_param_data(PARAM_DEBUG_LEVEL, KERNEL_SEC_DEBUG_LEVEL_HIGH, NULL, 1);
	                        writel(0x12345678, INFORM2);
	                }
			else if(rst_mode == HWRST_STATUS_FASTBOOT){}
			//	return FASTBOOT_MODE;
			else if(rst_mode == HWRST_STATUS_NORMAL) {
				set_param_data(PARAM_REBOOT_MODE, REBOOT_MODE_NONE, NULL, 1);
				writel(0x12345678, INFORM2);
			}
			//	return NORMAL_MODE;
			else if(rst_mode == HWRST_STATUS_NORMAL2){}
			//	return WATCHDOG_REBOOT;
			else if(rst_mode == HWRST_STATUS_ALARM){}
			//	return ALARM_MODE;
			else if(rst_mode == HWRST_STATUS_SLEEP){}
			//	return SLEEP_MODE;
			else if(rst_mode == HWRST_STATUS_CALIBRATION){}
			//	return CALIBRATION_MODE;
			else if(rst_mode == HWRST_STATUS_PANIC){}
			//	return PANIC_REBOOT;
			else if(rst_mode == HWRST_STATUS_SPECIAL){}
			//	return SPECIAL_MODE;
			else if(rst_mode == HWRST_STATUS_AUTODLOADER){}
			//	return AUTODLOADER_REBOOT;
			else{
				printf(" a boot mode not supported\n");
				return 0;
			}
		}else{
			printf("no hw watchdog rst int pending\n");
			if(rst_mode == HWRST_STATUS_NORMAL2)
				return UNKNOW_REBOOT_MODE;
			else
				return 0;
		}	
#endif
		printf("skip\n", __func__);
		return;
	}
	

	/* Clear reboot mode to 0
	   Otherwise we see this value in the next booting session too */
	writel(0, INFORM3);
}

int sprd_check_T32(void)
{
#if defined(SB_BUILD_ERROR)
	u32 mCode = readl(MAGIC_RAM_BASE);

	if (mCode == MAGIC_CODE_T32) {
		/* available boot partition = 1MB.
		   but 1KB is used for param datas. */
		mmc_boot_binary_write((u8 *)USBDOWN_RAM_BASE, (1<<20)-1024);

		/* erase checksum informations. */
		mmc_berase(0, 8192);

		printf("\n%s: T32 fuse completed.\n", __func__);
		mdelay(1000);

		sys_restart('N', NULL);
	} else if (mCode == MAGIC_CODE_SDCARD) {
		ulong size = (1<<20) - 1024;
		u8 *pdb = (u8 *)&_nocached_buf;

		lcd_Clear();
		display_power_on();

		lcd_printf(10, 10, COLOR_RED, COLOR_BLACK, "SDCARD MODE\n");

		/* SD card initialization. */
		sdcard_initialize();
		lcd_printf(10, 30, COLOR_WHITE, COLOR_BLACK,
				"COPY BINARY FROM SDCARD..\n");
		sdcard_bread(pdb, 1, size);
		memcpy((u8 *)USBDOWN_RAM_BASE, pdb, size);

		lcd_printf(10, 50, COLOR_WHITE, COLOR_BLACK, "COPY BINARY TO EMMC..\n");
		mmc_bwrite_bootsector(pdb, 0, size);

		memset(pdb, 0, size);
		mmc_bread_bootsector(pdb, 0, size);

		if (!memcmp(pdb, (u8 *)USBDOWN_RAM_BASE, size)) {
			lcd_printf(10, 70, COLOR_GREEN, COLOR_BLACK,
					"SDCARD DOWNLOAD COMPLETED.\n");
		} else {
			lcd_printf(10, 70, COLOR_RED, COLOR_BLACK,
					"SDCARD DOWNLOAD FAILED.\n");
		}

		while (1);
	}
#endif
	return 0;
}

void sprd_cont_driver_register(void)
{
	timer_register();
	serial_register();
	display_register();
}

void sprd_restart_handler(char mode, char *cmd)
{
	printf("%s ('%c':%s)\n\n", __func__, mode, cmd ? cmd : "null");

	writel(0x0, MAGIC_RAM_BASE);

	/* LPM status setting. */
	if (mode == 'L') {
		writel(0x0, INFORM2);
	} else if (mode == 'N') {
		writel(0x12345678, INFORM2);
	}

	/* reboot mode setting when cmd is not null. */
	if (cmd) {
		u32 value;

		if (!strcmp(cmd, "download")) {
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_DOWNLOAD,
					INFORM3);
		} else if (!strcmp(cmd, "fota_bl")) {
			writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA_BL,
					INFORM3);
		} else if (!strncmp(cmd, "sud", 3)) {
			strtou32(cmd + 3, &value);
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SUD | value,
					INFORM3);
		}
	}

	d_cache_block_clean(MAGIC_RAM_BASE);
	d_cache_block_clean(INFORM1);
	d_cache_block_clean(INFORM2);
	d_cache_block_clean(INFORM3);
	d_cache_block_clean(INFORM4);
	d_cache_block_clean(INFORM5);
	d_cache_block_clean(INFORM6);
	d_cache_block_clean(INFORM7);

	arch_reset();

	while (1);
}
