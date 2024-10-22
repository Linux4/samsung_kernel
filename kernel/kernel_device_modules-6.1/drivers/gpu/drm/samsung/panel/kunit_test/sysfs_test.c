// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_bl.h"
#include "util.h"
#include "panel_wrapper.h"

extern struct class *lcd_class;
extern int panel_create_lcd_device(struct panel_device *panel, unsigned int id);
extern int panel_destroy_lcd_device(struct panel_device *panel);
extern int panel_bl_init(struct panel_bl_device *panel_bl);
extern int panel_bl_exit(struct panel_bl_device *panel_bl);
extern int panel_remove_svc_octa(struct panel_device *panel);
extern int panel_create_svc_octa(struct panel_device *panel);
extern int panel_remove_sysfs(struct panel_device *panel);
extern int panel_create_sysfs(struct panel_device *panel);
extern bool panel_check_create_sysfs(struct panel_device_attr *panel_dev_attrs);

DEFINE_FUNCTION_MOCK(test_rw_show, RETURNS(ssize_t),
		PARAMS(struct device *, struct device_attribute *, char *));
DEFINE_FUNCTION_MOCK(test_rw_store, RETURNS(ssize_t),
		PARAMS(struct device *, struct device_attribute *, const char *, size_t));
DEFINE_FUNCTION_MOCK(test_rw_0_show, RETURNS(ssize_t),
		PARAMS(struct device *, struct device_attribute *, char *));
DEFINE_FUNCTION_MOCK(test_rw_0_store, RETURNS(ssize_t),
		PARAMS(struct device *, struct device_attribute *, const char *, size_t));
DEFINE_FUNCTION_MOCK(test_ro_show, RETURNS(ssize_t),
		PARAMS(struct device *, struct device_attribute *, char *));
DEFINE_FUNCTION_MOCK(test_ro_0_show, RETURNS(ssize_t),
		PARAMS(struct device *, struct device_attribute *, char *));

struct panel_device_attr test_dev_attrs[] = {
	__PANEL_ATTR_RW(test_rw, 0664, PA_DEFAULT),
	__PANEL_ATTR_RW(test_rw_0, 0664, PA_DEFAULT),
	__PANEL_ATTR_RO(test_ro, 0444, PA_DEFAULT),
	__PANEL_ATTR_RO(test_ro_0, 0444, PA_DEFAULT),
};

static const char *panel_sysfs_names[] = {
	"lcd_type",
	"window_type",
	"manufacture_code",
	"cell_id",
	"octa_id",
	"SVC_OCTA",
	"SVC_OCTA_CHIPID",
	"SVC_OCTA_DDI_CHIPID",
#ifdef CONFIG_USDM_FACTORY_MST_TEST
	"mst",
#endif
#ifdef CONFIG_USDM_PANEL_POC_FLASH
	"poc",
	"poc_mca",
	"poc_info",
#endif
	"gamma_flash",
	"gamma_check",
#ifdef CONFIG_USDM_FACTORY_SSR_TEST
	"ssr",
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	"ecc",
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	"gct",
#endif
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	"dsc_crc",
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	"grayspot",
#endif
	"irc_mode",
	"dia",
	"color_coordinate",
	"manufacture_date",
	"brightness_table",
	"adaptive_control",
	"siop_enable",
	"temperature",
	"read_mtp",
	"write_mtp",
	"error_flag",
	"mcd_mode",
	"partial_disp",
	"mcd_resistance",
#if defined(CONFIG_USDM_MDNIE)
	"lux",
#endif
#if defined(CONFIG_USDM_PANEL_COPR)
	"copr",
	"read_copr",
	"copr_roi",
	"brt_avg",
#endif
	"alpm",
	"lpm_opr",
	"fingerprint",
#ifdef CONFIG_USDM_PANEL_HMD
	"hmt_bright",
	"hmt_on",
#endif
#ifdef CONFIG_USDM_PANEL_DPUI
	"dpui",
	"dpui_dbg",
	"dpci",
	"dpci_dbg",
#endif
	"poc_onoff",
#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
	"self_mask",
	"self_mask_check",
#endif
#ifdef SUPPORT_NORMAL_SELF_MOVE
	"self_move",
#endif
#ifdef CONFIG_SUPPORT_SPI_IF_SEL
	"spi_if_sel",
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	"ccd_state",
#endif
#ifdef CONFIG_USDM_POC_SPI
	"spi_flash_ctrl",
#endif
	"vrr",
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	"display_mode",
#endif
	"conn_det",
#ifdef CONFIG_USDM_PANEL_MAFPC
	"mafpc_time",
	"mafpc_check",
#endif
	"vrr_lfd",
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	"enable_fd",
#endif
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	"mask_brightness",
	"actual_mask_brightness",
#endif
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
	"brightdot",
#endif
#ifdef CONFIG_USDM_FACTORY_VGLHIGHDOT_TEST
	"vglhighdot",
#endif
	"te_check",
#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
	"vcom_trim",
#endif
};

static const char *panel_factory_sysfs_names[] = {
#ifdef CONFIG_SUPPORT_XTALK_MODE
	"xtalk_mode",
#endif
#ifdef CONFIG_SUPPORT_ISC_DEFECT
	"isc_defect",
#endif
};

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct kunit *)`. `struct kunit` is a context object that stores
 * information about the current test.
 */

/* NOTE: UML TC */
static void sysfs_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void sysfs_test_panel_create_svc_octa_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_create_svc_octa(NULL), -EINVAL);
}

static void sysfs_test_panel_remove_svc_octa_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_remove_svc_octa(NULL), -EINVAL);
}

static void sysfs_test_panel_create_and_remove_svc_octa_success_with_backlight_device(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	KUNIT_ASSERT_EQ(test, panel_bl_init(panel_bl), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel_bl->bd);

	KUNIT_EXPECT_EQ(test, panel_create_svc_octa(panel), 0);
	KUNIT_EXPECT_EQ(test, panel_remove_svc_octa(panel), 0);

	KUNIT_ASSERT_EQ(test, panel_bl_exit(panel_bl), 0);
	KUNIT_ASSERT_TRUE(test, !panel_bl->bd);
}

static void sysfs_test_panel_create_and_remove_svc_octa_success_without_backlight_device(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	KUNIT_ASSERT_TRUE(test, !panel_bl->bd);

	KUNIT_EXPECT_EQ(test, panel_create_svc_octa(panel), 0);
	KUNIT_EXPECT_EQ(test, panel_remove_svc_octa(panel), 0);
}

static void sysfs_test_panel_create_sysfs_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_create_sysfs(NULL), -EINVAL);
}

static void sysfs_test_panel_remove_sysfs_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_remove_sysfs(NULL), -EINVAL);
}

static void sysfs_test_panel_create_and_remove_sysfs_success(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	char message[256];
	int i;

	/* sysfs must be not exist */
	for (i = 0; i < ARRAY_SIZE(panel_sysfs_names); i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected attr_exist_for_each(\"%s\") == false, but\n", panel_sysfs_names[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"attr_exist_for_each(lcd_class, \"%s\") == %ld\n",
				panel_sysfs_names[i], attr_exist_for_each(lcd_class, panel_sysfs_names[i]));
		KUNIT_EXPECT_TRUE_MSG(test, (attr_exist_for_each(lcd_class, panel_sysfs_names[i]) == false), message);
	}

	KUNIT_EXPECT_EQ(test, panel_create_sysfs(panel), 0);

	/* sysfs must be created */
	for (i = 0; i < ARRAY_SIZE(panel_sysfs_names); i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected attr_exist_for_each(\"%s\") == true, but\n", panel_sysfs_names[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"attr_exist_for_each(lcd_class, \"%s\") == %ld\n",
				panel_sysfs_names[i], attr_exist_for_each(lcd_class, panel_sysfs_names[i]));
		KUNIT_EXPECT_TRUE_MSG(test, (attr_exist_for_each(lcd_class, panel_sysfs_names[i]) == true), message);
	}

	/* factory sysfs must be not exist */
	for (i = 0; i < ARRAY_SIZE(panel_factory_sysfs_names); i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected attr_exist_for_each(\"%s\") == false, but\n", panel_factory_sysfs_names[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"attr_exist_for_each(lcd_class, \"%s\") == %ld\n",
				panel_factory_sysfs_names[i], attr_exist_for_each(lcd_class, panel_factory_sysfs_names[i]));
		KUNIT_EXPECT_TRUE_MSG(test, (attr_exist_for_each(lcd_class, panel_factory_sysfs_names[i]) == false), message);
	}

	KUNIT_EXPECT_EQ(test, panel_remove_sysfs(panel), 0);

	/* sysfs must be removed exist */
	for (i = 0; i < ARRAY_SIZE(panel_sysfs_names); i++) {
		u32 len = snprintf(message, sizeof(message),
				"Expected attr_exist_for_each(\"%s\") == false, but\n", panel_sysfs_names[i]);
		len += snprintf(message + len, sizeof(message) - len,
				"attr_exist_for_each(lcd_class, \"%s\") == %ld\n",
				panel_sysfs_names[i], attr_exist_for_each(lcd_class, panel_sysfs_names[i]));
		KUNIT_EXPECT_TRUE_MSG(test, (attr_exist_for_each(lcd_class, panel_sysfs_names[i]) == false), message);
	}
}

static void sysfs_test_attr_show_for_each_fail_with_invalid_arg(struct kunit *test)
{
	struct class *test_class;
	char *buffer = "test string";

	KUNIT_EXPECT_EQ(test, attr_show_for_each(NULL, "test_node", buffer), (ssize_t)-EINVAL);

	test_class = class_create_wrapper(THIS_MODULE, "test_class");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_class);

	KUNIT_EXPECT_EQ(test, attr_show_for_each(test_class, "test_node", NULL), (ssize_t)-EINVAL);

	class_destroy(test_class);
}

static void sysfs_test_attr_show_for_each_success(struct kunit *test)
{
	struct class *test_class;
	struct device *test_device;
	char *buffer = "test string";
	struct mock_expectation *expectation_rw, *expectation_ro;
	int i;

	/* SetUp */
	test_class = class_create_wrapper(THIS_MODULE, "test_class");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_class);
	test_device = device_create(test_class, NULL, 0, NULL, "test_device");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_device);
	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		KUNIT_ASSERT_TRUE(test, !device_create_file(test_device, &test_dev_attrs[i].dev_attr));

	expectation_rw = Times(1, KUNIT_EXPECT_CALL(test_rw_show(ptr_eq(test, test_device), any(test), ptr_eq(test, buffer))));
	expectation_rw->action = int_return(test, 0);
	expectation_ro = Times(1, KUNIT_EXPECT_CALL(test_ro_show(ptr_eq(test, test_device), any(test), ptr_eq(test, buffer))));
	expectation_ro->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, attr_show_for_each(test_class, "test_rw", buffer), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, attr_show_for_each(test_class, "test_ro", buffer), (ssize_t)0);

	/* TearDown */
	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		device_remove_file(test_device, &test_dev_attrs[i].dev_attr);
	device_unregister(test_device);
	class_destroy(test_class);
}

static void sysfs_test_attr_store_for_each_fail_with_invalid_arg(struct kunit *test)
{
	struct class *test_class;
	char *buffer = "test string";
	size_t size = strlen(buffer) + 1;

	KUNIT_EXPECT_EQ(test, attr_store_for_each(NULL, "test_node", buffer, size), (ssize_t)-EINVAL);

	test_class = class_create_wrapper(THIS_MODULE, "test_class");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_class);

	KUNIT_EXPECT_EQ(test, attr_store_for_each(test_class, "test_node", NULL, 0), (ssize_t)-EINVAL);

	class_destroy(test_class);
}

static void sysfs_test_attr_store_for_each_success(struct kunit *test)
{
	struct class *test_class;
	struct device *test_device;
	char *buffer = "test string";
	size_t size = strlen(buffer) + 1;
	struct mock_expectation *expectation_rw;
	int i;

	/* SetUp */
	test_class = class_create_wrapper(THIS_MODULE, "test_class");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_class);
	test_device = device_create(test_class, NULL, 0, NULL, "test_device");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_device);
	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		KUNIT_ASSERT_TRUE(test, !device_create_file(test_device, &test_dev_attrs[i].dev_attr));

	expectation_rw = Times(1, KUNIT_EXPECT_CALL(test_rw_store(ptr_eq(test, test_device), any(test), ptr_eq(test, buffer), int_eq(test, size))));
	expectation_rw->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, attr_store_for_each(test_class, "test_rw", buffer, size), (ssize_t)0);

	/* TearDown */
	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		device_remove_file(test_device, &test_dev_attrs[i].dev_attr);
	device_unregister(test_device);
	class_destroy(test_class);
}

static void sysfs_test_attr_exist_for_each_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, attr_exist_for_each(NULL, "test_node"), (ssize_t)-EINVAL);
}

static void sysfs_test_attr_exist_for_each_success(struct kunit *test)
{
	struct class *test_class;
	struct device *test_device;
	int i;

	/* SetUp */
	test_class = class_create_wrapper(THIS_MODULE, "test_class");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_class);
	test_device = device_create(test_class, NULL, 0, NULL, "test_device");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_device);
	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		KUNIT_ASSERT_TRUE(test, !device_create_file(test_device, &test_dev_attrs[i].dev_attr));

	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(test_class, "test_rw"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(test_class, "test_rw_0"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(test_class, "test_ro"));
	KUNIT_EXPECT_TRUE(test, attr_exist_for_each(test_class, "test_ro_0"));

	KUNIT_EXPECT_FALSE(test, attr_exist_for_each(test_class, "test_not_exist"));

	/* TearDown */
	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		device_remove_file(test_device, &test_dev_attrs[i].dev_attr);
	device_unregister(test_device);
	class_destroy(test_class);
}

static void sysfs_test_panel_check_create_sysfs_success(struct kunit *test)
{
	int i;

	/* We assume that test is not Factory build */

	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		KUNIT_EXPECT_TRUE(test, panel_check_create_sysfs(&test_dev_attrs[i]));

	/* change flags PA_DEFAULT => PA_FACTORY */
	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		test_dev_attrs[i].flags = PA_FACTORY;

	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		KUNIT_EXPECT_FALSE(test, panel_check_create_sysfs(&test_dev_attrs[i]));

	/* Recover PA_FACTORY => PA_DEFAULT */
	for (i = 0; i < ARRAY_SIZE(test_dev_attrs); i++)
		test_dev_attrs[i].flags = PA_DEFAULT;
}

static void sysfs_test_store_param_test(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	KUNIT_ASSERT_EQ(test, panel_create_sysfs(panel), 0);
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "_enable_node", "1", 1), (ssize_t)0);

#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "brightdot", "1", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "brightdot", "0", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE), 0);
#endif
#ifdef CONFIG_USDM_FACTORY_VGLHIGHDOT_TEST
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "vglhighdot", "1", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_VGLHIGHDOT), 1);
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "vglhighdot", "0", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_VGLHIGHDOT), 0);
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "xtalk_mode", "1", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_XTALK_MODE), XTALK_ON);
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "xtalk_mode", "0", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_XTALK_MODE), XTALK_OFF);
#endif

	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "irc_mode", "1", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_IRC_MODE), IRC_MODE_FLAT_GAMMA);
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "irc_mode", "0", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_IRC_MODE), IRC_MODE_MODERATO);

	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "dia", "1", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_DIA_MODE), 1);
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "dia", "0", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_DIA_MODE), 0);

#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "enable_fd", "1", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_FD_ENABLED), 1);
	KUNIT_EXPECT_EQ(test, attr_store_for_each(lcd_class, "enable_fd", "0", 1), (ssize_t)0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, PANEL_PROPERTY_FD_ENABLED), 0);
#endif

	KUNIT_EXPECT_EQ(test, panel_remove_sysfs(panel), 0);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int sysfs_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};

	struct panel_prop_enum_item wait_tx_done_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_AUTO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_ON),
	};

	struct panel_prop_enum_item separate_tx_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_ON),
	};

	struct panel_prop_enum_item vrr_mode_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_NORMAL_MODE),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_HS_MODE),
	};

#ifdef CONFIG_SUPPORT_XTALK_MODE
	struct panel_prop_enum_item xtalk_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(XTALK_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(XTALK_ON),
	};
#endif

	struct panel_prop_enum_item irc_mode_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(IRC_MODE_MODERATO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(IRC_MODE_FLAT_GAMMA),
	};

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	struct panel_prop_enum_item gct_vddm_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VDDM_ORIG),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VDDM_LV),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VDDM_HV),
	};

	struct panel_prop_enum_item gct_pattern_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GCT_PATTERN_NONE),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GCT_PATTERN_1),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(GCT_PATTERN_2),
	};
#endif

	struct panel_prop_list prop_array[] = {
		/* enum property */
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_STATE,
				PANEL_STATE_OFF, panel_state_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_WAIT_TX_DONE,
				WAIT_TX_DONE_AUTO, wait_tx_done_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_SEPARATE_TX,
				SEPARATE_TX_OFF, separate_tx_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items),
#ifdef CONFIG_SUPPORT_XTALK_MODE
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_XTALK_MODE,
				XTALK_OFF, xtalk_enum_items),
#endif
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_IRC_MODE,
				IRC_MODE_MODERATO, irc_mode_enum_items),
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_GCT_VDDM,
				VDDM_ORIG, gct_vddm_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_GCT_PATTERN,
				GCT_PATTERN_NONE, gct_pattern_enum_items),
#endif
		/* range property */
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_NUMBER_0,
				0, 0, 0),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_1,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_2,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_3,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE,
				60, 0, 120),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE,
				60, 0, 120),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_RESOLUTION_CHANGED,
				false, false, true),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_DSI_FREQ,
				0, 0, 1000000000),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_OSC_FREQ,
				0, 0, 1000000000),
#ifdef CONFIG_USDM_FACTORY
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_IS_FACTORY_MODE,
				1, 0, 1),
#else
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_IS_FACTORY_MODE,
				0, 0, 1),
#endif
		/* DISPLAY TEST */
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE,
				0, 0, 1),
#endif
#ifdef CONFIG_USDM_FACTORY_VGLHIGHDOT_TEST
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_VGLHIGHDOT,
				0, 0, 2),
#endif
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_DIA_MODE,
				1, 0, 1),
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_FD_ENABLED,
				0, 0, 1),
#endif
	};

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel_mutex_init(panel, &panel->io_lock);
	panel_mutex_init(panel, &panel->op_lock);
	panel_mutex_init(panel, &panel->panel_bl.lock);

	panel_create_lcd_device(panel, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->lcd_dev);

	INIT_LIST_HEAD(&panel->maptbl_list);
	INIT_LIST_HEAD(&panel->seq_list);
	INIT_LIST_HEAD(&panel->rdi_list);
	INIT_LIST_HEAD(&panel->res_list);
	INIT_LIST_HEAD(&panel->dump_list);
	INIT_LIST_HEAD(&panel->key_list);
	INIT_LIST_HEAD(&panel->pkt_list);
	INIT_LIST_HEAD(&panel->dly_list);
	INIT_LIST_HEAD(&panel->cond_list);
	INIT_LIST_HEAD(&panel->pwrctrl_list);
	INIT_LIST_HEAD(&panel->cfg_list);
	INIT_LIST_HEAD(&panel->func_list);
	INIT_LIST_HEAD(&panel->gpio_list);
	INIT_LIST_HEAD(&panel->regulator_list);
	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);
	INIT_LIST_HEAD(&panel->prop_list);

	KUNIT_ASSERT_TRUE(test,
			!panel_add_property_from_array(panel,
			prop_array, ARRAY_SIZE(prop_array)));

	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void sysfs_test_exit(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);
	panel_delete_property_all(panel);
	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));

	panel_destroy_lcd_device(panel);
	KUNIT_ASSERT_TRUE(test, !panel->lcd_dev);

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case sysfs_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(sysfs_test_test),

	KUNIT_CASE(sysfs_test_panel_create_svc_octa_fail_with_invalid_arg),
	KUNIT_CASE(sysfs_test_panel_remove_svc_octa_fail_with_invalid_arg),
	KUNIT_CASE(sysfs_test_panel_create_and_remove_svc_octa_success_with_backlight_device),
	KUNIT_CASE(sysfs_test_panel_create_and_remove_svc_octa_success_without_backlight_device),

	KUNIT_CASE(sysfs_test_panel_create_sysfs_fail_with_invalid_arg),
	KUNIT_CASE(sysfs_test_panel_remove_sysfs_fail_with_invalid_arg),
	KUNIT_CASE(sysfs_test_panel_create_and_remove_sysfs_success),

	KUNIT_CASE(sysfs_test_attr_show_for_each_fail_with_invalid_arg),
	KUNIT_CASE(sysfs_test_attr_show_for_each_success),

	KUNIT_CASE(sysfs_test_attr_store_for_each_fail_with_invalid_arg),
	KUNIT_CASE(sysfs_test_attr_store_for_each_success),

	KUNIT_CASE(sysfs_test_attr_exist_for_each_fail_with_invalid_arg),
	KUNIT_CASE(sysfs_test_attr_exist_for_each_success),

	KUNIT_CASE(sysfs_test_panel_check_create_sysfs_success),

	KUNIT_CASE(sysfs_test_store_param_test),
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
static struct kunit_suite sysfs_test_module = {
	.name = "sysfs_test",
	.init = sysfs_test_init,
	.exit = sysfs_test_exit,
	.test_cases = sysfs_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&sysfs_test_module);

MODULE_LICENSE("GPL v2");
