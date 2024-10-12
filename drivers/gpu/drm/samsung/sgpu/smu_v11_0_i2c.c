/*
 * Copyright 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "smuio/smuio_11_0_0_offset.h"
#include "smuio/smuio_11_0_0_sh_mask.h"

#include "smu_v11_0_i2c.h"
#include "amdgpu.h"
#include "soc15_common.h"
#include <drm/drm_fixed.h>
#include <drm/drm_drv.h>
#include "amdgpu_amdkfd.h"
#include <linux/i2c.h>
#include <linux/pci.h>

/* error codes */
#define I2C_OK                0
#define I2C_NAK_7B_ADDR_NOACK 1
#define I2C_NAK_TXDATA_NOACK  2
#define I2C_TIMEOUT           4
#define I2C_SW_TIMEOUT        8
#define I2C_ABORT             0x10

#define I2C_X_RESTART         BIT(31)

#if 0
static void smu_v11_0_i2c_set_clock_gating(struct i2c_adapter *control, bool en)
{
}

/* The T_I2C_POLL_US is defined as follows:
 *
 * "Define a timer interval (t_i2c_poll) equal to 10 times the
 *  signalling period for the highest I2C transfer speed used in the
 *  system and supported by DW_apb_i2c. For instance, if the highest
 *  I2C data transfer mode is 400 kb/s, then t_i2c_poll is 25 us."  --
 * DesignWare DW_apb_i2c Databook, Version 1.21a, section 3.8.3.1,
 * page 56, with grammar and syntax corrections.
 *
 * Vcc for our device is at 1.8V which puts it at 400 kHz,
 * see Atmel AT24CM02 datasheet, section 8.3 DC Characteristics table, page 14.
 *
 * The procedure to disable the IP block is described in section
 * 3.8.3 Disabling DW_apb_i2c on page 56.
 */
#define I2C_SPEED_MODE_FAST     2
#define T_I2C_POLL_US           25
#define I2C_MAX_T_POLL_COUNT    1000
static int smu_v11_0_i2c_enable(struct i2c_adapter *control, bool enable)
{
	return 0;
}

static void smu_v11_0_i2c_clear_status(struct i2c_adapter *control)
{
}

static void smu_v11_0_i2c_configure(struct i2c_adapter *control)
{
}

static void smu_v11_0_i2c_set_clock(struct i2c_adapter *control)
{
}

static void smu_v11_0_i2c_set_address(struct i2c_adapter *control, u16 address)
{
}

static uint32_t smu_v11_0_i2c_poll_tx_status(struct i2c_adapter *control)
{
	return 0;
}

static uint32_t smu_v11_0_i2c_poll_rx_status(struct i2c_adapter *control)
{
	return 0;
}

/**
 * smu_v11_0_i2c_transmit - Send a block of data over the I2C bus to a slave device.
 *
 * @control: I2C adapter reference
 * @address: The I2C address of the slave device.
 * @data: The data to transmit over the bus.
 * @numbytes: The amount of data to transmit.
 * @i2c_flag: Flags for transmission
 *
 * Returns 0 on success or error.
 */
static uint32_t smu_v11_0_i2c_transmit(struct i2c_adapter *control,
				       u16 address, u8 *data,
				       u32 numbytes, u32 i2c_flag)
{
	return 0;
}


/**
 * smu_v11_0_i2c_receive - Receive a block of data over the I2C bus from a slave device.
 *
 * @control: I2C adapter reference
 * @address: The I2C address of the slave device.
 * @data: Placeholder to store received data.
 * @numbytes: The amount of data to transmit.
 * @i2c_flag: Flags for transmission
 *
 * Returns 0 on success or error.
 */
static uint32_t smu_v11_0_i2c_receive(struct i2c_adapter *control,
				      u16 address, u8 *data,
				      u32 numbytes, u32 i2c_flag)
{
	return 0;
}

static void smu_v11_0_i2c_abort(struct i2c_adapter *control)
{
}

static bool smu_v11_0_i2c_activity_done(struct i2c_adapter *control)
{
	return false;
}

static void smu_v11_0_i2c_init(struct i2c_adapter *control)
{
	int res;

	/* Disable clock gating */
	smu_v11_0_i2c_set_clock_gating(control, false);

	if (!smu_v11_0_i2c_activity_done(control))
		DRM_WARN("I2C busy !");

	/* Disable I2C */
	res = smu_v11_0_i2c_enable(control, false);
	if (res != I2C_OK)
		smu_v11_0_i2c_abort(control);

	/* Configure I2C to operate as master and in standard mode */
	smu_v11_0_i2c_configure(control);

	/* Initialize the clock to 50 kHz default */
	smu_v11_0_i2c_set_clock(control);

}

static void smu_v11_0_i2c_fini(struct i2c_adapter *control)
{

}

static bool smu_v11_0_i2c_bus_lock(struct i2c_adapter *control)
{
	return false;
}

static bool smu_v11_0_i2c_bus_unlock(struct i2c_adapter *control)
{
	return false;
}

/***************************** I2C GLUE ****************************/

static uint32_t smu_v11_0_i2c_read_data(struct i2c_adapter *control,
					struct i2c_msg *msg, uint32_t i2c_flag)
{
	uint32_t  ret;

	ret = smu_v11_0_i2c_receive(control, msg->addr, msg->buf, msg->len, i2c_flag);

	if (ret != I2C_OK)
		DRM_ERROR("ReadData() - I2C error occurred :%x", ret);

	return ret;
}

static uint32_t smu_v11_0_i2c_write_data(struct i2c_adapter *control,
					struct i2c_msg *msg, uint32_t i2c_flag)
{
	uint32_t  ret;

	ret = smu_v11_0_i2c_transmit(control, msg->addr, msg->buf, msg->len, i2c_flag);

	if (ret != I2C_OK)
		DRM_ERROR("WriteI2CData() - I2C error occurred :%x", ret);
	
	return ret;

}

static void lock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
}

static int trylock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
	WARN_ONCE(1, "This operation not supposed to run in atomic context!");
	return false;
}

static void unlock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
}

static const struct i2c_lock_operations smu_v11_0_i2c_i2c_lock_ops = {
	.lock_bus = lock_bus,
	.trylock_bus = trylock_bus,
	.unlock_bus = unlock_bus,
};

static int smu_v11_0_i2c_xfer(struct i2c_adapter *i2c_adap,
			      struct i2c_msg *msg, int num)
{
	int i, ret;
	u16 addr, dir;

	smu_v11_0_i2c_init(i2c_adap);

	/* From the client's point of view, this sequence of
	 * messages-- the array i2c_msg *msg, is a single transaction
	 * on the bus, starting with START and ending with STOP.
	 *
	 * The client is welcome to send any sequence of messages in
	 * this array, as processing under this function here is
	 * striving to be agnostic.
	 *
	 * Record the first address and direction we see. If either
	 * changes for a subsequent message, generate ReSTART. The
	 * DW_apb_i2c databook, v1.21a, specifies that ReSTART is
	 * generated when the direction changes, with the default IP
	 * block parameter settings, but it doesn't specify if ReSTART
	 * is generated when the address changes (possibly...). We
	 * don't rely on the default IP block parameter settings as
	 * the block is shared and they may change.
	 */
	if (num > 0) {
		addr = msg[0].addr;
		dir  = msg[0].flags & I2C_M_RD;
	}

	for (i = 0; i < num; i++) {
		u32 i2c_flag = 0;

		if (msg[i].addr != addr || (msg[i].flags ^ dir) & I2C_M_RD) {
			addr = msg[i].addr;
			dir  = msg[i].flags & I2C_M_RD;
			i2c_flag |= I2C_X_RESTART;
		}

		if (i == num - 1) {
			/* Set the STOP bit on the last message, so
			 * that the IP block generates a STOP after
			 * the last byte of the message.
			 */
			i2c_flag |= I2C_M_STOP;
		}

		if (msg[i].flags & I2C_M_RD)
			ret = smu_v11_0_i2c_read_data(i2c_adap,
						      msg + i,
						      i2c_flag);
		else
			ret = smu_v11_0_i2c_write_data(i2c_adap,
						       msg + i,
						       i2c_flag);

		if (ret != I2C_OK) {
			num = -EIO;
			break;
		}
	}

	smu_v11_0_i2c_fini(i2c_adap);
	return num;
}

static u32 smu_v11_0_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm smu_v11_0_i2c_algo = {
	.master_xfer = smu_v11_0_i2c_xfer,
	.functionality = smu_v11_0_i2c_func,
};

static const struct i2c_adapter_quirks smu_v11_0_i2c_control_quirks = {
	.flags = I2C_AQ_NO_ZERO_LEN,
};

int smu_v11_0_i2c_control_init(struct i2c_adapter *control)
{
	return 0;
}

void smu_v11_0_i2c_control_fini(struct i2c_adapter *control)
{
	i2c_del_adapter(control);
}

#endif
/*
 * Keep this for future unit test if bugs arise
 */
#if 0
#define I2C_TARGET_ADDR 0xA0

bool smu_v11_0_i2c_test_bus(struct i2c_adapter *control)
{

	uint32_t ret = I2C_OK;
	uint8_t data[6] = {0xf, 0, 0xde, 0xad, 0xbe, 0xef};


	DRM_INFO("Begin");

	if (!smu_v11_0_i2c_bus_lock(control)) {
		DRM_ERROR("Failed to lock the bus!.");
		return false;
	}

	smu_v11_0_i2c_init(control);

	/* Write 0xde to address 0x0000 on the EEPROM */
	ret = smu_v11_0_i2c_write_data(control, I2C_TARGET_ADDR, data, 6);

	ret = smu_v11_0_i2c_read_data(control, I2C_TARGET_ADDR, data, 6);

	smu_v11_0_i2c_fini(control);

	smu_v11_0_i2c_bus_unlock(control);


	DRM_INFO("End");
	return true;
}
#endif
