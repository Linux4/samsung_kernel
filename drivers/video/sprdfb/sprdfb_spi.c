/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#ifdef CONFIG_OF
#include <linux/fb.h>
#endif
#include <mach/sci.h>
#include "spi/spi_simple_drv.h"
#include "sprdfb.h"
#include "sprdfb_panel.h"

SPI_INIT_PARM spi_int_parm[] =
{  //                                                                clk_div  
	{
		TX_POS_EDGE,
		RX_NEG_EDGE,
		TX_RX_MSB,
		RX_TX_MODE,
		NO_SWITCH,
		MASTER_MODE,
		0x0,
		0x0,
		0xc0,   //clk_div:(n+1)*2
		0x0,    //data_width.0-32bits per word; n-nbits per word
		0x0,
		SPI_TX_FIFO_DEPTH - 1,
		0x0,
		SPI_RX_FIFO_DEPTH - 1
	},  //for spi_lcm test
	//{TX_POS_EDGE,RX_NEG_EDGE,TX_RX_LSB,RX_TX_MODE,NO_SWITCH,SLAVE_MODE,0x0,0x0,0xF0,0x0,0x0,SPI_TX_FIFO_DEPTH - 1,0x0,SPI_RX_FIFO_DEPTH - 1},
};


/**---------------------------------------------------------------------------*
 **                 SPI Interface for LCM test case  depend on spi_drv.c      *
 **---------------------------------------------------------------------------*/
//CASE1: 
// --------------------------------------------------------------------------- //
//  Description:   configure the start byte
//	Global resource dependence: 
//  Author:         lichd
//  Note  : LCM test code 
// --------------------------------------------------------------------------- //

static void DISPC_SpiWriteCmd(uint32_t cmd)
{
	SPI_SetDatawidth(8);
	SPI_SetCsLow(0, true);    
	SPI_SetCd( 0 );    

	// Write a data identical with buswidth
	SPI_WriteData( cmd, 1, 0);

	SPI_SetCsLow(0, false);    
}

static void DISPC_SpiWriteData(uint32_t data)
{
	SPI_SetDatawidth(8); 

	SPI_SetCsLow(0, true);       
	SPI_SetCd( 1 );

	// Write a data identical with buswidth
	SPI_WriteData( data, 1, 0);    

	SPI_SetCsLow(0, false);
}

static void SPI_Read( uint32_t* data)
{
	uint32_t lcm_id=0;

	SPI_SetCsLow(0, false);  
	{
		uint32_t i=0;
		for(i=0; i<1000; i++);
	}

	SPI_SetCsLow(0, true);  
	SPI_SetCd( 1 );        
	SPI_SetDatawidth(8);   

	//Read data 16bits  
	lcm_id = SPI_ReadData(2, 0);  //unit of buswidth  

	*data = lcm_id;
}

void SPI_PinCfg( void )
{
	/*enable access the spi reg*/

	//	sci_glb_set((SPRD_GREG_BASE+0x8), 0x2);
	//	sci_glb_set((SPRD_GREG_BASE+0xc0), 0x1);

	/*
	//select spi0_2
	CHIP_REG_SET (PIN_LCD_D6_REG, (PIN_FPD_EN | PIN_FUNC_1 | PIN_O_EN)); //SPI0_2_CD
	CHIP_REG_SET (PIN_LCD_RDN_REG, (PIN_FPD_EN | PIN_FUNC_1 | PIN_I_EN)); //SPI0_2_DI
	CHIP_REG_SET (PIN_LCD_WRN_REG, (PIN_FPD_EN | PIN_FUNC_1 | PIN_O_EN)); //SPI0_2_DO     
	CHIP_REG_SET (PIN_LCD_CD_REG, (PIN_FPD_EN | PIN_FUNC_1 | PIN_O_EN)); //SPI0_2_CLK     
	CHIP_REG_SET (PIN_LCD_CSN0_REG, (PIN_FPD_EN | PIN_FUNC_1 | PIN_O_EN)); //SPI0_2_CS0       
	*/
#if 0
	TB_REG_OR(GEN0, (1 << 13));

	//select spi0_1
	TB_REG_SET (PIN_LCD_D0_REG, (PIN_DS_2 | PIN_FUNC_1 )); //SPI0_1_DI
	TB_REG_SET (PIN_LCD_D1_REG, (PIN_DS_2 | PIN_FUNC_1 )); //SPI0_1_DO
	TB_REG_SET (PIN_LCD_D2_REG, (PIN_DS_2 | PIN_FUNC_1 )); //SPI0_1_CLK     
	TB_REG_SET (PIN_LCD_D3_REG, (PIN_DS_2 | PIN_FUNC_1 )); //SPI0_1_CS0    
	TB_REG_SET (PIN_LCD_D4_REG, (PIN_DS_2 | PIN_FUNC_1 )); //SPI0_1_CS1       
	TB_REG_SET (PIN_LCD_D5_REG, (PIN_DS_2 | PIN_FUNC_1 )); //SPI0_1_CD 

	// 
	TB_REG_AND(PIN_CTL_REG, ~(BIT_29|BIT_30));
	TB_REG_OR(PIN_CTL_REG, (1<<29));
#endif
}

bool sprdfb_spi_init(struct sprdfb_device *dev)
{
#ifdef CONFIG_OF
	SPI_Init(dev->of_dev, SPI_USED_ID, spi_int_parm );
#else
	SPI_Init( SPI_USED_ID, spi_int_parm );
#endif
	return true;
}

bool sprdfb_spi_uninit(struct sprdfb_device *dev)
{
	return true;
}

struct ops_spi sprdfb_spi_ops = {
	.spi_send_cmd = DISPC_SpiWriteCmd,
	.spi_send_data = DISPC_SpiWriteData,
	.spi_read = SPI_Read,
};
