/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sprd_adf_hw_information.h"
#include "sprd_adf_bridge.h"

static struct sprd_display_config_entry *config_entery = ((void *)0);

/**
 * sprd_adf_get_device_capability - get device capability
 *
 * @dev_id: sprd adf device unique id
 *
 * sprd_adf_get_device_capability() according to the device
 * id then get the device capability
 */
struct sprd_adf_device_capability *sprd_adf_get_device_capability(
				unsigned int dev_id)
{
	struct sprd_adf_device_capability *capability = NULL;

	ADF_DEBUG_GENERAL("entern\n");
	if (!config_entery) {
		ADF_DEBUG_WARN("config entery is illegal\n");
		goto out;
	}

	if (config_entery->get_device_capability)
		capability = config_entery->
				get_device_capability(dev_id);

out:
	return capability;
}

/**
 * sprd_adf_get_interface_capability - get interface capability
 *
 * @dev_id: sprd adf device unique id
 * @intf_id: sprd adf interface unique id
 *
 * sprd_adf_get_interface_capability() according to the device
 * id adn interface id then get the interface capability
 */
struct sprd_adf_interface_capability
	*sprd_adf_get_interface_capability(unsigned int dev_id,
		unsigned int intf_id)
{
	struct sprd_adf_interface_capability *capability = NULL;

	ADF_DEBUG_GENERAL("entern\n");
	if (!config_entery) {
		ADF_DEBUG_WARN("config entery is illegal\n");
		goto out;
	}

	if (config_entery->get_interface_capability)
		capability = config_entery->
				get_interface_capability(dev_id, intf_id);

out:
	return capability;
}

/**
 * sprd_adf_get_overlayengine_capability - get overlayengine
 * capability
 *
 * @dev_id: sprd adf device unique id
 * @eng_id: sprd adf overlayengines unique id
 *
 * sprd_adf_get_overlayengine_capability() according to the device
 * id and overlayengines id then get the interface capability
 */
struct sprd_adf_overlayengine_capability
	*sprd_adf_get_overlayengine_capability(unsigned int dev_id,
		unsigned int eng_id)
{
	struct sprd_adf_overlayengine_capability *capability = NULL;

	ADF_DEBUG_GENERAL("entern\n");
	if (!config_entery) {
		ADF_DEBUG_WARN("config entery is illegal\n");
		goto out;
	}

	if (config_entery->get_overlayengine_capability)
		capability = config_entery->
				get_overlayengine_capability(dev_id, eng_id);

out:
	return capability;
}

/**
 * sprd_adf_get_display_config - get display hw config
 *
 * @pdev: platform device ptr
 * @display_config: sprd display config ptr
 *
 * sprd_adf_get_display_config() according to the platform id
 * to get display config and device ops callabck functions,intf ops
 * callback functions
 */
int sprd_adf_get_display_config(struct sprd_display_config *display_config)
{
	struct sprd_display_config *config = NULL;

	ADF_DEBUG_GENERAL("entern\n");
	if (!display_config) {
		ADF_DEBUG_WARN("parameter display_config is null\n");
		goto error_out;
	}

	config_entery = sprd_adf_get_config_entry();
	if (!config_entery) {
		ADF_DEBUG_WARN("config entery is illegal\n");
		goto error_out;
	}

	if (config_entery->get_display_config)
		config = config_entery->get_display_config();

	if (!config) {
		ADF_DEBUG_WARN("get display config failed\n");
		goto error_out;
	}

	memcpy(display_config, config, sizeof(struct sprd_display_config));

	return 0;

error_out:
	return -1;
}

