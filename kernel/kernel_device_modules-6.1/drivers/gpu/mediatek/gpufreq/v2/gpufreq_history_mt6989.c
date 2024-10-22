// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    gpufreq_history_mt6989_fgpa.c
 * @brief   GPU DVFS History log DB Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */

#include <linux/sched/clock.h>
#include <linux/string.h>
#include <linux/io.h>

/* GPUFREQ */
#include <gpufreq_v2.h>
#include <gpufreq_history_common.h>
#include <gpufreq_history_mt6989.h>
#include <gpuppm.h>
#include <gpufreq_common.h>

/**
 * ===============================================
 * Variable Definition
 * ===============================================
 */

static void __iomem *next_log_offs;
static void __iomem *start_log_offs;
static void __iomem *end_log_offs;

static enum gpufreq_history_state g_history_state;
static int g_history_target_opp_stack;
static int g_history_target_opp_top;
static unsigned int g_history_sel;
static unsigned int g_history_delsel;

/**
 * ===============================================
 * Common Function Definition
 * ===============================================
 */

/* API: set target oppidx */
void gpufreq_set_history_target_opp(enum gpufreq_target target, int oppidx)
{
	if (target == TARGET_STACK)
		g_history_target_opp_stack = oppidx;
	else
		g_history_target_opp_top = oppidx;
}

/* API: get target oppidx */
int gpufreq_get_history_target_opp(enum gpufreq_target target)
{
	if (target == TARGET_STACK)
		return g_history_target_opp_stack;
	else
		return g_history_target_opp_top;
}

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */

/* API: set sel bit */
void __gpufreq_set_sel_bit(unsigned int sel)
{
	g_history_sel = sel;
}

/* API: get sel bit*/
unsigned int __gpufreq_get_sel_bit(void)
{
	return g_history_sel;
}

/* API: set delsel bit */
void __gpufreq_set_delsel_bit(unsigned int delsel)
{
	g_history_delsel = delsel;
}

/* API: get delsel bit*/
unsigned int __gpufreq_get_delsel_bit(void)
{
	return g_history_delsel;
}

/***********************************************************************************
 *  Function Name      : __gpufreq_record_history_entry
 *  Inputs             : -
 *  Outputs            : -
 *  Returns            : -
 *  Description        : -
 ************************************************************************************/
void __gpufreq_record_history_entry(enum gpufreq_history_state history_state)
{

}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_init
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : initialize gpueb log db sysram memory
 ************************************************************************************/
void __gpufreq_history_memory_init(void)
{

}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_reset
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : reset gpueb log db sysram memory
 ************************************************************************************/
void __gpufreq_history_memory_reset(void)
{

}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_uninit
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : -
 ************************************************************************************/
void __gpufreq_history_memory_uninit(void)
{

}
