#ifndef __LINUX_PTN5150_H__
#define __LINUX_PTN5150_H__

#define PTN5150_NAME                            "nxp-ptn5150"
#define NXP_PTN_ADDR				0x38

#define DEVICE_VERSION_ID(x)                    (x & 0xF8)
#define VENDOR_ID(x)                            (x & 0x07)
#define DFP_RP_80uA_SELET                       ((BIT(3) | BIT(4)) & 0x00)
#define DFP_RP_180uA_SELET                      ((BIT(3) | BIT(4)) & 0x0F)
#define DFP_RP_330uA_SELET                      ((BIT(3) | BIT(4)) & 0xF0)
#define UFP_MODE                                ((BIT(1) | BIT(2)) & 0x0)
#define DFP_MODE                                ((BIT(1) | BIT(2)) & 0x2)
#define DRP_MODE                                ((BIT(1) | BIT(2)) & 0x4)
#define DETACHED_MASK_EN                        BIT(0)
#define ATTACHED_INTERRUPT                      BIT(0)
#define DETACHED_INTERRUPT                      BIT(1)
#define UFP_VBUS_DETECTED                       BIT(7)
#define UFP_RP_80uA_DETECTED                    ((BIT(6) | BIT(5)) & (~(BIT(6))))
#define UFP_RP_180uA_DETECTED                   ((BIT(6) | BIT(5)) & (~(BIT(5))))
#define UFP_RP_330uA_DETECTED                   (BIT(6) | BIT(5))
#define UFP_DFP_STANDBY                          0x00
#define UFP_DETECTED                            (BIT(2) & 0x1C)
#define DFP_DETECTED                            (BIT(3) & 0x1C)
#define AUDIO_DETECTED                          ((BIT(2) | BIT(3))&(0x1C))
#define DEBUG_ACCESS_DETECTED                   (BIT(4) & 0x1C)
#define CC1_CONNECTED                            (BIT(0) & 0x3)
#define CC2_CONNECTED                           (BIT(1) & 0x3)
#define CC1_CC2_CONNECTED                       ((BIT(1) | BIT(0)) & (0x03))
#define DISABLE_CON_DET                         BIT(0)
#define VCONN_POWER_CC1                         ((BIT(0) | BIT(1)) & (~(BIT(1))))
#define VCONN_POWER_CC2                         ((BIT(1) | BIT(0)) & (~(BIT(0))))
#define MASK_CC1_INTERRUPTS                     BIT(6)
#define MASK_CC2_INTERRUPTS                     BIT(5)
#define MASK_CURRENT_SENSE_INTERRUPTS           BIT(4)
#define MASK_ROLE_CHANGE                        BIT(3)
#define MASK_ORIENTATION_FOUND                  BIT(2)
#define MASK_DEBUG_ACCESS                       BIT(1)
#define MASK_AUDIO_ASSESS                       BIT(0)
#define INT_STAT_CC1                            BIT(6)
#define INT_STAT_CC2                            BIT(5)
#define INT_STAT_CURRENT_SENSE                  BIT(4)
#define INT_STAT_ROLE_CHANGE                    BIT(3)
#define INT_STAT_ORIENTATION_FOUND              BIT(2)
#define INT_STAT_DEBUG_ACCESS                   BIT(1)
#define INT_STAT_AUDIO_ASSESS                   BIT(0)

int ptn5150_i2c_Read(struct i2c_client *client, char *writebuf, int writelen,
		     char *readbuf, int readlen);
int ptn5150_i2c_Write(struct i2c_client *client, char *writebuf, int writelen);


#endif
