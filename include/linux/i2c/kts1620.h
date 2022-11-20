/* platform data for the KTS1620 16-bit I/O expander driver */

#ifndef _KTS1620_H_
#define _KTS1620_H_

#define DRV_NAME	"kts1620-gpio"

#define KTS1620_INPUT          		0x00  /* Input port [RO]                            */
#define KTS1620_DAT_OUT        		0x04  /* GPIO DATA OUT [R/W]                        */
#define KTS1620_POLARITY       		0x08  /* Polarity Inversion port [R/W]              */
#define KTS1620_CONFIG         		0x0C  /* Configuration port [R/W]                   */
#define KTS1620_DRIVE0         		0x40  /* Output drive strength register Port0 [R/W] */
#define KTS1620_DRIVE1         		0x42  /* Output drive strength register Port1 [R/W] */
#define KTS1620_DRIVE2         		0x44  /* Output drive strength register Port1 [R/W] */
#define KTS1620_INPUT_LATCH        	0x48  /* Port0 Input latch register  [R/W]          */
#define KTS1620_EN_PULLUPDOWN      	0x4C  /* Port0 Pull-up/Pull-down Enable [R/W]       */
#define KTS1620_SEL_PULLUPDOWN     	0x50  /* Port0 Pull-up/Pull-down selection [R/W]    */
#define KTS1620_INT_MASK       		0x54  /* Interrupt mask register [R/W]              */
#define KTS1620_INT_STATUS     		0x58  /* Interrupt status register [RO]             */
#define KTS1620_OUTPUT_CONFIG      	0x5C  /* Output port configuration register [R/W]   */

#define NO_PULL             		0x00
#define PULL_DOWN           		0x01
#define PULL_UP             		0x02

#define POWER_ON            		1
#define POWER_OFF           		0
#define KTS1620_PORT_CNT       		3
#define KTS1620_MAX_GPIO       		23

struct device *kts1620_dev;
extern struct class *sec_class;

/* EXPANDER GPIO Drive Strength */
enum {
    GPIO_CFG_6_25MA,
    GPIO_CFG_12_5MA,
    GPIO_CFG_18_75MA,
    GPIO_CFG_25MA,
};

struct kts1620_platform_data {
    /* number of the first GPIO */
    unsigned int gpio_base;
    int gpio_start;
    int ngpio;
    int irq_base;
    int irq_gpio;
    int reset_gpio;
    uint32_t support_init;
    uint32_t init_config;
    uint32_t init_data_out;
    uint32_t init_en_pull;
    uint32_t init_sel_pull;
    struct regulator *vdd;
};
#endif

