/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

	/*
	 * DF: for single variable
	 * DF_A: for array variable
	 * DF_S: for segmented single byte
	 * NOTE: fmt string must be less than FMT_MAX_LEN
	 */
	DF(fiq_step, " fiq step: %u "),
	DF(exp_type, " exception type: %u\n"),
	DF(kick, "kick=0x%x,"),
	DF(check, "check=0x%x\n"),
	DF(kaslr_offset, "Kernel Offset: 0x%llx\n"),
	DF(oops_in_progress_addr, "&oops_in_progress: 0x%llx\n"),
	DF(wdk_ktime, "wdk_ktime=%lld\n"),
	DF(wdk_systimer_cnt, "wdk_systimer_cnt=%lld\n"),
	/* ensure info related to HWT always be bottom and keep their order*/
	DF_A(clk_data, "clk_data: 0x%x\n", 8),
	DF(gpu_dvfs_vgpu, "gpu_dvfs_vgpu: 0x%x\n"),
	DF(gpu_dvfs_oppidx, "gpu_dvfs_oppidx: 0x%x\n"),
	DF(gpu_dvfs_status, "gpu_dvfs_status: 0x%x\n"),
	DF(gpu_dvfs_power_count, "gpu_dvfs_power_count: %d\n"),
	DF_S(ptp_vboot, "ptp_bank_[%d]_vboot = 0x%llx\n"),
	DF_S(ptp_gpu_volt, "ptp_gpu_volt[%d] = %llx\n"),
	DF_S(ptp_gpu_volt_1, "ptp_gpu_volt_1[%d] = %llx\n"),
	DF_S(ptp_gpu_volt_2, "ptp_gpu_volt_2[%d] = %llx\n"),
	DF_S(ptp_gpu_volt_3, "ptp_gpu_volt_3[%d] = %llx\n"),
	DF_S(ptp_temp, "ptp_temp[%d] = %llx\n"),
	DF(ptp_status, "ptp_status: 0x%x\n"),
	DF(hang_detect_timeout_count, "hang detect time out: 0x%x\n"),
	DF(gz_irq, "GZ IRQ: 0x%x\n"),
	/* kparams (unused) */

	/* ensure info related to HWT always be bottom and keep their order*/
	DF(last_init_func, "last init function: 0x%lx\n"),
	DF_SA(last_init_func_name, "last_init_func_name: %s\n"),
	DF_SA(last_shutdown_device, "last_shutdown_device: %s\n"),
