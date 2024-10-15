/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include <linux/platform_device.h>

#include "hts_data.h"

int hts_external_initialize(struct platform_device *pdev);

void hts_update_request(struct plist_node *req, struct hts_data *data, int value);
void hts_remove_request(struct plist_node *req, struct hts_data *data);

