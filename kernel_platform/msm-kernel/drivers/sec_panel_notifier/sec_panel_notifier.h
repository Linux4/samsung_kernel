enum panel_notifier_event_t {
	PANEL_EVENT_BL_CHANGED,
	PANEL_EVENT_VRR_CHANGED,
	PANEL_EVENT_STATE_CHANGED,
	PANEL_EVENT_LFD_CHANGED,
	PANEL_EVENT_UB_CON_CHANGED,
	PANEL_EVENT_TEST_MODE_CHANGED,
	PANEL_EVENT_SCREEN_MODE_CHANGED,
	PANEL_EVENT_ESD,
};

enum panel_notifier_event_ub_con_state {
	PANEL_EVENT_UB_CON_CONNECTED = 0,
	PANEL_EVENT_UB_CON_DISCONNECTED = 1,
};

struct panel_ub_con_event_data {
	enum panel_notifier_event_ub_con_state state;
	int display_idx;
};

struct panel_bl_event_data {
	int bl_level;
	int aor_data;
	int display_idx;
	int finger_mask_hbm_on;
	int acl_status;
};

struct panel_dms_data {
	int fps;
	int lfd_min_freq;
	int lfd_max_freq;
	int display_idx;
};

enum panel_state {
	PANEL_OFF,
	PANEL_ON,
	PANEL_LPM,
	MAX_PANEL_STATE,
};

enum panel_test_mode_state {
	TEST_NONE,
	TEST_GCT,
	MAX_TEST_MODE,
};

struct panel_state_data {
	int display_idx;
	enum panel_state state;
};

struct panel_test_mode_data {
	int display_idx;
	enum panel_test_mode_state state;
};

struct panel_screen_mode_data {
	int display_idx;
	int mode;
};

int ss_panel_notifier_register(struct notifier_block *nb);
int ss_panel_notifier_unregister(struct notifier_block *nb);
int ss_panel_notifier_call_chain(unsigned long val, void *v);

