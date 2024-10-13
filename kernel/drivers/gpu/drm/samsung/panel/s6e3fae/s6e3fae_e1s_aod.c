#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../aod/aod_drv.h"

bool s6e3fae_aod_is_factory_mode(struct panel_device *panel)
{
	return panel_get_property_value(panel, PANEL_PROPERTY_IS_FACTORY_MODE) ? true : false;
}

int s6e3fae_aod_self_mask_data_check(struct aod_dev_info *aod)
{
	struct panel_device *panel;
	char *dump_str;
	int ret;

	if (!aod)
		return -EINVAL;

	panel = to_panel_device(aod);
	if (!check_aod_seqtbl_exist(aod, SELF_MASK_DATA_CHECK_SEQ))
		return -ENOENT;

	ret = panel_do_aod_seqtbl_by_name_nolock(aod, SELF_MASK_DATA_CHECK_SEQ);
	if (ret < 0) {
		panel_err("failed to run sequence(%s)\n", SELF_MASK_DATA_CHECK_SEQ);
		return -EBUSY;
	}
	dump_str = s6e3fae_aod_is_factory_mode(panel) ?
		"self_mask_factory_checksum" : "self_mask_checksum";

	if (!panel_is_dump_status_success(panel, dump_str)) {
		panel_err("checksum(%s) is invalid\n", dump_str);
		return -ENODATA;
	}
	return 0;
}
