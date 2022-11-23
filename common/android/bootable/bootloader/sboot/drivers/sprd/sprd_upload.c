
#include <config.h>
#include <common.h>
#include <rdx_dump.h>
#include <param.h>
#include <drawimg.h>
#include <display.h>
#include <asm/arch/cpu.h>

/* function definitions. */
static void sprd_print_debug_info(unsigned char * strptr, int drop_prefix_length)
{
	int c = 0,line = 0;
	int FONTX = 8, FONTY= 19, MARGINY = 10, MAXCOL=89;

	while(*strptr > 0x00 && *strptr <= 0x7F) {
		if(*strptr == 0x0A) {
			c = 0;
			line++;
		}
		if(c >= drop_prefix_length && c <= (MAXCOL + drop_prefix_length)) {
			lcd_draw_font(FONTX*(c-drop_prefix_length), FONTY*MARGINY+FONTY*line, COLOR_GREEN, COLOR_BLACK, 1 ,strptr++);
			c++;
		} else if(c > (MAXCOL + drop_prefix_length)) {
			c = drop_prefix_length;
			line++;
		} else {
			/* drop the time information because of time limitation */
			c++;
			strptr++;
		}
	}
	return;
}

static void display_upload_image(void)
{
	char string[64] = {0, };
	
	unsigned int upload_code = readl(INFORM3);
	/* it's written by kernel_sec_set_cause_strptr */
	/* we do not have enough Memory so Only "CP Crash" String */
//	unsigned char *strptr = "CP crashed"; // (unsigned char *)(MAGIC_RAM_BASE + 4);
	/* In upload mode, reset cause is written in MAGIC_RAM_BASE. */
	u32 rst_stat = readl(RST_STAT);

	printf("RST_STAT = 0x%x\n", rst_stat);
	printf("INFORM0 = 0x%x\n", readl(INFORM0));
	printf("INFORM1 = 0x%x\n", readl(INFORM1));
	printf("INFORM2 = 0x%x\n", readl(INFORM2));
	printf("INFORM3 = 0x%x\n", readl(INFORM3));
	printf("INFORM4 = 0x%x\n", readl(INFORM4));
	printf("INFORM5 = 0x%x\n", readl(INFORM5));
	printf("INFORM6 = 0x%x\n", readl(INFORM6));
//	printf("INFORM7 = 0x%x\n", readl(INFORM7));

	lcd_Clear();

	switch (upload_code & KERNEL_SEC_UPLOAD_CAUSE_MASK) {
		/*
		 *  1st ROW : UPLOAD_ICON_1_X, UPLOAD_ICON_1_Y : Kernel panic, CP crash
		 *  2nd ROW : UPLOAD_ICON_2_X, UPLOAD_ICON_2_Y : User fault, Forced dump, Power reset
		 *  3rd ROW : UPLOAD_ICON_2_X, UPLOAD_ICON_3_Y
		 *  4th ROW : UPLOAD_ICON_2_X, UPLOAD_ICON_4_Y
		 */
	case UPLOAD_CAUSE_KERNEL_PANIC:
		draw_file("ani_upload_1_kernel_panic.jpg", UPLOAD_ICON_1_X,
		     UPLOAD_ICON_1_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		break;

	case UPLOAD_CAUSE_CP_ERROR_FATAL:
		draw_file("ani_upload_2_cp_crash.jpg",
		     UPLOAD_ICON_1_X, UPLOAD_ICON_1_Y, UPLOAD_ICON_W,
		     UPLOAD_ICON_H);
		break;

	case UPLOAD_CAUSE_FORCED_UPLOAD:
		draw_file("ani_upload_3_forced_upload.jpg", UPLOAD_ICON_2_X,
		     UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		break;

	case UPLOAD_CAUSE_USER_FAULT:
		draw_file("ani_upload_5_user_fault.jpg", UPLOAD_ICON_2_X,
		     UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		break;

	case UPLOAD_CAUSE_HSIC_DISCONNECTED:
		draw_file("ani_upload_6_hsic_disconnected.jpg", UPLOAD_ICON_2_X,
		     UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		break;

	default:
	#if defined(PM_BUILD_ERROR)
		if (pmic_check_wtsr_intr())
			draw_file("ani_upload_4_wtsr.jpg", UPLOAD_ICON_2_X,
				 UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		else if (pmic_check_smpl_intr())
			draw_file("ani_upload_4_smpl.jpg", UPLOAD_ICON_2_X,
				 UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		else if (rst_stat & (1 << 16))
			draw_file("ani_upload_4_hardware_reset.jpg", UPLOAD_ICON_2_X,
					UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		else if (rst_stat & (1 << 20))
			draw_file("ani_upload_4_watchdog_reset.jpg", UPLOAD_ICON_2_X,
			     UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		else 
			draw_file("ani_upload_4_unknown_reset.jpg", UPLOAD_ICON_2_X,
					UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
	#endif
		if (rst_stat == POWER_ON_CAUSE_WATCHDOG_RESET)
			draw_file("ani_upload_4_watchdog_reset.jpg", UPLOAD_ICON_2_X,
			     UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);
		else
			draw_file("ani_upload_4_unknown_reset.jpg", UPLOAD_ICON_2_X,
			     UPLOAD_ICON_2_Y, UPLOAD_ICON_W, UPLOAD_ICON_H);

		break;
	}

	/* display rst_stat value on lcd. */
	sprintf(string, "[ RST_STAT = 0x%x ]", rst_stat);
	lcd_draw_font(10, 10, COLOR_RED, COLOR_BLACK,strlen(string), string); //huyue

#ifdef CONFIG_DISP_ERR_INFO
	/* display errer information(kernel panic or cp crash) on screen */
	if ((upload_code & KERNEL_SEC_UPLOAD_CAUSE_MASK) \
		== UPLOAD_CAUSE_KERNEL_PANIC) {
		sprd_print_debug_info((unsigned char *)(MAGIC_RAM_BASE + 0x400), 22);
	} else if((upload_code & KERNEL_SEC_UPLOAD_CAUSE_MASK) \
		== UPLOAD_CAUSE_CP_ERROR_FATAL) {
		sprd_print_debug_info(strptr, 0);
	} else
		lcd_draw_font(10, 19 * 22, COLOR_RED, COLOR_BLACK, 512, strptr);
#else
//	lcd_draw_font(10, 19 * 22, COLOR_RED, COLOR_BLACK, 512, strptr);
#endif

	display_power_on();
}

#if 0 //huyue modified for shark/sc8830
static void set_rdx_info(void)
{
	struct s_rdx_mem_reg *mr;

	/* setup rdx data information. */
	mr = rdx_info_setup();
//sprd	display_power_on();
#ifdef CONFIG_BOOTLOADER_LOGGING
	// Due to limitation of rdx tool, add dumpp address(0x10000000)
	rdx_info_fill(mr++, "log", (ulong)_log_buf + 0x10000000, (ulong)LOG_LENGTH, 1 /*valid*/);
#else
	rdx_info_fill(mr++, "Reserved_log", 0xFFFFFFF0, 0xFFFFFFF3 - 0xFFFFFFF0 + 1, 0 /*invalid*/);        // RDX limitation
#endif
	if(CP_RAM_SIZE != 0) // If CP_RAM_SIZE == 0, don't need to get cp ram dump
		{rdx_info_fill(mr++, "cp", (LPDDR0_BASE_ADDR + 0x10000000), CP_RAM_SIZE, 1 /*valid*/);}
	else
		rdx_info_fill(mr++, "Reserved_cp", 0xFFFFFFF0, 0xFFFFFFF3 - 0xFFFFFFF0 + 1, 0 /*invalid*/);        // RDX limitation

	rdx_info_fill(mr++, "ap", (LPDDR0_AP_BASE_ADDR+ 0x10000000 ), ((LPDDR0_BASE_ADDR + LPDDR0_SIZE) - LPDDR0_AP_BASE_ADDR), 1 /*valid*/);

	/* Support NAND Dump by RDX Tool */
	// 0x474B0000; // Change Signature because full dump size over 0x7777474B - 0x474B0000 ( 800MB)
	rdx_info_fill(mr++, "nanddump", 0x174BC01C, ((0x7777474B +1) - 0x174BC01C), 0 /*valid*/);

#ifdef CONFIG_USE_VLX
        rdx_info_fill(mr++, "vm", (LPDDR0_BASE_ADDR+ CP_RAM_SIZE +0x10000000), VM_RAM_SIZE, 1 /*valid*/);
#else
	rdx_info_fill(mr++, "Reserved_vm", 0xFFFFFFF0, 0xFFFFFFF3 - 0xFFFFFFF0 + 1, 0 /*invalid*/);        // RDX limitation
#endif	

	rdx_info_fill(mr++, "Reserved", 0xFFFFFFF0, 0xFFFFFFF3 - 0xFFFFFFF0 + 1, 0 /*invalid*/);	// RDX limitation

		/* Support AP reboot after ramdump by RDX Tool */
//	rdx_info_fill(mr++, "dummy", 0xFFFFFFFC, ((0xFFFFFFFF + 1) - 0xFFFFFFFC), 1 /*valid*/);	// OLD Reboot Command
	rdx_info_fill(mr++, "dummy", 0xFFFFFFF0, 0xFFFFFFF3 - 0xFFFFFFF0 + 1,  1 /*valid*/ );
}
#else
static void set_rdx_info(void)
{
#if 0 // TODO: apply memory map
        struct s_rdx_mem_reg *mr;

        /* setup rdx data information. */
        mr = rdx_info_setup();

#ifdef CONFIG_BOOTLOADER_LOGGING
        // Due to limitation of rdx tool, add dumpp address(0x10000000)
        rdx_info_fill(mr++, "log", (ulong)_log_buf + 0x10000000, (ulong)LOG_LENGTH, 1 /*valid*/);
#else
        rdx_info_fill(mr++, "Reserved_log", 0xFFFFFFF0, 0xFFFFFFF3 - 0xFFFFFFF0 + 1, 0 /*invalid*/);        // RDX limitation
#endif
        if(CP_RAM_SIZE != 0) // If CP_RAM_SIZE == 0, don't need to get cp ram dump
                {rdx_info_fill(mr++, "cp", (LPDDR0_CP_BASE_ADDR + 0x10000000), CP_RAM_SIZE, 1 /*valid*/);}
        else
                rdx_info_fill(mr++, "Reserved_cp", 0xFFFFFFF0, 0xFFFFFFF3 - 0xFFFFFFF0 + 1, 0 /*invalid*/);        // RDX limitation

        rdx_info_fill(mr++, "ap", (LPDDR0_AP_BASE_ADDR+ 0x10000000 ), (AP_RAM_SIZE), 1 /*valid*/);
        rdx_info_fill(mr++, "ap2", (LPDDR0_AP2_BASE_ADDR+ 0x10000000 ), (AP2_RAM_SIZE), 1 /*valid*/);

        /* Support NAND Dump by RDX Tool */
        // 0x474B0000; // Change Signature because full dump size over 0x7777474B - 0x474B0000 ( 800MB)
        // rdx_info_fill(mr++, "nanddump", 0x174BC01C, ((0x7777474B +1) - 0x174BC01C), 0 /*valid*/);

#ifdef CONFIG_USE_VLX
        rdx_info_fill(mr++, "vm", LPDDR0_VM_BASE_ADDR, VM_RAM_SIZE, 1 /*valid*/);
#else
        rdx_info_fill(mr++, "Reserved_vm", 0xFFFFFFF0, 0xFFFFFFF3 - 0xFFFFFFF0 + 1, 0 /*invalid*/);        // RDX limitation
#endif

        /* Support AP reboot after ramdump by RDX Tool */
        rdx_info_fill(mr++, "dummy", 0xFFFFFFFC, ((0xFFFFFFFF + 1) - 0xFFFFFFFC), 0 /*valid*/); // OLD Reboot Command
#else
    printf("%s entered\n", __func__);
#endif
}

#endif

static int do_upload(void)
{
	if (!(g_bd.sys_bootm & SYS_BOOTM_UP)) {
		printf("%s: error!(0x%x)\n", __func__, g_bd.sys_bootm);
		return -EERROR;
	}

	set_rdx_info();
	display_upload_image();

	usb_download();
	return 0;
}

int sprd_check_upload(int check)
{
	s32 debug_level = cur_param_data_int(PARAM_DEBUG_LEVEL);
	s32 upmode = 0;

	printf("%s: MAGIC(0x%x), RST_STAT(0x%x)\n", __func__,
	       readl(MAGIC_RAM_BASE), readl(RST_STAT));

	if (readl(MAGIC_RAM_BASE) == MAGIC_CODE_DUMP) {
		unsigned int upload_code =
		    readl(INFORM3) & KERNEL_SEC_UPLOAD_CAUSE_MASK;
		printf("%s: debug level is %x!\n", __func__, debug_level);
		switch (debug_level) {
		case KERNEL_SEC_DEBUG_LEVEL_MID:
		case KERNEL_SEC_DEBUG_LEVEL_AUTO:
#if 0
			/* not to upload when hardware reset with jigon occurred */
			if ((UPLOAD_CAUSE_KERNEL_PANIC != upload_code)
			    && (UPLOAD_CAUSE_FORCED_UPLOAD != upload_code)
			    && (UPLOAD_CAUSE_CP_ERROR_FATAL != upload_code)
			    && (UPLOAD_CAUSE_HSIC_DISCONNECTED != upload_code)) {
//			    && pmic_check_jigon_pwron()) {
				printf("%s: skip anything else %x\n",
				     __func__, upload_code);
				break;
			}
#endif			
			/* Check whether we are in upload mode, do not reset the
			  * magic setting.
			  */
			if (!check)		
			writel(0, MAGIC_RAM_BASE);
			upmode = 1;			

			/* If checking only, do not fall through.
			 */
			if (check)
				break;

			/* fall through */
		case KERNEL_SEC_DEBUG_LEVEL_HIGH:
			/* keep 7sec power on reset case from upload */
		#if defined(SB_BUILD_ERROR)
			if ((readl(RST_STAT) & 0x10000)
			    && pmic_7sec_reset()) {
				printf("%s: skip 7 second power on reset\n",
				       __func__);
				break;
			}
		#endif
			/* Check whether we are in upload mode, do not reset the
			  * magic setting.
			  */
			if (!check)		
			writel(0, MAGIC_RAM_BASE);
			upmode = 1;
			break;

		case KERNEL_SEC_DEBUG_LEVEL_LOW:
		default:
			printf("%s: level low or unknown: skip %x\n",
			       __func__, upload_code);
			break;
		}
	}

	switch (debug_level) {
	case KERNEL_SEC_DEBUG_LEVEL_AUTO:
	case KERNEL_SEC_DEBUG_LEVEL_HIGH:
	#if defined(SB_BUILD_ERROR)
		/* Check WTSR and SMPL */
		if (pmic_check_wtsr_intr()) {
			printf("%s: WTSR interrupt!!!!\n", __func__);
			upmode = 1;
		}

		if (pmic_check_smpl_intr()) {
			printf("%s: SMPL interrupt!!!!\n", __func__);
			upmode = 1;
		}
	#endif
		break;
	case KERNEL_SEC_DEBUG_LEVEL_MID:
	#if defined(SB_BUILD_ERROR)
		/* Check WTSR and SMPL */
		if (pmic_check_wtsr_intr()) {
			printf("%s: WTSR interrupt!!!!\n", __func__);
			upmode = 1;
		}

		if (pmic_check_smpl_intr()) {
			printf("%s: SMPL interrupt!!!!\n", __func__);
			/* upmode = 1; */
		}
	#endif
	case KERNEL_SEC_DEBUG_LEVEL_LOW:
	default:
		break;
	}

	return upmode;
}

int sprd_start_upload(void)
{
	g_bd.sys_bootm |= SYS_BOOTM_UP;
	
	return do_upload();
}

static s32 cmd_upload(s32 argc, s8 * argv[])
{	
	return sprd_start_upload();
}
static s8 upload_help[] = "usb upload command.";
__commandlist(cmd_upload, "upload", upload_help);
