// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/sec_panel_notifier_v2.h>

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set KUNIT_EXPECTATIONs and KUNIT_ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct kunit *)`. `struct kunit` is a context object that stores
 * information about the current test.
 */

struct sec_panel_notifier_v2_test {
	struct notifier_block nb;

	int times_called;
	unsigned long notified_event;
	unsigned int notified_display_index;
	unsigned long notified_event_state;
};

#ifdef CONFIG_UML
static int test_notifier_call(struct notifier_block *nb, unsigned long action, void *data)
{
	struct sec_panel_notifier_v2_test *ctx =
		container_of(nb, struct sec_panel_notifier_v2_test, nb);
	struct panel_notifier_event_data *ev_data = data;

	ctx->times_called++;
	ctx->notified_event = action;
	if (ev_data) {
		ctx->notified_display_index = ev_data->display_index;
		ctx->notified_event_state = ev_data->state;
	}

	return 0;
}

/* NOTE: UML TC */
static void sec_panel_notifier_v2_test_bar(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void sec_panel_notifier_v2_test_panel_notify_invalid_event_type_should_be_ignored(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
	};
	int event;

	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(MAX_PANEL_EVENT, &ev_data), -EINVAL);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 0);

	for (event = 0; event < MAX_PANEL_EVENT; event++) {
		KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(event, NULL), -EINVAL);
		KUNIT_EXPECT_EQ(test, ctx->times_called, 0);
	}
}

static void sec_panel_notifier_v2_test_panel_notify_after_unregistration_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
	};

	KUNIT_ASSERT_EQ(test, panel_notifier_unregister(&ctx->nb), 0);
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_BL_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 0);
	KUNIT_ASSERT_EQ(test, panel_notifier_register(&ctx->nb), 0);
}

static void sec_panel_notifier_v2_test_panel_notify_bl_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
		.d.bl = {
			.level = 128,
			.aor = 8,
			.finger_mask_hbm_on = false,
			.acl_status = true,
			.gradual_acl_val = 100,
		},
	};

	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_BL_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_BL_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);
}

static void sec_panel_notifier_v2_test_panel_notify_vrr_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
		.d.dms = {
			.fps = 120,
			.lfd_min_freq = 10,
			.lfd_max_freq = 120,
		},
	};

	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_VRR_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_VRR_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);
}

static void sec_panel_notifier_v2_test_panel_notify_lfd_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
		.d.dms = {
			.fps = 120,
			.lfd_min_freq = 10,
			.lfd_max_freq = 120,
		},
	};

	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_LFD_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_LFD_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);
}

static void sec_panel_notifier_v2_test_panel_notify_copr_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
	};

	ev_data.state = PANEL_EVENT_COPR_STATE_ENABLED;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_COPR_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_COPR_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_COPR_STATE_ENABLED);

	ev_data.state = PANEL_EVENT_COPR_STATE_DISABLED;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_COPR_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 2);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_COPR_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_COPR_STATE_DISABLED);
}

static void sec_panel_notifier_v2_test_panel_notify_panel_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 1U,
	};

	ev_data.state = PANEL_EVENT_PANEL_STATE_OFF;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_PANEL_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_PANEL_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 1U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_PANEL_STATE_OFF);

	ev_data.state = PANEL_EVENT_PANEL_STATE_ON;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_PANEL_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 2);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_PANEL_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 1U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_PANEL_STATE_ON);

	ev_data.state = PANEL_EVENT_PANEL_STATE_LPM;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_PANEL_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 3);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_PANEL_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 1U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_PANEL_STATE_LPM);
}

static void sec_panel_notifier_v2_test_panel_notify_ub_con_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
	};

	ev_data.state = PANEL_EVENT_UB_CON_STATE_CONNECTED;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_UB_CON_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_UB_CON_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_UB_CON_STATE_CONNECTED);

	ev_data.state = PANEL_EVENT_UB_CON_STATE_DISCONNECTED;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_UB_CON_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 2);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_UB_CON_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_UB_CON_STATE_DISCONNECTED);
}

static void sec_panel_notifier_v2_test_panel_notify_test_mode_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
	};

	ev_data.state = PANEL_EVENT_TEST_MODE_STATE_GCT;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_TEST_MODE_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_TEST_MODE_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_TEST_MODE_STATE_GCT);

	ev_data.state = PANEL_EVENT_TEST_MODE_STATE_NONE;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_TEST_MODE_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 2);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_TEST_MODE_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_TEST_MODE_STATE_NONE);
}

static void sec_panel_notifier_v2_test_panel_notify_screen_mode_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
	};

	ev_data.d.screen_mode = 10;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_SCREEN_MODE_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_SCREEN_MODE_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);

	ev_data.d.screen_mode = 20;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_SCREEN_MODE_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 2);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_SCREEN_MODE_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);
}

static void sec_panel_notifier_v2_test_panel_notify_esd_state_changed_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
	};

	ev_data.display_index = 0U;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_ESD_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_ESD_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);

	ev_data.display_index = 1U;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_ESD_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 2);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_ESD_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 1U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);
}

/* input/touchscreen/novatek/nt36523_spi/nt36xxx.c */
struct mock_spi_device {
	struct device dev;
};

struct mock_nvt_ts_data {
	struct mock_spi_device *client;
	int lcd_esd_recovery;
	struct mutex lock;
};
#define input_info(mode, dev, fmt, ...) do {} while (0)
struct mock_nvt_ts_data *ts;
#define NVT_TOUCH_SUPPORT_HW_RST 0

static int nvt_notifier_call(struct notifier_block *nb, unsigned long data, void *v)
{
	struct panel_notifier_event_data *evt_data = v;

	if (data == PANEL_EVENT_ESD_STATE_CHANGED) {
		input_info(true, &ts->client->dev, "%s: LCD ESD INTERRUPT CALLED\n", __func__);
		ts->lcd_esd_recovery = 1; 
	} else if (data == PANEL_EVENT_PANEL_STATE_CHANGED) {
		if (evt_data->state == PANEL_EVENT_PANEL_STATE_ON) {
			if (ts->lcd_esd_recovery == 1) { 
				input_info(true, &ts->client->dev, "%s: LCD ESD LCD ON POST, run fw update\n", __func__);
#if NVT_TOUCH_SUPPORT_HW_RST
				gpio_set_value(ts->reset_gpio, 1);
#endif
				mutex_lock(&ts->lock);
				//nvt_update_firmware(ts->platdata->firmware_name);
				//nvt_check_fw_reset_state(RESET_STATE_REK);

				//nvt_ts_mode_restore(ts);
				mutex_unlock(&ts->lock);
				ts->lcd_esd_recovery = 0; 
			}    
		}    
	}    

	if (data == PANEL_EVENT_UB_CON_STATE_CHANGED) {
		input_info(true, &ts->client->dev, "%s: data = %ld, state = %d\n", __func__, data, evt_data->state);

		if (evt_data->state == PANEL_EVENT_UB_CON_STATE_DISCONNECTED) {
			input_info(true, &ts->client->dev, "%s: UB_CON_DISCONNECTED : disable irq & pin control\n", __func__);
			//nvt_irq_enable(false);
			//pinctrl_configure(ts, false);
		}    
	}    


	/* for test */
	{
		struct sec_panel_notifier_v2_test *ctx =
			container_of(nb, struct sec_panel_notifier_v2_test, nb);

		ctx->times_called++;
		ctx->notified_event = data;
		if (evt_data) {
			ctx->notified_display_index = evt_data->display_index;
			ctx->notified_event_state = evt_data->state;
		}
	}

	return 0;
}

static void sec_panel_notifier_v2_test_nvt_notifier_call_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data ev_data = {
		.display_index = 0U,
		.state = PANEL_EVENT_STATE_NONE,
	};

	/* setup */
	ts = kunit_kzalloc(test, sizeof(*ts), GFP_KERNEL);
	ts->client = kunit_kzalloc(test, sizeof(*ts->client), GFP_KERNEL);
	mutex_init(&ts->lock);

	/* replace notifier callback */
	KUNIT_ASSERT_EQ(test, panel_notifier_unregister(&ctx->nb), 0);
	ctx->nb.notifier_call = nvt_notifier_call;
	KUNIT_ASSERT_EQ(test, panel_notifier_register(&ctx->nb), 0);

	/* notify event */
	ev_data.display_index = 0U;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_ESD_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_ESD_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);

	KUNIT_EXPECT_EQ(test, ts->lcd_esd_recovery, 1);

	ev_data.state = PANEL_EVENT_PANEL_STATE_ON;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_PANEL_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 2);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_PANEL_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_PANEL_STATE_ON);

	KUNIT_EXPECT_EQ(test, ts->lcd_esd_recovery, 0);

	ev_data.state = PANEL_EVENT_UB_CON_STATE_DISCONNECTED;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_UB_CON_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 3);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_UB_CON_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 0U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_UB_CON_STATE_DISCONNECTED);
}

/*
 * sensorhub/lsi/s5e9935/src/ssp_dev.c
 * sensorhub/lsi/s5e9925/src/ssp_dev.c
 */
struct ssp_panel_bl_event_data {
	int brightness;
	int aor_ratio;
	int display_idx;
	int finger_mask_hbm_on;
	int acl_status;
};
struct ssp_panel_screen_mode_data {
	int display_idx;
	int mode;
};
struct mock_ssp_data {
	char uLastAPState;
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	struct ssp_panel_bl_event_data panel_event_data;
	struct ssp_panel_screen_mode_data panel_mode_data;
#endif
	bool ub_disabled;
};
static struct mock_ssp_data *ssp_data_info;

#define send_panel_information(...) do {} while (0)
#define send_ub_connected(...) do {} while (0)
#define ssp_send_cmd(...) do {} while (0)
#define send_screen_mode_information(...) do {} while (0)
#define get_copr_status(...) do {} while (0)
#define MSG2SSP_AP_STATUS_WAKEUP		0xD1
#define MSG2SSP_AP_STATUS_SLEEP		0xD2

static int panel_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{       
	struct panel_notifier_event_data *evt_data = data;

	if (event == PANEL_EVENT_BL_STATE_CHANGED) {
		ssp_data_info->panel_event_data.display_idx = evt_data->display_index;
		ssp_data_info->panel_event_data.brightness = evt_data->d.bl.level;
		ssp_data_info->panel_event_data.aor_ratio = evt_data->d.bl.aor;
		ssp_data_info->panel_event_data.finger_mask_hbm_on = evt_data->d.bl.finger_mask_hbm_on;
		ssp_data_info->panel_event_data.acl_status = evt_data->d.bl.acl_status;
		send_panel_information(&ssp_data_info->panel_event_data);
	} else if (event == PANEL_EVENT_UB_CON_STATE_CHANGED) {
		if (evt_data->state != PANEL_EVENT_UB_CON_STATE_CONNECTED &&
			evt_data->state != PANEL_EVENT_UB_CON_STATE_DISCONNECTED){
			pr_info("[SSP] %s PANEL_EVENT_UB_CON_STATE_CHANGED, event errno(%d)\n", __func__, evt_data->state);
		} else {
			ssp_data_info->ub_disabled = (evt_data->state == PANEL_EVENT_UB_CON_STATE_DISCONNECTED);
			send_ub_connected(evt_data->state == PANEL_EVENT_UB_CON_STATE_DISCONNECTED);

			if (ssp_data_info->ub_disabled) {
				ssp_send_cmd(ssp_data_info, get_copr_status(ssp_data_info), 0);
			}
		}
	} else if (event == PANEL_EVENT_COPR_STATE_CHANGED) {
		pr_info("[SSP] %s PANEL_EVENT_COPR_STATE_CHANGED, event(%d)\n", __func__, evt_data->state);
		if (evt_data->state != PANEL_EVENT_COPR_STATE_ENABLED &&
			evt_data->state != PANEL_EVENT_COPR_STATE_DISABLED) {
			pr_info("[SSP] %s PANEL_EVENT_COPR_STATE_CHANGED, event errno(%d)\n", __func__, evt_data->state);
		} else {
			ssp_data_info->uLastAPState = evt_data->state == 0 ? MSG2SSP_AP_STATUS_SLEEP : MSG2SSP_AP_STATUS_WAKEUP;
			ssp_send_cmd(ssp_data_info, get_copr_status(ssp_data_info), 0);
		}
	} else if (event == PANEL_EVENT_SCREEN_MODE_STATE_CHANGED) {
		ssp_data_info->panel_mode_data.display_idx = evt_data->display_index;
		ssp_data_info->panel_mode_data.mode = evt_data->d.screen_mode;
		send_screen_mode_information(&ssp_data_info->panel_mode_data);
		pr_info("[SSP] %s panel screen mode %d %d", __func__, ssp_data_info->panel_mode_data.display_idx, ssp_data_info->panel_mode_data.mode);
	}

	/* for test */
	{
		struct sec_panel_notifier_v2_test *ctx =
			container_of(self, struct sec_panel_notifier_v2_test, nb);

		ctx->times_called++;
		ctx->notified_event = event;
		if (evt_data) {
			ctx->notified_display_index = evt_data->display_index;
			ctx->notified_event_state = evt_data->state;
		}
	}

	return 0;
}

static void sec_panel_notifier_v2_test_ssp_dev_panel_notifier_callback_success(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;
	struct panel_notifier_event_data bl_evt_data = {
		.display_index = 5U,
		.state = PANEL_EVENT_STATE_NONE,
		.d.bl = {
			.level = 128,
			.aor = 8,
			.finger_mask_hbm_on = false,
			.acl_status = true,
			.gradual_acl_val = 100,
		},
	};
	struct panel_notifier_event_data ev_data = {
		.display_index = 5U,
		.state = PANEL_EVENT_STATE_NONE,
	};

	/* setup */
	ssp_data_info = kunit_kzalloc(test, sizeof(*ssp_data_info), GFP_KERNEL);

	/* replace notifier callback */
	KUNIT_ASSERT_EQ(test, panel_notifier_unregister(&ctx->nb), 0);
	ctx->nb.notifier_call = panel_notifier_callback;
	KUNIT_ASSERT_EQ(test, panel_notifier_register(&ctx->nb), 0);

	/* notify event */
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_BL_STATE_CHANGED, &bl_evt_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 1);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_BL_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 5U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);
	KUNIT_EXPECT_EQ(test, ssp_data_info->panel_event_data.brightness, 128);
	KUNIT_EXPECT_EQ(test, ssp_data_info->panel_event_data.aor_ratio, 8);
	KUNIT_EXPECT_EQ(test, ssp_data_info->panel_event_data.display_idx, 5);
	KUNIT_EXPECT_EQ(test, ssp_data_info->panel_event_data.finger_mask_hbm_on, 0);
	KUNIT_EXPECT_EQ(test, ssp_data_info->panel_event_data.acl_status, 1);

	ev_data.state = PANEL_EVENT_UB_CON_STATE_DISCONNECTED;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_UB_CON_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 2);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_UB_CON_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 5U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_UB_CON_STATE_DISCONNECTED);
	KUNIT_EXPECT_TRUE(test, ssp_data_info->ub_disabled);

	ev_data.state = PANEL_EVENT_UB_CON_STATE_CONNECTED;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_UB_CON_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 3);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_UB_CON_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 5U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_UB_CON_STATE_CONNECTED);
	KUNIT_EXPECT_FALSE(test, ssp_data_info->ub_disabled);

	ev_data.d.screen_mode = 20;
	ev_data.state = PANEL_EVENT_STATE_NONE;
	KUNIT_EXPECT_EQ(test, panel_notifier_call_chain(PANEL_EVENT_SCREEN_MODE_STATE_CHANGED, &ev_data), 0);
	KUNIT_EXPECT_EQ(test, ctx->times_called, 4);
	KUNIT_EXPECT_EQ(test, ctx->notified_event, (unsigned long)PANEL_EVENT_SCREEN_MODE_STATE_CHANGED);
	KUNIT_EXPECT_EQ(test, ctx->notified_display_index, 5U);
	KUNIT_EXPECT_EQ(test, ctx->notified_event_state, (unsigned long)PANEL_EVENT_STATE_NONE);
	KUNIT_EXPECT_EQ(test, ssp_data_info->panel_mode_data.display_idx, 5);
	KUNIT_EXPECT_EQ(test, ssp_data_info->panel_mode_data.mode, 20);
}
#else
/* NOTE: On device TC */
static void sec_panel_notifier_v2_test_foo(struct kunit *test)
{
	/*
	 * This is an KUNIT_EXPECTATION; it is how KUnit tests things. When you want
	 * to test a piece of code, you set some expectations about what the
	 * code should do. KUnit then runs the test and verifies that the code's
	 * behavior matched what was expected.
	 */
	KUNIT_EXPECT_EQ(test, 1, 2); // Obvious failure.
}
#endif

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int sec_panel_notifier_v2_test_init(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx =
		kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	ctx->nb.notifier_call = test_notifier_call;
	KUNIT_ASSERT_EQ(test, panel_notifier_register(&ctx->nb), 0);

	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void sec_panel_notifier_v2_test_exit(struct kunit *test)
{
	struct sec_panel_notifier_v2_test *ctx = test->priv;

	KUNIT_ASSERT_EQ(test, panel_notifier_unregister(&ctx->nb), 0);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case sec_panel_notifier_v2_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#ifdef CONFIG_UML
	/* NOTE: UML TC */
	KUNIT_CASE(sec_panel_notifier_v2_test_bar),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_invalid_event_type_should_be_ignored),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_after_unregistration_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_bl_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_vrr_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_lfd_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_copr_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_panel_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_ub_con_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_test_mode_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_screen_mode_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_panel_notify_esd_state_changed_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_nvt_notifier_call_success),
	KUNIT_CASE(sec_panel_notifier_v2_test_ssp_dev_panel_notifier_callback_success),
#else
	/* NOTE: On device TC */
	KUNIT_CASE(sec_panel_notifier_v2_test_foo),
#endif
	{},
};

/*
 * This defines a suite or grouping of tests.
 *
 * Test cases are defined as belonging to the suite by adding them to
 * `test_cases`.
 *
 * Often it is desirable to run some function which will set up things which
 * will be used by every test; this is accomplished with an `init` function
 * which runs before each test case is invoked. Similarly, an `exit` function
 * may be specified which runs after every test case and can be used to for
 * cleanup. For clarity, running tests in a test module would behave as follows:
 *
 * module.init(test);
 * module.test_case[0](test);
 * module.exit(test);
 * module.init(test);
 * module.test_case[1](test);
 * module.exit(test);
 * ...;
 */
struct kunit_suite sec_panel_notifier_v2_test_module = {
	.name = "sec_panel_notifier_v2_test",
	.init = sec_panel_notifier_v2_test_init,
	.exit = sec_panel_notifier_v2_test_exit,
	.test_cases = sec_panel_notifier_v2_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&sec_panel_notifier_v2_test_module);

MODULE_LICENSE("GPL v2");
