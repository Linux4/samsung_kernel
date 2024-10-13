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

struct hts_config_control *hts_core_default(int cpu);
void hts_core_reset_default(void);
int initialize_hts_sysfs(struct platform_device *pdev);
int uninitialize_hts_sysfs(struct platform_device *pdev);
