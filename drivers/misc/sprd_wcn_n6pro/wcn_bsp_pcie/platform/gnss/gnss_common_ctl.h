#ifndef __GNSS_COMMON_CTL_H__
#define __GNSS_COMMON_CTL_H__

/* originally defined in gnss/gnss.h begin */
#ifdef CONFIG_WCN_INTEG

#include <linux/regmap.h>

#define GNSS_STATUS_OFFSET                 (0x0014F004)

#ifdef CONFIG_UMW2631_I
#define GNSS_QOGIRL6_STATUS_OFFSET        (0x001ffc34)
#endif

#endif
/* originally defined in gnss/gnss.h end */

/* originally defined in gnss/gnss_common.h begin */
#ifndef CONFIG_WCN_INTEG
#define GNSS_INDIRECT_OP_REG            0x40b20000

#ifdef CONFIG_UMW2652_REMOVE
#define SC2730_PIN_REG_BASE     0x0480
#define PTEST0                  0x0
#define PTEST0_MASK             (BIT(4) | BIT(5))
#define PTEST0_sel(x)           (((x)&0x3)<<4)

#define REGS_ANA_APB_BASE       0x1800
#define XTL_WAIT_CTRL0          0x378
#define BIT_XTL_EN              BIT(8)

#define TSEN_CTRL0              0x334
#define BIT_TSEN_CLK_SRC_SEL    BIT(4)
#define BIT_TSEN_ADCLDO_EN      BIT(15)

#define TSEN_CTRL1               0x338
#define BIT_TSEN_CLK_EN         BIT(7)
#define BIT_TSEN_SDADC_EN       BIT(11)
#define BIT_TSEN_UGBUF_EN       BIT(14)

#define TSEN_CTRL2              0x33c
#define TSEN_CTRL3              0x340
#define BIT_TSEN_EN             BIT(0)
#define BIT_TSEN_TIME_SEL_MASK  (BIT(4) | BIT(5))
#define BIT_TSEN_TIME_sel(x)    (((x)&0x3)<<4)

#define TSEN_CTRL4              0x344
#define TSEN_CTRL5              0x348
#define CLK32KLESS_CTRL0        0x368
#define M26_TSX_32KLESS         0x8010

enum{
	TSEN_EXT,
	TSEN_INT,
};
#endif
#endif

/* qogirl6 begin */
#ifdef CONFIG_UMW2631_I
/* for tsx */
#define SC2730_PIN_REG_BASE     0x0480
#define PTEST0                  0x0
#define PTEST0_MASK             (BIT(4) | BIT(5))
#define PTEST0_sel(x)           (((x) & 0x3) << 4)

#define REGS_ANA_APB_BASE       0x1800
#define XTL_WAIT_CTRL0          0x378
#define BIT_XTL_EN              BIT(8)

#define TSEN_CTRL0              0x334
#define BIT_TSEN_CLK_SRC_SEL    BIT(4)
#define BIT_TSEN_ADCLDO_EN      BIT(15)

#define TSEN_CTRL1               0x338
#define BIT_TSEN_CLK_EN         BIT(7)
#define BIT_TSEN_SDADC_EN       BIT(11)
#define BIT_TSEN_UGBUF_EN       BIT(14)

#define TSEN_CTRL2              0x33c
#define TSEN_CTRL3              0x340
#define BIT_TSEN_EN             BIT(0)
#define BIT_TSEN_TIME_SEL_MASK  (BIT(4) | BIT(5))
#define BIT_TSEN_TIME_sel(x)    (((x) & 0x3) << 4)

#define TSEN_CTRL4              0x344
#define TSEN_CTRL5              0x348
#define CLK32KLESS_CTRL0        0x368
#define M26_TSX_32KLESS         0x8010

enum{
	TSEN_EXT,
	TSEN_INT,
};
/* for tcxo */
#define XTLBUF3_REL_CFG_ADDR    0x640209e0
#define XTLBUF3_REL_CFG_OFFSET  0x9e0
#define WCN_XTL_CTRL_ADDR     0x64020fe4
#define WCN_XTL_CTRL_OFFSET    0xfe4
#endif
/* qogirl6 end */

bool gnss_delay_ctl(void);
/* originally defined in gnss/gnss_common.h end */

#endif
