#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <mach/hardware.h>
#include <linux/sprd_2351.h>
#include <asm/io.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#ifdef CONFIG_OF
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>

static struct sprd_2351_data rfspi;

static int s_gpio_ctrl_num = 0;
static bool s_is_gpio_register = false;
static bool s_is_vddwpa_register = false;
#define GPIO_RF2351_POWER_CTRL s_gpio_ctrl_num

void sprd_sr2351_gpio_ctrl_power_register(int gpio_num)
{
    s_gpio_ctrl_num  = gpio_num;
    s_is_gpio_register = true;
    return;
}

static int sprd_rfspi_wait_write_idle(void)
{
	u32 time_out;
	u32 reg_data;

	for (time_out = 0, reg_data = 0x10; \
		reg_data & BIT_4; time_out++) {/*wait fifo empty*/
		if (time_out > 1000) {
			RF2351_PRINT("mspi time out!\r\n");
			return -1;
		}
		reg_data = sci_glb_read(RFSPI_CFG0,-1UL);
		//RF2351_PRINT("reg_data is: %x\n", reg_data);
	}

	return 0;
}

static unsigned int sprd_rfspi_write(u16 Addr, u16 Data)
{
	u32 reg_data = 0;
	if (sprd_rfspi_wait_write_idle() == -1) {
		RF2351_PRINT("Error: mspi is busy\n");
		return -1;
	}
	reg_data = (Addr << 16) | Data;
	sci_glb_write(RFSPI_MCU_WCMD, reg_data, -1UL);

	sprd_rfspi_wait_write_idle();
	return 0;
}

static int sprd_rfspi_wait_read_idle(void)
{
	u32 time_out;
	u32 reg_data;

	for (time_out = 0, reg_data = 0; \
		!(reg_data & BIT_7); time_out++) {/*wait fifo empty*/
		if (time_out > 1000) {
			RF2351_PRINT("mspi time out!\r\n");
			return -1;
		}
		reg_data = sci_glb_read(RFSPI_CFG0,-1UL);
		//RF2351_PRINT("reg_data is: %x\n", reg_data);
	}
	
	return 0;
}

static unsigned int sprd_rfspi_read(u16 Addr, u32 *Read_data)
{
	u32 reg_data = 0;
	if (sprd_rfspi_wait_read_idle() == -1) {
		RF2351_PRINT("Error: mspi is busy\n");
		return -1;
	}
	
	reg_data = (Addr << 16) | BIT_31;
	sci_glb_write(RFSPI_MCU_RCMD, reg_data, -1UL);

	sprd_rfspi_wait_read_idle();
	*Read_data = sci_glb_read(RFSPI_MCU_RDATA,-1UL);
	return 0;
}

static void sprd_rfspi_clk_enable(void)
{
	if(rfspi.clk)
		#ifdef CONFIG_OF
		clk_prepare_enable(rfspi.clk);
		#else
		clk_enable(rfspi.clk);
		#endif
	else 
		RF2351_PRINT("rfspi.clk is NULL\n");
}

static void sprd_rfspi_clk_disable(void)
{
	if(rfspi.clk)
		#ifdef CONFIG_OF
		clk_disable_unprepare(rfspi.clk);
		#else
		clk_disable(rfspi.clk);
		#endif
	else
		RF2351_PRINT("rfspi.clk is NULL\n");
}

static void sprd_mspi_enable(void)
{
	sci_glb_set(APB_EB0, RFSPI_ENABLE_CTL);
}

static void sprd_mspi_disable(void)
{
	/*Because of BT can't work when connet BT Earphones if disable the mspi*/
	//sci_glb_clr(APB_EB0, RFSPI_ENABLE_CTL);
}

static unsigned int sprd_rfspi_enable(void)
{
	rfspi.count++;
	if(rfspi.count == 1)
	{
		#ifndef CONFIG_OF
		rfspi.clk = clk_get(NULL, "clk_cpll");
		if(rfspi.clk == NULL)
{
			RF2351_PRINT("rfspi get clk_cpll failed\n");
			return -1;
		}
		#endif
		sprd_rfspi_clk_enable();
		sprd_mspi_enable();
	}

	return 0;
}

static unsigned int sprd_rfspi_disable(void)
{
	if(rfspi.count == 1)
	{
		sprd_mspi_disable();
		sprd_rfspi_clk_disable();
		#ifndef CONFIG_OF
		clk_put(rfspi.clk);
		#endif
	}
	rfspi.count--;
	if(rfspi.count < 0)
		rfspi.count = 0;

	return 0;
}

static DEFINE_MUTEX(rf2351_lock);
static int rf2351_power_count=0;



void rf2351_gpio_ctrl_power_enable(int flag)
{
    if(!s_is_gpio_register)
    return;

    if(!flag && (0 == rf2351_power_count))//avoid calling the func  first time with flag =0
    return;
    
    RF2351_PRINT("rf2351_power_count =%d\n", rf2351_power_count);
    mutex_lock(&rf2351_lock);
    if(0 == rf2351_power_count)
    {
        if (gpio_request(GPIO_RF2351_POWER_CTRL, "rf2351_power_pin"))
        {
            RF2351_PRINT("request gpio %d failed\n",GPIO_RF2351_POWER_CTRL);
        }
        gpio_direction_output(GPIO_RF2351_POWER_CTRL, 1);	 
    }

    if(flag)
    {
        if(0 == rf2351_power_count)
        {
            gpio_set_value(GPIO_RF2351_POWER_CTRL, 1);
           RF2351_PRINT("rf2351_gpio_ctrl_power_enable  on\n"); 
        }
        rf2351_power_count++;
    }
    else
    {
         rf2351_power_count--;
        if(0 == rf2351_power_count)
        {
            gpio_set_value(GPIO_RF2351_POWER_CTRL, 0);
            gpio_free(GPIO_RF2351_POWER_CTRL);
            RF2351_PRINT("rf2351_gpio_ctrl_power_enable  off \n"); 
        }
    }
     mutex_unlock(&rf2351_lock);
}


EXPORT_SYMBOL(rf2351_gpio_ctrl_power_enable);

void sprd_sr2351_vddwpa_ctrl_power_register(void)
{
    s_is_vddwpa_register = true;
    return;
}

int rf2351_vddwpa_ctrl_power_enable(int flag)
{
    static struct regulator *wpa_rf2351 = NULL;
    static int f_enabled = 0;

    if (!s_is_vddwpa_register)
    return 0;

    printk("[wpa_rf2351] LDO control : %s\n", flag ? "ON" : "OFF");

    if (flag && (!f_enabled)) {
        wpa_rf2351 = regulator_get(NULL, "vddwpa");
        if (IS_ERR(wpa_rf2351)) {
            printk("rf2351 could not find the vddwpa regulator\n");
            wpa_rf2351 = NULL;
            return EIO;
        } else {
            regulator_set_voltage(wpa_rf2351, 3400000, 3400000);
            regulator_enable(wpa_rf2351);
        }
        f_enabled = 1;
    }
    if (f_enabled && (!flag)) {
        if (wpa_rf2351) {
            regulator_disable(wpa_rf2351);
            regulator_put(wpa_rf2351);
            wpa_rf2351 = NULL;
        }
        f_enabled = 0;
    }
    return 0;
}

EXPORT_SYMBOL(rf2351_vddwpa_ctrl_power_enable);

static struct sprd_2351_interface sprd_rf2351_ops = {
	.name = "rf2351",
	.mspi_enable = sprd_rfspi_enable,
	.mspi_disable = sprd_rfspi_disable,
	.read_reg = sprd_rfspi_read,
	.write_reg = sprd_rfspi_write,
};

int sprd_get_rf2351_ops(struct sprd_2351_interface **rf2351_ops)
{
	*rf2351_ops = &sprd_rf2351_ops;

	return 0;
}
EXPORT_SYMBOL(sprd_get_rf2351_ops);

int sprd_put_rf2351_ops(struct sprd_2351_interface **rf2351_ops)
{
	*rf2351_ops = NULL;

	return 0;
}
EXPORT_SYMBOL(sprd_put_rf2351_ops);

#ifdef CONFIG_OF
static struct rf2351_addr
{
	u32 rfspi_base;
	u32 apb_base;
}rf2351_base;

u32 rf2351_get_rfspi_base(void)
{
	return rf2351_base.rfspi_base;
}

u32 rf2351_get_apb_base(void)
{
	return rf2351_base.apb_base;
}

static int __init sprd_rfspi_init(void)
{
	int ret;

	struct device_node *np;
	struct resource res;

	np = of_find_node_by_name(NULL, "sprd_rf2351");
	if (!np) {
		RF2351_PRINT("Can't get the sprd_rfspi node!\n");
		return -ENODEV;
	}
	RF2351_PRINT(" find the sprd_rfspi node!\n");

	ret = of_address_to_resource(np, 0, &res);
	if (ret < 0) {
		RF2351_PRINT("Can't get the rfspi reg base!\n");
		return -EIO;
	}
	rf2351_base.rfspi_base = res.start;
	RF2351_PRINT("rfspi reg base is 0x%x\n", rf2351_base.rfspi_base);

	ret = of_address_to_resource(np, 1, &res);
	if (ret < 0) {
		RF2351_PRINT("Can't get the rfspi reg base!\n");
		return -EIO;
	}
	rf2351_base.apb_base = res.start;
	RF2351_PRINT("rfspi reg base is 0x%x\n", rf2351_base.apb_base);

	rfspi.clk = of_clk_get_by_name(np,"clk_cpll");
	if (IS_ERR(rfspi.clk)) {
		RF2351_PRINT("get clk_cpll fail!\n");
		return -1;
	} else {
		RF2351_PRINT("get clk_cpll ok!\n");
	}

	return ret;
}

module_init(sprd_rfspi_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sprd 2351 rfspi driver");
#endif


