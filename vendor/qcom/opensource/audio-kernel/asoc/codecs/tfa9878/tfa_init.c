/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "inc/dbgprint.h"
#include "inc/tfa_service.h"
#include "inc/tfa_internal.h"
#include "inc/tfa_container.h"
#include "inc/tfa98xx_tfafieldnames.h"

/* The CurrentSense4 registers are not in the datasheet */
#define TFA98XX_CURRENTSENSE4_CTRL_CLKGATECFOFF (1 << 2)
#define TFA98XX_CURRENTSENSE4 0x49

/*********************/
/* GLOBAL (Defaults) */
/*********************/

static bool ipc_loaded;

void tfa_set_ipc_loaded(int status)
{
	pr_info("set ipc_loaded %d for tfadsp\n", status);
	ipc_loaded = (status) ? true : false;
}

int tfa_get_ipc_loaded(void)
{
	pr_info("get ipc_loaded %d for tfadsp\n", ipc_loaded);
	return ipc_loaded ? 1 : 0;
}

static enum tfa98xx_error no_overload_function_available
(struct tfa_device *tfa, int not_used)
{
	(void)tfa;
	(void)not_used;

	return TFA98XX_ERROR_OK;
}

static enum tfa98xx_error no_overload_function_available2
(struct tfa_device *tfa)
{
	(void)tfa;

	return TFA98XX_ERROR_OK;
}

/* tfa98xx_dsp_system_stable
 *  return: *ready = 1 when clocks are stable to allow DSP subsystem access
 */
static enum tfa98xx_error tfa_dsp_system_stable
(struct tfa_device *tfa, int *ready)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short status;
	int value;

	/* check the contents of the STATUS register */
	value = TFA_READ_REG(tfa, AREFS);
	if (value < 0) {
		error = -value;
		*ready = 0;
		_ASSERT(error);		/* an error here can be fatal */
		return error;
	}
	status = (unsigned short)value;

	/* check AREFS and CLKS: not ready if either is clear */
	*ready = !((TFA_GET_BF_VALUE(tfa, AREFS, status) == 0)
		|| (TFA_GET_BF_VALUE(tfa, CLKS, status) == 0));

	return error;
}

/* tfa98xx_toggle_mtp_clock
 * Allows to stop clock for MTP/FAim needed for PLMA5505
 */
static enum tfa98xx_error tfa_faim_protect(struct tfa_device *tfa, int state)
{
	(void)tfa;
	(void)state;

	return TFA98XX_ERROR_OK;
}

/** Set internal oscillator into power down mode.
 *
 *  This function is a worker for tfa98xx_set_osc_powerdown().
 *
 *  @param[in] tfa device description structure
 *  @param[in] state new state 0 - oscillator is on, 1 oscillator is off.
 *
 *  @return TFA98XX_ERROR_OK when successful, error otherwise.
 */
static enum tfa98xx_error tfa_set_osc_powerdown
(struct tfa_device *tfa, int state)
{
	/* This function has no effect in general case, only for tfa9912 */
	(void)tfa;
	(void)state;

	return TFA98XX_ERROR_OK;
}

static enum tfa98xx_error tfa_update_lpm(struct tfa_device *tfa, int state)
{
	/* This function has no effect in general case, only for tfa9912 */
	(void)tfa;
	(void)state;

	return TFA98XX_ERROR_OK;
}

static enum tfa98xx_error tfa_dsp_reset(struct tfa_device *tfa, int state)
{
	/* generic function */
	TFA_SET_BF_VOLATILE(tfa, RST, (uint16_t)state);

	return TFA98XX_ERROR_OK;
}

int tfa_set_swprofile(struct tfa_device *tfa, unsigned short new_value)
{
	int mtpk, active_value = tfa->profile;

	/* Also set the new value in the struct */
	tfa->profile = new_value - 1;

	/* for TFA1 devices */
	/* it's in MTP shadow, so unlock if not done already */
	mtpk = TFA_GET_BF(tfa, MTPK); /* get current key */
	TFA_SET_BF_VOLATILE(tfa, MTPK, 0x5a);
	TFA_SET_BF_VOLATILE(tfa, SWPROFIL, new_value); /* set current profile */
	TFA_SET_BF_VOLATILE(tfa, MTPK, (uint16_t)mtpk); /* restore key */

	return active_value;
}

static int tfa_get_swprofile(struct tfa_device *tfa)
{
	/* return TFA_GET_BF(tfa, SWPROFIL) - 1; */
	return tfa->profile;
}

static int tfa_set_swvstep(struct tfa_device *tfa, unsigned short new_value)
{
	int mtpk, active_value = tfa->vstep;

	/* Also set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* for TFA1 devices */
	/* it's in MTP shadow, so unlock if not done already */
	mtpk = TFA_GET_BF(tfa, MTPK); /* get current key */
	TFA_SET_BF_VOLATILE(tfa, MTPK, 0x5a);
	TFA_SET_BF_VOLATILE(tfa, SWVSTEP, new_value); /* set current vstep */
	TFA_SET_BF_VOLATILE(tfa, MTPK, (uint16_t)mtpk); /* restore key */

	return active_value;
}

static int tfa_get_swvstep(struct tfa_device *tfa)
{
	int value = 0;
	/* Set the new value in the hw register */
	value = TFA_GET_BF(tfa, SWVSTEP);

	/* Also set the new value in the struct */
	tfa->vstep = value - 1;

	return value - 1; /* invalid if 0 */
}

static int tfa_get_mtpb(struct tfa_device *tfa)
{

	int value = 0;

	/* Set the new value in the hw register */
	value = TFA_GET_BF(tfa, MTPB);

	return value;
}

static enum tfa98xx_error tfa_set_mute_nodsp(struct tfa_device *tfa, int mute)
{
	(void)tfa;
	(void)mute;

	return TFA98XX_ERROR_OK;
}

static int tfa_set_bitfield(struct tfa_device *tfa,
	uint16_t bitfield, uint16_t value)
{
	return tfa_set_bf(tfa, (uint16_t)bitfield, value);
}

void set_ops_defaults(struct tfa_device_ops *ops)
{
	/* defaults */
	ops->reg_read = tfa98xx_read_register16;
	ops->reg_write = tfa98xx_write_register16;
	ops->mem_read = tfa98xx_dsp_read_mem;
	ops->mem_write = tfa98xx_dsp_write_mem_word;
	if (!ipc_loaded) {
		ops->dsp_msg = tfa_dsp_msg_rpc;
		ops->dsp_msg_read = tfa_dsp_msg_read_rpc;
	}
	ops->dsp_write_tables = no_overload_function_available;
	ops->dsp_reset = tfa_dsp_reset;
	ops->dsp_system_stable = tfa_dsp_system_stable;
	ops->auto_copy_mtp_to_iic = no_overload_function_available2;
	ops->factory_trimmer = no_overload_function_available2;
	ops->set_swprof = tfa_set_swprofile;
	ops->get_swprof = tfa_get_swprofile;
	ops->set_swvstep = tfa_set_swvstep;
	ops->get_swvstep = tfa_get_swvstep;
	ops->get_mtpb = tfa_get_mtpb;
	ops->set_mute = tfa_set_mute_nodsp;
	ops->faim_protect = tfa_faim_protect;
	ops->set_osc_powerdown = tfa_set_osc_powerdown;
	ops->update_lpm = tfa_update_lpm;
	ops->set_bitfield = tfa_set_bitfield;
}

/****************************/
/* no TFA
 * external DSP SB instance
 ****************************/
static short tfanone_swvstep, swprof; /* TODO emulate in hal plugin */
static enum tfa98xx_error tfanone_dsp_system_stable
(struct tfa_device *tfa, int *ready)
{
	(void)tfa; /* suppress warning */
	*ready = 1; /* assume always ready */

	return TFA98XX_ERROR_OK;
}

static int tfanone_set_swprofile
(struct tfa_device *tfa, unsigned short new_value)
{
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	/* Set the new value in the hw register */
	swprof = new_value;

	return active_value;
}

static int tfanone_get_swprofile(struct tfa_device *tfa)
{
	(void)tfa; /* suppress warning */

	return swprof;
}

static int tfanone_set_swvstep
(struct tfa_device *tfa, unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	/* Set the new value in the hw register */
	tfanone_swvstep = new_value;

	return new_value;
}

static int tfanone_get_swvstep(struct tfa_device *tfa)
{
	(void)tfa; /* suppress warning */

	return tfanone_swvstep;
}

void tfanone_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->dsp_system_stable = tfanone_dsp_system_stable;
	ops->set_swprof = tfanone_set_swprofile;
	ops->get_swprof = tfanone_get_swprofile;
	ops->set_swvstep = tfanone_set_swvstep;
	ops->get_swvstep = tfanone_get_swvstep;
}

/***********/
/* TFA9878 */
/***********/
static enum tfa98xx_error tfa9878_faim_protect(struct tfa_device *tfa,
	int status)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;

	/* 0b = FAIM protection enabled 1b = FAIM protection disabled*/
	ret = tfa_set_bf_volatile(tfa, TFA9878_BF_OPENMTP, (uint16_t)(status));

	return ret;
}

static enum tfa98xx_error tfa9878_specific(struct tfa_device *tfa)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	unsigned short value, xor;
	unsigned short irqmask;

	if (tfa->in_use == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	/* Unlock key 1 and 2 */
	error = reg_write(tfa, 0x0F, 0x5A6B);
	error = reg_read(tfa, 0xFB, &value);
	xor = value ^ 0x005A;
	error = reg_write(tfa, 0xA0, xor);
	tfa98xx_key2(tfa, 0);

	switch (tfa->rev) {
	case 0x0a78: /* Initial revision ID */
		/* ----- generated code start ----- */
		/* -----  version 28 ----- */
		reg_write(tfa, 0x01, 0x2e18); /* POR=0x2e88 */
		reg_write(tfa, 0x02, 0x0628); /* POR=0x0008 */
		reg_write(tfa, 0x04, 0x0240); /* POR=0x0340 */
		reg_write(tfa, 0x52, 0x587c); /* POR=0x57dc */
		reg_write(tfa, 0x61, 0x0183); /* POR=0x0a82 */
		reg_write(tfa, 0x63, 0x055a); /* POR=0x0a9a */
		reg_write(tfa, 0x65, 0x0542); /* POR=0x0a82 */
		reg_write(tfa, 0x71, 0x303e); /* POR=0x307e */
		reg_write(tfa, 0x83, 0x009a); /* POR=0x0799 */
		/* ----- generated code end   ----- */
		break;

	case 0x1a78: /* Initial revision ID */
		/* ----- generated code start ----- */
		/* -----  version 12 ----- */
		reg_write(tfa, 0x01, 0x2e18); /* POR=0x2e88 */
		reg_write(tfa, 0x02, 0x0628); /* POR=0x0008 */
		reg_write(tfa, 0x04, 0x0241); /* POR=0x0340 */
		reg_write(tfa, 0x52, 0x587c); /* POR=0x57dc */
		reg_write(tfa, 0x61, 0x0183); /* POR=0x0a82 */
		reg_write(tfa, 0x63, 0x055a); /* POR=0x0a9a */
		reg_write(tfa, 0x65, 0x0542); /* POR=0x0a82 */
		reg_write(tfa, 0x70, 0xb7ff); /* POR=0x37ff */
		reg_write(tfa, 0x71, 0x303e); /* POR=0x307e */
		reg_write(tfa, 0x83, 0x009a); /* POR=0x0799 */
		reg_write(tfa, 0x84, 0x0211); /* POR=0x0011 */
		reg_write(tfa, 0x8c, 0x0210); /* POR=0x0010 */
		reg_write(tfa, 0xce, 0x2202); /* POR=0xa202 */
		reg_write(tfa, 0xd5, 0x0000); /* POR=0x0100 */
		/* ----- generated code end   ----- */
		break;

	default:
		pr_info("\nWarning: Optimal settings not found for device with revid = 0x%x\n",
			tfa->rev);
		break;
	}

	/* select interrupt flags */
	irqmask = (TFA_BF_MSK(TFA9878_BF_IEOTDS)
		| TFA_BF_MSK(TFA9878_BF_IEOCPR)
		| TFA_BF_MSK(TFA9878_BF_IEUVDS)
		| TFA_BF_MSK(TFA9878_BF_IEMANALARM));
	tfa->interrupt_enable[0] = irqmask; /* save mask */
	tfa_irq_init(tfa); /* init irq regs */

	return error;
}

static int tfa9878_set_swprofile(struct tfa_device *tfa,
	unsigned short new_value)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	int active_value = tfa_dev_get_swprof(tfa);

	/* Set the new value in the struct */
	tfa->profile = new_value - 1;

	if (tfa->swprof != tfa->profile)
		/* Set the new value in the hw register */
		err = tfa_set_bf_volatile(tfa,
			TFA9878_BF_SWPROFIL, new_value);

	if (err == TFA98XX_ERROR_OK)
		tfa->swprof = tfa->profile;

	return active_value;
}

static int tfa9878_get_swprofile(struct tfa_device *tfa)
{
	if (tfa->swprof == -1)
		tfa->swprof = tfa_get_bf(tfa, TFA9878_BF_SWPROFIL) - 1;

	return tfa->swprof;
}

static int tfa9878_set_swvstep(struct tfa_device *tfa,
	unsigned short new_value)
{
	/* Set the new value in the struct */
	tfa->vstep = new_value - 1;

	return new_value;
}

static int tfa9878_get_swvstep(struct tfa_device *tfa)
{
	return tfa->vstep; /* vstep is not used */
}

/* tfa98xx_dsp_system_stable
 *  return: *ready = 1 when clocks are stable to allow DSP subsystem access
 */
static enum tfa98xx_error tfa9878_dsp_system_stable(struct tfa_device *tfa,
	int *ready)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;

	/* check CLKS: ready if set */
	*ready = tfa_get_bf(tfa, TFA9878_BF_CLKS) == 1;

	return error;
}

static int tfa9878_get_mtpb(struct tfa_device *tfa)
{
	int value;

	value = tfa_get_bf(tfa, TFA9878_BF_MTPB);

	return value;
}

void tfa9878_ops(struct tfa_device_ops *ops)
{
	/* Set defaults for ops */
	set_ops_defaults(ops);

	ops->tfa_init = tfa9878_specific;
	ops->set_swprof = tfa9878_set_swprofile;
	ops->get_swprof = tfa9878_get_swprofile;
	ops->set_swvstep = tfa9878_set_swvstep;
	ops->get_swvstep = tfa9878_get_swvstep;
	ops->dsp_system_stable = tfa9878_dsp_system_stable;
	ops->faim_protect = tfa9878_faim_protect;
	ops->get_mtpb = tfa9878_get_mtpb;
	ops->set_mute = tfa_set_mute_nodsp;
}

