/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_INTERFACE_H__
#define __DSP_INTERFACE_H__

#include "dsp-hw-interface.h"

enum dsp_to_ca5_int_num {
	DSP_TO_CA5_INT_RESET,
	DSP_TO_CA5_INT_MAILBOX,
	DSP_TO_CA5_INT_NUM,
};

int dsp_interface_interrupt(struct dsp_interface *itf, int status);

int dsp_interface_open(struct dsp_interface *itf);
int dsp_interface_close(struct dsp_interface *itf);
int dsp_interface_probe(struct dsp_system *sys);
void dsp_interface_remove(struct dsp_interface *itf);

#endif
