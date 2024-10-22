/*******************************************************************************
 * Copyright (C) 2012 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated shall
 * not be used except as stated in the Maxim Integrated Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all ownership rights.
 *******************************************************************************/

//#include <windows.h>
//#include <stdio.h>

#define OneWire_Protocol
#include <linux/io.h>
#include "1wire_protocol.h"
#include "sec_auth_ds28e30.h"


// Spinlock controls
spinlock_t spinlock_swi_gpio;
unsigned long spinlock_swi_gpio_flag;

static int ow_gpio;
static void __iomem *control_reg_addr, *data_reg_addr, *control_reg_addr1, *data_reg_addr1,
				*control_reg_addr2, *data_reg_addr2;
static u32 wr_delay_ns, rd_delay_ns, wr_delay_ns1, rd_delay_ns1, wr_delay_ns2, rd_delay_ns2;
static u32 ctrl_bit_pos, ctrl_bit_pos1, ctrl_bit_pos2;
static u32 ctrl_reg_val, ctrl_reg_val1, ctrl_reg_val2;
static bool gpio_dir_out;


// misc utility functions
unsigned char docrc8(unsigned char value);

#define EGPIO_BIT_MASK  (1<<11)

#define OUT_HIGH  writel_relaxed(0x2, data_reg_addr)
#define OUT_LOW  writel_relaxed(0x0, data_reg_addr)
#define READ_GPIO_IN  (unsigned char)(readl_relaxed(data_reg_addr) & 0x01)
#define READ_GPIO_OUT  (unsigned char)(readl_relaxed(data_reg_addr) & 0x02)

static inline void config_out(void)
{
		writel_relaxed((ctrl_reg_val | 1<<ctrl_bit_pos), control_reg_addr);
}

static inline void config_in(void)
{
		writel_relaxed((ctrl_reg_val | 0<<ctrl_bit_pos), control_reg_addr);
}

static void set_active_dev_param(void)
{
	if (Active_Device == 1) {
		control_reg_addr = control_reg_addr1;
		data_reg_addr = data_reg_addr1;
		wr_delay_ns = wr_delay_ns1;
		rd_delay_ns = rd_delay_ns1;
		ctrl_bit_pos = ctrl_bit_pos1;
		ctrl_reg_val = ctrl_reg_val1;
	} else if (Active_Device == 2) {
		control_reg_addr = control_reg_addr2;
		data_reg_addr = data_reg_addr2;
		wr_delay_ns = wr_delay_ns2;
		rd_delay_ns = rd_delay_ns2;
		ctrl_bit_pos = ctrl_bit_pos2;
		ctrl_reg_val = ctrl_reg_val2;
	}
}


//---------------------------------------------------------------------------
//-------- GPIO operation subroutines for 1-Wire interface
//---------------------------------------------------------------------------
/**
*@brief Read GPIO state value.
*
*/
uint8_t GPIO_read(void)
{
	unsigned char val = 0;

	set_active_dev_param();
	spin_lock_irqsave(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
	if (gpio_dir_out)
		val = READ_GPIO_OUT;
	else
		val = READ_GPIO_IN;
	spin_unlock_irqrestore(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
	pr_info("%s: val = %d\n", __func__, val);
	return val;
}

/**
*@brief Writes GPIO level.
*
*/
void GPIO_write(uint8_t level)
{
	set_active_dev_param();
	spin_lock_irqsave(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
	if (level == GPIO_LEVEL_HIGH) {
		OUT_HIGH;       // Output High
	} else if (level == GPIO_LEVEL_LOW) {
		OUT_LOW;        // Output Low
	}
	spin_unlock_irqrestore(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
}

/**
* @brief Change the GPIO direction.
* @param dir Set the direction of the GPIO GPIO_DIRECTION_IN(0) or GPIO_DIRECTION_OUT(1)
*
*/
void GPIO_set_dir(uint8_t dir)
{
	set_active_dev_param();
	spin_lock_irqsave(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
	if (dir == GPIO_DIRECTION_IN) {
		config_in();     // set gpio as input
		gpio_dir_out = false;
	} else {
		config_out();    // set gpio as output
		gpio_dir_out = true;
	}
	spin_unlock_irqrestore(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
}

/**
* @brief Toggles GPIO level to generate a pulse
*
*/
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
void GPIO_toggle(void)
{
	pr_info("%s\n", __func__);

	spin_lock_irqsave(&spinlock_swi_gpio, spinlock_swi_gpio_flag);

	OUT_HIGH;	// Output High
	OUT_LOW;	// Output Low
	OUT_HIGH;	// Output High

	spin_unlock_irqrestore(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
}
#endif

/*****************************************************************************
// delay us subroutine
*****************************************************************************/
void Delay_us(unsigned int T)    //1 micro second
{
	udelay(T);
}

/*****************************************************************************
// delay ms subroutine
*****************************************************************************/
void delayms(unsigned int T)    //1 milisecond
{
	mdelay(T);
}



//---------------------------------------------------------------------------
//-------- Basic 1-Wire functions
//---------------------------------------------------------------------------

//--------------------------------------------------------------------------
// Reset all of the devices on the 1-Wire Net and return the result.
//
// Returns: TRUE(1):  presense pulse(s) detected, device(s) reset
//          FALSE(0): no presense pulses detected
//
///
//
int ow_reset(void)
{
	unsigned char presence = 0;
	int loop_cnt = 0;
	set_active_dev_param();
	spin_lock_irqsave(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
	config_out();                                  // set 1-Wire Data Pin to output
	OUT_LOW;                                     // 0
	udelay(54);
	config_in();                                   // set 1-Wire Data pin to input mode
//	udelay(9);                                   // wait for presence
//	presence = !(READ_GPIO_IN);   // presence= ((One_Wire->IDR)>>8)&(0x01);

	loop_cnt = 4;
	while (loop_cnt--) {
		if (READ_GPIO_IN)
			break;
		ndelay(900);
	}

	loop_cnt = 20;
	while (loop_cnt--) {
		if (!(READ_GPIO_IN)) {
			presence = 1;
			break;
		}
		ndelay(900);
	}

	udelay(50);
	OUT_HIGH;                                    // 1
	config_out();                                  // set 1-Wire Data Pin to output
	spin_unlock_irqrestore(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
	if (presence == 0)
		pr_info("[auth_ds28e30] %s: presence fail\n", __func__);

	return presence;                            // presence signal returned
}

//--------------------------------------------------------------------------
// Send 1 bit of communication to the 1-Wire Net.
// The parameter 'sendbit' least significant bit is used.
//
// 'bitvalue' - 1 bit to send (least significant byte)
//
void write_bit(unsigned char bitval)
{
	set_active_dev_param();
	OUT_LOW;					//output '0'
	OUT_LOW;					//output '0'
	OUT_LOW;					//output '0'
	//udelay(1);				//keeping logic low for 1 us
	//ndelay(600);
	if (bitval != 0)
		OUT_HIGH;				// set 1-wire to logic high if bitval='1'
	udelay(8);					// waiting for 10us
	OUT_HIGH;					// restore 1-wire dat pin to high
	udelay(6);					// waiting for 5us to recover to logic high
}

//--------------------------------------------------------------------------
// Send 1 bit of read communication to the 1-Wire Net and and return the
// result 1 bit read from the 1-Wire Net.
//
// Returns:  1 bits read from 1-Wire Net
//
unsigned char read_bit(void)
{
	unsigned char vamm = 0, i = 0;
	set_active_dev_param();
	config_out();                 //set 1-wire as output
	OUT_LOW;                    //output '0'
	OUT_LOW;                    //output '0'
	OUT_LOW;                    //output '0'
	//udelay(1);                //keeping 1us to generate read bit clock
	//ndelay(600);
	config_in();                  // set 1-wire as input
	//udelay(1);                //keeping 1us to generate read bit clock
	//ndelay(300);

	for (i = 0; i < 7; i++)
		vamm += READ_GPIO_IN;

	if (vamm > 6)
		vamm = 1;
	else
		vamm = 0;

	udelay(5);                  //Keep GPIO at the input state
	OUT_HIGH;                   // prepare to output logic '1'
	config_out();                 //set 1-wire as output
	udelay(6);                  //Keep GPIO at the output state
	return vamm;               // return value of 1-wire dat pin
}


//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Net and verify that the
// 8 bits read from the 1-Wire Net is the same (write operation).
// The parameter 'sendbyte' least significant 8 bits are used.
//
// 'val' - 8 bits to send (least significant byte)
//
// Returns:  TRUE: bytes written and echo was the same
//           FALSE: echo was not the same
//
void write_byte(unsigned char val)
{
	unsigned char i;
	unsigned char temp;

	set_active_dev_param();
	spin_lock_irqsave(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
	config_out();                 //set 1-wire as output
	for (i = 0; i < 8; i++) {     // writes byte, one bit at a time
		temp = val>>i;            // shifts val right
		temp &= 0x01;             // copy that bit to temp
		write_bit(temp);          // write bit in temp into
	}
	spin_unlock_irqrestore(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
}

//--------------------------------------------------------------------------
// Send 8 bits of read communication to the 1-Wire Net and and return the
// result 8 bits read from the 1-Wire Net.
//
// Returns:  8 bits read from 1-Wire Net
//
unsigned char read_byte(void)
{
	unsigned char i;
	unsigned char value = 0;

	set_active_dev_param();
	spin_lock_irqsave(&spinlock_swi_gpio, spinlock_swi_gpio_flag);
	for (i = 0; i < 8; i++) {
		if (read_bit())
			value |= 0x01<<i;      // reads byte in, one byte at a time and then shifts it left
	}
	spin_unlock_irqrestore(&spinlock_swi_gpio, spinlock_swi_gpio_flag);

	return value;
}


//--------------------------------------------------------------------------
// The 'OWReadROM' function does a Read-ROM.  This function
// uses the read-ROM function 33h to read a ROM number and verify CRC8.
//
// Returns:   TRUE (1) : OWReset successful and Serial Number placed
//                       in the global ROM, CRC8 valid
//            FALSE (0): OWReset did not have presence or CRC8 invalid
//
int OWReadROM(void)
{
	uchar buf[16];
	int i;

	if (ow_reset() == 1) {
		write_byte(0x33); // READ ROM command
		for (i = 0; i < 8; i++)
			buf[i] = read_byte();
		// verify CRC8
		crc8 = 0;
		for (i = 0; i < 8; i++)
			docrc8(buf[i]);

		if ((crc8 == 0) && (buf[0] != 0)) {
			memcpy(ROM_NO[Active_Device - 1], &buf[0], 8);
			return TRUE;
		}
	}

	return FALSE;
}


//--------------------------------------------------------------------------
// The 'OWSkipROM' function does a skip-ROM.  This function
// uses the Skip-ROM function CCh.
//
// Returns:   TRUE (1) : OWReset successful and skip rom sent.
//            FALSE (0): OWReset did not have presence
//
int OWSkipROM(void)
{
	if (ow_reset() == 1) {
		write_byte(0xCC);
		return TRUE;
	}

	return FALSE;
}

static unsigned char dscrc_table[] = {
	0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
	157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
	35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
	190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
	70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
	219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
	101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
	248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91,  5, 231, 185,
	140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
	17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
	175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
	50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
	202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
	87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
	233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
	116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53};

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current
// global 'crc8' value.
// Returns current global crc8 value
//
unsigned char docrc8(unsigned char value)
{
	// See Application Note 27

	// TEST BUILD
	crc8 = dscrc_table[crc8 ^ value];
	return crc8;
}


void set_1wire_ow_gpio(struct sec_auth_ds28e30_platform_data *pdata)
{
	void __iomem *auth_base = NULL;
	u32 val;

	if (Active_Device == 1) {
		ow_gpio = pdata->swi_gpio[0];
		gpio_dir_out = false;
		pr_info("%s: Active_Device (%d), Gpio(%d)\n", __func__, Active_Device, ow_gpio);
		auth_base = ioremap(pdata->base_phys_addr_and_size[0], pdata->base_phys_addr_and_size[1]);
		control_reg_addr1 = auth_base + pdata->control_and_data_offset[0];
		data_reg_addr1 = auth_base + pdata->control_and_data_offset[1];
		val = readl_relaxed(control_reg_addr1);
		if (val & EGPIO_BIT_MASK) {
			ctrl_reg_val1 = val | (1<<12);
			pr_info("%s: egpio case\n", __func__);
		} else {
			ctrl_reg_val1 = val;
		}
		ctrl_bit_pos1 = pdata->control_and_data_bit[0];
		rd_delay_ns1 = pdata->rw_delay_ns[0];
		wr_delay_ns1 = pdata->rw_delay_ns[1];
	} else if (Active_Device == 2) {
		ow_gpio = pdata->swi_gpio[1];
		gpio_dir_out = false;
		pr_info("%s: Active_Device (%d), Gpio(%d)\n", __func__, Active_Device, ow_gpio);
		auth_base = ioremap(pdata->base_phys_addr_and_size2[0], pdata->base_phys_addr_and_size2[1]);
		control_reg_addr2 = auth_base + pdata->control_and_data_offset2[0];
		data_reg_addr2 = auth_base + pdata->control_and_data_offset2[1];
		val = readl_relaxed(control_reg_addr2);
		if (val & EGPIO_BIT_MASK) {
			ctrl_reg_val2 = val | (1<<12);
			pr_info("%s: egpio case\n", __func__);
		} else {
			ctrl_reg_val2 = val;
		}
		ctrl_bit_pos2 = pdata->control_and_data_bit2[0];
		rd_delay_ns2 = pdata->rw_delay_ns2[0];
		wr_delay_ns2 = pdata->rw_delay_ns2[1];
	}
}
