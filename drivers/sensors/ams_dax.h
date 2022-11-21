/*
*****************************************************************************
* Copyright by ams AG                                                       *
* All rights are reserved.                                                  *
*                                                                           *
* IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
* THE SOFTWARE.                                                             *
*                                                                           *
* THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.  *
* USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY    *
* EXCLUDED.                                                                 *
*                                                                           *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
*****************************************************************************
*/

/*! \file
* \brief Device driver for monitoring ambient light intensity in (lux)
* proximity detection (prox), and color temperature functionality within the
* AMS-TAOS DAX family of devices.
*/

#ifndef __DAX_H
#define __DAX_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>


//#define ABI_SET_GET_REGISTERS

#ifdef AMS_MUTEX_DEBUG
#define AMS_MUTEX_LOCK(m) { \
        printk(KERN_INFO "%s: Mutex Lock\n", __func__); \
        mutex_lock(m); \
    }
#define AMS_MUTEX_UNLOCK(m) { \
        printk(KERN_INFO "%s: Mutex Unlock\n", __func__); \
        mutex_unlock(m); \
    }
#else
#define AMS_MUTEX_LOCK(m) { \
        mutex_lock(m); \
    }
#define AMS_MUTEX_UNLOCK(m) { \
        mutex_unlock(m); \
    }
#endif

/* Default Params */

#define COEF_SCALE_DEFAULT 1000
#define DGF_DEFAULT        898
#define CLR_COEF_DEFAULT   (210)
#define RED_COEF_DEFAULT   (-60)
#define GRN_COEF_DEFAULT   (10)
#define BLU_COEF_DEFAULT   (-290)
#define CT_COEF_DEFAULT    (4366)
#define CT_OFFSET_DEFAULT  (1849)

#define PROX_MAX_THRSHLD   (0x3FFF)

// fraction out of 10
#define TENTH_FRACTION_OF_VAL(v, x) ({ \
  int __frac = v; \
  if (((x) > 0) && ((x) < 10)) __frac = (__frac*(x)) / 10 ; \
  __frac; \
})


#define MAX_REGS 256
struct device;

enum dax_pwr_state {
    POWER_ON,
    POWER_OFF,
    POWER_STANDBY,
};

enum dax_prox_state {
    PROX_STATE_NONE = 0,
    PROX_STATE_INIT,
    PROX_STATE_CALIB,
    PROX_STATE_WAIT_AND_CALIB
};



// pldrive
#define PDRIVE_MA(p) ({ \
        u8 __reg = (((u8)((p) - 2) / 2) & 0xf); \
        /* artf24717 limit PLDRIVE to 19mA */ \
        __reg = (__reg > 0x08) ? 0x08 : __reg; \
        __reg; \
})
#define P_TIME_US(p)   ((((p) / 88) - 1.0) + 0.5)
#define PRX_PERSIST(p) (((p) & 0xf) << 4)

#define INTEGRATION_CYCLE 2780
#define AW_TIME_MS(p)  ((((p) * 1000) + (INTEGRATION_CYCLE - 1)) / INTEGRATION_CYCLE)
#define ALS_PERSIST(p) (((p) & 0xf) << 0)

// lux
#define INDOOR_LUX_TRIGGER    6000
#define OUTDOOR_LUX_TRIGGER    10000
#define DAX_MAX_LUX        0xffff
#define DAX_MAX_ALS_VALUE    0xffff
#define DAX_MIN_ALS_VALUE    10

struct dax_parameters {
    /* Common */
    u8  persist;

    /* Prox */
    u16 prox_th_min;
    u16 prox_th_max;
    u8  prox_apc;
    u8  prox_pulse_cnt;
    u8  prox_pulse_len;
    u8  prox_pulse_16x;
    u8  prox_gain;
    s16 poffset;
    u8  prox_drive;

    /* ALS / Color */
    u8  als_gain;
    u16 als_deltaP;
    u8  als_time;
    u32 dgf;
    u32 ct_coef;
    u32 ct_offset;
    u32 c_coef;
    u32 r_coef;
    u32 g_coef;
    u32 b_coef;
    u32 coef_scale;
};

struct dax_als_info {
    u32 cpl;
    u32 saturation;
    u16 clear_raw;
    u16 red_raw;
    u16 green_raw;
    u16 blue_raw;
    u16 data4_raw;
    u16 data5_raw;
    u16 cct;
    s16 ir;
    u16 lux;
};

struct dax_prox_info {
    u16 raw;
    int detected;
};

struct dax_chip {
	struct device *ls_dev;
    struct mutex lock;
    struct i2c_client *client;
    struct gpio_desc *gpiod_interrupt;
    struct dax_prox_info prx_inf;
    struct dax_als_info als_inf;
    struct dax_parameters params;
    struct dax_i2c_platform_data *pdata;
    u8 shadow[MAX_REGS];

    struct input_dev *input_dev;
	
	struct delayed_work main_work;
	atomic_t delay;

    int in_suspend;
    int wake_irq;
    int irq_pending;

    bool unpowered;
    bool als_enabled;
    bool als_gain_auto;
    bool prx_enabled;
    bool amsCalComplete;
    bool amsFirstProx;
    bool amsIndoorMode;

    enum dax_prox_state prox_state;
    struct work_struct work;
    struct work_struct fifo_work;
    struct work_struct als_ch_cfg_work;
    //dax_rec_2_fifo dax_fifo;
    int brightness;
    u16 astep;
    u8 again;
    u8 wtime;
    u8 fifo_level;
    u32 fifo_overflowx;
    u32 kfifo_overflowx;
    u32 kfifo_len;
    u8 bytes;
    u8 fifo_map;
    u8 als_ch_cfg;
    u8 als_ch_disable;
    u16 kfifo_max;
    u8 cfg_mode;

	u16 ch0;
	u16 ch1;

    u8 device_index;
};

// Must match definition in ../arch file
struct dax_i2c_platform_data {
    /* The following callback for power events received and handled by
       the driver.  Currently only for SUSPEND and RESUME */
    int (*platform_power)(struct device *dev, enum dax_pwr_state state);
    int (*platform_init)(void);
    void (*platform_teardown)(struct device *dev);

    char const *prox_name;
    char const *als_name;
    struct dax_parameters parameters;
    bool proximity_can_wake;
    bool als_can_wake;
#ifdef CONFIG_OF
      struct device_node  *of_node;
#endif
};

#endif /* __DAX_H */
