
#ifndef __NU2115A_H__
#define __NU2115A_H__

//#define pr_fmt(fmt)    "[Nu2115A] %s: " fmt, __func__

enum {
    NU2115A_STANDALONG = 0,
    NU2115A_MASTER,
    NU2115A_SLAVE,
};

static const char* nu2115a_psy_name[] = {
    [NU2115A_STANDALONG] = "nu2115a-cp-standalone",
    [NU2115A_MASTER] = "sc-cp-master",
    [NU2115A_SLAVE] = "sc-cp-slave",
};

static const char* nu2115a_irq_name[] = {
    [NU2115A_STANDALONG] = "nu2115a-standalone-irq",
    [NU2115A_MASTER] = "nu2115a-master-irq",
    [NU2115A_SLAVE] = "nu2115a-slave-irq",
};

static int nu2115a_mode_data[] = {
    [NU2115A_STANDALONG] = NU2115A_STANDALONG,
    [NU2115A_MASTER] = NU2115A_MASTER,
    [NU2115A_SLAVE] = NU2115A_SLAVE,
};

typedef enum {
    ADC_IBUS,
    ADC_VBUS,
    ADC_VAC1,
    ADC_VAC2,
    ADC_VOUT,
    ADC_VBAT,
    ADC_IBAT,
    ADC_TSBUS,
    ADC_TSBAT,
    ADC_TDIE,
    ADC_MAX_NUM,
}ADC_CH;

#define NU2115A_ROLE_STDALONE       0
#define NU2115A_ROLE_MASTER        1
#define NU2115A_ROLE_SLAVE        2

#define    BAT_OVP_ALARM        BIT(7)
#define BAT_OCP_ALARM        BIT(6)
#define    BUS_OVP_ALARM        BIT(5)
#define    BUS_OCP_ALARM        BIT(4)
#define    BAT_UCP_ALARM        BIT(3)
#define    VBUS_INSERT            BIT(7)
#define VBAT_INSERT            BIT(1)
#define    ADC_DONE            BIT(0)

#define BAT_OVP_FAULT        BIT(7)
#define BAT_OCP_FAULT        BIT(6)
#define BUS_OVP_FAULT        BIT(5)
#define BUS_OCP_FAULT        BIT(4)
#define TBUS_TBAT_ALARM        BIT(3)
#define TS_BAT_FAULT        BIT(2)
#define    TS_BUS_FAULT        BIT(1)
#define    TS_DIE_FAULT        BIT(0)

#define    VBUS_ERROR_H        (0)
#define    VBUS_ERROR_L        (1)

/*below used for comm with other module*/
#define    BAT_OVP_FAULT_SHIFT            0
#define    BAT_OCP_FAULT_SHIFT            1
#define    BUS_OVP_FAULT_SHIFT            2
#define    BUS_OCP_FAULT_SHIFT            3
#define    BAT_THERM_FAULT_SHIFT        4
#define    BUS_THERM_FAULT_SHIFT        5
#define    DIE_THERM_FAULT_SHIFT        6

#define    BAT_OVP_FAULT_MASK            (1 << BAT_OVP_FAULT_SHIFT)
#define    BAT_OCP_FAULT_MASK            (1 << BAT_OCP_FAULT_SHIFT)
#define    BUS_OVP_FAULT_MASK            (1 << BUS_OVP_FAULT_SHIFT)
#define    BUS_OCP_FAULT_MASK            (1 << BUS_OCP_FAULT_SHIFT)
#define    BAT_THERM_FAULT_MASK        (1 << BAT_THERM_FAULT_SHIFT)
#define    BUS_THERM_FAULT_MASK        (1 << BUS_THERM_FAULT_SHIFT)
#define    DIE_THERM_FAULT_MASK        (1 << DIE_THERM_FAULT_SHIFT)

#define    BAT_OVP_ALARM_SHIFT            0
#define    BAT_OCP_ALARM_SHIFT            1
#define    BUS_OVP_ALARM_SHIFT            2
#define    BUS_OCP_ALARM_SHIFT            3
#define    BAT_THERM_ALARM_SHIFT        4
#define    BUS_THERM_ALARM_SHIFT        5
#define    DIE_THERM_ALARM_SHIFT        6
#define BAT_UCP_ALARM_SHIFT            7

#define    BAT_OVP_ALARM_MASK            (1 << BAT_OVP_ALARM_SHIFT)
#define    BAT_OCP_ALARM_MASK            (1 << BAT_OCP_ALARM_SHIFT)
#define    BUS_OVP_ALARM_MASK            (1 << BUS_OVP_ALARM_SHIFT)
#define    BUS_OCP_ALARM_MASK            (1 << BUS_OCP_ALARM_SHIFT)
#define    BAT_THERM_ALARM_MASK        (1 << BAT_THERM_ALARM_SHIFT)
#define    BUS_THERM_ALARM_MASK        (1 << BUS_THERM_ALARM_SHIFT)
#define    DIE_THERM_ALARM_MASK        (1 << DIE_THERM_ALARM_SHIFT)
#define    BAT_UCP_ALARM_MASK            (1 << BAT_UCP_ALARM_SHIFT)

#define VBAT_REG_STATUS_SHIFT        0
#define IBAT_REG_STATUS_SHIFT        1

#define VBAT_REG_STATUS_MASK        (1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK        (1 << VBAT_REG_STATUS_SHIFT)


#define VBUS_ERROR_H_SHIFT      3
#define VBUS_ERROR_L_SHIFT      2
#define VBUS_ERROR_H_MASK       (1 << VBUS_ERROR_H_SHIFT)
#define VBUS_ERROR_L_MASK       (1 << VBUS_ERROR_L_SHIFT)


#define ADC_REG_BASE NU2115A_REG_17

#define nu_err(fmt, ...)                                \
do {                                            \
    if (chip->mode == NU2115A_ROLE_MASTER)                        \
        printk(KERN_ERR "[nu2115a-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);    \
    else if (chip->mode == NU2115A_ROLE_SLAVE)                    \
        printk(KERN_ERR "[nu2115a-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);    \
    else                                        \
        printk(KERN_ERR "[nu2115a-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define nu_info(fmt, ...)                                \
do {                                            \
    if (chip->mode == NU2115A_ROLE_MASTER)                        \
        printk(KERN_INFO "[nu2115a-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);    \
    else if (chip->mode == NU2115A_ROLE_SLAVE)                    \
        printk(KERN_INFO "[nu2115A-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);    \
    else                                        \
        printk(KERN_INFO "[nu2115a-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define nu_dbg(fmt, ...)                                \
do {                                            \
    if (chip->mode == NU2115A_ROLE_MASTER)                        \
        printk(KERN_DEBUG "[nu2115a-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);    \
    else if (chip->mode == NU2115A_ROLE_SLAVE)                    \
        printk(KERN_DEBUG "[nu2115a-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);    \
    else                                        \
        printk(KERN_DEBUG "[nu2115a-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);


struct nu2115a_cfg {
    int bat_ovp_disable;
    int bat_ocp_disable;
    bool bat_ovp_alm_disable;
    bool bat_ocp_alm_disable;

    int bat_ovp_th;
    int bat_ovp_alm_th;
    int bat_ocp_th;
    int bat_ocp_alm_th;

    bool bus_ovp_alm_disable;
    int bus_ocp_disable;
    bool bus_ocp_alm_disable;

    int bus_ovp_th;
    int bus_ovp_alm_th;
    int bus_ocp_th;
    int bus_ocp_alm_th;

    bool bat_ucp_alm_disable;

    int bat_ucp_alm_th;
    int ac_ovp_th;

    bool bat_therm_disable;
    bool bus_therm_disable;
    bool die_therm_disable;

    int bat_therm_th; /*in %*/
    int bus_therm_th; /*in %*/
    int die_therm_th; /*in degC*/

    int sense_r_mohm;
};

struct nu2115a {
    struct device *dev;
    struct charger_device *cp_dev;
    struct i2c_client *client;

    int part_no;
    int revision;

    int mode;

    struct mutex data_lock;
    struct mutex i2c_rw_lock;

    bool batt_present;
    bool vbus_present;

    bool usb_present;
    bool charge_enabled;    /* Register bit status */

    bool acdrv1_enable;
    bool otg_enable;

    int  vbus_error_low;
    int  vbus_error_high;

    /* ADC reading */
    int vbat_volt;
    int vbus_volt;
    int vout_volt;
    int vac_volt;

    int ibat_curr;
    int ibus_curr;

    int bat_temp;
    int bus_temp;
    int die_temp;

    /* alarm/fault status */
    bool bat_ovp_fault;
    bool bat_ocp_fault;
    bool bus_ovp_fault;
    bool bus_ocp_fault;

    bool bat_ovp_alarm;
    bool bat_ocp_alarm;
    bool bus_ovp_alarm;
    bool bus_ocp_alarm;

    bool bat_ucp_alarm;

    bool bat_therm_alarm;
    bool bus_therm_alarm;
    bool die_therm_alarm;

    bool bat_therm_fault;
    bool bus_therm_fault;
    bool die_therm_fault;

    bool therm_shutdown_flag;
    bool therm_shutdown_stat;

    bool vbat_reg;
    bool ibat_reg;

    int  prev_alarm;
    int  prev_fault;

    int chg_ma;
    int chg_mv;

    struct nu2115a_cfg *cfg;
    int irq_gpio;
    int irq;

    int skip_writes;
    int skip_reads;

    struct delayed_work monitor_work;
    struct dentry *debug_root;

    struct charger_device *chg_dev;

    //#ifdef CONFIG_HUAQIN_CP_POLICY_MODULE
    struct charger_device *master_cp_chg;
    struct charger_device *slave_cp_chg;
    //#endif

    const char *chg_dev_name;

    struct power_supply_desc psy_desc;
    struct power_supply_config psy_cfg;
    struct power_supply *psy;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 start*/
    bool otg_tx_mode;
    /*A06 code for AL7160A-607 by shixuanxuan at 20240507 end*/
};

#endif /* __NU2115_H__ */
