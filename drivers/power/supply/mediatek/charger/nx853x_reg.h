#ifndef __NX853X_HEADER__
#define __NX853X_HEADER__

/* Register 00h */
#define NX853X_REG_00        0x00
#define NX853X_BAT_OVP_DIS_MASK            0x80
#define NX853X_BAT_OVP_DIS_SHIFT            7
#define NX853X_BAT_OVP_ENABLE        0
#define NX853X_BAT_OVP_DISABLE        1

#define NX853X_BAT_OVP_MASK        0x7F
#define NX853X_BAT_OVP_SHIFT        0
#define NX853X_BAT_OVP_BASE        3491000
#define NX853X_BAT_OVP_LSB            9985
#define NX853X_BAT_OVP_OFFSET        0
#define NX853X_BAT_OVP_MAX            4759000

/* Register 01h */
#define NX853X_REG_01        0x01
#define NX853X_BAT_OVP_ALM_DIS_MASK        0x80
#define NX853X_BAT_OVP_ALM_DIS_SHIFT        7
#define NX853X_BAT_OVP_ALM_ENABLE            0
#define NX853X_BAT_OVP_ALM_DISABLE            1

#define NX853X_BAT_OVP_ALM_MASK            0x7F
#define NX853X_BAT_OVP_ALM_SHIFT            0
#define NX853X_BAT_OVP_ALM_BASE            3500
#define NX853X_BAT_OVP_ALM_LSB        10
#define NX853X_BAT_OVP_ALM_OFFSET            0
#define NX853X_BAT_OVP_ALM_MAX        4770

/* Register 02h */
#define NX853X_REG_02        0x02
#define NX853X_BAT_OCP_DIS_MASK            0x80
#define NX853X_BAT_OCP_DIS_SHIFT            7
#define NX853X_BAT_OCP_ENABLE        0
#define NX853X_BAT_OCP_DISABLE        1

#define NX853X_BAT_OCP_MASK        0x7F
#define NX853X_BAT_OCP_SHIFT        0
#define NX853X_BAT_OCP_BASE        0
#define NX853X_BAT_OCP_LSB            100
#define NX853X_BAT_OCP_OFFSET        200
#define NX853X_BAT_OCP_MIN            2000
#define NX853X_BAT_OCP_MAX            8800

/* Register 03h */
#define NX853X_REG_03        0x03
#define NX853X_BAT_OCP_ALM_DIS_MASK        0x80
#define NX853X_BAT_OCP_ALM_DIS_SHIFT        7
#define NX853X_BAT_OCP_ALM_ENABLE            0
#define NX853X_BAT_OCP_ALM_DISABLE            1

#define NX853X_BAT_OCP_ALM_MASK            0x7F
#define NX853X_BAT_OCP_ALM_SHIFT            0
#define NX853X_BAT_OCP_ALM_BASE            0
#define NX853X_BAT_OCP_ALM_LSB        100
#define NX853X_BAT_OCP_ALM_OFFSET            0
#define NX853X_BAT_OCP_ALM_MAX        12700

/* Register 04h */
#define NX853X_REG_04        0x04
#define NX853X_BAT_UCP_ALM_DIS_MASK        0x80
#define NX853X_BAT_UCP_ALM_DIS_SHIFT        7
#define NX853X_BAT_UCP_ALM_ENABLE            0
#define NX853X_BAT_UCP_ALM_DISABLE            1

#define NX853X_BAT_UCP_ALM_MASK            0x7F
#define NX853X_BAT_UCP_ALM_SHIFT            0
#define NX853X_BAT_UCP_ALM_BASE            0
#define NX853X_BAT_UCP_ALM_LSB        50
#define NX853X_BAT_UCP_ALM_OFFSET            0
#define NX853X_BAT_UCP_ALM_MAX        4500

/* Register 05h */
#define NX853X_REG_05        0x05
#define NX853X_BUS_UCP_DIS_MASK            0x80
#define NX853X_BUS_UCP_DIS_SHIFT            7
#define NX853X_BUS_UCP_ENABLE        0
#define NX853X_BUS_UCP_DISABLE        1

#define NX853X_BUS_UCP_MASK        0x40
#define NX853X_BUS_UCP_SHIFT        6
#define NX853X_BUS_UCP_RESERVED            0
#define NX853X_BUS_UCP_250MA        1

#define NX853X_BUS_RCP_DIS_MASK            0x20
#define NX853X_BUS_RCP_DIS_SHIFT            5
#define NX853X_BUS_RCP_ENABLE        0
#define NX853X_BUS_RCP_DISABLE        1

#define NX853X_BUS_RCP_MASK        0x10
#define NX853X_BUS_RCP_SHIFT        4
#define NX853X_BUS_RCP_300MA        0
#define NX853X_BUS_RCP_RESERVED            1

#define NX853X_CHG_CONFIG_1_MASK            0x08
#define NX853X_CHG_CONFIG_1_SHIFT            3
#define NX853X_CHG_CONFIG_1_TRUE            1
#define NX853X_CHG_CONFIG_1_FALSE            0

#define NX853X_VBUS_ERRHI_DIS_MASK            0x04
#define NX853X_VBUS_ERRHI_DIS_SHIFT        2
#define NX853X_VBUS_ERRHI_DIS_ENABLE        0
#define NX853X_VBUS_ERRHI_DIS_DISABLE        1

/* Register 06h */
#define NX853X_REG_06        0x06
#define NX853X_VBUS_PD_EN_MASK        0x80
#define NX853X_VBUS_PD_EN_SHIFT            7
#define NX853X_VBUS_PD_ENABLE        1
#define NX853X_VBUS_PD_DISABLE        0

#define NX853X_BUS_OVP_MASK        0x7F
#define NX853X_BUS_OVP_SHIFT        0
#define NX853X_SW_CAP_BUS_OVP_BASE            7000
#define NX853X_SW_CAP_BUS_OVP_LSB            50
#define NX853X_SW_CAP_BUS_OVP_OFFSET        0
#define NX853X_SW_CAP_BUS_OVP_MAX            12750
#define NX853X_BYPASS_BUS_OVP_BASE            3500
#define NX853X_BYPASS_BUS_OVP_LSB            25
#define NX853X_BYPASS_BUS_OVP_OFFSET        0
#define NX853X_BYPASS_BUS_OVP_MAX            6500

/* Register 07h */
#define NX853X_REG_07        0x07
#define NX853X_BUS_OVP_ALM_DIS_MASK        0x80
#define NX853X_BUS_OVP_ALM_DIS_SHIFT        7
#define NX853X_BUS_OVP_ALM_DIS_ENABLE        0
#define NX853X_BUS_OVP_ALM_DIS_DISABLE        1

#define NX853X_BUS_OVP_ALM_MASK            0x7F
#define NX853X_BUS_OVP_ALM_SHIFT            0
#define NX853X_SW_CAP_BUS_OVP_ALM_BASE        7000
#define NX853X_SW_CAP_BUS_OVP_ALM_LSB        50
#define NX853X_SW_CAP_BUS_OVP_ALM_OFFSET    0
#define NX853X_SW_CAP_BUS_OVP_ALM_MAX        13350
#define NX853X_BYPASS_BUS_OVP_ALM_BASE        3500
#define NX853X_BYPASS_BUS_OVP_ALM_LSB        25
#define NX853X_BYPASS_BUS_OVP_ALM_OFFSET    0
#define NX853X_BYPASS_BUS_OVP_ALM_MAX        6675

/* Register 08h */
#define NX853X_REG_08        0x08
#define NX853X_BUS_OCP_MASK        0x1F
#define NX853X_BUS_OCP_SHIFT        0
#define NX853X_SW_CAP_BUS_OCP_BASE            1075
#define NX853X_SW_CAP_BUS_OCP_LSB            250
#define NX853X_SW_CAP_BUS_OCP_OFFSET        0
#define NX853X_SW_CAP_BUS_OCP_MAX            4575
#define NX853X_BYPASS_BUS_OCP_BASE            1075
#define NX853X_BYPASS_BUS_OCP_LSB            250
#define NX853X_BYPASS_BUS_OCP_OFFSET        0
#define NX853X_BYPASS_BUS_OCP_MAX            6825

/* Register 09h */
#define NX853X_REG_09        0x09
#define NX853X_BUS_OCP_ALM_DIS_MASK        0x80
#define NX853X_BUS_OCP_ALM_DIS_SHIFT        7
#define NX853X_BUS_OCP_ALM_ENABLE            0
#define NX853X_BUS_OCP_ALM_DISABLE            1

#define NX853X_BUS_OCP_ALM_MASK            0x1F
#define NX853X_BUS_OCP_ALM_SHIFT            0
#define NX853X_BUS_OCP_ALM_BASE            1000
#define NX853X_BUS_OCP_ALM_LSB        250
#define NX853X_BUS_OCP_ALM_OFFSET            0
#define NX853X_BUS_OCP_ALM_MAX        8750

/* Register 0Ah */
#define NX853X_REG_0A        0x0A
#define NX853X_TDIE_FLT_DIS_MASK            0x80
#define NX853X_TDIE_FLT_DIS_SHIFT            7
#define NX853X_TDIE_FLT_ENABLE        0
#define NX853X_TDIE_FLT_DISABLE            1

#define NX853X_TDIE_FLT_MASK        0x60
#define NX853X_TDIE_FLT_SHIFT        5
#define NX853X_TDIE_FLT_BASE        80
#define NX853X_TDIE_FLT_LSB        20
#define NX853X_TDIE_FLT_MAX        140

#define NX853X_TDIE_ALM_DIS_MASK            0x10
#define NX853X_TDIE_ALM_DIS_SHIFT            4
#define NX853X_TDIE_ALM_ENABLE        0
#define NX853X_TDIE_ALM_DISABLE            1

#define NX853X_TSBUS_FLT_DIS_MASK            0x08
#define NX853X_TSBUS_FLT_DIS_SHIFT            3
#define NX853X_TSBUS_FLT_ENABLE            0
#define NX853X_TSBUS_FLT_DISABLE            1

#define NX853X_TSBAT_FLT_DIS_MASK            0x04
#define NX853X_TSBAT_FLT_DIS_SHIFT            2
#define NX853X_TSBAT_FLT_ENABLE            0
#define NX853X_TSBAT_FLT_DISABLE            1

/* Register 0Bh */
#define NX853X_REG_0B        0x0B
#define NX853X_TDIE_ALM_MASK        0xFF
#define NX853X_TDIE_ALM_SHIFT        0
#define NX853X_TDIE_ALM_BASE        250        //25.0℃
#define NX853X_TDIE_ALM_LSB        5        //0.5℃
#define NX853X_TDIE_ALM_OFFSET        0
#define NX853X_TDIE_ALM_MAX        1500    //150.0℃

/* Register 0Ch */
#define NX853X_REG_0C        0x0C
#define NX853X_TSBUS_FLT_MASK        0xFF
#define NX853X_TSBUS_FLT_SHIFT        0
#define NX853X_TSBUS_FLT_BASE        0
#define NX853X_TSBUS_FLT_LSB        19531        //0.19531%
#define NX853X_TSBUS_FLT_OFFSET            0
#define NX853X_TSBUS_FLT_MAX        4980410        //49.8041%

/* Register 0Dh */
#define NX853X_REG_0D        0x0D
#define NX853X_TSBAT_FLT_MASK        0xFF
#define NX853X_TSBAT_FLT_SHIFT        0
#define NX853X_TSBAT_FLT_BASE        0
#define NX853X_TSBAT_FLT_LSB        19531        //0.19531%
#define NX853X_TSBAT_FLT_OFFSET            0
#define NX853X_TSBAT_FLT_MAX        4980410        //49.8041%

/* Register 0Eh */
#define NX853X_REG_0E        0x0E
#define NX853X_AC1_OVP_MASK        0xE0
#define NX853X_AC1_OVP_SHIFT        5
#define NX853X_AC1_OVP_6V5            0
#define NX853X_AC1_OVP_10V5        1
#define NX853X_AC1_OVP_12V            2
#define NX853X_AC1_OVP_14V            3
#define NX853X_AC1_OVP_16V            4
#define NX853X_AC1_OVP_18V            5

#define NX853X_AC2_OVP_MASK        0x1C
#define NX853X_AC2_OVP_SHIFT        2
#define NX853X_AC2_OVP_6V5            0
#define NX853X_AC2_OVP_10V5        1
#define NX853X_AC2_OVP_12V            2
#define NX853X_AC2_OVP_14V            3
#define NX853X_AC2_OVP_16V            4
#define NX853X_AC2_OVP_18V            5

#define NX853X_AC1_PD_EN_MASK        0x02
#define NX853X_AC1_PD_EN_SHIFT        1
#define NX853X_AC1_PD_ENABLE        1
#define NX853X_AC1_PD_DISABLE        0

#define NX853X_AC2_PD_EN_MASK        0x01
#define NX853X_AC2_PD_EN_SHIFT        0
#define NX853X_AC2_PD_ENABLE        1
#define NX853X_AC2_PD_DISABLE        0

/* Register 0Fh */
#define NX853X_REG_0F        0x0F
#define NX853X_REG_RST_MASK        0x80
#define NX853X_REG_RST_SHIFT        7
#define NX853X_REG_RST_ENABLE        1
#define NX853X_REG_RST_DISABLE        0

#define NX853X_EN_HIZ_MASK            0x40
#define NX853X_EN_HIZ_SHIFT        6
#define NX853X_EN_HIZ_ENABLE        1
#define NX853X_EN_HIZ_DISABLE        0

#define NX853X_EN_OTG_MASK            0x20
#define NX853X_EN_OTG_SHIFT        5
#define NX853X_EN_OTG_ENABLE        1
#define NX853X_EN_OTG_DISABLE        0

#define NX853X_CHG_EN_MASK            0x10
#define NX853X_CHG_EN_SHIFT        4
#define NX853X_CHG_EN_ENABLE        1
#define NX853X_CHG_EN_DISABLE        0

#define NX853X_EN_BYPASS_MASK        0x08
#define NX853X_EN_BYPASS_SHIFT        3
#define NX853X_EN_BYPASS_ENABLE            1
#define NX853X_EN_BYPASS_DISABLE            0

#define NX853X_DIS_ACDRV_BOTH_MASK            0x04
#define NX853X_DIS_ACDRV_BOTH_SHIFT        2
#define NX853X_DIS_ACDRV_BOTH_ENABLE        1
#define NX853X_DIS_ACDRV_BOTH_DISABLE        0

#define NX853X_ACDRV1_STAT_MASK            0x02
#define NX853X_ACDRV1_STAT_SHIFT            1
#define NX853X_ACDRV1_STAT_ON        1
#define NX853X_ACDRV1_STAT_OFF        0

#define NX853X_ACDRV2_STAT_MASK            0x01
#define NX853X_ACDRV2_STAT_SHIFT            0
#define NX853X_ACDRV2_STAT_ON        1
#define NX853X_ACDRV2_STAT_OFF        0

/* Register 10h */
#define NX853X_REG_10        0x10
#define NX853X_FSW_SET_MASK        0xE0
#define NX853X_FSW_SET_SHIFT        5
#define NX853X_FSW_SET_187K5        0
#define NX853X_FSW_SET_250K        1
#define NX853X_FSW_SET_300K        2
#define NX853X_FSW_SET_375K        3
#define NX853X_FSW_SET_500K        4
#define NX853X_FSW_SET_750K        5

#define NX853X_WATCHDOG_MASK        0x18
#define NX853X_WATCHDOG_SHIFT        3
#define NX853X_WATCHDOG_0S5        0
#define NX853X_WATCHDOG_1S            1
#define NX853X_WATCHDOG_5S            2
#define NX853X_WATCHDOG_30S        3

#define NX853X_WATCHDOG_DIS_MASK            0x04
#define NX853X_WATCHDOG_DIS_SHIFT            2
#define NX853X_WATCHDOG_DISABLE            1
#define NX853X_WATCHDOG_ENABLE        0

/* Register 11h */
#define NX853X_REG_11        0x11
#define NX853X_RSNS_MASK            0x80
#define NX853X_RSNS_SHIFT            7
#define NX853X_RSNS_2MOHM            0
#define NX853X_RSNS_5MOHM            1


#define NX853X_SS_TIMEOUT_MASK        0x70
#define NX853X_SS_TIMEOUT_SHIFT            4
#define NX853X_SS_TIMEOUT_6MS25            0
#define NX853X_SS_TIMEOUT_12MS5            1
#define NX853X_SS_TIMEOUT_25MS        2
#define NX853X_SS_TIMEOUT_50MS        3
#define NX853X_SS_TIMEOUT_100MS            4
#define NX853X_SS_TIMEOUT_400MS            5
#define NX853X_SS_TIMEOUT_1S5        6
#define NX853X_SS_TIMEOUT_10S        7


#define NX853X_IBUS_UCP_FALL_DG_SEL_MASK            0x0C
#define NX853X_IBUS_UCP_FALL_DG_SEL_SHIFT            2
#define NX853X_IBUS_UCP_FALL_DG_SEL_0MS01            0
#define NX853X_IBUS_UCP_FALL_DG_SEL_5MS            1
#define NX853X_IBUS_UCP_FALL_DG_SEL_50MS            2
#define NX853X_IBUS_UCP_FALL_DG_SEL_150MS            3

/* Register 12h */
#define NX853X_REG_12        0x12
#define NX853X_VOUT_OVP_DIS_MASK            0x80
#define NX853X_VOUT_OVP_DIS_SHIFT            7
#define NX853X_VOUT_OVP_DIS_ENABLE            0
#define NX853X_VOUT_OVP_DIS_DISABLE        1

#define NX853X_VOUT_OVP_MASK        0x60
#define NX853X_VOUT_OVP_SHIFT        5
#define NX853X_VOUT_OVP_4V7        0
#define NX853X_VOUT_OVP_4V8        1
#define NX853X_VOUT_OVP_4V9        2
#define NX853X_VOUT_OVP_5V            3

#define NX853X_FREQ_SHIFT_MASK        0x18
#define NX853X_FREQ_SHIFT_SHIFT            3
#define NX853X_FREQ_SHIFT_NONE        0
#define NX853X_FREQ_SHIFT_10P_HIHGER        0
#define NX853X_FREQ_SHIFT_10P_LOWER        0

#define NX853X_MS_MASK        0x03
#define NX853X_MS_SHIFT            0
#define NX853X_MS_STANDALONE        0
#define NX853X_MS_SECONDARY        1
#define NX853X_MS_PRIMARY            2

/* Register 13h */
#define NX853X_REG_13        0x13
#define NX853X_BAT_OVP_STAT_MASK            0x80
#define NX853X_BAT_OVP_STAT_SHIFT            7
#define NX853X_BAT_OVP_STAT_TRUE            1
#define NX853X_BAT_OVP_STAT_FALSE            0

#define NX853X_BAT_OVP_ALM_STAT_MASK        0x40
#define NX853X_BAT_OVP_ALM_STAT_SHIFT        6
#define NX853X_BAT_OVP_ALM_STAT_TRUE        1
#define NX853X_BAT_OVP_ALM_STAT_FALSE        0

#define NX853X_VOUT_OVP_STAT_MASK            0x20
#define NX853X_VOUT_OVP_STAT_SHIFT            5
#define NX853X_VOUT_OVP_STAT_TRUE            1
#define NX853X_VOUT_OVP_STAT_FALSE            0

#define NX853X_BAT_OCP_STAT_MASK            0x10
#define NX853X_BAT_OCP_STAT_SHIFT            4
#define NX853X_BAT_OCP_STAT_TRUE            1
#define NX853X_BAT_OCP_STAT_FALSE            0

#define NX853X_BAT_OCP_ALM_STAT_MASK        0x08
#define NX853X_BAT_OCP_ALM_STAT_SHIFT        3
#define NX853X_BAT_OCP_ALM_STAT_TRUE        1
#define NX853X_BAT_OCP_ALM_STAT_FALSE        0

#define NX853X_BAT_UCP_ALM_STAT_MASK        0x04
#define NX853X_BAT_UCP_ALM_STAT_SHIFT        2
#define NX853X_BAT_UCP_ALM_STAT_TRUE        1
#define NX853X_BAT_UCP_ALM_STAT_FALSE        0

#define NX853X_BUS_OVP_STAT_MASK            0x02
#define NX853X_BUS_OVP_STAT_SHIFT            1
#define NX853X_BUS_OVP_STAT_TRUE            1
#define NX853X_BUS_OVP_STAT_FALSE            0

#define NX853X_BUS_OVP_ALM_STAT_MASK        0x01
#define NX853X_BUS_OVP_ALM_STAT_SHIFT        0
#define NX853X_BUS_OVP_ALM_STAT_TRUE        1
#define NX853X_BUS_OVP_ALM_STAT_FALSE        0

/* Register 14h */
#define NX853X_REG_14        0x14
#define NX853X_BUS_OCP_STAT_MASK            0x80
#define NX853X_BUS_OCP_STAT_SHIFT            7
#define NX853X_BUS_OCP_STAT_TRUE            1
#define NX853X_BUS_OCP_STAT_FALSE            0

#define NX853X_BUS_OCP_ALM_STAT_MASK        0x40
#define NX853X_BUS_OCP_ALM_STAT_SHIFT        6
#define NX853X_BUS_OCP_ALM_STAT_TRUE        1
#define NX853X_BUS_OCP_ALM_STAT_FALSE        0

#define NX853X_BUS_UCP_STAT_MASK            0x20
#define NX853X_BUS_UCP_STAT_SHIFT            5
#define NX853X_BUS_UCP_STAT_TRUE            1
#define NX853X_BUS_UCP_STAT_FALSE            0

#define NX853X_BUS_RCP_STAT_MASK            0x10
#define NX853X_BUS_RCP_STAT_SHIFT            4
#define NX853X_BUS_RCP_STAT_TRUE            1
#define NX853X_BUS_RCP_STAT_FALSE            0

#define NX853X_CFLY_SHORT_STAT_MASK        0x40
#define NX853X_CFLY_SHORT_STAT_SHIFT        2
#define NX853X_CFLY_SHORT_STAT_TRUE        1
#define NX853X_CFLY_SHORT_STAT_FALSE        0

/* Register 15h */
#define NX853X_REG_15        0x15
#define NX853X_CP_SWITCH_MASK            0x80
#define NX853X_VAC1_OVP_STAT_SHIFT            7
#define NX853X_VAC1_OVP_STAT_TRUE            1
#define NX853X_VAC1_OVP_STAT_FALSE            0

#define NX853X_VBUS_ERRORHI_FLAG_MASK            0x40
#define NX853X_VAC2_OVP_STAT_SHIFT            6
#define NX853X_VAC2_OVP_STAT_TRUE            1
#define NX853X_VAC2_OVP_STAT_FALSE            0

#define NX853X_VBUS_ERRORLO_FLAG_MASK        0x20
#define NX853X_VOUT_PRESENT_STAT_SHIFT        5
#define NX853X_VOUT_PRESENT_STAT_TRUE        1
#define NX853X_VOUT_PRESENT_STAT_FALSE        0

#define NX853X_VAC1_PRESENT_STAT_MASK        0x10
#define NX853X_VAC1_PRESENT_STAT_SHIFT        4
#define NX853X_VAC1_PRESENT_STAT_TRUE        1
#define NX853X_VAC1_PRESENT_STAT_FALSE        0

#define NX853X_VOUT_REV_EN_MASK        0x08
#define NX853X_VAC2_PRESENT_STAT_SHIFT        3
#define NX853X_VAC2_PRESENT_STAT_TRUE        1
#define NX853X_VAC2_PRESENT_STAT_FALSE        0

#define NX853X_VOUT_CHG_EN_MASK        0x04
#define NX853X_VBUS_PRESENT_STAT_SHIFT        2
#define NX853X_VBUS_PRESENT_STAT_TRUE        1
#define NX853X_VBUS_PRESENT_STAT_FALSE        0

#define NX853X_VBUS_INSERT_FLAG_MASK        0x02
#define NX853X_ACRB1_CONFIG_STAT_SHIFT        1
#define NX853X_ACRB1_CONFIG_STAT_TRUE        1
#define NX853X_ACRB1_CONFIG_STAT_FALSE        0

#define NX853X_VOUT_INSERT_FLAG_MASK        0x01
#define NX853X_ACRB2_CONFIG_STAT_SHIFT        0
#define NX853X_ACRB2_CONFIG_STAT_TRUE        1
#define NX853X_ACRB2_CONFIG_STAT_FALSE        0

/* Register 16h */
#define NX853X_REG_16        0x16
#define NX853X_ADC_DONE_STAT_MASK            0x80
#define NX853X_ADC_DONE_STAT_SHIFT            7
#define NX853X_ADC_DONE_STAT_COMPLETE        1
#define NX853X_ADC_DONE_STAT_NOTCOMPLETE    0

#define NX853X_PIN_DIAG_FAIL_FLAG_MASK        0x40
#define NX853X_SS_TIMEOUT_STAT_SHIFT        6
#define NX853X_SS_TIMEOUT_STAT_TRUE        1
#define NX853X_SS_TIMEOUT_STAT_FALSE        0

#define NX853X_IBUS_UCP_RISE_FLAG_MASK        0x20
#define NX853X_TSBUS_TSBAT_ALM_STAT_SHIFT        5
#define NX853X_TSBUS_TSBAT_ALM_STAT_TRUE        1
#define NX853X_TSBUS_TSBAT_ALM_STAT_FALSE        0

#define NX853X_TDIE_ALM_FLAG_MASK            0x10
#define NX853X_TSBUS_FLT_STAT_SHIFT        4
#define NX853X_TSBUS_FLT_STAT_TRUE            1
#define NX853X_TSBUS_FLT_STAT_FALSE        0

#define NX853X_TSBAT_FLT_STAT_MASK            0x08
#define NX853X_TSBAT_FLT_STAT_SHIFT        3
#define NX853X_TSBAT_FLT_STAT_TRUE            1
#define NX853X_TSBAT_FLT_STAT_FALSE        0

#define NX853X_VBAT_REG_ACTIVE_MASK            0x04
#define NX853X_TDIE_FLT_STAT_SHIFT            2
#define NX853X_TDIE_FLT_STAT_TRUE            1
#define NX853X_TDIE_FLT_STAT_FALSE            0

#define NX853X_IBAT_REG_ACTIVE_MASK            0x02
#define NX853X_TDIE_ALM_STAT_SHIFT            1
#define NX853X_TDIE_ALM_STAT_TRUE            1
#define NX853X_TDIE_ALM_STAT_FALSE            0

#define NX853X_IBUS_REG_ACTIVE_MASK        0x01
#define NX853X_WD_STAT_SHIFT        0
#define NX853X_WD_STAT_TIME_EXPIRED        1
#define NX853X_WD_STAT_NORMAL        0

/* Register 17h */
#define NX853X_REG_17        0x17
#define NX853X_POR_FLAG_MASK            0x80
#define NX853X_REGN_GOOD_STAT_SHIFT        7
#define NX853X_REGN_GOOD_STAT_GOOD            1
#define NX853X_REGN_GOOD_STAT_NOTGOOD        0

#define NX853X_SS_FAIL_FLAG_MASK            0x40
#define NX853X_CONV_ACTIVE_STAT_SHIFT            6
#define NX853X_CONV_ACTIVE_STAT_RUNNING        1
#define NX853X_CONV_ACTIVE_STAT_NOTRUNNING        0

#define NX853X_IBUS_UCP_FAIL_FLAG_MASK        0x20
#define NX853X_VBUS_ERRHI_STAT_SHIFT        4
#define NX853X_VBUS_ERRHI_STAT_TRUE        1
#define NX853X_VBUS_ERRHI_STAT_FALSE        0

#define NX853X_IBUS_OCP_FAIL_FLAG_MASK    0x10
#define NX853X_IBAT_OCP_FLAG_MASK    0x08
#define NX853X_VBUS_OVP_FLAG_MASK    0x04
#define NX853X_VOUT_OVP_FLAG_MASK    0x02
#define NX853X_VBAT_OVP_FLAG_MASK    0x01



/* Register 18h */
#define NX853X_REG_18        0x18
#define NX853X_PMID2OUT_UVP_FLAG_MASK    0x20
#define NX853X_PMID2OUT_OVP_FLAG_MASK    0x10
#define NX853X_TSHUT_FLAG_MASK    0x08
#define NX853X_THEM_FLT_FLAG_MASK    0x04
#define NX853X_SS_TIMOUT_FLAG_MASK    0x02
#define NX853X_WD_TIMOUT_FLAG_MASK    0x01


/* Register 19h */
#define NX853X_REG_19        0x19
#define NX853X_BUS_OCP_FLAG_MASK            0x80
#define NX853X_BUS_OCP_FLAG_SHIFT            7
#define NX853X_BUS_OCP_FLAG_TRUE            1
#define NX853X_BUS_OCP_FLAG_FALSE            0

#define NX853X_BUS_OCP_ALM_FLAG_MASK        0x40
#define NX853X_BUS_OCP_ALM_FLAG_SHIFT        6
#define NX853X_BUS_OCP_ALM_FLAG_TRUE        1
#define NX853X_BUS_OCP_ALM_FLAG_FALSE        0

#define NX853X_BUS_UCP_FLAG_MASK            0x20
#define NX853X_BUS_UCP_FLAG_SHIFT            5
#define NX853X_BUS_UCP_FLAG_TRUE            1
#define NX853X_BUS_UCP_FLAG_FALSE            0

#define NX853X_BUS_RCP_FLAG_MASK            0x10
#define NX853X_BUS_RCP_FLAG_SHIFT            4
#define NX853X_BUS_RCP_FLAG_TRUE            1
#define NX853X_BUS_RCP_FLAG_FALSE            0

#define NX853X_CFLY_SHORT_FLAG_MASK        0x04
#define NX853X_CFLY_SHORT_FLAG_SHIFT        2
#define NX853X_CFLY_SHORT_FLAG_TRUE        1
#define NX853X_CFLY_SHORT_FLAG_FALSE        0

/* Register 1Ah */
#define NX853X_REG_1A        0x1A
#define NX853X_VAC1_OVP_FLAG_MASK            0x80
#define NX853X_VAC1_OVP_FLAG_SHIFT            7
#define NX853X_VAC1_OVP_FLAG_TRUE            1
#define NX853X_VAC1_OVP_FLAG_FALSE            0

#define NX853X_VAC2_OVP_FLAG_MASK            0x40
#define NX853X_VAC2_OVP_FLAG_SHIFT            6
#define NX853X_VAC2_OVP_FLAG_TRUE            1
#define NX853X_VAC2_OVP_FLAG_FALSE            0

#define NX853X_VOUT_PRESENT_FLAG_MASK        0x20
#define NX853X_VOUT_PRESENT_FLAG_SHIFT        5
#define NX853X_VOUT_PRESENT_FLAG_TRUE        1
#define NX853X_VOUT_PRESENT_FLAG_FALSE        0

#define NX853X_VAC1_PRESENT_FLAG_MASK        0x10
#define NX853X_VAC1_PRESENT_FLAG_SHIFT        4
#define NX853X_VAC1_PRESENT_FLAG_TRUE        1
#define NX853X_VAC1_PRESENT_FLAG_FALSE        0

#define NX853X_VAC2_PRESENT_FLAG_MASK        0x08
#define NX853X_VAC2_PRESENT_FLAG_SHIFT        3
#define NX853X_VAC2_PRESENT_FLAG_TRUE        1
#define NX853X_VAC2_PRESENT_FLAG_FALSE        0

#define NX853X_VBUS_PRESENT_FLAG_MASK        0x04
#define NX853X_VBUS_PRESENT_FLAG_SHIFT        2
#define NX853X_VBUS_PRESENT_FLAG_TRUE        1
#define NX853X_VBUS_PRESENT_FLAG_FALSE        0

#define NX853X_ACRB1_CONFIG_FLAG_MASK        0x02
#define NX853X_ACRB1_CONFIG_FLAG_SHIFT        1
#define NX853X_ACRB1_CONFIG_FLAG_TRUE        1
#define NX853X_ACRB1_CONFIG_FLAG_FALSE        0

#define NX853X_ACRB2_CONFIG_FLAG_MASK        0x01
#define NX853X_ACRB2_CONFIG_FLAG_SHIFT        0
#define NX853X_ACRB2_CONFIG_FLAG_TRUE        1
#define NX853X_ACRB2_CONFIG_FLAG_FALSE        0

/* Register 1Bh */
#define NX853X_REG_1B        0x1B
#define NX853X_ADC_DONE_FLAG_MASK            0x80
#define NX853X_ADC_DONE_FLAG_SHIFT            7
#define NX853X_ADC_DONE_FLAG_TRUE            1
#define NX853X_ADC_DONE_FLAG_FALSE            0

#define NX853X_SS_TIMEOUT_FLAG_MASK        0x40
#define NX853X_SS_TIMEOUT_FLAG_SHIFT        6
#define NX853X_SS_TIMEOUT_FLAG_TRUE        1
#define NX853X_SS_TIMEOUT_FLAG_FALSE        0

#define NX853X_TSBUS_TSBAT_ALM_FLAG_MASK    0x20
#define NX853X_TSBUS_TSBAT_ALM_FLAG_SHIFT    5
#define NX853X_TSBUS_TSBAT_ALM_FLAG_TRUE    1
#define NX853X_TSBUS_TSBAT_ALM_FLAG_FALSE    0

#define NX853X_TSBUS_FLT_FLAG_MASK            0x10
#define NX853X_TSBUS_FLT_FLAG_SHIFT        4
#define NX853X_TSBUS_FLT_FLAG_TRUE            1
#define NX853X_TSBUS_FLT_FLAG_FALSE        0

#define NX853X_TSBAT_FLT_FLAG_MASK            0x08
#define NX853X_TSBAT_FLT_FLAG_SHIFT        3
#define NX853X_TSBAT_FLT_FLAG_TRUE            1
#define NX853X_TSBAT_FLT_FLAG_FALSE        0

#define NX853X_TDIE_FLT_FLAG_MASK            0x04
#define NX853X_TDIE_FLT_FLAG_SHIFT            2
#define NX853X_TDIE_FLT_FLAG_TRUE            1
#define NX853X_TDIE_FLT_FLAG_FALSE            0

#define NX853X_WD_FLAG_MASK        0x01
#define NX853X_WD_FLAG_SHIFT        0
#define NX853X_WD_FLAG_TRUE        1
#define NX853X_WD_FLAG_FALSE        0


/* Register 1Dh */
#define NX853X_REG_1D        0x1D
#define NX853X_BAT_OVP_INT_MASK            0x80
#define NX853X_BAT_OVP_INT_SHIFT            7
#define NX853X_BAT_OVP_INT_NOTPRODUCE        1
#define NX853X_BAT_OVP_INT_PRODUCE            0

#define NX853X_BAT_OVP_ALM_INT_MASK        0x40
#define NX853X_BAT_OVP_ALM_INT_SHIFT        6
#define NX853X_BAT_OVP_ALM_INT_NOTPRODUCE    1
#define NX853X_BAT_OVP_ALM_INT_PRODUCE        0

#define NX853X_VOUT_OVP_INT_MASK            0x20
#define NX853X_VOUT_OVP_INT_SHIFT            5
#define NX853X_VOUT_OVP_INT_NOTPRODUCE        1
#define NX853X_VOUT_OVP_INT_PRODUCE        0

#define NX853X_BAT_OCP_INT_MASK            0x10
#define NX853X_BAT_OCP_INT_SHIFT            4
#define NX853X_BAT_OCP_INT_NOTPRODUCE        1
#define NX853X_BAT_OCP_INT_PRODUCE            0

#define NX853X_BAT_OCP_ALM_INT_MASK        0x08
#define NX853X_BAT_OCP_ALM_INT_SHIFT        3
#define NX853X_BAT_OCP_ALM_INT_NOTPRODUCE    1
#define NX853X_BAT_OCP_ALM_INT_PRODUCE        0

#define NX853X_BAT_UCP_ALM_INT_MASK        0x04
#define NX853X_BAT_UCP_ALM_INT_SHIFT        2
#define NX853X_BAT_UCP_ALM_INT_NOTPRODUCE    1
#define NX853X_BAT_UCP_ALM_INT_PRODUCE        0

#define NX853X_BUS_OVP_INT_MASK            0x02
#define NX853X_BUS_OVP_INT_SHIFT            1
#define NX853X_BUS_OVP_INT_NOTPRODUCE        1
#define NX853X_BUS_OVP_INT_PRODUCE            0

#define NX853X_BUS_OVP_ALM_INT_MASK        0x01
#define NX853X_BUS_OVP_ALM_INT_SHIFT        0
#define NX853X_BUS_OVP_ALM_INT_NOTPRODUCE    1
#define NX853X_BUS_OVP_ALM_INT_PRODUCE        0

/* Register 1Eh */
#define NX853X_REG_1E        0x1E
#define NX853X_BUS_OCP_INT_MASK            0x80
#define NX853X_BUS_OCP_INT_SHIFT            7
#define NX853X_BUS_OCP_INT_NOTPRODUCE        1
#define NX853X_BUS_OCP_INT_PRODUCE            0

#define NX853X_BUS_OCP_ALM_INT_MASK        0x40
#define NX853X_BUS_OCP_ALM_INT_SHIFT        6
#define NX853X_BUS_OCP_ALM_INT_NOTPRODUCE    1
#define NX853X_BUS_OCP_ALM_INT_PRODUCE        0

#define NX853X_BUS_UCP_INT_MASK            0x20
#define NX853X_BUS_UCP_INT_SHIFT            5
#define NX853X_BUS_UCP_INT_NOTPRODUCE        1
#define NX853X_BUS_UCP_INT_PRODUCE            0

#define NX853X_BUS_RCP_INT_MASK            0x10
#define NX853X_BUS_RCP_INT_SHIFT            4
#define NX853X_BUS_RCP_INT_NOTPRODUCE        1
#define NX853X_BUS_RCP_INT_PRODUCE            0

#define NX853X_CFLY_SHORT_INT_MASK            0x04
#define NX853X_CFLY_SHORT_INT_SHIFT        2
#define NX853X_CFLY_SHORT_INT_NOTPRODUCE    1
#define NX853X_CFLY_SHORT_INT_PRODUCE        0

#define NX853X_CFLY_SHORT_INT_MASK            0x04
#define NX853X_CFLY_SHORT_INT_SHIFT        2
#define NX853X_CFLY_SHORT_INT_NOTPRODUCE    1
#define NX853X_CFLY_SHORT_INT_PRODUCE        0

/* Register 1Fh */
#define NX853X_REG_1F        0x1F
#define NX853X_VAC1_OVP_INT_MASK            0x80
#define NX853X_VAC1_OVP_INT_SHIFT            7
#define NX853X_VAC1_OVP_INT_NOTPRODUCE        1
#define NX853X_VAC1_OVP_INT_PRODUCE        0

#define NX853X_VAC2_OVP_INT_MASK            0x40
#define NX853X_VAC2_OVP_INT_SHIFT            6
#define NX853X_VAC2_OVP_INT_NOTPRODUCE        1
#define NX853X_VAC2_OVP_INT_PRODUCE        0

#define NX853X_VOUT_PRESENT_INT_MASK        0x20
#define NX853X_VOUT_PRESENT_INT_SHIFT        5
#define NX853X_VOUT_PRESENT_INT_NOTPRODUCE    1
#define NX853X_VOUT_PRESENT_INT_PRODUCE    0

#define NX853X_VAC1_PRESENT_INT_MASK        0x10
#define NX853X_VAC1_PRESENT_INT_SHIFT        4
#define NX853X_VAC1_PRESENT_INT_NOTPRODUCE    1
#define NX853X_VAC1_PRESENT_INT_PRODUCE    0

#define NX853X_VAC2_PRESENT_INT_MASK        0x08
#define NX853X_VAC2_PRESENT_INT_SHIFT        3
#define NX853X_VAC2_PRESENT_INT_NOTPRODUCE    1
#define NX853X_VAC2_PRESENT_INT_PRODUCE    0

#define NX853X_VBUS_PRESENT_INT_MASK        0x04
#define NX853X_VBUS_PRESENT_INT_SHIFT        2
#define NX853X_VBUS_PRESENT_INT_NOTPRODUCE    1
#define NX853X_VBUS_PRESENT_INT_PRODUCE    0

#define NX853X_ACRB1_CONFIG_INT_MASK        0x02
#define NX853X_ACRB1_CONFIG_INT_SHIFT        1
#define NX853X_ACRB1_CONFIG_INT_NOTPRODUCE    1
#define NX853X_ACRB1_CONFIG_INT_PRODUCE    0

#define NX853X_ACRB2_CONFIG_INT_MASK        0x01
#define NX853X_ACRB2_CONFIG_INT_SHIFT        0
#define NX853X_ACRB2_CONFIG_INT_NOTPRODUCE    1
#define NX853X_ACRB2_CONFIG_INT_PRODUCE    0

/* Register 20h */
#define NX853X_REG_20        0x20
#define NX853X_ADC_DONE_INT_MASK            0x80
#define NX853X_ADC_DONE_INT_SHIFT            7
#define NX853X_ADC_DONE_INT_NOTPRODUCE        1
#define NX853X_ADC_DONE_INT_PRODUCE        0

#define NX853X_SS_TIMEOUT_INT_MASK            0x40
#define NX853X_SS_TIMEOUT_INT_SHIFT        6
#define NX853X_SS_TIMEOUT_INT_NOTPRODUCE    1
#define NX853X_SS_TIMEOUT_INT_PRODUCE        0

#define NX853X_TSBUS_TSBAT_ALM_INT_MASK        0x20
#define NX853X_TSBUS_TSBAT_ALM_INT_SHIFT        5
#define NX853X_TSBUS_TSBAT_ALM_INT_NOTPRODUCE    1
#define NX853X_TSBUS_TSBAT_ALM_INT_PRODUCE        0

#define NX853X_TSBUS_FLT_INT_MASK            0x10
#define NX853X_TSBUS_FLT_INT_SHIFT            4
#define NX853X_TSBUS_FLT_INT_NOTPRODUCE    1
#define NX853X_TSBUS_FLT_INT_PRODUCE        0

#define NX853X_TSBAT_FLT_INT_MASK            0x08
#define NX853X_TSBAT_FLT_INT_SHIFT            3
#define NX853X_TSBAT_FLT_INT_NOTPRODUCE    1
#define NX853X_TSBAT_FLT_INT_PRODUCE        0

#define NX853X_TDIE_FLT_INT_MASK            0x04
#define NX853X_TDIE_FLT_INT_SHIFT            2
#define NX853X_TDIE_FLT_INT_NOTPRODUCE        1
#define NX853X_TDIE_FLT_INT_PRODUCE        0

#define NX853X_TDIE_ALM_INT_MASK            0x02
#define NX853X_TDIE_ALM_INT_SHIFT            1
#define NX853X_TDIE_ALM_INT_NOTPRODUCE        1
#define NX853X_TDIE_ALM_INT_PRODUCE        0

#define NX853X_WD_INT_MASK            0x01
#define NX853X_WD_INT_SHIFT        0
#define NX853X_WD_INT_NOTPRODUCE            1
#define NX853X_WD_INT_PRODUCE        0

/* Register 21h */
#define NX853X_REG_21        0x21
#define NX853X_REGN_GOOD_INT_MASK            0x80
#define NX853X_REGN_GOOD_INT_SHIFT            7
#define NX853X_REGN_GOOD_INT_NOTPRODUCE    1
#define NX853X_REGN_GOOD_INT_PRODUCE        0

#define NX853X_CONV_ACTIVE_INT_MASK        0x40
#define NX853X_CONV_ACTIVE_INT_SHIFT        6
#define NX853X_CONV_ACTIVE_INT_NOTPRODUCE    1
#define NX853X_CONV_ACTIVE_INT_PRODUCE        0

#define NX853X_VBUS_ERRHI_INT_MASK            0x10
#define NX853X_VBUS_ERRHI_INT_SHIFT        4
#define NX853X_VBUS_ERRHI_INT_NOTPRODUCE    1
#define NX853X_VBUS_ERRHI_INT_PRODUCE        0

/* Register 22h */
#define NX853X_REG_22        0x22
#define NX853X_DEVICE_REV_MASK        0xF0
#define NX853X_DEVICE_REV_SHIFT            4

#define NX853X_DEVICE_ID_MASK        0x0F
#define NX853X_DEVICE_ID_SHIFT        0

/* Register 23h */
#define NX853X_REG_23        0x23
#define NX853X_ADC_EN_MASK            0x80
#define NX853X_ADC_EN_SHIFT        7
#define NX853X_ADC_EN_ENABLE        1
#define NX853X_ADC_EN_DISABLE        0

#define NX853X_ADC_RATE_MASK        0x40
#define NX853X_ADC_RATE_SHIFT        6
#define NX853X_ADC_RATE_ONE_SHOT            1
#define NX853X_ADC_RATE_CONTINUES            0

#define NX853X_ADC_AVG_MASK        0x20
#define NX853X_ADC_AVG_SHIFT        5
#define NX853X_ADC_AVG_RUNNING_VALUE        1
#define NX853X_ADC_AVG_SINGLE_VALUE        0

#define NX853X_ADC_AVG_INIT_MASK            0x10
#define NX853X_ADC_AVG_INIT_SHIFT            4
#define NX853X_ADC_AVG_INIT_EXIST_VALUE    0
#define NX853X_ADC_AVG_INIT_NEW_VALUE        1

#define NX853X_ADC_SAMPLE_MASK        0x0C
#define NX853X_ADC_SAMPLE_SHIFT            2
#define NX853X_ADC_SAMPLE_15BIT            0
#define NX853X_ADC_SAMPLE_14BIT            1
#define NX853X_ADC_SAMPLE_13BIT            2
#define NX853X_ADC_SAMPLE_11BIT            3

#define NX853X_IBUS_ADC_DIS_MASK            0x02
#define NX853X_IBUS_ADC_DIS_SHIFT            1
#define NX853X_IBUS_ADC_DIS_ENABLE            0
#define NX853X_IBUS_ADC_DIS_DISABLE        1

#define NX853X_VBUS_ADC_DIS_MASK            0x02
#define NX853X_VBUS_ADC_DIS_SHIFT            1
#define NX853X_VBUS_ADC_DIS_ENABLE            0
#define NX853X_VBUS_ADC_DIS_DISABLE        1

/* Register 24h */
#define NX853X_REG_24        0x24
#define NX853X_VAC1_ADC_DIS_MASK            0x80
#define NX853X_VAC1_ADC_DIS_SHIFT            7
#define NX853X_VAC1_ADC_DIS_ENABLE            0
#define NX853X_VAC1_ADC_DIS_DISABLE        1

#define NX853X_VAC2_ADC_DIS_MASK            0x40
#define NX853X_VAC2_ADC_DIS_SHIFT            6
#define NX853X_VAC2_ADC_DIS_ENABLE            0
#define NX853X_VAC2_ADC_DIS_DISABLE        1

#define NX853X_VOUT_ADC_DIS_MASK            0x20
#define NX853X_VOUT_ADC_DIS_SHIFT            5
#define NX853X_VOUT_ADC_DIS_ENABLE            0
#define NX853X_VOUT_ADC_DIS_DISABLE        1

#define NX853X_VBAT_ADC_DIS_MASK            0x10
#define NX853X_VBAT_ADC_DIS_SHIFT            4
#define NX853X_VBAT_ADC_DIS_ENABLE            0
#define NX853X_VBAT_ADC_DIS_DISABLE        1

#define NX853X_IBAT_ADC_DIS_MASK            0x08
#define NX853X_IBAT_ADC_DIS_SHIFT            3
#define NX853X_IBAT_ADC_DIS_ENABLE            0
#define NX853X_IBAT_ADC_DIS_DISABLE        1

#define NX853X_TSBUS_ADC_DIS_MASK            0x04
#define NX853X_TSBUS_ADC_DIS_SHIFT            2
#define NX853X_TSBUS_ADC_DIS_ENABLE        0
#define NX853X_TSBUS_ADC_DIS_DISABLE        1

#define NX853X_TSBAT_ADC_DIS_MASK            0x02
#define NX853X_TSBAT_ADC_DIS_SHIFT            1
#define NX853X_TSBAT_ADC_DIS_ENABLE        0
#define NX853X_TSBAT_ADC_DIS_DISABLE        1

#define NX853X_TDIE_ADC_DIS_MASK            0x01
#define NX853X_TDIE_ADC_DIS_SHIFT            0
#define NX853X_TDIE_ADC_DIS_ENABLE            0
#define NX853X_TDIE_ADC_DIS_DISABLE        1

/* Register 25h */
#define NX853X_REG_25        0x25
#define NX853X_IBUS_ADC_MASK        0xFF
#define NX853X_IBUS_ADC_SHIFT        0
#define NX853X_IBUS_ADC_SW_CAP_BASE        0
#define NX853X_IBUS_ADC_SW_CAP_LSB            10000
#define NX853X_IBUS_ADC_SW_CAP_PRECISION    10000
#define NX853X_IBUS_ADC_SW_CAP_OFFSET        0
#define NX853X_IBUS_ADC_BYPASS_BASE        0
#define NX853X_IBUS_ADC_BYPASS_LSB            10279
#define NX853X_IBUS_ADC_BYPASS_PRECISION    10000
#define NX853X_IBUS_ADC_BYPASS_OFFSET        (0)

/* Register 27h */
#define NX853X_REG_27        0x27
#define NX853X_VBUS_ADC_MASK        0xFF
#define NX853X_VBUS_ADC_SHIFT        0
#define NX853X_VBUS_ADC_BASE        0
#define NX853X_VBUS_ADC_LSB        1000
#define NX853X_VBUS_ADC_PRECISION            1000
#define NX853X_VBUS_ADC_OFFSET        (0)

/* Register 29h */
#define NX853X_REG_29        0x29
#define NX853X_VAC1_ADC_MASK        0xFF
#define NX853X_VAC1_ADC_SHIFT        0
#define NX853X_VAC1_ADC_BASE        0
#define NX853X_VAC1_ADC_LSB        10000
#define NX853X_VAC1_ADC_PRECISION            10000
#define NX853X_VAC1_ADC_OFFSET        (0)

/* Register 2Bh */
#define NX853X_REG_2B        0x2B
#define NX853X_VAC2_ADC_MASK        0xFF
#define NX853X_VAC2_ADC_SHIFT        0
#define NX853X_VAC2_ADC_BASE        0
#define NX853X_VAC2_ADC_LSB        10000
#define NX853X_VAC2_ADC_PRECISION            10000
#define NX853X_VAC2_ADC_OFFSET        (0)

/* Register 2Ch */
#define NX853X_REGMAX                   0x2C

#endif /* __NX853X_HEADER__ */
