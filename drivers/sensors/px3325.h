/*
 * This file is part of the PX3325 sensor driver.
 * PX3325 is combined proximity sensor and IRLED.
 *
 * Contact: YC Hou <yc.hou@liteonsemi.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 *
 * Filename: px3325_v1.02_20130103.h
 *
 * Summary:
 *	PX3325 sensor dirver.
 *
 * Modification History:
 * Date     By       Summary
 * -------- -------- -------------------------------------------------------
 * 07/10/13 YC       Original Creation (version:1.00 base on datasheet rev0.50)
 * 01/02/14 YC       Set default settings to 2X, 25T for recommend settings. 
 *                   Ver 1.01.   
 * 01/03/14 YC       Modify to support devicetree. Change to ver 1.02.
 */
  
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_i2c.h>
#include "sensors_core.h"
#include <linux/regulator/consumer.h>


#define PX3325_DRV_NAME	"px3325"

#define DRIVER_VERSION		"1.02"

#define PX3325_NUM_CACHABLE_REGS	19

#define PX3325_MODE_COMMAND	0x00
#define PX3325_MODE_SHIFT	(0)
#define PX3325_MODE_MASK	0x07

#define PX3325_PX_LTHL			0x2a
#define PX3325_PX_LTHL_SHIFT	(0)
#define PX3325_PX_LTHL_MASK		0xff

#define PX3325_PX_LTHH			0x2b
#define PX3325_PX_LTHH_SHIFT	(0)
#define PX3325_PX_LTHH_MASK		0x03

#define PX3325_PX_HTHL			0x2c
#define PX3325_PX_HTHL_SHIFT	(0)
#define PX3325_PX_HTHL_MASK		0xff

#define PX3325_PX_HTHH			0x2d
#define PX3325_PX_HTHH_SHIFT	(0)
#define PX3325_PX_HTHH_MASK		0x03

#define PX3325_OBJ_COMMAND	0x01
#define PX3325_OBJ_MASK		0x10
#define PX3325_OBJ_SHIFT	(4)

#define PX3325_INT_COMMAND	0x01
#define PX3325_INT_SHIFT	(0)
#define PX3325_INT_MASK		0x02
#define PX3325_INT_PMASK	0x02

#define	PX3325_PX_LSB		0x0e
#define	PX3325_PX_MSB		0x0f
#define	PX3325_PX_LSB_MASK	0xff
#define	PX3325_PX_MSB_MASK	0x03

#define PX3325_PX_CALI_L	0x28
#define PX3325_PX_CALI_L_MASK   0xff
#define PX3325_PX_CALI_L_SHIFT	(0)

#define PX3325_PX_CALI_H	0x29
#define PX3325_PX_CALI_H_MASK	0x01
#define PX3325_PX_CALI_H_SHIFT	(0)

#define PX3325_PX_GAIN	0x20
#define PX3325_PX_GAIN_MASK	0x0c
#define PX3325_PX_GAIN_SHIFT	(2)

#define PX3325_PX_INTEGRATE	0x25
#define PX3325_PX_INTEGRATE_MASK	0x3f
#define PX3325_PX_INTEGRATE_SHIFT	(0)

#define PS_ACTIVE    0x02

/*============================================================================*/
#define DI_DBG
/*----------------------------------------------------------------------------*/
#ifdef DI_DBG
#define PX_TAG                  "[PX3325] "
#define PX_FUN(f)               printk(KERN_ERR PX_TAG"<%s>\n", __FUNCTION__)
#define PX_ERR(fmt, args...)    printk(KERN_ERR PX_TAG"<%s> %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define PX_LOG(fmt, args...)    printk(KERN_ERR PX_TAG fmt, ##args)
#define PX_DBG(fmt, args...)    printk(KERN_ERR PX_TAG fmt, ##args)                
#else
#define PX_FUN(f)               {}
#define PX_ERR(fmt, args...)    {}
#define PX_LOG(fmt, args...)    {}
#define PX_DBG(fmt, args...)    {}        
#endif
/*============================================================================*/

struct px3325_data {
	struct i2c_client *client;
	struct mutex lock;
	u8 reg_cache[PX3325_NUM_CACHABLE_REGS];
	int irq;
	struct device *proximity_dev;

	struct workqueue_struct *wq;

	struct input_dev *proximity_input_dev;
	struct work_struct work_proximity;
	struct hrtimer proximity_timer;
	ktime_t proximity_poll_delay;
	struct wake_lock prx_wake_lock;
	int en; /*ldo power control*/
	bool is_requested;
	u32 irq_int_flags;
	u32 ldo_gpio_flags;

};

// PX3325 register
static u8 px3325_reg[PX3325_NUM_CACHABLE_REGS] = 
	{0x00,0x01,0x02,0x06,0x0e,0x0f,
	 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x28,0x29,0x2a,0x2b,0x2c,0x2d};

// Set the polling mode or int mode here. 1 = polling, 0 = interrtup.
u8 ps_polling = 1;

#define ADD_TO_IDX(addr,idx)	{														\
									int i;												\
									for(i = 0; i < PX3325_NUM_CACHABLE_REGS; i++)						\
									{													\
										if (addr == px3325_reg[i])						\
										{												\
											idx = i;									\
											break;										\
										}												\
									}													\
								}
 
