#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_gpio.h"
#include "panel_regulator.h"

extern struct panel_drv_funcs panel_drv_funcs;
extern struct panel_drv_funcs mock_panel_drv_funcs;

DECLARE_FUNCTION_MOCK(mock_register_error_cb,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DECLARE_FUNCTION_MOCK(mock_register_cb,
		RETURNS(int), PARAMS(struct panel_device *, int, void *));
DECLARE_FUNCTION_MOCK(mock_get_panel_state,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DECLARE_FUNCTION_MOCK(mock_attach_adapter,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DECLARE_FUNCTION_MOCK(mock_probe,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_sleep_in,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_sleep_out,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_display_on,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_display_off,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_power_on,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_power_off,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_debug_dump,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_doze,
		RETURNS(int), PARAMS(struct panel_device *));
DECLARE_FUNCTION_MOCK(mock_set_display_mode,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DECLARE_FUNCTION_MOCK(mock_get_display_mode,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DECLARE_FUNCTION_MOCK(mock_frame_done,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DECLARE_FUNCTION_MOCK(mock_vsync,
		RETURNS(int), PARAMS(struct panel_device *, void *));
DECLARE_FUNCTION_MOCK(mock_set_mask_layer,
		RETURNS(int), PARAMS(struct panel_device *, void *));

extern struct panel_gpio_funcs panel_gpio_funcs;
extern struct panel_gpio_funcs mock_panel_gpio_funcs;

DECLARE_FUNCTION_MOCK(mock_panel_gpio_is_valid,
		RETURNS(bool), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_get_num,
		RETURNS(int), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_get_name,
		RETURNS(const char *), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_set_value,
		RETURNS(int), PARAMS(struct panel_gpio *, int));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_get_value,
		RETURNS(int), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_get_state,
		RETURNS(int), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_is_irq_valid,
		RETURNS(bool), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_get_irq_num,
		RETURNS(int), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_get_irq_type,
		RETURNS(int), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_enable_irq,
		RETURNS(int), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_disable_irq,
		RETURNS(int), PARAMS(struct panel_gpio *));
DECLARE_FUNCTION_MOCK(mock_panel_gpio_clear_irq_pending_bit,
		RETURNS(int), PARAMS(struct panel_gpio *));

extern struct panel_regulator_funcs panel_regulator_funcs;
extern struct panel_regulator_funcs mock_panel_regulator_funcs;

DECLARE_FUNCTION_MOCK(mock_panel_regulator_init,
		RETURNS(int), PARAMS(struct panel_regulator *));
DECLARE_FUNCTION_MOCK(mock_panel_regulator_enable,
		RETURNS(int), PARAMS(struct panel_regulator *));
DECLARE_FUNCTION_MOCK(mock_panel_regulator_disable,
		RETURNS(int), PARAMS(struct panel_regulator *));
DECLARE_FUNCTION_MOCK(mock_panel_regulator_set_voltage,
		RETURNS(int), PARAMS(struct panel_regulator *, int));
DECLARE_FUNCTION_MOCK(mock_panel_regulator_set_current_limit,
		RETURNS(int), PARAMS(struct panel_regulator *, int));
