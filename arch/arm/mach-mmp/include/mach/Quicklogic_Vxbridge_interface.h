/*
* Copyright (C) 2006-2013, Samsung Electronics Co., Ltd. All Rights Reserved.
* Written by System S/W Group, Mobile Communication Division.
*/

#ifndef __VX6B3E_H
#define __VX6B3E_H

#if defined (CONFIG_MACH_CS05) || defined(CONFIG_MACH_DEGAS)
extern int vx6b3e_brige_init(struct pxa168fb_info *fbi);
extern int vx6b3e_reset(void);
extern int vx6b3e_power(struct pxa168fb_info *fbi, int on);
extern int hx8369_backlight_updata(void);
extern int mpmu_vcxoen(void);
extern int get_panel_id(void);

/* CABC PWM */
static char hx8369b_backlight_51h[] = {
	0x51,
	0x00, 0x3C
};

#define VX6B3E_CONTROL_BY_I2C

#define VX6B3E_PWR_EN_VXE_1P8 64
#define VX6B3E_PWR_EN_VXE_1P2 63
#define VX6B3E_RESET 125
/*13M_EN FOR F/F*/
#define GPIO_13M_EN (30)
/*LCD/VX Bridge HW switch*/
#define GPIO18_LCD_RESET (18)
#endif

#if defined(CONFIG_MACH_GOYA)
extern int vx5b3d_brige_init(struct pxa168fb_info *fbi);
extern int vx5b3d_power(struct pxa168fb_info *fbi, int on);
extern int lcd_power(struct pxa168fb_info * fbi, int on);
extern int get_panel_id(void);

/*Vee strenght*/
#define V5D3BX_VEESTRENGHT		0x00001f07
#define V5D3BX_VEEDEFAULTVAL		0
#define V5D3BX_DEFAULT_STRENGHT		5
#define V5D3BX_DEFAULT_LOW_STRENGHT	8
#define V5D3BX_DEFAULT_HIGH_STRENGHT	10
#define V5D3BX_MAX_STRENGHT		15

#define V5D3BX_CABCBRIGHTNESSRATIO	815 /*18P5%*/

#define LVDS_CLK_48P05Mhz		0
#define LVDS_CLK_50P98Mhz		1
#define V5D3BX_6P3KHZ_DEFAULT_RATIO	8000
#define V5D3BX_6P3KHZ_REG_VALUE		2040

/*VX Bridge Power*/
#define VX5B3D_PWR_EN_VXE_3P3	19
#define LED_BACK_LIGHT_RST	12
#define VX5B3D_RST 20
#define GOYA_LCD_3P3 9

/*Panel type*/
#define HX8282A_PANEL_BOE 0x0
#define HX8282A_PANEL_SDC 0x1
#define HX8282A_PANEL_CPT 0x2
#define HX8282A_PANEL_HIMAX 0x3
#define HX8282A_PANEL_NONE 0x4

extern int board_version;
extern unsigned int system_rev;
#endif


/**************************************************
*	Common Interface of Quicklogic Bridge IC		*
***************************************************/
#if defined(CONFIG_QUICKLOGIC_BRIDGE)

extern int Quicklogic_mipi_write(struct pxa168fb_info *fbi, \
					u32 address, u32 value, u32 data_size);
extern int Quicklogic_i2c_read32(u16 reg, u32 *pval);
extern int Quicklogic_i2c_read(u32 addr, u32 *val, u32 data_size);
extern int Quicklogic_i2c_write32(u16 reg, u32 val);
extern int Quicklogic_i2c_release(void);
extern int Quicklogic_i2cTomipi_write(int dtype, int vit_com, \
					u8 data_size, u8 *ptr );
extern int Quicklogic_i2cTomipi_read(int dtype, int vit_com, \
					u8 data_size, u8 reg , u8 *pval);

/*For shared I2C with Touch*/
extern void i2c1_pin_changed(int gpio);
#if  defined(CONFIG_SPA) || defined(CONFIG_SPA_LPM_MODE)
extern int spa_lpm_charging_mode_get();
#else
extern unsigned int lpcharge;
#endif
#endif

#define QUICKLOGIC_MIPI_VENDOR_ID_1		0x5
#define QUICKLOGIC_MIPI_VENDOR_ID_2		0x1
#define QUICKLOGIC_MIPI_COMMAND_CSR_WRITE	0x40
#define QUICKLOGIC_MIPI_COMMAND_CSR_OFFSET	0x41

#define QUICKLOGIC_IIC_READ_CONTROL 0x0E
#define QUICKLOGIC_IIC_WRITE_CONTROL 0x0A
#define QUICKLOGIC_BYTE_I2C_RELEASE (0x0u)
#define QUICKLOGIC_IIC_RELEASE  {\
		QUICKLOGIC_BYTE_I2C_RELEASE, \
}

#define QL_MIPI_PANEL_CMD_SIZE 255
#define QL_VX_LCD_VC 0			/* dcs read/write */
#define CONTROL_BYTE_DCS       (0x08u)
#define CONTROL_BYTE_GEN       (0x09u)

#define DTYPE_DCS_WRITE		0x05	/* short write, 0 parameter */
#define DTYPE_DCS_WRITE1	0x15	/* short write, 1 parameter */
#define DTYPE_DCS_READ		0x06	/* read */
#define DTYPE_DCS_LWRITE	0x39	/* long write *//* generic read/write */
#define DTYPE_GEN_WRITE		0x03	/* short write, 0 parameter */
#define DTYPE_GEN_WRITE1	0x13	/* short write, 1 parameter */
#define DTYPE_GEN_WRITE2	0x23	/* short write, 2 parameter */
#define DTYPE_GEN_LWRITE	0x29	/* long write */
#define DTYPE_GEN_READ		0x04	/* long read, 0 parameter */
#define DTYPE_GEN_READ1		0x14	/* long read, 1 parameter */
#define DTYPE_GEN_READ2		0x24	/* long read, 2 parameter */

/*Brightness level*/
#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS_LEVEL		255
#define MID_BRIGHTNESS_LEVEL		195
#define LOW_BRIGHTNESS_LEVEL		20
#define DIM_BRIGHTNESS_LEVEL		19
#define DEFAULT_BRIGHTNESS		MID_BRIGHTNESS_LEVEL
#define AUTOBRIGHTNESS_LIMIT_VALUE	207

#define LOW_BATTERY_LEVEL		10
#define MINIMUM_VISIBILITY_LEVEL	30

/**************************************************
*		QUICKLOGIC MACRO FOR MIPI		*
***************************************************/
static char quicklogic_csr_wr_payload[9] = {
					QUICKLOGIC_MIPI_VENDOR_ID_1,
					QUICKLOGIC_MIPI_VENDOR_ID_2,
					QUICKLOGIC_MIPI_COMMAND_CSR_WRITE,
					0x0, 0x0,	/* address 16bits */
					0x0, 0x0, 0x0, 0x0 /* data max 32bits */
};


/**************************************************
*		QUICKLOGIC MACRO FOR I2C			*
***************************************************/
#define GEN_QL_CSR_OFFSET_LENGTH  {\
		CONTROL_BYTE_GEN, \
        0x29,  /* Data ID */\
        0x05,  /* Vendor Id 1 */\
        0x01,  /* Vendor Id 2 */\
        0x41,  /* Vendor Unique Command */\
        0x00,  /* Address LS */\
        0x00,  /* Address MS */\
        0x00,  /* Length LS */\
        0x00,  /* Length MS */\
    }

#define GEN_QL_CSR_WRITE  {\
		CONTROL_BYTE_GEN, \
        0x29,  /* Data ID */\
        0x05,  /* Vendor Id 1 */\
        0x01,  /* Vendor Id 2 */\
        0x40,  /* Vendor Unique Command */\
        0x00,  /* Address LS */\
        0x00,  /* Address MS */\
        0x00,  /* data LS */\
	0x00, \
	0x00, \
        0x00,  /* data MS */\
    }

/**************************************************
*		Structure for QUICKLOGIC Driver control	*
***************************************************/
enum CABC {
	CABC_OFF,
	CABC_ON,
	CABC_MAX,
};

struct Quicklogic_backlight_value {
	const unsigned int max;
	const unsigned int mid;
	const unsigned char low;
	const unsigned char dim;
};

typedef struct Quicklogic_bridge_info {
	enum CABC			cabc;

	struct class			*mdnie;
	struct device			*dev_mdnie;
	struct device			*dev_bd;

	struct backlight_device		*bd;
	struct lcd_device		*lcd;
	struct Quicklogic_backlight_value	*vee_lightValue;
	struct mutex			lock;
	struct mutex			pwr_lock;
	struct mutex			lvds_clk_switch_lock;

	unsigned int			auto_brightness;
	unsigned int			vee_strenght;
	unsigned int			prevee_strenght;
	unsigned int			first_count;
	unsigned int			lcd_panel;
	unsigned int			lvds_clk;
	unsigned int			orig_lvds_clk;
	unsigned int			quick_bl_frq;
	unsigned int			vx5b3d_backlight_frq;	
	int				lvds_clk_switching;
	int				vx5b3d_enable;
	int				negative;
	unsigned int			current_bl;
};

static struct Quicklogic_backlight_value backlight_table[5] = {
	{	/*BOE*/
	.max = 254,
	.mid = 135,
	.low = 2,
	.dim = 1,
	}, {	/*SDC*/
	.max = 254,
	.mid = 135,
	.low = 2,
	.dim = 1,
	}, {	/*CPT*/
	.max = 254,
	.mid = 135,
	.low = 2,
	.dim = 1,
	}, {	/*HIMAX*/
	.max = 254,
	.mid = 135,
	.low = 2,
	.dim = 1,
	}, {	/*NONE*/
	.max = 254,
	.mid = 135,
	.low = 2,
	.dim = 1,
	}
};

#endif

