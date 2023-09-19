#include "panel_drv.h"
#include "panel_property.h"
#include "panel_config.h"
#include "panel_debug.h"

int panel_apply_pnobj_config(struct panel_device *panel,
		struct pnobj_config *pnobj_config)
{
	int ret = 0;

	if (!panel || !pnobj_config)
		return -EINVAL;

	switch (pnobj_config->prop_type) {
	case PANEL_PROP_TYPE_RANGE:
		panel_info("prop_name:%s prop_value:%u\n",
				pnobj_config->prop_name, pnobj_config->value);
		ret = panel_property_set_value(&panel->prop_list,
				pnobj_config->prop_name, pnobj_config->value);
		break;
	default:
		panel_err("property type is abnormal(%d)",
				get_pnobj_cmd_type(&pnobj_config->base));
		break;
	}

	return ret;
}
