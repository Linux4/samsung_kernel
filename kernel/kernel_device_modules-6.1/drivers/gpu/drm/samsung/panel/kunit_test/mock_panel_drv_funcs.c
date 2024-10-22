#include "mock_panel_drv_funcs.h"

DEFINE_FUNCTION_MOCK(mock_register_error_cb,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DEFINE_FUNCTION_MOCK(mock_register_cb,
		RETURNS(int), PARAMS(struct panel_device *, int, void *));
DEFINE_FUNCTION_MOCK(mock_get_panel_state,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DEFINE_FUNCTION_MOCK(mock_attach_adapter,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DEFINE_FUNCTION_MOCK(mock_probe,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_sleep_in,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_sleep_out,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_display_on,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_display_off,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_power_on,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_power_off,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_debug_dump,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_doze,
		RETURNS(int), PARAMS(struct panel_device *));
DEFINE_FUNCTION_MOCK(mock_set_display_mode,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DEFINE_FUNCTION_MOCK(mock_get_display_mode,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DEFINE_FUNCTION_MOCK(mock_frame_done,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DEFINE_FUNCTION_MOCK(mock_vsync,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DEFINE_FUNCTION_MOCK(mock_set_mask_layer,
		RETURNS(int), PARAMS(struct panel_device *, void *));

struct panel_drv_funcs mock_panel_drv_funcs = {
	.register_cb = mock_register_cb,
	.register_error_cb = mock_register_error_cb,
	.get_panel_state = mock_get_panel_state,
	.attach_adapter = mock_attach_adapter,
	.probe = mock_probe,
	.sleep_in = mock_sleep_in,
	.sleep_out = mock_sleep_out,
	.display_on = mock_display_on,
	.display_off = mock_display_off,
	.power_on = mock_power_on,
	.power_off = mock_power_off,
	.debug_dump = mock_debug_dump,
#ifdef CONFIG_USDM_PANEL_LPM
	.doze = mock_doze,
	.doze_suspend = mock_doze,
#endif
	.set_display_mode = mock_set_display_mode,
	.get_display_mode = mock_get_display_mode,
	.frame_done = mock_frame_done,
	.vsync = mock_vsync,
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	.set_mask_layer = mock_set_mask_layer,
#endif
};

DEFINE_FUNCTION_MOCK(mock_panel_gpio_is_valid,
		RETURNS(bool), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_get_num,
		RETURNS(int), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_get_name,
		RETURNS(const char *), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_set_value,
		RETURNS(int), PARAMS(struct panel_gpio *, int));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_get_value,
		RETURNS(int), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_get_state,
		RETURNS(int), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_is_irq_valid,
		RETURNS(bool), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_get_irq_num,
		RETURNS(int), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_get_irq_type,
		RETURNS(int), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_enable_irq,
		RETURNS(int), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_disable_irq,
		RETURNS(int), PARAMS(struct panel_gpio *));
DEFINE_FUNCTION_MOCK(mock_panel_gpio_clear_irq_pending_bit,
		RETURNS(int), PARAMS(struct panel_gpio *));

struct panel_gpio_funcs mock_panel_gpio_funcs = {
	.is_valid = mock_panel_gpio_is_valid,
	.get_num = mock_panel_gpio_get_num,
	.get_name = mock_panel_gpio_get_name,
	.set_value = mock_panel_gpio_set_value,
	.get_value = mock_panel_gpio_get_value,
	.get_state = mock_panel_gpio_get_state,

	/* panel gpio irq */
	.is_irq_valid = mock_panel_gpio_is_irq_valid,
	.get_irq_num = mock_panel_gpio_get_irq_num,
	.get_irq_type = mock_panel_gpio_get_irq_type,
	.enable_irq = mock_panel_gpio_enable_irq,
	.disable_irq = mock_panel_gpio_disable_irq,
	.clear_irq_pending_bit = mock_panel_gpio_clear_irq_pending_bit,
};

DEFINE_FUNCTION_MOCK(mock_panel_regulator_init,
		RETURNS(int), PARAMS(struct panel_regulator *));
DEFINE_FUNCTION_MOCK(mock_panel_regulator_enable,
		RETURNS(int), PARAMS(struct panel_regulator *));
DEFINE_FUNCTION_MOCK(mock_panel_regulator_disable,
		RETURNS(int), PARAMS(struct panel_regulator *));
DEFINE_FUNCTION_MOCK(mock_panel_regulator_set_voltage,
		RETURNS(int), PARAMS(struct panel_regulator *, int));
DEFINE_FUNCTION_MOCK(mock_panel_regulator_set_current_limit,
		RETURNS(int), PARAMS(struct panel_regulator *, int));

struct panel_regulator_funcs mock_panel_regulator_funcs = {
	.init = mock_panel_regulator_init,
	.enable = mock_panel_regulator_enable,
	.disable = mock_panel_regulator_disable,
	.set_voltage = mock_panel_regulator_set_voltage,
	.set_current_limit = mock_panel_regulator_set_current_limit,
};
