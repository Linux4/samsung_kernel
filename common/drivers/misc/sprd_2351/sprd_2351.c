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

static struct sprd_2351_data rfspi;

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
		reg_data = readl((const volatile void *)RFSPI_CFG0);
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
	writel(reg_data,(volatile void *)RFSPI_MCU_WCMD);

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
		reg_data = readl((const volatile void *)RFSPI_CFG0);
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
	writel(reg_data,(volatile void *)RFSPI_MCU_RCMD);

	sprd_rfspi_wait_read_idle();
	*Read_data = readl((const volatile void *)RFSPI_MCU_RDATA);
	return 0;
}

static void sprd_rfspi_clk_enable(void)
{
	if(rfspi.clk)
		clk_enable(rfspi.clk);
	else 
		RF2351_PRINT("rfspi.clk is NULL\n");
}

static void sprd_rfspi_clk_disable(void)
{
	if(rfspi.clk)
		clk_disable(rfspi.clk);
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
		rfspi.clk = clk_get(NULL, "clk_cpll");
		if(rfspi.clk == NULL){
			RF2351_PRINT("rfspi get clk_cpll failed\n");
			return -1;
		}
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
		clk_put(rfspi.clk);
	}
	rfspi.count--;
	if(rfspi.count < 0)
		rfspi.count = 0;

	return 0;
}

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


