#ifndef __SERIAL_SPRD_H__
#define __SERIAL_SPRD_H__

/*
 * Spreadtrum Bluetooth host wake up type
 */
typedef enum bt_host_wakeup_type{
	BT_NO_WAKE_UP,
	BT_RX_WAKE_UP,
	BT_RTS_HIGH_WHEN_SLEEP,
}BT_HOST_WAKEUP_TYPE;

/*
 * Spreadtrum platform_data for serial
 */
struct serial_data{
	BT_HOST_WAKEUP_TYPE wakeup_type;
	int clk;
};

#endif
