#include "../utility/shub_utility.h"
#include "../comm/shub_comm.h"
#include "../sensor/light.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_file_manager.h"

#include <linux/notifier.h>
#ifdef CONFIG_PANEL_NOTIFY
#include <linux/panel_notify.h>

static bool ub_disconnected;
static bool panel_copr_state;

static struct notifier_block panel_notifier = {
	.notifier_call = panel_notifier_callback,
};

static void init_shub_panel_notifier(void)
{
	shub_infof("");
	panel_notifier_register(&panel_notifier);
}

static void remove_shub_panel_notifier(void)
{
	shub_infof("");
	panel_notifier_unregister(&panel_notifier);
}

static void send_panel_state(void)
{
	int ret;
	bool panel_state = !ub_disconnected && panel_copr_state;

	ret = shub_send_command(CMD_SETVALUE, TYPE_MCU, PANEL_STATE, &panel_state, sizeof(panel_state));
	if (ret < 0)
		shub_errf("comm fail %d", ret);
}

static int panel_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	if (event == PANEL_EVENT_UB_CON_CHANGED) {
		struct panel_ub_con_event_data evt_data = *((struct panel_ub_con_event_data *)data);

		if (evt_data.state != PANEL_EVENT_UB_CON_CONNECTED &&
		    evt_data.state != PANEL_EVENT_UB_CON_DISCONNECTED) {
			shub_errf("PANEL_EVENT_UB_CON_CHANGED, event errno(%d)", evt_data.state);
		} else {
			shub_infof("PANEL_EVENT_UB_CON_CHANGED, event(%d)", evt_data.state);
			ub_disconnected = evt_data.state;
			if (ub_disconnected)
				send_panel_state();
		}
	} else if (event == PANEL_EVENT_COPR_STATE_CHANGED) {
		struct panel_notifier_event_copr_state evt_data = *((struct panel_notifier_event_copr_state *)data);

		if (evt_data.state != PANEL_EVENT_COPR_ENABLED && evt_data.state != PANEL_EVENT_COPR_DISABLED) {
			shub_errf("PANEL_EVENT_COPR_STATE_CHANGED, event errno(%d)", evt_data.state);
		} else {
			shub_infof("PANEL_EVENT_COPR_STATE_CHANGED, event(%d)", evt_data.state);

			panel_copr_state = evt_data.state;
			send_panel_state();
		}
	}
	return 0;
}
#endif

#define SVC_OCTA_DATA_SIZE 22
#define LCD_PANEL_SVC_OCTA "/sys/class/lcd/panel/SVC_OCTA"
#define LCD_PANEL_LCD_TYPE "/sys/class/lcd/panel/lcd_type"
static u8 lcd_type_flag;
static bool lcd_changed;

static void send_panel_lcd_type(void)
{
	char lcd_type_data[256];
	int ret;

	ret = shub_file_read(LCD_PANEL_LCD_TYPE, lcd_type_data, sizeof(lcd_type_data), 0);
	if (ret < 0) {
		shub_errf("file read error %d", ret);
		return;
	}

	/*
	 * lcd_type_data[ret - 2], which type have different transmission ratio.
	 * [0 ~ 2] : 0.7%, [3] : 15%, [4] : 40%
	 */
	if (lcd_type_data[ret - 2] >= '0' && lcd_type_data[ret - 2] <= '2')
		lcd_type_flag = 0;
	else if (lcd_type_data[ret - 2] == '3')
		lcd_type_flag = 1;
	else
		lcd_type_flag = 2;

	ret = shub_send_command(CMD_SETVALUE, TYPE_MCU, PANEL_LCD_TYPE, &lcd_type_flag, sizeof(lcd_type_flag));
	if (ret < 0)
		shub_errf("comm fail %d", ret);
}

bool check_panel_svc_octa_is_chagned(void)
{
	char svc_octa_data[2][SVC_OCTA_DATA_SIZE + 1] = {{0, }, };
	int ret;
	bool changed = false;

	ret = shub_file_read(LCD_PANEL_SVC_OCTA, svc_octa_data[0], SVC_OCTA_DATA_SIZE, 0);
	if (ret != SVC_OCTA_DATA_SIZE) {
		shub_errf("svc octa read fail %d", ret);
		return false;
	}

	ret = shub_file_read(LIGHT_CALIBRATION_FILE_PATH, svc_octa_data[1], SVC_OCTA_DATA_SIZE, LIGHT_CAL_DATA_SIZE);
	if (ret != SVC_OCTA_DATA_SIZE) {
		shub_errf("svc octa read fail %d", ret);
		changed = true;
	} else {
		if (strcmp(svc_octa_data[0], svc_octa_data[1]) != 0) {
			changed = true;
			// this is test log : svc octa data need to delete before submit
			shub_infof("svc octa is changed %s -> %s", svc_octa_data[1], svc_octa_data[0]);
		}
	}

	if (changed) {
		ret = shub_file_write(LIGHT_CALIBRATION_FILE_PATH, svc_octa_data[0], SVC_OCTA_DATA_SIZE,
				      LIGHT_CAL_DATA_SIZE);
		if (ret != SVC_OCTA_DATA_SIZE)
			shub_errf("write faile %d", ret);
	}

	return changed;
}

static int fm_ready_panel(struct notifier_block *this, unsigned long event, void *ptr)
{
	shub_infof("notify event %d", event);

	send_panel_lcd_type();
	lcd_changed = check_panel_svc_octa_is_chagned();

	return NOTIFY_STOP;
}

static struct notifier_block fm_notifier = {
    .notifier_call = fm_ready_panel,
};

void init_shub_panel(void)
{
	shub_infof();
#ifdef CONFIG_PANEL_NOTIFY
	init_shub_panel_notifier();
#endif
	register_file_manager_ready_callback(&fm_notifier);
}

void remove_shub_panel(void)
{
	shub_infof();

#ifdef CONFIG_PANEL_NOTIFY
	remove_shub_panel_notifier();
#endif
}

inline bool is_lcd_chagned(void)
{
	return lcd_changed;
}
