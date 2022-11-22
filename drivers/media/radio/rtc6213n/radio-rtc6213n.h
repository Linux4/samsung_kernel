/*
 *  drivers/media/radio/rtc6213n/radio-rtc6213n.h
 *
 *  Driver for Richwave RTC6213N FM Tuner
 *
 *  Copyright (c) 2013 TianTsai Chang <changtt@richwave.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/* driver definitions */
#define DRIVER_NAME "rtc6213n-fmtuner"


/* kernel includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/mutex.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <asm/unaligned.h>
#include <linux/interrupt.h>


/**************************************************************************
 * Register Definitions
 **************************************************************************/
#define RADIO_REGISTER_SIZE         2       /* 16 register bit width */
#define RADIO_REGISTER_NUM          16      /* DEVICEID */
#define RDS_REGISTER_NUM            6       /* STATUSRSSI */

#define DEVICEID                    0       /* Device ID Code */
#define DEVICE_ID                   0xffff  /* [15:00] Device ID */
#define DEVICEID_PN                 0xf000  /* [15:12] Part Number */
#define DEVICEID_MFGID              0x0fff  /* [11:00] Manufacturer ID */

#define CHIPID                      1       /* Chip ID Code */
#define CHIPID_REVISION_NO          0xfc00  /* [15:10] Chip Reversion */

#define MPXCFG                      2       /* Power Configuration */
#define MPXCFG_CSR0_DIS_SMUTE       0x8000  /* [15:15] Disable Softmute */
#define MPXCFG_CSR0_DIS_MUTE        0x4000  /* [14:14] Disable Mute */
#define MPXCFG_CSR0_MONO            0x2000  /* [13:13] Force Mono or Auto Detect */
#define MPXCFG_CSR0_DEEM            0x1000  /* [12:12] DE-emphasis */
#define MPXCFG_CSR0_BLNDADJUST      0x0300  /* [09:08] Blending Level Adjustment */
#define MPXCFG_CSR0_SMUTERATE       0x00c0  /* [07:06] Softmute Enter and Recover Rate */
#define MPXCFG_CSR0_SMUTEATT        0x0030  /* [05:04] Softmute Attenuation value */
#define MPXCFG_CSR0_VOLUME          0x000f  /* [03:00] Volume */

#define CHANNEL                     3       /* Tuning Channel Setting */
#define CHANNEL_CSR0_TUNE           0x8000  /* [15:15] Tune */
#define CHANNEL_CSR0_BAND           0x3000  /* [13:12] Band Select */
#define CHANNEL_CSR0_CHSPACE        0x0c00  /* [11:10] Channel Sapcing */
#define CHANNEL_CSR0_CH             0x03ff  /* [09:00] Tuning Channel */

#define SYSCFG                      4       /* System Configuration 1 */
#define SYSCFG_CSR0_RDSIRQEN        0x8000  /* [15:15] RDS Interrupt Enable (RTC6213N only) */
#define SYSCFG_CSR0_STDIRQEN        0x4000  /* [14:14] Seek/Tune Done Interrupt Enable */
#define SYSCFG_CSR0_DIS_AGC         0x2000  /* [13:13] Disable AGC */
#define SYSCFG_CSR0_RDS_EN          0x1000  /* [12:12] RDS Enable */
#define SYSCFG_CSR0_RBDS_M          0x0300  /* [09:08] MMBS setting */

#define SEEKCFG1                    5       /* Seek Configuration 1 */
#define SEEKCFG1_CSR0_SEEK          0x8000  /* [15:15] Enable Seek Function */
#define SEEKCFG1_CSR0_SEEKUP        0x4000  /* [14:14] Seek Direction */
#define SEEKCFG1_CSR0_SKMODE        0x2000  /* [13:13] Seek Mode */
#define SEEKCFG1_CSR0_SEEKRSSITH    0x00ff  /* [07:00] RSSI Seek Threshold */

#define POWERCFG                    6       /* Power Configuration */
#define POWERCFG_CSR0_ENABLE        0x8000  /* [15:15] Power-up Enable */
#define POWERCFG_CSR0_DISABLE       0x4000  /* [14:14] Power-up Disable */
#define POWERCFG_CSR0_BLNDOFS       0x0f00  /* [11:08] Blending Offset Value */

#define PADCFG                      7       /* PAD Configuration */
#define PADCFG_CSR0_RC_EN           0xf000  /* Internal crystal RC enable*/
#define PADCFG_CSR0_GPIO            0x0004  /* [03:02] General purpose I/O - for STD and RDS_RDY*/

#define BANKCFG                     8       /* Bank Serlection */
/* contains reserved */

#define SEEKCFG2                    9       /* Seek Configuration 2 */
#define SEEKCFG2_CSR0_OFSTH         0x4000  /* [15:08] Seek DC offset fail threshold */
#define SEEKCFG2_CSR0_QLTTH         0x0010  /* [07:00] Seek audio quality fail threshold */

/* BOOTCONFIG only contains reserved [*/
#define STATUS                      10      /* Status register and work channel */
#define STATUS_RDS_RDY              0x8000  /* [15:15] RDS Ready (RTC6213N only) */
#define STATUS_STD                  0x4000  /* [14:14] Seek/Tune Done */
#define STATUS_SF                   0x2000  /* [13:13] Seek Fail */
#define STATUS_RDS_SYNC             0x0800  /* [11:11] RDS in synchronization (RTC6213N only) */
#define STATUS_SI                   0x0400  /* [10:10] Stereo Indicator */
#define STATUS_READCH               0x03ff  /* [09:00] Read Channel */

#define RSSI                        11      /* RSSI and RDS error */
#define RSSI_RDS_BA_ERRS            0xc000  /* [15:14] RDS Block A Errors (RTC6213N only) */
#define RSSI_RDS_BB_ERRS            0x3000  /* [15:14] RDS Block B Errors (RTC6213N only) */
#define RSSI_RDS_BC_ERRS            0x0c00  /* [13:12] RDS Block C Errors (RTC6213N only) */
#define RSSI_RDS_BD_ERRS            0x0300  /* [11:10] RDS Block D Errors (RTC6213N only) */
#define RSSI_RSSI                   0x00ff  /* [09:00] Read Channel */

#define BA_DATA                     12      /* Block A data */
#define RDSA_RDSA                   0xffff  /* [15:00] RDS Block A Data (RTC6213N only) */

#define BB_DATA                     13      /* Block B data */
#define RDSB_RDSB                   0xffff  /* [15:00] RDS Block B Data (RTC6213N only) */

#define BC_DATA                     14      /* Block C data */
#define RDSC_RDSC                   0xffff  /* [15:00] RDS Block C Data (RTC6213N only) */

#define BD_DATA                     15      /* Block D data */
#define RDSD_RDSD                   0xffff  /* [15:00] RDS Block D Data (RTC6213N only) */

/* (V4L2_CID_PRIVATE_BASE + (<Register> << 4) + (<Bit Position> << 0)) */
// #define V4L2_CID_PRIVATE_BASE        0x08000000

#define RTC6213N_IOC_POWERUP                (V4L2_CID_PRIVATE_BASE + (POWERCFG<<4) + 15)
#define RTC6213N_IOC_POWERDOWN              (V4L2_CID_PRIVATE_BASE + (POWERCFG<<4) + 14)

#if 0
#define V4L2_CID_PRIVATE_DEVICEID           (V4L2_CID_PRIVATE_BASE + (DEVICEID<<4) + 0)
//#define RTC6213N_IOC_DSMUTE                 (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 15)
////#define RTC6213N_IOC_MUTE                   (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 14)
////#define RTC6213N_IOC_MONO_SET               (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 13)
//#define RTC6213N_IOC_DE_SET                 (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 12)
////#define RTC6213N_IOC_VOLUME_GET             (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 1)
////#define RTC6213N_IOC_VOLUME_SET             (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 0)

#define RTC6213N_IOC_CHAN_SELECT            (V4L2_CID_PRIVATE_BASE + (CHANNEL<<4) + 15)
//#define RTC6213N_IOC_BAND_SET               (V4L2_CID_PRIVATE_BASE + (CHANNEL<<4) + 12)
//#define RTC6213N_IOC_CHAN_SPACING_SET       (V4L2_CID_PRIVATE_BASE + (CHANNEL<<4) + 10)

#define RTC6213N_IOC_RDS_ENABLE             (V4L2_CID_PRIVATE_BASE + (SYSCFG<<4) + 12)
#define RTC6213N_IOC_RESET_RDS_DATA         (V4L2_CID_PRIVATE_BASE + (SYSCFG<<4) + 11)

#define RTC6213N_IOC_SEEK_FULL              (V4L2_CID_PRIVATE_BASE + (SEEKCFG1<<4) + 15)
#define RTC6213N_IOC_SEEK_UP                (V4L2_CID_PRIVATE_BASE + (SEEKCFG1<<4) + 14)
#define RTC6213N_IOC_SEEK_DOWN              (V4L2_CID_PRIVATE_BASE + (SEEKCFG1<<4) + 13)
#define RTC6213N_IOC_SEEK_CANCEL            (V4L2_CID_PRIVATE_BASE + (SEEKCFG1<<4) + 10)
//#define RTC6213N_IOC_RSSI_SEEK_TH_SET       (V4L2_CID_PRIVATE_BASE + (SEEKCFG1<<4) + 0)

#define RTC6213N_IOC_RDS_DATA_GET           (V4L2_CID_PRIVATE_BASE + (STATUS<<4) + 15)
#define RTC6213N_IOC_CHAN_GET               (V4L2_CID_PRIVATE_BASE + (STATUS<<4) + 0)

//#define RTC6213N_IOC_STATUS_RSSI_GET        (V4L2_CID_PRIVATE_BASE + (RSSI<<4) + 0)

#else
#define V4L2_CID_PRIVATE_DEVICEID           (V4L2_CID_PRIVATE_BASE + (DEVICEID<<4) + 0)
#define V4L2_CID_PRIVATE_CSR0_DIS_SMUTE     (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 15)
#define V4L2_CID_PRIVATE_CSR0_DEEM          (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 12)
#define V4L2_CID_PRIVATE_CSR0_BLNDADJUST    (V4L2_CID_PRIVATE_BASE + (MPXCFG<<4) + 8)

#define V4L2_CID_PRIVATE_CSR0_BAND          (V4L2_CID_PRIVATE_BASE + (CHANNEL<<4) + 12)
#define V4L2_CID_PRIVATE_CSR0_CHSPACE       (V4L2_CID_PRIVATE_BASE + (CHANNEL<<4) + 10)

#define V4L2_CID_PRIVATE_CSR0_DIS_AGC       (V4L2_CID_PRIVATE_BASE + (SYSCFG<<4) + 13)
#define V4L2_CID_PRIVATE_CSR0_RDS_EN        (V4L2_CID_PRIVATE_BASE + (SYSCFG<<4) + 12)

#define V4L2_CID_PRIVATE_CSR0_SEEKRSSITH    (V4L2_CID_PRIVATE_BASE + (SEEKCFG1<<4) + 0)

#define V4L2_CID_PRIVATE_RSSI               (V4L2_CID_PRIVATE_BASE + (RSSI<<4) + 0)
#endif
/**************************************************************************
 * General Driver Definitions
 **************************************************************************/

/*
 * rtc6213n_device - private data
 */
struct rtc6213n_device {
    struct video_device *videodev;

    /* driver management */
    unsigned int users;

    /* Richwave internal registers (0..15) */
    unsigned short registers[RADIO_REGISTER_NUM];

    /* RDS receive buffer */
    wait_queue_head_t read_queue;
    struct mutex lock;      /* buffer locking */
    unsigned char *buffer;      /* size is always multiple of three */
    unsigned int buf_size;
    unsigned int rd_index;
    unsigned int wr_index;

    struct completion completion;
    bool stci_enabled;      /* Seek/Tune Complete Interrupt */

    struct i2c_client *client;
};

/**************************************************************************
 * Firmware Versions
 **************************************************************************/

#define RADIO_FW_VERSION    15

/**************************************************************************
 * Frequency Multiplicator
 **************************************************************************/

#define FREQ_MUL 1000


//#define CONFIG_STD_POLLING
//#define CONFIG_REJECTRDS_WITHERROR
#define CONFIG_RDS


/**************************************************************************
 * Common Functions
 **************************************************************************/
extern struct video_device rtc6213n_viddev_template;
extern struct tasklet_struct my_tasklet;
extern int rtc6213n_get_all_registers(struct rtc6213n_device *radio);
extern int rtc6213n_get_register(struct rtc6213n_device *radio, int regnr);
extern int rtc6213n_set_register(struct rtc6213n_device *radio, int regnr);
extern int rtc6213n_set_serial_registers(struct rtc6213n_device *radio, u16 *data, int bytes);
//int rtc6213n_get_allbanks_registers(struct rtc6213n_device *radio);
int rtc6213n_disconnect_check(struct rtc6213n_device *radio);
int rtc6213n_set_freq(struct rtc6213n_device *radio, unsigned int freq);
int rtc6213n_start(struct rtc6213n_device *radio);
int rtc6213n_stop(struct rtc6213n_device *radio);
int rtc6213n_fops_open(struct file *file);
int rtc6213n_fops_release(struct file *file);
int rtc6213n_vidioc_querycap(struct file *file, void *priv,
        struct v4l2_capability *capability);
