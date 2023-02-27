#ifndef _LINUX_LTR_558ALS_H
#define _LINUX_LTR_558ALS_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define LTR558_I2C_NAME      "ltr_558als"
#define LTR558_INPUT_DEV     "alps_pxy"
#define LTR558_I2C_ADDR      0x23
#define LTR558_PLS_IRQ_PIN   "ltr558_irq_pin"
#define LTR558_PS_THRESHOLD  0x03B6

/*LTR-558ALS IO Control*/
#define LTR_IOCTL_MAGIC         0x1C
#define LTR_IOCTL_GET_PFLAG     _IOR(LTR_IOCTL_MAGIC, 1, int)
#define LTR_IOCTL_GET_LFLAG     _IOR(LTR_IOCTL_MAGIC, 2, int)
#define LTR_IOCTL_SET_PFLAG     _IOW(LTR_IOCTL_MAGIC, 3, int)
#define LTR_IOCTL_SET_LFLAG     _IOW(LTR_IOCTL_MAGIC, 4, int)
#define LTR_IOCTL_GET_DATA      _IOW(LTR_IOCTL_MAGIC, 5, unsigned char)

/*LTR-558ALS Registers*/
#define LTR558_ALS_CONTR        0x80
#define LTR558_PS_CONTR         0x81
#define LTR558_PS_LED           0x82
#define LTR558_PS_N_PULSES      0x83
#define LTR558_PS_MEAS_RATE     0x84
#define LTR558_ALS_MEAS_RATE    0x85
#define LTR558_PART_NUMBER_ID   0x86
#define LTR558_MANUFACTURER_ID  0x87

#define LTR558_ALS_DATA_CH1     0x88
#define LTR558_ALS_DATA_CH1_0   0x88
#define LTR558_ALS_DATA_CH1_1   0x89
#define LTR558_ALS_DATA_CH0     0x8A
#define LTR558_ALS_DATA_CH0_0   0x8A
#define LTR558_ALS_DATA_CH0_1   0x8B
#define LTR558_ALS_PS_STATUS    0x8C
#define LTR558_PS_DATA_0        0x8D
#define LTR558_PS_DATA_1        0x8E

#define LTR558_INTERRUPT        0x8F
#define LTR558_PS_THRES_UP      0x90
#define LTR558_PS_THRES_UP_0    0x90
#define LTR558_PS_THRES_UP_1    0x91
#define LTR558_PS_THRES_LOW     0x92
#define LTR558_PS_THRES_LOW_0   0x92
#define LTR558_PS_THRES_LOW_1   0x93

#define LTR558_ALS_THRES_UP     0x97
#define LTR558_ALS_THRES_UP_0   0x97
#define LTR558_ALS_THRES_UP_1   0x98
#define LTR558_ALS_THRES_LOW    0x99
#define LTR558_ALS_THRES_LOW_0  0x99
#define LTR558_ALS_THRES_LOW_1  0x9A

#define LTR558_INTERRUPT_PERSIST 0x9E

/*Basic Operating Modes*/
#define MODE_ALS_ACTIVE_GAIN1   0x0B
#define MODE_ALS_ACTIVE_GAIN2   0x03
#define MODE_ALS_STANDBY        0x00

#define MODE_PS_ACTIVE_GAIN1    0x03
#define MODE_PS_ACTIVE_GAIN4    0x07
#define MODE_PS_ACTIVE_GAIN8    0x0B
#define MODE_PS_ACTIVE_GAIN16   0x0F
#define MODE_PS_STANDBY         0x00

#define PS_RANGE1         1
#define PS_RANGE2         2
#define PS_RANGE4         4
#define PS_RANGE8         8

#define ALS_RANGE1_320    1
#define ALS_RANGE2_64K    2

#define PON_DELAY         600
#define WAKEUP_DELAY      10

#define LTR558_PART_ID    0x86
#define LTR558_PS_OFFSET_1  0x94
#define LTR558_PS_OFFSET_2  0x95
// Basic Operating Modes
#define MODE_ALS_558_ON_Range1   0x0B
#define MODE_ALS_558_ON_Range2   0x03

#define MODE_ALS_553_ON_Range1   0x01
#define MODE_ALS_553_ON_Range2   0x05
#define MODE_ALS_553_ON_Range4   0x09
#define MODE_ALS_553_ON_Range8   0x0D
#define MODE_ALS_553_ON_Range48  0x19
#define MODE_ALS_553_ON_Range96  0x1D

#define MODE_ALS_StdBy          0x00
#define MODE_PS_558_ON_Gain1         0x03
#define MODE_PS_558_ON_Gain4         0x07
#define MODE_PS_558_ON_Gain8         0x0B
#define MODE_PS_558_ON_Gain16        0x0F

#define MODE_PS_553_ON_Gain16        0x03
#define MODE_PS_553_ON_Gain32        0x0B
#define MODE_PS_553_ON_Gain64        0x0F

#define MODE_PS_StdBy     0x00

#define PS_558_RANGE1     1
#define PS_558_RANGE2     2
#define PS_558_RANGE4     4
#define PS_558_RANGE8     8

#define PS_553_RANGE16    16
#define PS_553_RANGE32    32
#define PS_553_RANGE64    64

#define ALS_558_RANGE1_320   1
#define ALS_558_RANGE2_64K   2

#define ALS_553_RANGE1_64K    1
#define ALS_553_RANGE2_32K    2
#define ALS_553_RANGE4_16K    4
#define ALS_553_RANGE8_8K     8
#define ALS_553_RANGE48_1K3   48
#define ALS_553_RANGE96_600    96

#define LTR_PLS_558        0
#define LTR_PLS_553        1

#define LTR_558_PART_ID        0x80
#define LTR_553_PART_ID        0x92

struct ltr558_pls_platform_data {
        int irq_gpio_number;
};

#endif
