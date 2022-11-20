#ifndef _SPRD_2351_H
#define _SPRD_2351_H

#define BIT_NONE            0x00
#define BIT_0               0x01
#define BIT_1               0x02
#define BIT_2               0x04
#define BIT_3               0x08
#define BIT_4               0x10
#define BIT_5               0x20
#define BIT_6               0x40
#define BIT_7               0x80
#define BIT_8               0x0100
#define BIT_9               0x0200
#define BIT_10              0x0400
#define BIT_11              0x0800
#define BIT_12              0x1000
#define BIT_13              0x2000
#define BIT_14              0x4000
#define BIT_15              0x8000
#define BIT_16              0x010000
#define BIT_17              0x020000
#define BIT_18              0x040000
#define BIT_19              0x080000
#define BIT_20              0x100000
#define BIT_21              0x200000
#define BIT_22              0x400000
#define BIT_23              0x800000
#define BIT_24              0x01000000
#define BIT_25              0x02000000
#define BIT_26              0x04000000
#define BIT_27              0x08000000
#define BIT_28              0x10000000
#define BIT_29              0x20000000
#define BIT_30              0x40000000
#define BIT_31              0x80000000


#define RF2351_PRINT(format, arg...)  \
	do { \
		printk("RF2351:%s:"format"\n", \
		__func__, ## arg); \
	} while (0)

#ifndef CONFIG_OF
#define APB_EB0_BASE_VA			SPRD_AONAPB_BASE
#define RFSPI_BASE_VA			SPRD_RFSPI_BASE
#else
extern u32 rf2351_get_rfspi_base(void);
extern u32 rf2351_get_apb_base(void);

#define APB_EB0_BASE_VA			rf2351_get_apb_base()
#define RFSPI_BASE_VA			rf2351_get_rfspi_base()
#endif

#define APB_EB0					(APB_EB0_BASE_VA+0x0000)
#define RFSPI_CFG0				(RFSPI_BASE_VA + 0x0000)
#define RFSPI_MCU_WCMD			(RFSPI_BASE_VA + 0x000C)
#define RFSPI_MCU_RCMD			(RFSPI_BASE_VA + 0x0010)
#define RFSPI_MCU_RDATA			(RFSPI_BASE_VA + 0x0014)

#define RFSPI_ENABLE_CTL		(BIT_23)

struct sprd_2351_interface{
	char *name;
	unsigned int (*mspi_enable) (void);
	unsigned int (*mspi_disable) (void);
	unsigned int (*read_reg) (u16 Addr, u32 *Read_data);
	unsigned int (*write_reg) (u16 Addr, u16 Data);
};

struct sprd_2351_data{
	struct clk *clk;
	int count;
};

extern void rf2351_gpio_ctrl_power_enable(int flag);
int rf2351_vddwpa_ctrl_power_enable(int flag);
extern int sprd_get_rf2351_ops(struct sprd_2351_interface **rf2351_ops);
extern int sprd_put_rf2351_ops(struct sprd_2351_interface **rf2351_ops);
void sprd_sr2351_gpio_ctrl_power_register(int gpio_num);
void sprd_sr2351_vddwpa_ctrl_power_register(void);
#endif
