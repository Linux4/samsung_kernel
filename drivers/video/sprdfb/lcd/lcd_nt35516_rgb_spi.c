/* drivers/video/sc8825/lcd_nt35516_spi.c
 *
 * Support for nt35516 spi LCD device
 *
 * Copyright (C) 2010 Spreadtrum
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
#include <linux/delay.h>
#include "../sprdfb_panel.h"

#define NT35516_SpiWriteCmd(cmd) \ 
{ \
	spi_send_cmd((cmd & 0xFF));\
}

#define  NT35516_SpiWriteData(data)\
{ \
	spi_send_data((data & 0xFF));\
}

static int32_t nt35516_rgb_spi_init(struct panel_spec *self)
{
	uint32_t data = 0;
	spi_send_cmd_t spi_send_cmd = self->info.rgb->bus_info.spi->ops->spi_send_cmd; 
	spi_send_data_t spi_send_data = self->info.rgb->bus_info.spi->ops->spi_send_data; 
	spi_read_t spi_read = self->info.rgb->bus_info.spi->ops->spi_read; 

	// NT35516 + AUO 4.29'
	// VCC=IOVCC=3.3V  RGB_24Bit
//#ifndef CONFIG_ARCH_SCX15

	//NT35516_SpiWriteCmd(0xDB00);
	//spi_read(&data);

	//TEST Commands

	NT35516_SpiWriteCmd(0xFF); 
	NT35516_SpiWriteData(0xAA);//AA
  NT35516_SpiWriteData(0x55);//55
	NT35516_SpiWriteData(0x25);//08
	 NT35516_SpiWriteData(0x01);//01

	//NT35516_SpiWriteCmd(0xFA0F); NT35516_SpiWriteData(0x20);

	//NT35516_SpiWriteCmd(0xF300); NT35516_SpiWriteData(0x02);
	//NT35516_SpiWriteCmd(0xF303); NT35516_SpiWriteData(0x15);

	//ENABLE PAGE 0    
	NT35516_SpiWriteCmd(0xF0); 
	NT35516_SpiWriteData(0x55); //Manufacture Command Set Control   
	NT35516_SpiWriteData(0xAA);
	NT35516_SpiWriteData(0x52);
	NT35516_SpiWriteData(0x08);
	 NT35516_SpiWriteData(0x00);

	NT35516_SpiWriteCmd(0xB8); 
	NT35516_SpiWriteData(0x01); 
	NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteData(0x02);

	//NT35516_SpiWriteCmd(0xBC00); NT35516_SpiWriteData(0x05); //Zig-Zag Inversion  
	//NT35516_SpiWriteCmd(0xBC01); NT35516_SpiWriteData(0x05);
	//NT35516_SpiWriteCmd(0xBC02); NT35516_SpiWriteData(0x05);

	//NT35516_SpiWriteCmd(0x4C00); NT35516_SpiWriteData(0x11); //DB4=1,Enable Vivid Color,DB4=0 Disable Vivid Color

	// ENABLE PAGE 1   
	NT35516_SpiWriteCmd(0xF0); 
	NT35516_SpiWriteData(0x55); //Manufacture Command Set Control      
	NT35516_SpiWriteData(0xAA);
	NT35516_SpiWriteData(0x52);
	NT35516_SpiWriteData(0x08);
	NT35516_SpiWriteData(0x01);//Page1

	NT35516_SpiWriteCmd(0xB0); 
	NT35516_SpiWriteData(0x05); // Setting AVDD Voltage 6V
	NT35516_SpiWriteData(0x05);
	NT35516_SpiWriteData(0x05);

	NT35516_SpiWriteCmd(0xB6); 
	NT35516_SpiWriteData(0x44); // Setting AVEE boosting time 2.5*vpnl 
	 NT35516_SpiWriteData(0x44);
	 NT35516_SpiWriteData(0x44);

	NT35516_SpiWriteCmd(0xB1);
	 NT35516_SpiWriteData(0x05); // Setting AVEE Voltage -6V
	NT35516_SpiWriteData(0x05);
	NT35516_SpiWriteData(0x05);

	//Setting AVEE boosting time -2.5xVPNL
	NT35516_SpiWriteCmd(0xB7); 
	NT35516_SpiWriteData(0x34); 
	NT35516_SpiWriteData(0x34);
	NT35516_SpiWriteData(0x34);

	//Setting VGLX boosting time  AVEE-AVDD
	NT35516_SpiWriteCmd(0xBA); 
	NT35516_SpiWriteData(0x14); //0x24 --> 0x14
	 NT35516_SpiWriteData(0x14);
	 NT35516_SpiWriteData(0x14);

	//Gamma Voltage
	NT35516_SpiWriteCmd(0xBC); 
	NT35516_SpiWriteData(0x00); 
NT35516_SpiWriteData(0xA0);//VGMP 0x88=4.7V  0x78=4.5V   0xA0=5.0V  
NT35516_SpiWriteData(0x00);//VGSP 

	//Gamma Voltage
	NT35516_SpiWriteCmd(0xBD); 
	NT35516_SpiWriteData(0x00); 
	NT35516_SpiWriteData(0xA0);//VGMN 0x88=-4.7V 0x78=-4.5V   0xA0=-5.0V
	NT35516_SpiWriteData(0x00);//VGSN  

	NT35516_SpiWriteCmd(0xBE); 
	NT35516_SpiWriteData(0x57); // Setting VCOM Offset Voltage  0x4E¸ÄÎª0x57  20111019 LIYAN

	//GAMMA RED Positive       
	NT35516_SpiWriteCmd(0xD1); 
	NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x32);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x41);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x54);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x67);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x7A);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x98);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xB0);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xDB);
	 
	NT35516_SpiWriteCmd(0xD2);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x3F);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x70);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xB4);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xEC);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xED);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x1E);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x51);
	 
	NT35516_SpiWriteCmd(0xD3); 
	NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x6C);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x8D);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xA5);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xC9);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xEA);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x19);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x45);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x7A);
	 
	NT35516_SpiWriteCmd(0xD4); 
	NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xB0);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xF4);

	//Positive Gamma for GREEN
	NT35516_SpiWriteCmd(0xD5); 
	NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x37);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x41);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x54);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x67);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x7A);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x98);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xB0);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xDB);
	 
	NT35516_SpiWriteCmd(0xD6); 
	NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x3F);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x70);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xB4);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xEC);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xED);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x1E);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x51);
	 
	NT35516_SpiWriteCmd(0xD7); 
	NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x6C);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x8D);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xA5);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xC9);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xEA);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x19);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x45);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x7A);
	 
	NT35516_SpiWriteCmd(0xD8); 
	NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteData(0xA0);
	NT35516_SpiWriteData(0x03);
	NT35516_SpiWriteData(0xF4);

	//Positive Gamma for BLUE
	NT35516_SpiWriteCmd(0xD9);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x32);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x41);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x54);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x67);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x7A);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x98);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xB0);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xDB);
	 
	NT35516_SpiWriteCmd(0xDD); 
	NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x3F);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x70);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xB4);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xEC);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xED);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x1E);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x51);
	 
	NT35516_SpiWriteCmd(0xDE); 
	NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x6C);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x8D);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xA5);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xC9);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xEA);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x19);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x45);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x7A);
	 
	NT35516_SpiWriteCmd(0xDF); 
	NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xA0);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xF4);

	//Negative Gamma for RED
	NT35516_SpiWriteCmd(0xE0); 
	NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x32);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x41);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x54);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x67);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x7A);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x98);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xB0);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xDB);
	 
	NT35516_SpiWriteCmd(0xE1); 
	NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x3F);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x70);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xB4);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xEC);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xED);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x1E);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x51);
	 
	NT35516_SpiWriteCmd(0xE2); 
	NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x6C);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x8D);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xA5);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xC9);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xEA);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x19);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x45);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x7A);
	 
	NT35516_SpiWriteCmd(0xE3); 
	NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xA0);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xF4);

	//Negative Gamma for GERREN
	NT35516_SpiWriteCmd(0xE4); 
	NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x32);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x41);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x54);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x67);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x7A);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x98);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xB0);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xDB);
	 
	NT35516_SpiWriteCmd(0xE5); 
	NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x3F);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0x70);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xB4);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xEC);
	 NT35516_SpiWriteData(0x01);
	 NT35516_SpiWriteData(0xED);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x1E);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x51);
	 
	NT35516_SpiWriteCmd(0xE6); 
	NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x6C);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x8D);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xA5);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xC9);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xEA);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x19);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x45);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x7A);
	 
	NT35516_SpiWriteCmd(0xE7); 
	NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xA0);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xF4);

	//Negative Gamma for BLUE
	NT35516_SpiWriteCmd(0xE8); 
	NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x32);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x41);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x54);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x67);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x7A);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0x98);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xB0);
	 NT35516_SpiWriteData(0x00);
	 NT35516_SpiWriteData(0xDB);
	 
	NT35516_SpiWriteCmd(0xE9); 
	NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteData(0x3F);
	NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteData(0x70);
	NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteData(0xB4);
	NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteData(0xEC);
	NT35516_SpiWriteData(0x01);
	NT35516_SpiWriteData(0xED);
	NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteData(0x1E);
	NT35516_SpiWriteData(0x02);
	NT35516_SpiWriteData(0x51);
	
	NT35516_SpiWriteCmd(0xEA); 
	NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x6C);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0x8D);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xA5);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xC9);
	 NT35516_SpiWriteData(0x02);
	 NT35516_SpiWriteData(0xEA);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x19);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x45);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0x7A);
	 
	NT35516_SpiWriteCmd(0xEB); 
	NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xA0);
	 NT35516_SpiWriteData(0x03);
	 NT35516_SpiWriteData(0xF4);
//#endif

	NT35516_SpiWriteCmd(0x3A); NT35516_SpiWriteData(0x77);

	NT35516_SpiWriteCmd(0x35); NT35516_SpiWriteData(0x00);

	NT35516_SpiWriteCmd(0x11); // Sleep out
	mdelay(140);

	NT35516_SpiWriteCmd(0x29); // Display On
	mdelay(10);
}

static uint32_t nt35516_rgb_spi_readid(struct panel_spec *self)
{
	/*Jessica TODO: need read id*/
	return 0x16;
}

#if 0
void NT35516_RGB_SPI_set_display_window(
    uint16 left,     // start Horizon address
    uint16 right,     // end Horizon address
    uint16 top,         // start Vertical address
    uint16 bottom    // end Vertical address
    )
{
    NT35516_SpiWriteCmd(0x2A00); NT35516_SpiWriteData((left>>8));// set left address
    NT35516_SpiWriteCmd(0x2A01); NT35516_SpiWriteData((left&0xff));
    NT35516_SpiWriteCmd(0x2A02); NT35516_SpiWriteData((right>>8));// set right address
    NT35516_SpiWriteCmd(0x2A03); NT35516_SpiWriteData((right&0xff));

    NT35516_SpiWriteCmd(0x2B00); NT35516_SpiWriteData((top>>8));// set left address
    NT35516_SpiWriteCmd(0x2B01); NT35516_SpiWriteData((top&0xff));
    NT35516_SpiWriteCmd(0x2B02); NT35516_SpiWriteData((bottom>>8));// set bottom address
    NT35516_SpiWriteCmd(0x2B03); NT35516_SpiWriteData((bottom&0xff));
}

LCD_ERR_E NT35516_RGB_SPI_EnterSleep(BOOLEAN is_sleep)
{
    if(is_sleep==1)
    {
        NT35516_SpiWriteCmd(0x28);
        LCD_Delay(200);
        NT35516_SpiWriteCmd(0x10);
        LCD_Delay(200);
        //Lcd_EnvidOnOff(0);//RGB TIMENG OFF
        //LCD_Delay(200);        

    }
    else
    {
        //Lcd_EnvidOnOff(1);//RGB TIMENG ON 
        //LCD_Delay(200);
        //LCDinit_TFT();
        //LCD_Delay(200);

    }

    return 0;
}

LCD_ERR_E NT35516_RGB_SPI_SetDisplayWindow(
    uint16 left,         //left of the window
    uint16 top,            //top of the window
    uint16 right,        //right of the window
    uint16 bottom        //bottom of the window
    )
{         
    //NT35516_RGB_SPI_set_display_window(left, right, top, bottom);

    NT35516_SpiWriteCmd(0x2C);
    return TRUE;
}
#endif


static struct panel_operations lcd_nt35516_rgb_spi_operations = {
	.panel_init = nt35516_rgb_spi_init,
	.panel_readid = nt35516_rgb_spi_readid,
};

static struct timing_rgb lcd_nt35516_rgb_timing = {
	.hfp = 30,  /* unit: pixel */
	.hbp = 16,
	.hsync = 1,
	.vfp = 16, /*unit: line*/
	.vbp = 16,
	.vsync = 1,
};

static struct spi_info lcd_nt35516_rgb_spi_info = {
	.ops = NULL,
};

static struct info_rgb lcd_nt35516_rgb_info = {
	.cmd_bus_mode  = SPRDFB_RGB_BUS_TYPE_SPI,
	.video_bus_width = 24, /*18,16*/
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.timing = &lcd_nt35516_rgb_timing,
	.bus_info = {
		.spi = &lcd_nt35516_rgb_spi_info,
	}
};

struct panel_spec lcd_nt35516_rgb_spi_spec = {
	.width = 540,
	.height = 960,
	.type = LCD_MODE_RGB,
	.direction = LCD_DIRECT_NORMAL,
#ifdef CONFIG_FB_SCX15
	.fps = 56,
#endif
	.info = {
		.rgb = &lcd_nt35516_rgb_info
	},
	.ops = &lcd_nt35516_rgb_spi_operations,
};

struct panel_cfg lcd_nt35516_rgb_spi = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x16,
	.lcd_name = "lcd_nt35516_rgb_spi",
	.panel = &lcd_nt35516_rgb_spi_spec,
};
static int __init lcd_nt35516_rgb_spi_init(void)
{
	return sprdfb_panel_register(&lcd_nt35516_rgb_spi);
}

subsys_initcall(lcd_nt35516_rgb_spi_init);
