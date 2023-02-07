#ifndef __LINUX_ET7303_H
#define __LINUX_ET7303_H

#include <linux/usb/typec/etek/et7303/std_tcpci_v10.h>
#include <linux/usb/typec/etek/et7303/pd_dbg_info.h>

/*show debug message or not */
#define ENABLE_ET7303_DBG	0

/* ET7303 Private RegMap */

#define ET7303_REG_PHY_CTRL1				(0x80)

#define ET7303_REG_CLK_CTRL2				(0x87)
#define ET7303_REG_CLK_CTRL3				(0x88)

#define ET7303_REG_BMC_CTRL				(0x90)
#define ET7303_REG_VBUS_MES				(0x91)
#define ET7303_REG_BMCIO_RXDZSEL			(0x93)
#define ET7303_REG_VCONN_CLIMITEN			(0x95)

#define ET7303_REG_RT_STATUS				(0x97)
#define ET7303_REG_RT_INT				(0x98)
#define ET7303_REG_RT_MASK				(0x99)

#define ET7303_REG_IDLE_CTRL				(0x9B)
#define ET7303_REG_INTRST_CTRL				(0x9C)
#define ET7303_REG_WATCHDOG_CTRL			(0x9D)
#define ET7303_REG_I2CRST_CTRL				(0X9E)

#define ET7303_REG_SWRESET				(0xA0)
#define ET7303_REG_TTCPC_FILTER				(0xA1)
#define ET7303_REG_DRP_TOGGLE_CYCLE			(0xA2)
#define ET7303_REG_DRP_DUTY_CTRL			(0xA3)
#define ET7303_REG_BMCIO_RXDZEN				(0xAF)

#define ET7303_REG_DEBUG1				(0xF0)
#define ET7303_REG_DEBUG2				(0xF1)
/*
 * Device ID
 */

#define ET7303_DID_A		0x2170
#define ET7303_DID_B		0x2171
#define ET7303_DID_C		0x2172

/*
 * ET7303_REG_PHY_CTRL1					(0x80)
 */

#define ET7303_REG_PHY_CTRL1_SET( \
	retry_discard, toggle_cnt, bus_idle_cnt, rx_filter) \
	((retry_discard << 7) | (toggle_cnt << 4) | \
	(bus_idle_cnt << 2) | (rx_filter & 0x03))

/*
 * ET7303_REG_CLK_CTRL2					(0x87)
 */

#define ET7303_REG_CLK_DIV_600K_EN			(1<<7)
#define ET7303_REG_CLK_BCLK2_EN				(1<<6)
#define ET7303_REG_CLK_BCLK2_TG_EN			(1<<5)
#define ET7303_REG_CLK_DIV_300K_EN			(1<<3)
#define ET7303_REG_CLK_CK_300K_EN			(1<<2)
#define ET7303_REG_CLK_BCLK_EN				(1<<1)
#define ET7303_REG_CLK_BCLK_TH_EN			(1<<0)

/*
 * ET7303_REG_CLK_CTRL3					(0x88)
 */

#define ET7303_REG_CLK_OSCMUX_RG_EN			(1<<7)
#define ET7303_REG_CLK_CK_24M_EN			(1<<6)
#define ET7303_REG_CLK_OSC_RG_EN			(1<<5)
#define ET7303_REG_CLK_DIV_2P4M_EN			(1<<4)
#define ET7303_REG_CLK_CK_2P4M_EN			(1<<3)
#define ET7303_REG_CLK_PCLK_EN				(1<<2)
#define ET7303_REG_CLK_PCLK_RG_EN			(1<<1)
#define ET7303_REG_CLK_PCLK_TG_EN			(1<<0)

/*
 * ET7303_REG_BMC_CTRL					(0x90)
 */

#define ET7303_REG_IDLE_EN				(1<<6)
#define ET7303_REG_DISCHARGE_EN				(1<<5)
#define ET7303_REG_BMCIO_LPRPRD				(1<<4)
#define ET7303_REG_BMCIO_LPEN				(1<<3)
#define ET7303_REG_BMCIO_BG_EN				(1<<2)
#define ET7303_REG_VBUS_DET_EN				(1<<1)
#define ET7303_REG_BMCIO_OSC_EN				(1<<0)

/*
 * ET7303_REG_VBUS_MES					(0x91)
 */

#define ET7303_REG_VBUS_MES_EN				(1<<7)
#define ET7303_REG_VBUS_MES_AUTO_MODE			(1<<6)
#define ET7303_REG_VBUS_BAND_10_20V			(1<<5) /* 0 for 5v-10v */
#define ET7303_REG_VBUS_COMP				(1<<4)
#define ET7303_REG_VBUS_CON				(0xf<<0)

/*
 * ET7303_REG_RT_STATUS					(0x97)
 */

#define ET7303_REG_RA_DETACH				(1<<5)
#define ET7303_REG_VBUS_80				(1<<1)

/*
 * ET7303_REG_RT_INT					(0x98)
 */

#define ET7303_REG_INT_RA_DETACH			(1<<5)
#define ET7303_REG_INT_VBUS_COMP_R			(1<<4)
#define ET7303_REG_INT_VBUS_COMP_F			(1<<3)
#define ET7303_REG_INT_WATCHDOG				(1<<2)
#define ET7303_REG_INT_VBUS_80				(1<<1)
#define ET7303_REG_INT_WAKEUP				(1<<0)

/*
 * ET7303_REG_RT_MASK					(0x99)
 */

#define ET7303_REG_M_RA_DETACH				(1<<5)
#define ET7303_REG_M_VBUS_COMP_R			(1<<4)
#define ET7303_REG_M_VBUS_COMP_F			(1<<3)
#define ET7303_REG_M_WATCHDOG				(1<<2)
#define ET7303_REG_M_VBUS_80				(1<<1)
#define ET7303_REG_M_WAKEUP				(1<<0)

/*
 * ET7303_REG_IDLE_CTRL					(0x9B)
 */

#define ET7303_REG_CK_300K_SEL				(1<<7)
#define ET7303_REG_SHIPPING_OFF				(1<<5)
#define ET7303_REG_AUTOIDLE_EN				(1<<3)

/* timeout = (tout*2+1) * 6.4ms */
#define ET7303_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) | (tout & 0x07))

/*
 * ET7303_REG_INTRST_CTRL				(0x9C)
 */

#define ET7303_REG_INTRST_EN				(1<<7)

/* timeout = (tout+1) * 0.2sec */
#define ET7303_REG_INTRST_SET(en, tout) \
	((en << 7) | (tout & 0x03))

/*
 * ET7303_REG_WATCHDOG_CTRL				(0x9D)
 */

#define ET7303_REG_WATCHDOG_EN				(1<<7)

/* timeout = (tout+1) * 0.4sec */
#define ET7303_REG_WATCHDOG_CTRL_SET(en, tout)	\
	((en << 7) | (tout & 0x07))

/*
 * ET7303_REG_I2CRST_CTRL				(0x9E)
 */

#define ET7303_REG_I2CRST_EN				(1<<7)

/* timeout = (tout+1) * 12.5ms */
#define ET7303_REG_I2CRST_SET(en, tout)	\
	((en << 7) | (tout & 0x0f))

#if ENABLE_ET7303_DBG
#define ET7303_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define ET7303_INFO(foramt, args...)
#endif

#endif /* #ifndef __LINUX_ET7303_H */
