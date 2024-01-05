#ifndef __ASM_MACH_BOARD_KANAS_NFC
#define __ASM_MACH_BOARD_KANAS_NFC

/* SRI-N:s.nitesh: NFC GPIO */
#define GPIO_NFC_IRQ            68
#define GPIO_NFC_EN             69
#define GPIO_NFC_FIRMWARE       70

#define PN547_I2C_BUS_ID         2   // Added for HW I2C2
#define PN547_I2C_SLAVE_ADDR  0x2B

void pn547_i2c_device_register(void);

#endif
