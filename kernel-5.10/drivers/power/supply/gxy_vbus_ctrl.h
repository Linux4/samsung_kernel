#ifndef __GXY_VBUS_CTRL__
#define __GXY_VBUS_CTRL__

#define VCHARGE_HOT_TEMP_VOLTAGE  67 /*70degC: 1800mV*(15.18/(15.18+390)) = 67mV*/
#define VCHARGE_WARM_TEMP_VOLTAGE 97 /*60degC  1800mV*(22.22/(22.22+390)) = 97mV*/
#define VCHARGE_DEFAULT_TEMP_VOLTAGE  100/*<60degC*/

#define CHG_ALERT_NORMAL_STATE 0 /*<60degC*/
#define CHG_ALERT_HOT_STATE  1 /*>70degC*/
#define CHG_ALERT_WARM_STATE 2 /*60-70degC*/
#define USB_TERMAL_DETECT_TIMER 30000

enum {
    VBUS_CTRL_LOW,    /* disconnect VBUS to GND */
    VBUS_CTRL_HIGH,    /* connect VBUS to GND */
};

struct vbus_ctrl_dev {
    struct device *dev;
    int state;
    int vbus_ctrl_gpio;
    struct delayed_work vbus_ctrl_state_work;
    struct iio_channel *channel;
};

#endif /*__GXY_VBUS_CTRL__ */
