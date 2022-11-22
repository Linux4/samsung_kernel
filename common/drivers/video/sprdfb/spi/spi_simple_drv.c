#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>
#ifdef CONFIG_OF
#include <linux/fb.h>
#endif

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

#include "spi_simple_drv.h"

#ifdef CONFIG_OF
#define FB_SPI_CLOCK ("fb_spi_clock")
#define FB_SPI_CLOCK_PARENT ("fb_spi_clock_parent")
#endif

static u32  used_spi_reg_base;

#ifdef CONFIG_OF
static void SPI_Enable(struct device *device,bool is_en)
#else
static void SPI_Enable( uint32_t spi_id, bool is_en)
#endif
{
	struct clk *spi_clk;

#ifndef CONFIG_OF
	switch (spi_id) {
	case 0:
		spi_clk = clk_get(NULL, "clk_spi0");
		break;
	case 1:
		spi_clk = clk_get(NULL, "clk_spi1");
		break;
	case 2:
		spi_clk = clk_get(NULL, "clk_spi2");
		break;
	default:
		BUG_ON(1);
	}
#else
	spi_clk = of_clk_get_by_name(device->of_node, FB_SPI_CLOCK);
	if (IS_ERR(spi_clk)) {
		printk(KERN_WARNING "sprdfb: get spi clock fail!\n");
		return ;
	} else {
		pr_debug(KERN_INFO "sprdfb: get spi clock ok!\n");
	}
#endif
	if(is_en)
	{
#ifdef CONFIG_OF
		clk_prepare_enable(spi_clk);
#else
		clk_enable(spi_clk);
#endif
	}
	else
	{
#ifdef CONFIG_OF
		clk_disable_unprepare(spi_clk);
#else
		clk_disable(spi_clk);
#endif
	}
}

static void SPI_Reset( uint32_t spi_id)
{
	u32 rst_reg, rst_bit;

	switch (spi_id) {
	case 0:
		rst_reg = REG_AP_APB_APB_RST;
		rst_bit = BIT_SPI0_SOFT_RST;
		break;
	case 1:
		rst_reg = REG_AP_APB_APB_RST;
		rst_bit = BIT_SPI1_SOFT_RST;
		break;
	case 2:
		rst_reg = REG_AP_APB_APB_RST;
		rst_bit = BIT_SPI2_SOFT_RST;
		break;
	default:
		BUG_ON(1);
	}

	sci_glb_set(rst_reg, rst_bit);
	msleep(50);
	sci_glb_clr(rst_reg, rst_bit);
}

/*just set the spi src clk = 26Mhz*/
#ifdef CONFIG_OF
static void SPI_ClkSetting(struct device *device)
#else
static void SPI_ClkSetting(uint32_t spi_id)
#endif
{
	struct clk *spi_clk, *spi_src_clk;
#ifndef CONFIG_OF
	switch (spi_id) {
	case 0:
		spi_clk = clk_get(NULL, "clk_spi0");
		break;
	case 1:
		spi_clk = clk_get(NULL, "clk_spi1");
		break;
	case 2:
		spi_clk = clk_get(NULL, "clk_spi2");
		break;
	default:
		BUG_ON(1);
	}
	spi_src_clk = clk_get(NULL, "ext_26m");
#else
	spi_clk = of_clk_get_by_name(device->of_node, FB_SPI_CLOCK);
	if (IS_ERR(spi_clk)) {
		printk(KERN_WARNING "sprdfb: get spi clock fail!\n");
		return ;
	} else {
		pr_debug(KERN_INFO "sprdfb: get spi clock ok!\n");
	}
	spi_src_clk = of_clk_get_by_name(device->of_node, FB_SPI_CLOCK_PARENT);
	if (IS_ERR(spi_src_clk)) {
		printk(KERN_WARNING "sprdfb: get spi clock parent fail!\n");
		return ;
	} else {
		pr_debug(KERN_INFO "sprdfb: get spi clock parent ok!\n");
	}
#endif
	clk_set_parent(spi_clk, spi_src_clk);
	clk_set_rate(spi_clk, SPI_DEF_SRC_CLK);
}

void SPI_SetCsLow( uint32_t spi_sel_csx , bool is_low)
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T*)(used_spi_reg_base);

	if(is_low)     {
		spi_ctr_ptr->ctl0 &= ~((1<<spi_sel_csx)<<SPI_SEL_CS_SHIFT);
	}
	else
	{
		spi_ctr_ptr->ctl0 |= ((1<<spi_sel_csx)<<SPI_SEL_CS_SHIFT);
	}
}

void SPI_SetCd( uint32_t cd)
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T*)(used_spi_reg_base);

	if(cd == 0) {
		spi_ctr_ptr->ctl8 &= ~(SPI_CD_MASK);
	} else {
		spi_ctr_ptr->ctl8 |= (SPI_CD_MASK);
	}
}


static void SPI_SetSpiMode(uint32_t spi_mode)
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);
	uint32_t temp = spi_ctr_ptr->ctl7;

	temp &= ~SPI_MODE_MASK;
	temp |= (spi_mode<<SPI_MODE_SHIFT);

	spi_ctr_ptr->ctl7 = temp;
}

void  SPI_SetDatawidth(uint32_t datawidth)
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);
	uint32_t temp = spi_ctr_ptr->ctl0;

	if( 32 == datawidth )
	{
		spi_ctr_ptr->ctl0 &= ~0x7C;
		return;
	}

	temp &= ~0x0000007C;
	temp |= (datawidth<<2);

	spi_ctr_ptr->ctl0 = temp;
}

static void SPI_SetTxLen(uint32_t data_len, uint32_t dummy_bitlen)
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);
	uint32_t ctl8 = spi_ctr_ptr->ctl8;
	uint32_t ctl9 = spi_ctr_ptr->ctl9;

	data_len &= TX_MAX_LEN_MASK;

	ctl8 &= ~((TX_DUMY_LEN_MASK<<4) | TX_DATA_LEN_H_MASK);
	ctl9 &= ~( TX_DATA_LEN_L_MASK );

	spi_ctr_ptr->ctl8 = (ctl8 | (dummy_bitlen<<4) | (data_len>>16));
	spi_ctr_ptr->ctl9 = (ctl9 | (data_len&0xFFFF));
}

static void SPI_SetRxLen(uint32_t data_len, uint32_t dummy_bitlen)
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);
	uint32_t ctl10 = spi_ctr_ptr->ctl10;
	uint32_t ctl11 = spi_ctr_ptr->ctl11;

	data_len &= RX_MAX_LEN_MASK;

	ctl10 &= ~((RX_DUMY_LEN_MASK<<4) | RX_DATA_LEN_H_MASK);
	ctl11 &= ~( RX_DATA_LEN_L_MASK );

	spi_ctr_ptr->ctl10 = (ctl10 | (dummy_bitlen<<4) | (data_len>>16));
	spi_ctr_ptr->ctl11 = (ctl11 | (data_len&0xFFFF));
}

static void SPI_TxReq( void )
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);

	spi_ctr_ptr->ctl12 |= SW_TX_REQ_MASK;
}

static void SPI_RxReq( void )
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);

	spi_ctr_ptr->ctl12 |= SW_RX_REQ_MASK;
}

#ifdef CONFIG_OF
void SPI_Init(struct device *device, u32 spi_id, SPI_INIT_PARM *spi_parm)
#else
void SPI_Init(u32 spi_id, SPI_INIT_PARM *spi_parm)
#endif
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr;
	u32 temp;
	u32 ctl0, ctl1, ctl3;

#ifdef CONFIG_OF
	SPI_ClkSetting(device);
	SPI_Enable(device, true);
#else
	SPI_ClkSetting(spi_id);
	SPI_Enable(spi_id, true);
#endif

	SPI_Reset(spi_id);

	switch (spi_id) {
	case 0:
		used_spi_reg_base = SPRD_SPI0_BASE;
		break;
	case 1:
		used_spi_reg_base = SPRD_SPI1_BASE;
		break;
	case 2:
		used_spi_reg_base = SPRD_SPI2_BASE;
		break;
	default:
		BUG_ON(1);
	}

	spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);

	/*fixme, the spi_clk is 1Mhz*/
	spi_ctr_ptr->clkd = (SPI_DEF_SRC_CLK / (SPI_DEF_CLK << 1)) - 1;

	temp  = 0;
	temp |= (spi_parm->tx_edge << 1)    |
		(spi_parm->rx_edge << 0)    |
		(0x1 << 13) |
		(spi_parm->msb_lsb_sel<< 7) ;
	spi_ctr_ptr->ctl0 = temp;

	spi_ctr_ptr->ctl1 |= BIT(12) | BIT(13);

	/*rx fifo full watermark is 16*/
	spi_ctr_ptr->ctl3 = 0x10;

	/*set SPIMODE_3WIRE_9BIT_SDIO mode*/
	spi_ctr_ptr->ctl7 &= ~(0x7 << 3);

	spi_ctr_ptr->ctl7 |= SPIMODE_3WIRE_9BIT_SDIO << 3;
}

static void SPI_WaitTxFinish()
{
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);

	while( !((spi_ctr_ptr->iraw)&BIT(8)) ) // IS tx finish
	{
	}
	spi_ctr_ptr->iclr |= BIT(8);

	// Wait for spi bus idle
	while((spi_ctr_ptr->sts2)&BIT(8))
	{
	}
	// Wait for tx real empty
	while( !((spi_ctr_ptr->sts2)&BIT(7)) )
	{
	}
}

void SPI_WriteData(uint32_t data, uint32_t data_len, uint32_t dummy_bitlen)
{
	//     uint32_t command;
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);

	// The unit of data_len is identical with buswidth
	SPI_SetTxLen(data_len, dummy_bitlen);
	SPI_TxReq( );

	spi_ctr_ptr->data = data;

	SPI_WaitTxFinish();
}

uint32_t SPI_ReadData( uint32_t data_len, uint32_t dummy_bitlen )
{
	uint32_t read_data=0, rxt_cnt=0;
	volatile SPI_CTL_REG_T *spi_ctr_ptr = (volatile SPI_CTL_REG_T *)(used_spi_reg_base);

	// The unit of data_len is identical with buswidth
	SPI_SetRxLen(data_len, dummy_bitlen);
	SPI_RxReq( );

	//Wait for spi receive finish
	while( !((spi_ctr_ptr->iraw)&BIT(9)) )
	{
		//wait rxt fifo full
		if((spi_ctr_ptr->iraw)&BIT(6))
		{
			rxt_cnt = (spi_ctr_ptr->ctl3)&0x1F;
			printk("---FIFOFULL:rxt_cnt=0x%x", rxt_cnt);
			while(rxt_cnt--)
			{
				read_data = spi_ctr_ptr->data;
				printk("---FIFOFULL: SPI_ReadData =0x%x", read_data);
			}
		}
	}

	while((spi_ctr_ptr->sts2)&BIT(8))
	{
	}

	while(data_len--)
	{
		read_data = spi_ctr_ptr->data;
		printk("---Finish: SPI_ReadData =0x%x", read_data);
	}

	return (read_data);
}
