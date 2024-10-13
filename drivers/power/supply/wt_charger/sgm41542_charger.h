#ifndef __SGM41542_HEADER__
#define __SGM41542_HEADER__

#include <linux/regulator/consumer.h>
#include <linux/extcon-provider.h>
#include <linux/module.h>

#define CONFIG_SGM41542_ENABLE_HVDCP

/* Register 00h */
#define SGM41542_REG_00      		0x00
#define SGM41542_ENHIZ_MASK		    0x80
#define SGM41542_ENHIZ_SHIFT		    7
#define SGM41542_HIZ_ENABLE          1
#define SGM41542_HIZ_DISABLE         0

#define SGM41542_IINLIM_MASK		    0x1F
#define SGM41542_IINLIM_SHIFT		0
#define SGM41542_IINLIM_BASE         100
#define SGM41542_IINLIM_LSB          100

/* Register 01h */
#define SGM41542_REG_01		    	0x01
#define SGM41542_WDT_RST_MASK		0x40
#define SGM41542_WDT_RST_SHIFT		6

#define SGM41542_OTG_CONFIG_MASK     0x20
#define SGM41542_OTG_CONFIG_SHIFT    5
#define SGM41542_OTG_ENABLE          1
#define SGM41542_OTG_DISABLE         0

#define SGM41542_CHG_CONFIG_MASK     0x10
#define SGM41542_CHG_CONFIG_SHIFT    4
#define SGM41542_CHG_ENABLE          1
#define SGM41542_CHG_DISABLE         0

#define SGM41542_SYS_MINV_MASK       0x0E
#define SGM41542_SYS_MINV_SHIFT      1

/* Register 0x02 */
#define SGM41542_REG_02              0x02
#define SGM41542_BOOST_LIM_MASK      0x80
#define SGM41542_BOOST_LIM_SHIFT     7
#define SGM41542_BOOST_LIM_1200MA    1
#define SGM41542_BOOST_LIM_500MA     0
#define SGM41542_BOOST_CURR_1200MA   1200


#define SGM41542_ICHG_MASK           0x3F
#define SGM41542_ICHG_SHIFT          0
#define SGM41542_ICHG_BASE           0
#define SGM41542_ICHG_LSB            60

/* Register 0x03*/
#define SGM41542_REG_03              0x03
#define SGM41542_IPRECHG_MASK        0xF0
#define SGM41542_IPRECHG_SHIFT       4
#define SGM41542_IPRECHG_BASE        60
#define SGM41542_IPRECHG_LSB         60

#define SGM41542_ITERM_MASK          0x0F
#define SGM41542_ITERM_SHIFT         0
#define SGM41542_ITERM_BASE          60
#define SGM41542_ITERM_LSB           60
#define SGM41542_ITERM_MAX           480
#define SGM41542_ITERM_MIN           60

/* Register 0x04*/
#define SGM41542_REG_04              0x04
#define SGM41542_VREG_MASK           0xF8
#define SGM41542_VREG_SHIFT          3
//P86801EA1-1356 gudi.wt,fix current shake when temp 3
#define SGM41542_VREG_BASE           3847
#define SGM41542_VREG_LSB            32

/* Register 0x05*/
#define SGM41542_REG_05              0x05
#define SGM41542_EN_TERM_MASK        0x80
#define SGM41542_EN_TERM_SHIFT       7
#define SGM41542_TERM_ENABLE         1
#define SGM41542_TERM_DISABLE        0

#define SGM41542_WDT_MASK            0x30
#define SGM41542_WDT_SHIFT           4
#define SGM41542_WDT_DISABLE         0
#define SGM41542_WDT_40S             1
#define SGM41542_WDT_80S             2
#define SGM41542_WDT_160S            3
#define SGM41542_WDT_BASE            0
#define SGM41542_WDT_LSB             40

#define SGM41542_EN_TIMER_MASK       0x08
#define SGM41542_EN_TIMER_SHIFT      3

#define SGM41542_CHG_TIMER_ENABLE    1
#define SGM41542_CHG_TIMER_DISABLE   0

#define SGM41542_CHG_TIMER_MASK      0x04
#define SGM41542_CHG_TIMER_SHIFT     2
#define SGM41542_CHG_TIMER_4HOURS    0
#define SGM41542_CHG_TIMER_6HOURS    1

#define SGM41542_JEITA_ISET_MASK     0x01
#define SGM41542_JEITA_ISET_SHIFT    0
#define SGM41542_JEITA_ISET_50PCT    0
#define SGM41542_JEITA_ISET_20PCT    1

/* Register 0x06*/
#define SGM41542_REG_06              0x06
#define SGM41542_OVP_MASK		0xC0
#define SGM41542_OVP_SHIFT        6

#define SGM41542_OVP_5V        0x00
#define SGM41542_OVP_6V5        0x01
#define SGM41542_OVP_9V        0x02
#define SGM41542_OVP_12V        0x03

#define SGM41542_BOOSTV_MASK         0x30
#define SGM41542_BOOSTV_SHIFT        4
#define SGM41542_BOOSTV_BASE         4550
#define SGM41542_BOOSTV_LSB          64

#define SGM41542_BOOSTV_SET_4850MV        0x00
#define SGM41542_BOOSTV_SET_5000MV        0x01
#define SGM41542_BOOSTV_SET_5150MV        0x02
#define SGM41542_BOOSTV_SET_5300MV        0x03

#define SGM41542_VINDPM_MASK         0x0F
#define SGM41542_VINDPM_SHIFT        0
#define SGM41542_VINDPM_BASE         3900
#define SGM41542_VINDPM_LSB          100

/* Register 0x07*/
#define SGM41542_REG_07           0x07
#define SGM41542_IINDET_MASK		0x80
#define SGM41542_IINDET_SHIFT     7
#define SGM41542_IINDET_ENABLE    1

#define SGM41542_BATFET_DIS_MASK     0x20
#define SGM41542_BATFET_DIS_SHIFT    5
#define SGM41542_BATFET_OFF          1

#define SGM41542_BATFET_DLY_MASK     0x08
#define SGM41542_BATFET_DLY_SHIFT    3
#define SGM41542_BATFET_DLY_OFF      0
#define SGM41542_BATFET_DLY_ON       1
#define SGM41542_BATFET_RST_EN_MASK  0x04
#define SGM41542_BATFET_RST_EN_SHIFT 2
#define SGM41542_BATFET_RST_ENABLE	 1
#define SGM41542_BATFET_RST_DISABLE	 0

/* Register 0x08*/
#define SGM41542_REG_08              0x08
#define SGM41542_VBUS_STAT_MASK      0xE0
#define SGM41542_VBUS_STAT_SHIFT     5
#define SGM41542_CHRG_STAT_MASK      0x18
#define SGM41542_CHRG_STAT_SHIFT     3
#define SGM41542_CHRG_STAT_IDLE      0
#define SGM41542_CHRG_STAT_PRECHG    1
#define SGM41542_CHRG_STAT_FASTCHG   2
#define SGM41542_CHRG_STAT_CHGDONE   3

#define SGM41542_PG_STAT_MASK        0x04
#define SGM41542_PG_STAT_SHIFT       2
#define SGM41542_THERM_STAT_MASK       0x02
#define SGM41542_THERM_STAT_SHIFT      1
#define SGM41542_VSYS_STAT_MASK      0x01
#define SGM41542_VSYS_STAT_SHIFT     0

/* Register 0x09*/
#define SGM41542_REG_09              0x09
#define SGM41542_FAULT_WDT_MASK      0x80
#define SGM41542_FAULT_WDT_SHIFT     7
#define SGM41542_FAULT_BOOST_MASK    0x40
#define SGM41542_FAULT_BOOST_SHIFT   6
#define SGM41542_FAULT_CHRG_MASK     0x30
#define SGM41542_FAULT_CHRG_SHIFT    4
#define SGM41542_FAULT_CHRG_NORMAL   0
#define SGM41542_FAULT_CHRG_INPUT    1
#define SGM41542_FAULT_CHRG_THERMAL  2
#define SGM41542_FAULT_CHRG_TIMER    3

#define SGM41542_FAULT_BAT_MASK      0x08
#define SGM41542_FAULT_BAT_SHIFT     3
#define SGM41542_FAULT_NTC_MASK      0x07
#define SGM41542_FAULT_NTC_SHIFT     0

#define SGM41542_FAULT_NTC_GOOD    1
#define SGM41542_FAULT_NTC_WARM      2
#define SGM41542_FAULT_NTC_COOL      3
#define SGM41542_FAULT_NTC_COLD      5
#define SGM41542_FAULT_NTC_HOT       6

#define SGM41542_REG_0A			0x0A
#define SGM41542_VINDPM_STAT_MASK      0x40
#define SGM41542_IINDPM_STAT_MASK      0x20


#define SGM41542_IINDPM_INT_MASK      	0x01
#define SGM41542_IINDPM_INT_SHIFT      	0
#define SGM41542_IINDPM_INT_ENABLE      0
#define SGM41542_IINDPM_INT_DISABLE     1

#define SGM41542_VINDPM_INT_MASK      	0x02
#define SGM41542_VINDPM_INT_SHIFT      	1
#define SGM41542_VINDPM_INT_ENABLE      0
#define SGM41542_VINDPM_INT_DISABLE     1


#define	REG0A_VBUS_GD_MASK			0x80
#define	REG0A_VBUS_GD_SHIFT			7
#define	REG0A_VBUS_GD				1

/* Register 0x0B*/
#define SGM41542_REG_0B              0x0B
#define SGM41542_RESET_MASK          0x80
#define SGM41542_RESET_SHIFT         7
#define SGM41542_RESET               1

#define SGM41542_PN_MASK             0x78
#define SGM41542_PN_SHIFT            3

#define SGM41542_DEV_REV_MASK        0x03
#define SGM41542_DEV_REV_SHIFT       0

/* Register 0x0C*/
#define SGM41542_REG_0C				0x0C

#define SGM41542_DP_VSET_MASK        0x18
#define SGM41542_DP_VSET_SHIFT       3
#define SGM41542_DM_VSET_MASK        0x06
#define SGM41542_DM_VSET_SHIFT       1

#define SGM41542_DM_DP_VSET_HIZ			0
#define SGM41542_DM_DP_VSET_0V       	1
#define SGM41542_DM_DP_VSET_0P6      	2
#define SGM41542_DM_DP_VSET_3P3       	3

#define SGM41542_ENHVDCP_MASK			0x80
#define SGM41542_ENHVDCP_SHIFT			7
#define SGM41542_ENHVDCP_ENABLE			1
#define SGM41542_ENHVDCP_DISABLE		0



#define SGM41542_JEITA_EN_MASK        0x01
#define SGM41542_JEITA_EN_SHIFT       0


/* Register 0x0E*/
#define SGM41542_REG_0E              0x0E
#define SGM41542_INPUT_DET_DONE_MASK		0x80
#define SGM41542_INPUT_DET_DONE_SHIFT	7

/* Register 0x0F*/
#define SGM41542_REG_0F              0x0F
#define SGM41542_VREG_FT_MASK           0xc0
//#define SGM41542_VINDPM_MASK        0x03
//#define SGM41542_VINDPM_SHIFT       0

#define SGM41542_REGISTER_MAX		0x0F

#define SGM41542_OVP_5000mV 5000
#define SGM41542_OVP_6000mV 6000
#define SGM41542_OVP_8000mV 8000
#define SGM41542_OVP_10000mV 10000

#define SGM41542_BOOSTV_4850mV 4850
#define SGM41542_BOOSTV_5000mV 5000
#define SGM41542_BOOSTV_5150mV 5150
#define SGM41542_BOOSTV_5300mV 5300

#define SGM41542_VINDPM_3900mV 3900
#define SGM41542_VINDPM_5400mV 5400
#define SGM41542_VINDPM_5900mV 5900
#define SGM41542_VINDPM_7500mV 7500
#define SGM41542_VINDPM_10500mV 10500

#define DEFAULT_PRECHG_CURR	240
#define DEFAULT_TERM_CURR	240
#define DEFAULT_CHG_CURR	500

#define SGM41542_VBUS_NONSTAND_1000MA 1200
#define SGM41542_VBUS_NONSTAND_1500MA 1600
#define SGM41542_VBUS_NONSTAND_2000MA 2000

#define BC12_FLOAT_CHECK_MAX 1

/*+P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification*/
#define AFC_DETECT_TIME   25
#define QC_DETECT_TIME    10
/*-P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification*/

enum sgm41542_part_no {
	SGM41511 = 0x02,
	SGM41542 = 0x0D,
	SGM41543D = 0x09,
};

enum sgm41542_chrs_status {
	SGM41542_NOT_CHARGING = 0,
	SGM41542_PRE_CHARGE,
	SGM41542_FAST_CHARGING,
	SGM41542_CHARGE_DONE,
};

enum sgm41542_ovp {
	SGM41542_OVP_5500mV,
	SGM41542_OVP_6500mV,
	SGM41542_OVP_10500mV,
	SGM41542_OVP_14000mV,		
};

enum sgm41542_vbus_type {
	SGM41542_VBUS_NONE,
	SGM41542_VBUS_USB_SDP,
	SGM41542_VBUS_USB_CDP,
	SGM41542_VBUS_USB_DCP,
	SGM41542_VBUS_UNKNOWN = 5,
	SGM41542_VBUS_NONSTAND,
	SGM41542_VBUS_OTG,
	SGM41542_VBUS_TYPE_NUM,
	SGM41542_VBUS_USB_HVDCP,
	SGM41542_VBUS_USB_HVDCP3,
	SGM41542_VBUS_USB_AFC,
	SGM41542_VBUS_NONSTAND_1A,
	SGM41542_VBUS_NONSTAND_1P5A,
	SGM41542_VBUS_NONSTAND_2A,
};

static const unsigned int sgm41542_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

struct sgm41542_wakeup_source {
	struct wakeup_source	*source;
	unsigned long		disabled;
};

struct sgm41542_config {
	bool	enable_auto_dpdm;
/*	bool	enable_12v;*/

	int	charge_voltage;
	int	charge_current;

	bool enable_term;
	int	term_current;

	int prechg_current;

	bool enable_ico;
	bool use_absolute_vindpm;
	bool disable_hvdcp;
	bool enable_ir_compensation;
};

struct sgm41542_device {
	struct device *dev;
	struct i2c_client *client;

	struct iio_dev *indio_dev;
	struct iio_chan_spec *iio_chan;
	struct iio_channel	*int_iio_chans;

	struct iio_channel	**wtchg_ext_iio_chans;
	struct iio_channel	**pmic_ext_iio_chans;
	struct iio_channel	**afc_ext_iio_chans;
	struct iio_channel	**qg_batt_ext_iio_chans;

	struct sgm41542_wakeup_source	sgm41542_wake_source;
	struct sgm41542_wakeup_source	sgm41542_apsd_wake_source;

	struct regulator	*dpdm_reg;
	bool			dpdm_enabled;

	struct sgm41542_config  cfg;
	struct delayed_work irq_work;
	struct delayed_work monitor_work;
	struct delayed_work rerun_apsd_work;
	struct delayed_work detect_work;
	struct delayed_work check_adapter_work;

	enum sgm41542_part_no part_no;
	int revision;
	bool is_sgm41542;

	unsigned int status;
	int usb_type;
	int vbus_type;
	int pre_vbus_type;

	//int bc12_done;
	int usb_online;
	int pre_usb_online;
	int apsd_rerun;
	int bc12_cnt;
	int hvdcp_det;
	int vbus_good;
	bool vindpm_stat;
	int init_bc12_detect;
	int bc12_float_check;

	int chg_status;
	bool enabled;

	int chg_en_gpio;
	int irq_gpio;
	int otg_en_gpio;

	int pulse_cnt;

	bool hiz_mode;

	//+ bug, tankaikun@wt, add 20220701, charger bring up
	struct extcon_dev	*extcon;
	int notify_usb_flag;
	//- bug, tankaikun@wt, add 20220701, charger bring up

	//+bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	unsigned long dynamic_adjust_charge_update_timer;
	int final_cc;
	int final_cv;
	int tune_cv;
	int adjust_cnt;
	//-bug816469,tankaikun.wt,add,2023/1/30, IR compensation

	int afc_type;
};

MODULE_LICENSE("GPL v2");

#endif
