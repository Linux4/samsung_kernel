/*
 * Copyright (C) 2011 System S/W Group, Samsung Electronics.
 * Geunyoung Kim <nenggun.kim@samsung.com>
 */

#include <config.h>
#include <common.h>
#include <param.h>
#include <drawimg.h>
#include <mmc.h>
#include <display.h>
#include <asm/mach-type.h>
#include <asm/arch/cpu.h>
#include <asm/arch/sprd_reg_base.h>
//#include <asm/arch/pmic.h>
#include <asm/arch/ldo.h>
#include <asm/arch/adc_drvapi.h>
#include <asm/arch/sprd_module_config.h>
#include <asm/arch/sprd_module_ver.h>
#include <asm/arch/regs_gpio.h>
#include <asm/arch/microusbic_sprd.h>

//#include <asm/arch/sprd_reg_aon_apb.h>
#include <asm/arch/pinmap.h>
#include <asm/arch/sprd_eic.h>

#define CONF_SW_RESET           1
#define CONF_HW_RESET           0


static int needOnKeyCheck;
static char ponLongOnkey = 0;

#ifdef CONFIG_eMMC_FIRMWARE_UPDATE
extern int check_eMMC_firm_update(void);
extern int start_eMMC_firm_update(void);
#endif

/* external functions. */
extern void LDO_TurnOffAllLDO(void);
extern void d2200_system_poweroff(void);


static int get_hwrev(void)
{
	/* we do not use h/w revision gpio pin */
	 return CONFIG_BOARD_REV;
}

#define PIN_CTL_REG 0x8C000000
static void chip_init(void)
{
	//ANA_REG_SET(ANA_ADIE_CHIP_ID,0);
	/* setup pins configration when LDO shutdown*/
	//__raw_writel(0x1fff00, PIN_CTL_REG);
	*(volatile unsigned int *)PIN_CTL_REG = 0x1fff00;
}

static void board_gd_init(void)
{
#ifdef CONFIG_TRUSTZONE
	int nr_bank = CONFIG_SDRAM_BANK_N;
#endif

	memset(&g_bd, 0, sizeof(g_bd));

	strcpy(g_bd.model_name, CONFIG_MODEL_NAME);
	strcpy(g_bd.cpu_name, CONFIG_SPRD_CHIP_NAME);

	g_bd.cpu_id = PRO_ID;
	g_bd.arch_number = ARCH_NUMBER;
	g_bd.boot_param_addr = BOOT_PARAM_RAM_BASE;

	g_bd.dram_bank[0].base = SDRAM0_BASE;
	g_bd.dram_bank[0].size = SDRAM0_SIZE;
	g_bd.dram_bank[1].base = SDRAM1_BASE;
	g_bd.dram_bank[1].size = SDRAM1_SIZE;

#ifdef CONFIG_2GDRAM
	/* if DDR is 1GB density, we have to resize bank information. */
	if (readl(INFORM7) == 0xCAFEDDE1) {
		const ulong bank_size = (ulong)0x10000000;
		int i;

		for (i = 0; i < nr_bank; i++) {
			g_bd.dram_bank[i].base = (ulong)(SDRAM0_BASE + (bank_size * i));
			g_bd.dram_bank[i].size = bank_size;
		}
	}
#endif

#ifdef CONFIG_TRUSTZONE
	g_bd.dram_bank[nr_bank - 1].size -= SDRAM_RESERVED_SIZE;
#endif
}

#define GPIO_ADDR_HW_REV 0x8C000408
#define ADDR_READ_HW_REV 0x8A000180
#define DETECT_BATT_VF_ADC 0x3F0
#define STC3115_I2C_ID			1
#define STC311x_REG_CTRL 0x01
#define STC311x_VF_DET			 0x08	 /* vf detect bit mask     */

static void board_power_off(void);
extern int i2c_read(u8 id, u8 reg_addr, u8 *buf);

static void board_gpio_init(void)
{
	unsigned int temp;

	temp = readl(GR_GEN0);
	temp |= (GEN0_GPIO_EN);
	writel(temp, GR_GEN0);
	
	volatile i = 0x2fff;
	while (i--);
	ANA_REG_OR (ANA_APB_CLK_EN,GPIO_EB);

	*(volatile u32 *) 0x8c000000 |= BIT_25|BIT_26; // mUSB_i2c
}

int charger_connected(void)
{
	sprd_eic_request(EIC_CHG_INT);
	udelay(3000);
	printf("eica status %x\n", sprd_eic_get(EIC_CHG_INT));
	return !!sprd_eic_get(EIC_CHG_INT);
}


void enable_charging(void)
{
	
}

void disable_charging(void)
{

}

#if 0 //huyue
static void check_power_source(void)
{
	*(volatile u32 *) GPIO_ADDR_HW_REV = 0x176;
	*(volatile u32 *)(ADDR_READ_HW_REV + 0x4) |= BIT_7; //set gpio DMSK 
	*(volatile u32 *)(ADDR_READ_HW_REV + 0x8) &=~ BIT_7; //set gpio input 
	*(volatile u32 *)(ADDR_READ_HW_REV + 0x28) |= BIT_7; //set gpio input enable 
	msleep(1);

	if(get_jig_status() != JIG_UART_OFF)
	{
		if(*(volatile u32 *)ADDR_READ_HW_REV &  BIT_7)
		{
			//for rev 0.0
			u32 adc = ADC_GetValue(ADC_CHANNEL_VF, ADC_SCALE_1V2);
			printf("%s: vf adc[%x]\n", __func__, adc);

			if (adc > DETECT_BATT_VF_ADC)
			{
				printf("%s: vf disconnected\n", __func__);
				board_power_off();
				while(1);		
			}
		}
		else
		{
			//for rev 0.1
#if 0
			u8 val;
			i2c_read(STC3115_I2C_ID, STC311x_REG_CTRL, &val);
			printf("%s: ctrl val[%x]\n", __func__, val);
		
			if (!(val & STC311x_VF_DET))
			{
				printf("%s: vf disconnected\n", __func__);
				board_power_off();
				while(1);		
			}
#endif
		}
	}
}
#endif

void board_chipset_init(void)
{
	static unsigned int check_onetime = 0;

	if (check_onetime == 0 )
	{
              //init_microusb_ic(); //huyue
#ifdef CONFIG_VF_CHECK_ENABLE
		// check_power_source(); //huyue temp
#endif
             lcd_ctrl_init(FB_RAM_BASE);
             //lcd_Clear(); 

	}

	check_onetime ++;
}

#if 0 // sprd doesn't use this feature.
static void board_power_init(void)
{
	int magc_ram_base= readl(MAGIC_RAM_BASE);

	pmic_init();

	if(pmic_check_rtc_alarm())
	{
		if((magc_ram_base!= 0) && (magc_ram_base != MAGIC_CODE_DUMP)) /* 0: normal reboot, MAGIC_CODE_DUMP: upload reboot */
		{
			printf("%s power off! : we don't use alarm power on\n", __func__);
			pmic_onoff(NULL, 1);
		}
			
	}
}

static void board_power_on(void)
{
	board_power_init();
}
#endif // sprd doesn't use this feature.

static void board_power_off(void)
{
	LDO_TurnOffAllLDO();
    while(1);

}

static void board_restart(char mode, char *cmd)
{
	sprd_restart_handler(mode, cmd);
}

static void board_set_dev_pm(int dev, int on)
{

}

static void board_set_dev_gpio(int dev)
{

}

#define CHARGER_RECHECK_DELAY   (500)

#if 0 //huyue
static int check_pm_status(void)
{
	int jig_state;

	// Maybe we do not use
	int chg_state;
//	int bat_state;
//	int fg_state;
//	int vcell;

	/* jig state */
	jig_state = get_jig_status();
	if (jig_state == 1) {
		chg_state = get_charger_status();
		if (chg_state == VALID_CHARGER) {

			printf("%s: chargable jig, LPM boot\n", __func__);
			g_bd.sys_bootm |= SYS_BOOTM_LPM;
			*(volatile unsigned int *)REG_EMU_AREA=CHARGING_STATE;			
		} else {
			printf("%s: non chargable jig,"
				" bypass check power\n", __func__);
		}

		return 0;
	}

	/* check power on src is acok */
	chg_state = get_charger_status();

	/* normal reset state, charger state, voltage state */
	if (readl(INFORM2) == 0x12345678) {
		printf("%s: normal reset, do not enter LPM mode.\n", __func__);
		needOnKeyCheck = 0;
	// If the recovery mode is enabled on TouchWiz, please enable the following codes.
	} else if (cur_param_data_int(PARAM_REBOOT_MODE) == REBOOT_MODE_RECOVERY) {
		printf("INFORM2 = 0x%x\n", readl(INFORM2));
		printf("%s: Recovery mode is enabled. don't enter LPM mode.\n", __func__);
		needOnKeyCheck = 0;
	} else if (d2200_check_poweron_cause_byLonkey()){
		printf("%s: power key long press reboot\n", __func__);		
		needOnKeyCheck = 0;	
		ponLongOnkey = 1;
	} else if ( chg_state == VALID_CHARGER) {
		/* charger detection */
		printf("%s: battery ok, charger boot, waiting %dms and check again\n",
				__func__, CHARGER_RECHECK_DELAY);
		msleep(CHARGER_RECHECK_DELAY);
		chg_state = get_charger_status();
		printf("%s: battery charging check chg_state %d\n",__func__, chg_state);
		if (chg_state == INVALID_CHARGER) {
			board_power_off();
		}
		g_bd.sys_bootm |= SYS_BOOTM_LPM;
		needOnKeyCheck = 0;	
		*(volatile unsigned int *)REG_EMU_AREA=CHARGING_STATE;	
	} else {
		printf("%s: charger is not detected\n", __func__);
		//In case of JIG insertion onkey check should be omitted
		needOnKeyCheck = 1;
	}

	return 0;
}
#endif

void board_interrupt_init(void)
{

}

void board_interrupt_enable(void)
{

}

int mach_board_initialize(void)
{
//	MMU_Init(CONFIG_MMU_TABLE_ADDR); // mmu_enable();  wangdali mmu

//	ADI_init();	
//	chip_init();	
//	LDO_Init();	

//	board_gd_init();	
	sprd_cont_driver_register();	
//	misc_init();
//	ADI_init();
//	LDO_Init();
//	ADC_Init();
//	pin_init();
//	sprd_eic_init();
//	sprd_gpio_init();
//	sound_init();
//    init_ldo_sleep_gr();//wangdali patch
//    TDPllRefConfig(1);
//	LDO_Init();
 	sprd_keypad_init();
	mmc_initialize();
	

//	MMU_Init_print(CONFIG_MMU_TABLE_ADDR); // mmu_enable();
//	usb_download(); 
	return 0;
}


#if defined(CONFIG_DETECT_USB_DOWNLOAD_MODE)
extern u32 is_usb_cable(void);
#endif

int mach_board_main(void)
{
	int ret;	
//	u32 nps_status;

	// Setting onkey_reset here instead of LDO_Init, bcoz can't use debug level in LDO_Init function : Nitin
	/* /// if(cur_param_data_int(PARAM_DEBUG_LEVEL) == KERNEL_SEC_DEBUG_LEVEL_LOW)
                        d2200_set_onkey_reset(CONF_HW_RESET);
        else
                        d2200_set_onkey_reset(CONF_SW_RESET);
		*/

	board_chipset_init();

#if 0 //huyue temp
	CHG_Init();
    sprd_start_download(GD_DN_MODE_KEYPAD); 
  #endif

	sprd_check_keypad();

#if defined(CONFIG_DETECT_USB_DOWNLOAD_MODE)
	if(is_usb_cable()==1)
        sprd_start_download(GD_DN_MODE_KEYPAD);
#endif
	

//	check_MD5_checksum();

	sprd_check_reboot_mode();

	//Temporary fix for the value not coming to JIG_UART_OFF
	if(get_jig_status() == JIG_UART_OFF || JIG_UART_ON)
	{
		calibration_detect(1);
	}
/*
#ifdef CONFIG_eMMC_FIRMWARE_UPDATE
	if( check_eMMC_firm_update() )
	{
		if( !start_eMMC_firm_update() )
		{
			printFactoryString("AST_DLOAD..\n");
			sprd_start_download(GD_DN_MODE_NONE);
		}
	}
#endif	*/		

	/*
	 * check boot conditions.
	 * download -> upload -> dn error -> reboot mode //upload -> download -> dn error -> ota self update.
	 */
	if ((ret = sprd_check_download()) != GD_DN_MODE_NONE) {
		printFactoryString("AST_DLOAD..\n");

		sprd_start_download(ret);
	} else if ((ret = sprd_check_upload(0)) != 0) {

		printFactoryString("AST_UPLOAD..\n");

		sprd_start_upload();
	} else if (check_dn_error() != 0) {
		sprd_start_download(GD_DN_MODE_DN_ERROR);
	} else if ((ret = cur_param_data_int(PARAM_REBOOT_MODE)) !=\
			REBOOT_MODE_NONE) {
		if (ret == REBOOT_MODE_RECOVERY) {
			g_bd.sys_bootm |= SYS_BOOTM_RECOVERY;
			printf("%s: recovery set!(0x%x)\n", __func__, g_bd.sys_bootm);
		} else if (ret == REBOOT_MODE_FOTA_BL) {
			set_param_data(PARAM_REBOOT_MODE, REBOOT_MODE_NONE,
					NULL, 1);
		
			sys_restart('N', NULL);
		}
	}	
#if 0
	/*
	 * check board power status - LPM and low battery.
	 */
	if (check_pm_status() < 0) {
		sys_power_off();
	} else if (g_bd.sys_bootm & SYS_BOOTM_LPM) {
		if (!(g_bd.sys_bootm & SYS_BOOTM_RECOVERY)) {
			printFactoryString("AST_CHARGING..\n");
			scr_draw(lpm);
// sprd		enable_charging();

			return display_power_on();
		} else {	
			/* remove lpm status if recovery was set already. */
			g_bd.sys_bootm &= ~SYS_BOOTM_LPM;
		}
	}
	/*else if(power_button_pressed() && readl(MAGIC_RAM_BASE) != 0 && get_jig_status() == 0  && (readl(INFORM3) != 0x12345670) )
	{
			board_power_off();
	}
	*/	
#endif

	printFactoryString("AST_POWERON..\n");
	//add for download fail remind function     myeong120510
	//temporary fix of g_bd.sys_bootm condition p.gaikwad
	if (g_bd.sys_bootm & SYS_BOOTM_LPM)
	scr_draw(lpm);
	else
	{
	scr_draw(logo);
	//lcd_display();
	}
	return display_power_on(); 
}

void switch_init(int sel)
{

}

int get_lpm_state(void)
{
	return (int)!!(g_bd.sys_bootm & SYS_BOOTM_LPM);
}

#if 1 //huyue
int check_jig_adc_value(int mode)
{
     return FALSE;
}
#else
int check_jig_adc_value(int mode)
{
	u32 muic_status;
	muic_status = microusb_get_device() & 0xff;

	if (mode == GD_JIG_STATUS_DOWNLOAD &&\
			muic_status == JIG_USB_ON) {
		return TRUE;
	} else if (mode == GD_JIG_STATUS_SWITCHED &&\
			muic_status == JIG_USB_OFF) {
		return TRUE;
	}

	return FALSE;
}
#endif

int power_button_pressed(void)
{
	printf("power_button_pressed \n");

	#if defined (CONFIG_SPX15)
	sci_glb_set(REG_AON_APB_APB_EB0,BIT_AON_GPIO_EB | BIT_EIC_EB);
	sci_glb_set(REG_AON_APB_APB_RTC_EB,BIT_EIC_RTC_EB);
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_EIC_EN);
	#else
	sci_glb_set(REG_AON_APB_APB_EB0,BIT_GPIO_EB | BIT_EIC_EB);
	sci_glb_set(REG_AON_APB_APB_RTC_EB,BIT_EIC_RTC_EB);
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_EIC_EN | BIT_ANA_GPIO_EN);
	#endif
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN,BIT_RTC_EIC_EN);

	ANA_REG_SET(ADI_EIC_MASK, 0xff);

	udelay(3000);

	int status = ANA_REG_GET(ADI_EIC_DATA);
	status = status & (1 << 2);

	printf("power_button_pressed eica status 0x%x\n", status );
	
	return !status;//low level if pb hold

}


#if 1 //huyue
void forced_enable_log(void)
{
}
#else
void forced_enable_log(void)
{
#if defined(CONFIG_FORCED_CONTROL_KERNEL_LOG)
#if defined(CONFIG_USING_UART_BOOT_ON_FOR_DOCK)
	const unsigned int CHECK_UART = JIG_UART_OFF;
#else
	const unsigned int CHECK_UART = JIG_UART_ON;
#endif
	if (get_jig_status() == CHECK_UART) // Check JIG Uart
 	{
		set_log_flag(TRUE);												// Sboot Log Enable
		set_param_data(PARAM_KERNEL_LOG_LEVEL, KERNEL_SEC_DEBUG_LEVEL_ENABLE, NULL, 0);	// Kernel Log Enable
		
		printf("\n");
		printf("Samsung S-Boot 4.0%s for %s (%s - %s)\n\n",
			CONFIG_LOCALVERSION, CONFIG_MODEL_NAME,
			__DATE__, __TIME__);
	
		print_sysinfo();
	}
#endif
}
#endif

/* pointers to machine specific control functions. */
/* to support board power off and restart anywhere. */

/* board device control functions. */
void (*sys_bd_set_dev_pm)(int, int) = board_set_dev_pm;
void (*sys_bd_set_dev_gpio)(int) = board_set_dev_gpio;
void (*sys_power_off)(void) = board_power_off;
void (*sys_restart)(char, char *) = board_restart;
