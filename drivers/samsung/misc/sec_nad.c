/* sec_nad.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/sec_nad.h>
#include <linux/fs.h>

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sec_debug.h>
#include <linux/sec_class.h>

#if defined(CONFIG_SEC_FACTORY)

#if defined(CONFIG_SEC_NAD_BPS_CLASSIFIER)
#include <linux/kq/sec_nad_bps_classifier.h>

#define NAD_BPS_PRINT(format, ...) printk("[NAD_BPS] " format, ##__VA_ARGS__)
#endif

#if defined(CONFIG_SEC_NAD_LOG)
static char nad_log_buffer[NAD_LOG_SIZE];
#endif

#if defined(CONFIG_SEC_NAD_BPS_CLASSIFIER)
static void sec_nad_bps_print_info(struct bps_info *bfo, char *info)
{
	NAD_BPS_PRINT(
		"\n=====================================================\n"
		" BPS info : %s\n"
		"=====================================================\n"
		" magic[0] = 0x%x\n"
		" magic[1] = 0x%x\n"
		"=====================================================\n"
		" sp : %d\n"
		" wp : %d\n"
		" dp : %d\n"
		" kp : %d\n"
		" mp : %d\n"
		" tp : %d\n"
		" cp : %d\n"
		"=====================================================\n"
		" pc_lr_cnt = %d\n"
		" pc_lr_last_idx = %d\n"
		" bus_cnt = %d\n"
		" smu_cnt = %d\n"
		" klg_cnt = %d\n"
		" dn_cnt = %d\n"
		" build_id = %s\n"
		"=====================================================\n",
		info, bfo->magic[0], bfo->magic[1], bfo->up_cnt.sp,
		bfo->up_cnt.wp, bfo->up_cnt.dp, bfo->up_cnt.kp,
		bfo->up_cnt.mp, bfo->up_cnt.tp, bfo->up_cnt.cp,
		bfo->pc_lr_cnt, bfo->pc_lr_last_idx, bfo->bus_cnt,
		bfo->smu_cnt, bfo->klg_cnt, bfo->dn_cnt,
		bfo->build_id);
}

static void sec_nad_bps_param_read(void)
{
	int ret;
	struct file *fp;

	/* return if bps data already loaded */
	if (sec_nad_bps_env_initialized == true)
		return;

	/* open file handle */
	fp = filp_open(NAD_PARAM_NAME, O_RDWR | O_SYNC, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		return;
	}

	/* move fp to bps classifier result location */
	ret = fp->f_op->llseek(fp, -(NAD_BPS_REFER_PARTITION_SIZE), SEEK_END);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}

	/* read bps related data from partition */
	ret = vfs_read(fp, (char *)&sec_nad_bps_env, sizeof(struct bps_info), &(fp->f_pos));
	if (ret < 0) {
		pr_err("%s: read error! %d\n", __func__, ret);
		goto close_fp_out;
	}

	/* print bps status */
	sec_nad_bps_print_info(&sec_nad_bps_env, "kernel");

	/* set bps data successfully loaded */
	sec_nad_bps_env_initialized = true;

close_fp_out:
	if (fp)
		filp_close(fp, NULL);
}
#endif

static void sec_nad_param_update(struct work_struct *work)
{
	int ret = -1;
	struct file *fp;
#if defined(CONFIG_SEC_NAD_LOG)
	struct file *fp2 = NULL;
#endif	
	struct sec_nad_param *param_data =
		container_of(work, struct sec_nad_param, sec_nad_work);

	pr_info("%s: set param at %s\n", __func__, param_data->state ? "write" : "read");

	fp = filp_open(NAD_PARAM_NAME, O_RDWR | O_SYNC, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));

		/* Rerun workqueue when nad_refer read fail */
		if (param_data->retry_cnt > param_data->curr_cnt) {
			pr_info("retry count : %d\n", param_data->curr_cnt++);
			schedule_delayed_work(&sec_nad_param_data.sec_nad_delay_work, HZ * 1);
		}
		return;
	}
	ret = fp->f_op->llseek(fp, -param_data->offset, SEEK_END);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}

#if defined(CONFIG_SEC_NAD_LOG)	
	fp2 = filp_open(NAD_UFS_TEST_BLOCK, O_RDWR | O_SYNC, 0);
	if (IS_ERR(fp2)) {
		pr_err("%s: NAD_UFS_TEST_BLOCK filp_open error %ld\n", __func__, PTR_ERR(fp2));
		return;
	}
	
	ret = fp2->f_op->llseek(fp2, NAD_LOG_OFFSET, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}
	
	pr_info("%s: NAD_LOG_OFFSET %d\n", __func__, ret);
#endif

	switch (param_data->state) {
	case NAD_PARAM_WRITE:
		ret = vfs_write(fp, (char *)&sec_nad_env, sizeof(struct nad_env), &(fp->f_pos));

		if (ret < 0) {
			pr_err("%s: write error! %d\n", __func__, ret);
			goto close_fp_out;
		}

			pr_info(
				"NAD factory : %s\n"
				"NAD result : %s\n"
				"NAD data : 0x%x\n"
				"NAD ACAT : %s\n"
				"NAD ACAT result : %s\n"
				"NAD ACAT data : 0x%x\n"
				"NAD ACAT loop count : %d\n"
				"NAD ACAT real count : %d\n"
				"NAD DRAM test need : %s\n"
				"NAD DRAM test result : 0x%x\n"
				"NAD DRAM test fail data : 0x%x\n"
				"NAD DRAM test fail address : 0x%08llx\n"
				"NAD status : %d\n"
				"NAD ACAT skip fail : %d\n"
				"NAD unlimited loop : %d\n", sec_nad_env.nad_factory,
				sec_nad_env.nad_result,
				sec_nad_env.nad_data,
				sec_nad_env.nad_acat,
				sec_nad_env.nad_acat_result,
				sec_nad_env.nad_acat_data,
				sec_nad_env.nad_acat_loop_count,
				sec_nad_env.nad_acat_real_count,
				sec_nad_env.nad_dram_test_need,
				sec_nad_env.nad_dram_test_result,
				sec_nad_env.nad_dram_fail_data,
				sec_nad_env.nad_dram_fail_address,
				sec_nad_env.current_nad_status,
				sec_nad_env.nad_acat_skip_fail,
				sec_nad_env.unlimited_loop);
#if defined(CONFIG_SEC_NAD_X)			
				pr_info("NAD X : %s\n", sec_nad_env.nad_extend);
				pr_info("sec_nad_env total size : %lu\n", sizeof(sec_nad_env));

				pr_info(
				"NADX : %s\n"
				"NADX result : %s\n"
				"NADX data : 0x%x\n"
				"NADX inform2_data : 0x%x\n"
				"NADX inform3_data : 0x%x\n"
				"NADX nadx_is_excuted : %d\n", sec_nad_env.nad_extend,
				sec_nad_env.nad_extend_result,
				sec_nad_env.nad_extend_data,
				sec_nad_env.nad_extend_inform2_data,
				sec_nad_env.nad_extend_inform3_data,
				sec_nad_env.nadx_is_excuted);
				
				
				pr_info(
				"NADX SECOND: %s\n"
				"NADX SECOND result : %s\n"
				"NADX SECOND data : 0x%x\n"
				"NADX SECOND inform2_data : 0x%x\n"
				"NADX SECOND inform3_data : 0x%x\n",
				sec_nad_env.nad_extend_second,
				sec_nad_env.nad_extend_second_result,
				sec_nad_env.nad_extend_second_data,
				sec_nad_env.nad_extend_second_inform2_data,
				sec_nad_env.nad_extend_second_inform3_data);
#endif
		break;
	case NAD_PARAM_READ:
		ret = vfs_read(fp, (char *)&sec_nad_env, sizeof(struct nad_env), &(fp->f_pos));

		if (ret < 0) {
			pr_err("%s: read error! %d\n", __func__, ret);
			goto close_fp_out;
		}
			pr_info(
				"NAD factory : %s\n"
				"NAD result : %s\n"
				"NAD data : 0x%x\n"
				"NAD DAS : %s BLOCK : %s LEVEL : %d VECTOR : %s\n"
				"NAD ACAT : %s\n"
				"NAD ACAT result : %s\n"
				"NAD ACAT data : 0x%x\n"
				"NAD ACAT DAS : %s BLOCK : %s LEVEL : %d VECTOR : %s\n"
				"NAD ACAT loop count : %d\n"
				"NAD ACAT real count : %d\n"
				"NAD DRAM test need : %s\n"
				"NAD DRAM test result : 0x%x\n"
				"NAD DRAM test fail data : 0x%x\n"
				"NAD DRAM test fail address : 0x%08llx\n"
				"NAD status : %d\n"
				"NAD ACAT skip fail : %d\n"
				"NAD unlimited loop : %d\n",
				sec_nad_env.nad_factory,
				sec_nad_env.nad_result,
				sec_nad_env.nad_data,
				sec_nad_env.nad_fail_info.das_string,
				sec_nad_env.nad_fail_info.block_string,
				sec_nad_env.nad_fail_info.level,
				sec_nad_env.nad_fail_info.vector_string,
				sec_nad_env.nad_acat,
				sec_nad_env.nad_acat_result,
				sec_nad_env.nad_acat_data,
				sec_nad_env.nad_acat_fail_info.das_string,
				sec_nad_env.nad_acat_fail_info.block_string,
				sec_nad_env.nad_acat_fail_info.level,
				sec_nad_env.nad_acat_fail_info.vector_string,
				sec_nad_env.nad_acat_loop_count,
				sec_nad_env.nad_acat_real_count,
				sec_nad_env.nad_dram_test_need,
				sec_nad_env.nad_dram_test_result,
				sec_nad_env.nad_dram_fail_data,
				sec_nad_env.nad_dram_fail_address,
				sec_nad_env.current_nad_status,
				sec_nad_env.nad_acat_skip_fail,
				sec_nad_env.unlimited_loop);

#if defined(CONFIG_NAD_55)
				pr_info(
				"nad_complete : %s\n",
				sec_nad_env.nad_complete);
#endif

#if defined(CONFIG_SEC_NAD_X)			
				pr_info("NAD X : %s\n", sec_nad_env.nad_extend);
				pr_info("sec_nad_env total size : %lu\n", sizeof(sec_nad_env));

				pr_info(
				"NADX : %s\n"
				"NADX result : %s\n"
				"NADX data : 0x%x\n"
				"NADX DAS : %s BLOCK : %s LEVEL : %d VECTOR : %s\n"
				"NADX nadx_is_excuted : %d\n", sec_nad_env.nad_extend,
				sec_nad_env.nad_extend_result,
				sec_nad_env.nad_extend_data,
				sec_nad_env.nad_extend_fail_info.das_string,
				sec_nad_env.nad_extend_fail_info.block_string,
				sec_nad_env.nad_extend_fail_info.level,
				sec_nad_env.nad_extend_fail_info.vector_string,
				sec_nad_env.nadx_is_excuted);	
#endif

#if defined(CONFIG_SEC_NAD_LOG)
		ret = vfs_read(fp2, (char *)nad_log_buffer, NAD_LOG_SIZE, &(fp2->f_pos));
		if (ret < 0)
			pr_err("%s: read error! %d\n", __func__, ret);
			
		pr_info("%s: NAD_LOG %s\n", __func__, nad_log_buffer);
#endif			
		break;
	}
	sec_nad_param_data.nad_param_complete = true;
	pr_info("%s: NAD READ COMPLETE \n", __func__);
close_fp_out:
	if (fp) {
		pr_info("%s: close\n", __func__);
		filp_close(fp, NULL);

#if defined(CONFIG_SEC_NAD_BPS_CLASSIFIER)
		// update nad bps param here!
		sec_nad_bps_param_read();
#endif
	}
		
#if defined(CONFIG_SEC_NAD_LOG)		
	if (fp2)
		filp_close(fp2, NULL);
#endif


	pr_info("%s: exit %d\n", __func__, ret);
	return;
}

int sec_set_nad_param(int val)
{
	int ret = -1;

	mutex_lock(&sec_nad_param_lock);

	switch (val) {
	case NAD_PARAM_WRITE:
	case NAD_PARAM_READ:
		goto set_param;
	default:
		goto unlock_out;
	}

set_param:
	sec_nad_param_data.state = val;
	schedule_work(&sec_nad_param_data.sec_nad_work);

	/* how to determine to return success or fail ? */
	ret = 0;
unlock_out:
	mutex_unlock(&sec_nad_param_lock);
	return ret;
}

static void sec_nad_init_update(struct work_struct *work)
{
	int ret = -1;

	pr_info("%s\n", __func__);

	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
	pr_err("%s: read error! %d\n", __func__, ret);

}

#if defined(CONFIG_SEC_SUPPORT_VST)
static int get_vst_adjust(void)
{
	int i = 0;
	int bit = 1;
	int tmp = 0;
	int crc = 0;
	int vst_adjust;

	vst_adjust = sec_nad_env.vst_info.vst_adjust;

	pr_info("vst_adjust : 0x%x\n", vst_adjust);

	/* check crc */
	crc = vst_adjust & 0x3;
	vst_adjust = vst_adjust >> 2;

	for (i = 0; i < 30; i++) {
		if (bit & vst_adjust)
			tmp++;
		bit = bit << 1;
	}

	tmp = (tmp % 4);
	if (tmp != 0)
		tmp = 4 - tmp;

	pr_info(" crc = %d, tmp = %d\n", crc, tmp);

	if (crc != tmp)
		return 0;
	else
		return vst_adjust & NAD_VST_ADJUST_MASK;
}

static int get_vst_result(void)
{
	int vst_result;


	vst_result = sec_nad_env.vst_info.vst_result;

	pr_info("vst_result : 0x%x\n", vst_result);

	/* check magic number */
	if (((vst_result >> 8) & NAD_VST_MAGIC_MASK) == NAD_VST_MAGIC_NUM)
		return ((vst_result >> 2) & NAD_VST_RESULT_MASK);
	else
		return 0;
}
#endif

#ifdef SEC_NAD_CURRENT_INFO
static int get_vst_temperature(int block)
{
	int temperature_sum;
	
	temperature_sum = sec_nad_env.vst_perform_data[block].temperature_sum;
	
	pr_info("temperature_sum : %d\n", temperature_sum);
	return temperature_sum;
}


static int get_vst_operation_time(int block)
{

	int operation_time;
	
	operation_time= sec_nad_env.vst_perform_data[block].time_sum;
	
	pr_info("operation_time : %d\n", operation_time);
	return operation_time;
}
#endif

#if defined(CONFIG_NAD_55)
int nad_check_rework()
{
	int val = -1;
	int ret = -1;
	
	int rework_info;
	
	rework_info = sec_nad_env.nad_rework_info;

	pr_info("%s fused nad_rework_info = %d\n", __func__, sec_nad_env.nad_rework_info);

	/* case 1 vst fail */
	if(( NAD_VST_RESULT_MASK & sec_nad_env.vst_info.vst_f_res) > 0) {
		val = REWORK_VST_FAIL;
	}
	/* case 2 sudden power off */
	else if (sec_nad_env.nad_data == 0 && sec_nad_env.nad_second_data == 0 ) {
		val = REWORK_SUDDEN_POWER_OFF;
	}
	/* case 3 first nad fail */
	else if (!strncasecmp(sec_nad_env.nad_result, "FAIL", 4) && sec_nad_env.nad_data != 0 ) {
		val = REWORK_FIRST_FAIL;
	}
	/* case 4 vst cp ecc nad fail */
	else if (sec_nad_env.cp_ecc_err[VST_ECC].ecc_error_sum > 0)
		val = REWORK_VST_CP_ECC_ERROR;
	/* case 5 nad cp ecc nad fail */
	else if (sec_nad_env.cp_ecc_err[NAD_ECC].ecc_error_sum > 0)
		val = REWORK_NAD_CP_ECC_ERROR;
	/* case 6 nad rtc time out */
	else if (((sec_nad_env.nad_data << 24) >> 29) == 0x7)
		val = REWORK_RTC_TIME_OVER;

	if (val > 0) {
		pr_info("%s new rework_info = %d\n", __func__, val);
		sec_nad_env.nad_rework_info = val;
		
		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0) {
			pr_err("%s: write error! %d\n", __func__, ret);
			return -1;
		}
	} else {
			if (rework_info == REWORK_RTC_TIME_OVER) {
				sec_nad_env.nad_rework_info = 0;
				ret = sec_set_nad_param(NAD_PARAM_WRITE);
				if (ret < 0) {
					pr_err("%s: write error! %d\n", __func__, ret);
					return -1;
				}
			}
	}
	if (rework_info > REWORK_START && rework_info < END_OF_REWORK_ITEM)
		val = -1;

	return val;
}
#endif

#if defined(CONFIG_SEC_NAD_PMIC)
static void make_pmic_data_to_string(void)
{
	if (!strlen(nad_ocp_read_result)) {
		if (sec_nad_env.pmic_ocp_status == NAD_PMIC_FAIL) {
			strcpy(nad_ocp_read_result, nad_ocp_data[sec_nad_env.pmic_ocp_fail_index]);
			pr_info("%s NAD_OCP_RESULT : %s %s\n", __func__,
				(sec_nad_env.pmic_ocp_status == 0) ? "pass" : "fail", nad_ocp_read_result);
		}
	}

	if (!strlen(nad_pmic_read_result)) {
		if (sec_nad_env.pmic_read_status == NAD_PMIC_FAIL) {
			if (sec_nad_env.pmic_read_channel != -1)
				strcpy(nad_pmic_read_result, nad_i2c_data[sec_nad_env.pmic_read_channel]);
			else
				strcpy(nad_pmic_read_result, nad_speedy_data);
		pr_info("%s NAD_PMIC_READ_RESULT : %s %s\n", __func__,
				(sec_nad_env.pmic_read_status == 0) ? "pass" : "fail", nad_pmic_read_result);
		}
	}
}
#endif

static ssize_t show_nad_stat(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int i;
	char * buf_offset;
	int ecc_err_total;
	int block, vector;

#if defined(CONFIG_NAD_55)
	int nad_need_rework;
	nad_need_rework = nad_check_rework();
#endif
#if defined(CONFIG_SEC_NAD_PMIC)
	make_pmic_data_to_string();
#endif
	
	pr_info("%s\n", __func__);
	

	if (!strncasecmp(sec_nad_env.nad_result, "FAIL", 4) && !strncasecmp(sec_nad_env.nad_second_result, "FAIL", 4)) {
		/* Both of NAD were failed. */
				buf_offset = buf;
				buf += sprintf(buf,
			       "NG_3.1_L_0x%x_0x%x_0x%x_0x%x,IT(%d),MT(%d),DAS(%s),BLOCK(%s),"
				"FN(%s_%s_%d_%s),FD(0x%08llx_0x%08llx_0x%08llx),"
#if defined(CONFIG_SEC_SUPPORT_VST)
				"FNS(%s_%s_%d_%s),FDS(0x%08llx_0x%08llx_0x%08llx),CHIP(%x),VER(%d),VIT(%d),VMT(%d),VRES(%d),VADJ(%d),FRES(%d),"
				"AVT(%d),"
#ifdef SEC_NAD_CURRENT_INFO
				"BIG_L11(%d),BIG_L16(%d),"
				"MID_L6(%d),MID_L10(%d),"
				"LIT_L4(%d),LIT_L6(%d),"
				"MIF_L1(%d),MIF_L3(%d),"
				"BIG_V_TEMP(%d),BIG_V_TIME(%d),"
				"MID_V_TEMP(%d),MID_V_TIME(%d),"
				"LIT_V_TEMP(%d),LIT_V_TIME(%d),"
				"MIF_V_TEMP(%d),MIF_V_TIME(%d),"
				"INT_V_TEMP(%d),INT_V_TIME(%d),"
#endif

				"ONC_B0(%d),ONC_B1(%d),ONC_B2(%d),"
				"ONC_M0(%d),ONC_M1(%d),ONC_M2(%d),"
				"ONC_G0(%d),ONC_G1(%d)",
#else
				"FNS(%s_%s_%d_%s),FDS(0x%08llx_0x%08llx_0x%08llx),CHIP(%x),VER(%d)",
#endif
				sec_nad_env.nad_data,
				sec_nad_env.nad_inform2_data,
				sec_nad_env.nad_second_data,
				sec_nad_env.nad_second_inform2_data,
				sec_nad_env.nad_init_temperature,
				sec_nad_env.max_temperature,
				sec_nad_env.nad_second_fail_info.das_string,
				sec_nad_env.nad_second_fail_info.block_string,
				sec_nad_env.nad_fail_info.das_string,
				sec_nad_env.nad_fail_info.block_string,
				sec_nad_env.nad_fail_info.level,
				sec_nad_env.nad_fail_info.vector_string,
				sec_nad_env.nad_dram_fail_information.nad_dram_fail_info[0].target_addr,
				sec_nad_env.nad_dram_fail_information.nad_dram_fail_info[0].read_val,
				sec_nad_env.nad_dram_fail_information.nad_dram_fail_info[0].expected_val,
#if defined(CONFIG_SEC_SUPPORT_VST)				
				sec_nad_env.nad_second_fail_info.das_string,
				sec_nad_env.nad_second_fail_info.block_string,
				sec_nad_env.nad_second_fail_info.level,
				sec_nad_env.nad_second_fail_info.vector_string,
				sec_nad_env.nad_second_dram_fail_information.nad_dram_fail_info[0].target_addr,
				sec_nad_env.nad_second_dram_fail_information.nad_dram_fail_info[0].read_val,
				sec_nad_env.nad_second_dram_fail_information.nad_dram_fail_info[0].expected_val,
				(sec_nad_env.nad_data >> 28) & 0xF,
				(sec_nad_env.nad_data >> 24) & 0xF,
				/* VST */
				sec_nad_env.vst_init_temperature,sec_nad_env.vst_max_temperature,
				get_vst_result(), get_vst_adjust(), sec_nad_env.vst_info.vst_f_res,
				sec_nad_env.nAsv_TABLE,
#ifdef SEC_NAD_CURRENT_INFO
				sec_nad_env.nad_ave_current_info.current_list[VST_BIG_UNZIP_L11].vst_current,sec_nad_env.nad_ave_current_info.current_list[VST_BIG_UNZIP_L16].vst_current,
				sec_nad_env.nad_ave_current_info.current_list[VST_MIDD_UNZIP_L6].vst_current,sec_nad_env.nad_ave_current_info.current_list[VST_MIDD_UNZIP_L10].vst_current,
				sec_nad_env.nad_ave_current_info.current_list[VST_LITT_UNZIP_L4].vst_current,sec_nad_env.nad_ave_current_info.current_list[VST_LITT_UNZIP_L6].vst_current,
				sec_nad_env.nad_ave_current_info.current_list[VST_MIF_MEMTEST_L1].vst_current,sec_nad_env.nad_ave_current_info.current_list[VST_MIF_MEMTEST_L3].vst_current,
				get_vst_temperature(NAD_BIG),get_vst_operation_time(NAD_BIG),
				get_vst_temperature(NAD_MIDD),get_vst_operation_time(NAD_MIDD),
				get_vst_temperature(NAD_LITT),get_vst_operation_time(NAD_LITT),
				get_vst_temperature(NAD_MIF),get_vst_operation_time(NAD_MIF),
				get_vst_temperature(NAD_INT),get_vst_operation_time(NAD_INT),
#endif

				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[0] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[0] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[0] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[0]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[1] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[1] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[1] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[1]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[2] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[2]== 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[2] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[2]),

				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[0] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[0] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[0] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[0]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[1] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[1] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[1] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[1]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[2] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[2] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[2] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[2]),

				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.g3d[0] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.g3d[0] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.g3d[0] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.g3d[0]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.g3d[1] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.g3d[1] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.g3d[1] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.g3d[1]));
#else
				sec_nad_env.nad_second_fail_info.das_string,
				sec_nad_env.nad_second_fail_info.block_string,
				sec_nad_env.nad_second_fail_info.level,
				sec_nad_env.nad_second_fail_info.vector_string,
				sec_nad_env.nad_second_dram_fail_information.nad_dram_fail_info[0].target_addr,
				sec_nad_env.nad_second_dram_fail_information.nad_dram_fail_info[0].read_val,
				sec_nad_env.nad_second_dram_fail_information.nad_dram_fail_info[0].expected_val,
				(sec_nad_env.nad_data >> 28) & 0xF,
				(sec_nad_env.nad_data >> 24) & 0xF);
#endif


				
				ecc_err_total = 0;
				
				for (i = 0; i < sec_nad_env.nad_vector_oper_info.total_num; i++) {
					if (sec_nad_env.nad_vector_oper_info.vector_list[i].ecc_err_count > 0)
					{
						block = sec_nad_env.nad_vector_oper_info.vector_list[i].block;
						vector = sec_nad_env.nad_vector_oper_info.vector_list[i].vector;
						buf += sprintf(buf, ",ECC_%s_%s_L%d(%d)",nad_block_name[block],nad_block_data[block].nad_block[vector],sec_nad_env.nad_vector_oper_info.vector_list[i].level, sec_nad_env.nad_vector_oper_info.vector_list[i].ecc_err_count);
						ecc_err_total ++;
					}
					if (ecc_err_total > 5) {
							break;
					}
				}
				
				if (sec_nad_env.last_nad_fail_status > 0) {
					buf += sprintf(buf, ",LN_%s_%s_%d_%s(0)",
					sec_nad_env.last_fail_data_backup.nad_fail_info.das_string,
					sec_nad_env.last_fail_data_backup.nad_fail_info.block_string,
					sec_nad_env.last_fail_data_backup.nad_fail_info.level,
					sec_nad_env.last_fail_data_backup.nad_fail_info.vector_string);
				}
#if defined(CONFIG_NAD_55)				
				buf += sprintf(buf, ",LRW(%d)", sec_nad_env.nad_rework_info);
				buf += sprintf(buf, ",RTO(%d)", (((sec_nad_env.nad_data << 24) >> 29) == 0x7)? 1 : 0);
				buf += sprintf(buf, ",VST_TIME(%d)", (sec_nad_env.vst_inform4_3_data >> 24));
				buf += sprintf(buf, ",NAD_TIME(%d)", ((sec_nad_env.nad_inform3_data << 16) >> 24));
				buf += sprintf(buf, ",COOLING_TIME(%d)", (sec_nad_env.nad_inform3_data >> 24));
				buf += sprintf(buf, ",NAD_ALLTIME(%ld)", sec_nad_env.nad_total_time);
#endif
#if defined(CONFIG_SEC_NAD_PMIC)
				if (sec_nad_env.pmic_ocp_status == NAD_PMIC_FAIL)
					buf += sprintf(buf, ",NAD_OCP(%d)", sec_nad_env.pmic_ocp_fail_index);
				if (sec_nad_env.pmic_read_status == NAD_PMIC_FAIL)
					buf += sprintf(buf, ",NAD_PMIC_READ(%d)", sec_nad_env.pmic_read_channel);
#endif
				buf += sprintf(buf, "\n");

				return (buf - buf_offset);
	} 
#if defined(CONFIG_NAD_55)		
	else if (nad_need_rework > 0) {
		buf_offset = buf;
	
		switch (nad_need_rework) {
	        case REWORK_VST_FAIL: {
				buf += sprintf(buf, "RE_WORK,RW(%d)",REWORK_VST_FAIL);
				buf += sprintf(buf, ",FRES(%d)", sec_nad_env.vst_info.vst_f_res);
	        } break;
	        case REWORK_SUDDEN_POWER_OFF: {
	            buf += sprintf(buf, "RE_WORK,RW(%d)",REWORK_SUDDEN_POWER_OFF);
	        } break;	
	        case REWORK_FIRST_FAIL: {
				buf += sprintf(buf, "RE_WORK,RW(%d)",REWORK_FIRST_FAIL);
				buf += sprintf(buf, ",FN(%s_%s_%d_%s)",
										sec_nad_env.nad_fail_info.das_string,
										sec_nad_env.nad_fail_info.block_string,
										sec_nad_env.nad_fail_info.level,
										sec_nad_env.nad_fail_info.vector_string);
			} break;
			case REWORK_VST_CP_ECC_ERROR: {
				buf += sprintf(buf, "RE_WORK,RW(%d)",REWORK_VST_CP_ECC_ERROR);
			} break;
			case REWORK_NAD_CP_ECC_ERROR: {
				buf += sprintf(buf, "RE_WORK,RW(%d)",REWORK_NAD_CP_ECC_ERROR);
			} break;
			case REWORK_RTC_TIME_OVER: {
				buf += sprintf(buf, "RE_WORK,RW(%d)",REWORK_RTC_TIME_OVER);
			} break;
		}
		buf += sprintf(buf, ",IT(%d),TN(%d),VIT(%d),RTO(%d),VST_TIME(%d),NAD_TIME(%d),COOLING_TIME(%d),NAD_ALLTIME(%ld)",
							sec_nad_env.nad_init_temperature,
							sec_nad_env.nad_inform3_data & 0xFF,
							sec_nad_env.vst_init_temperature,
							(((sec_nad_env.nad_data << 24) >> 29) == 0x7)? 1 : 0,
							sec_nad_env.vst_inform4_3_data >> 24,
							((sec_nad_env.nad_inform3_data << 16) >> 24),
							sec_nad_env.nad_inform3_data >> 24,
							sec_nad_env.nad_total_time);
		buf += sprintf(buf, "\n");
		return (buf - buf_offset);
	} 
#endif	
	else {
				buf_offset = buf;
				buf += sprintf(buf, "OK_3.1_L_0x%x_0x%x_0x%x_0x%x,IT(%d),MT(%d),OT(0x%x),"
#if defined(CONFIG_SEC_SUPPORT_VST)
				"TN(%d),CHIP(%x),VER(%d),VIT(%d),VMT(%d),VRES(%d),VADJ(%d),FRES(%d),"
				"AVT(%d),"
#ifdef SEC_NAD_CURRENT_INFO
				"BIG_L11(%d),BIG_L16(%d),"
				"MID_L6(%d),MID_L10(%d),"
				"LIT_L4(%d),LIT_L6(%d),"
				"MIF_L1(%d),MIF_L3(%d),"
				"BIG_V_TEMP(%d),BIG_V_TIME(%d),"
				"MID_V_TEMP(%d),MID_V_TIME(%d),"
				"LIT_V_TEMP(%d),LIT_V_TIME(%d),"
				"MIF_V_TEMP(%d),MIF_V_TIME(%d),"
				"INT_V_TEMP(%d),INT_V_TIME(%d),"
#endif

				"ONC_B0(%d),ONC_B1(%d),ONC_B2(%d),"
				"ONC_M0(%d),ONC_M1(%d),ONC_M2(%d),"
				"ONC_G0(%d),ONC_G1(%d)",
#else
				"TN(%d),CHIP(%x),VER(%d)",
#endif
				sec_nad_env.nad_data,
				sec_nad_env.nad_inform2_data,
				sec_nad_env.nad_second_data,
				sec_nad_env.nad_second_inform2_data,
				sec_nad_env.nad_init_temperature,
				sec_nad_env.max_temperature,
				sec_nad_env.nad_inform3_data,
#if defined(CONFIG_SEC_SUPPORT_VST)
				sec_nad_env.nad_inform3_data & 0xFF,
				(sec_nad_env.nad_data >> 28) & 0xF,
				(sec_nad_env.nad_data >> 24) & 0xF,
				/* VST */
				sec_nad_env.vst_init_temperature,sec_nad_env.vst_max_temperature,
				get_vst_result(), get_vst_adjust(), sec_nad_env.vst_info.vst_f_res,
				sec_nad_env.nAsv_TABLE,
#ifdef SEC_NAD_CURRENT_INFO
				sec_nad_env.nad_ave_current_info.current_list[VST_BIG_UNZIP_L11].vst_current,sec_nad_env.nad_ave_current_info.current_list[VST_BIG_UNZIP_L16].vst_current,
				sec_nad_env.nad_ave_current_info.current_list[VST_MIDD_UNZIP_L6].vst_current,sec_nad_env.nad_ave_current_info.current_list[VST_MIDD_UNZIP_L10].vst_current,
				sec_nad_env.nad_ave_current_info.current_list[VST_LITT_UNZIP_L4].vst_current,sec_nad_env.nad_ave_current_info.current_list[VST_LITT_UNZIP_L6].vst_current,
				sec_nad_env.nad_ave_current_info.current_list[VST_MIF_MEMTEST_L1].vst_current,sec_nad_env.nad_ave_current_info.current_list[VST_MIF_MEMTEST_L3].vst_current,
				get_vst_temperature(NAD_BIG),get_vst_operation_time(NAD_BIG),
				get_vst_temperature(NAD_MIDD),get_vst_operation_time(NAD_MIDD),
				get_vst_temperature(NAD_LITT),get_vst_operation_time(NAD_LITT),
				get_vst_temperature(NAD_MIF),get_vst_operation_time(NAD_MIF),
				get_vst_temperature(NAD_INT),get_vst_operation_time(NAD_INT),
#endif

				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[0] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[0] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[0] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[0]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[1] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[1] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[1] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[1]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[2] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[2]== 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[2] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[2]),

				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[0] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[0] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[0] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[0]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[1] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[1] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[1] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[1]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[2] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[2] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[2] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[2]),

				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.g3d[0] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.g3d[0] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.g3d[0] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.g3d[0]),
				((sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.g3d[1] == 0 || sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.g3d[1] == 0)? 0: sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.g3d[1] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.g3d[1]));

#else
				sec_nad_env.nad_inform3_data & 0xFF,
				(sec_nad_env.nad_data >> 28) & 0xF,
				(sec_nad_env.nad_data >> 24) & 0xF);
#endif




				
				ecc_err_total = 0;
				
				for (i = 0; i < sec_nad_env.nad_vector_oper_info.total_num; i++) {
					if (sec_nad_env.nad_vector_oper_info.vector_list[i].ecc_err_count > 0)
					{
						block = sec_nad_env.nad_vector_oper_info.vector_list[i].block;
						vector = sec_nad_env.nad_vector_oper_info.vector_list[i].vector;
						buf += sprintf(buf, ",ECC_%s_%s_L%d(%d)",nad_block_name[block],nad_block_data[block].nad_block[vector],sec_nad_env.nad_vector_oper_info.vector_list[i].level, sec_nad_env.nad_vector_oper_info.vector_list[i].ecc_err_count);
						ecc_err_total ++;
					}
					if (ecc_err_total > 5) {
							break;
					}	
				}

				
				if (sec_nad_env.last_nad_fail_status > 0) {
					buf += sprintf(buf, ",LN_%s_%s_%d_%s(0)",
					sec_nad_env.last_fail_data_backup.nad_fail_info.das_string,
					sec_nad_env.last_fail_data_backup.nad_fail_info.block_string,
					sec_nad_env.last_fail_data_backup.nad_fail_info.level,
					sec_nad_env.last_fail_data_backup.nad_fail_info.vector_string);
				}

#if defined(CONFIG_SEC_NAD_BPS_CLASSIFIER)
				/* update bps data from refer partition */
				sec_nad_bps_param_read();

				/* if bps data successfully initialized and if magic needs to be checked */
				if (sec_nad_bps_env_initialized &&
					sec_nad_bps_env.magic[1] == NAD_BPS_CLASSIFIER_MAGIC2) {
					buf += sprintf(buf, ",BPS(%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d)",
						sec_nad_bps_env.up_cnt.sp,
						sec_nad_bps_env.up_cnt.wp,
						sec_nad_bps_env.up_cnt.dp,
						sec_nad_bps_env.up_cnt.kp,
						sec_nad_bps_env.up_cnt.mp,
						sec_nad_bps_env.up_cnt.tp,
						sec_nad_bps_env.up_cnt.cp,
						sec_nad_bps_env.pc_lr_cnt,
						sec_nad_bps_env.pc_lr_last_idx,
						sec_nad_bps_env.bus_cnt,
						sec_nad_bps_env.smu_cnt,
						sec_nad_bps_env.klg_cnt,
						sec_nad_bps_env.dn_cnt);
				}
#endif


#if defined(CONFIG_NAD_55)				
				buf += sprintf(buf, ",LRW(%d)", sec_nad_env.nad_rework_info);
				buf += sprintf(buf, ",RTO(%d)", (((sec_nad_env.nad_data << 24) >> 29) == 0x7)? 1 : 0);
				buf += sprintf(buf, ",VST_TIME(%d)", (sec_nad_env.vst_inform4_3_data >> 24));
				buf += sprintf(buf, ",NAD_TIME(%d)", ((sec_nad_env.nad_inform3_data << 16) >> 24));
				buf += sprintf(buf, ",COOLING_TIME(%d)", (sec_nad_env.nad_inform3_data >> 24));
				buf += sprintf(buf, ",NAD_ALLTIME(%ld)", sec_nad_env.nad_total_time);
#endif
#if defined(CONFIG_SEC_NAD_PMIC)
				if (sec_nad_env.pmic_ocp_status == NAD_PMIC_FAIL)
					buf += sprintf(buf, ",NAD_OCP(%d)", sec_nad_env.pmic_ocp_fail_index);
				if (sec_nad_env.pmic_read_status == NAD_PMIC_FAIL)
					buf += sprintf(buf, ",NAD_PMIC_READ(%d)", sec_nad_env.pmic_read_channel);
#endif
				buf += sprintf(buf, "\n");
				
				return (buf - buf_offset);
	}
}
static DEVICE_ATTR(nad_stat, S_IRUGO, show_nad_stat, NULL);

static ssize_t store_nad_erase(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = -1;

	if (!strncmp(buf, "erase", 5)) {
		strncpy(sec_nad_env.nad_factory, "GAP", 3);
#if defined(CONFIG_NAD_55)		
		strncpy(sec_nad_env.nad_complete, "GAP", 3);
#endif		
		strncpy(sec_nad_env.nad_second, "DUMM", 4);
#if defined(CONFIG_SEC_NAD_API)
		sec_nad_env.nad_api_status = 0;
#endif
		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0)
		pr_err("%s: write error! %d\n", __func__, ret);

		return count;
	} else
		return count;
}

static ssize_t show_nad_erase(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	pr_info("%s\n", __func__);

	if (!strncmp(sec_nad_env.nad_factory, "GAP", 3))
		return sprintf(buf, "OK\n");
	else
		return sprintf(buf, "NG\n");
}

static DEVICE_ATTR(nad_erase, S_IRUGO | S_IWUSR, show_nad_erase, store_nad_erase);

static ssize_t show_nad_acat(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	pr_info("%s\n", __func__);

	/* Check status if ACAT NAD was excuted */
	if (sec_nad_env.current_nad_status >= NAD_ACAT_FLAG) {
		if (!strncasecmp(sec_nad_env.nad_acat_result, "FAIL", 4) && !strncasecmp(sec_nad_env.nad_acat_second_result, "FAIL", 4)) {
			return sprintf(buf, "NG_3.0_L_0x%x_0x%x_0x%x_0x%x,%d,%d,%s,%s,"
					"FN(%s_%s_%d_%s),FD(0x%08llx_0x%08llx_0x%08llx),"
					"FNS(%s_%s_%d_%s),FDS(0x%08llx_0x%08llx_0x%08llx)\n",
					sec_nad_env.nad_acat_data,
					sec_nad_env.nad_acat_inform2_data,
					sec_nad_env.nad_acat_second_data,
					sec_nad_env.nad_acat_second_inform2_data,
					sec_nad_env.nad_acat_init_temperature,
					sec_nad_env.nad_acat_max_temperature,
					sec_nad_env.nad_acat_second_fail_info.das_string,
					sec_nad_env.nad_acat_second_fail_info.block_string,
					sec_nad_env.nad_acat_fail_info.das_string,
					sec_nad_env.nad_acat_fail_info.block_string,
					sec_nad_env.nad_acat_fail_info.level,
					sec_nad_env.nad_acat_fail_info.vector_string,
					sec_nad_env.nad_acat_dram_fail_information.nad_dram_fail_info[0].target_addr,
					sec_nad_env.nad_acat_dram_fail_information.nad_dram_fail_info[0].read_val,
					sec_nad_env.nad_acat_dram_fail_information.nad_dram_fail_info[0].expected_val,
					sec_nad_env.nad_acat_second_fail_info.das_string,
					sec_nad_env.nad_acat_second_fail_info.block_string,
					sec_nad_env.nad_acat_second_fail_info.level,
					sec_nad_env.nad_acat_second_fail_info.vector_string,
					sec_nad_env.nad_acat_second_dram_fail_information.nad_dram_fail_info[0].target_addr,
					sec_nad_env.nad_acat_second_dram_fail_information.nad_dram_fail_info[0].read_val,
					sec_nad_env.nad_acat_second_dram_fail_information.nad_dram_fail_info[0].expected_val);
		} else {
			return sprintf(buf, "OK_3.0_L_0x%x_0x%x_0x%x_0x%x,%d,%d,OT(0x%x)\n",
					sec_nad_env.nad_acat_data,
					sec_nad_env.nad_acat_inform2_data,
					sec_nad_env.nad_acat_second_data,
					sec_nad_env.nad_acat_second_inform2_data,
					sec_nad_env.nad_acat_init_temperature,
					sec_nad_env.nad_acat_max_temperature,
					sec_nad_env.nad_acat_inform3_data);
		}
	} else
	return sprintf(buf, "NO_3.0_L_NADTEST\n");
}

static ssize_t store_nad_acat(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = -1;
	int idx = 0;
    char temp[NAD_BUFF_SIZE*3];
	char nad_cmd[NAD_CMD_LIST][NAD_BUFF_SIZE];
	char *nad_ptr, *string;
	unsigned int len = 0;

	pr_info("buf : %s count : %d\n", buf, (int)count);

	if ((int)count < NAD_BUFF_SIZE)
		return -EINVAL;

	/* Copy buf to nad temp */
	len = (unsigned int)min(count, sizeof(temp) - 1);
	strncpy(temp, buf, len);
	temp[len] = '\0';
	string = temp;

	while (idx < NAD_CMD_LIST) {
		nad_ptr = strsep(&string, ",");
		if (nad_ptr ==  NULL || strlen(nad_ptr) >= NAD_BUFF_SIZE) {
				pr_info(" %s: invalid input\n",__func__);
				return -EINVAL;	
		}
		strcpy(nad_cmd[idx++], nad_ptr);
	}

/*
* ACAT NAD Write Data format
*
* nad_acat,20,0 : ACAT Loop test (20 cycles)
* nad_acat,20,1 : ACAT Loop & DRAM test (20 cycles & 1 DRAM test)
* nad_acat,0,1  : ACAT DRAM test (1 DRAM test)
*
*/
	if (!strncmp(buf, "nad_acat", 8)) {
		/* Get NAD loop count */
		ret = sscanf(nad_cmd[1], "%d\n", &sec_nad_env.nad_acat_loop_count);
		if (ret != 1)
			return -EINVAL;
#if defined(CONFIG_NAD_55)
		if (sec_nad_env.nad_acat_loop_count == 0) {
			strncpy(sec_nad_env.nad_factory, "GAP", 3);	
			strncpy(sec_nad_env.nad_complete, "GAP", 3);		
			strncpy(sec_nad_env.nad_second, "DUMM", 4);
#if defined(CONFIG_SEC_NAD_API)
			sec_nad_env.nad_api_status = 0;
#endif
			sec_nad_env.nad_skip_usb_cmd_flag = NAD_SKIP_USB_CMD_FLAG;
		}
#endif
		/* case 1 : ACAT NAD */
		if (sec_nad_env.nad_acat_loop_count > 0) {
			pr_info("ACAT NAD test command.\n");

			strncpy(sec_nad_env.nad_acat, "ACAT", 4);
			strncpy(sec_nad_env.nad_acat_second, "DUMM", 4);
			sec_nad_env.nad_acat_second_running_count = 0;
			sec_nad_env.acat_fail_retry_count = 0;
			sec_nad_env.nad_acat_real_count = 0;
			sec_nad_env.nad_acat_data = 0;
			sec_nad_env.nad_acat_real_count = 0;
			sec_nad_env.current_nad_status = 0;
			sec_nad_env.nad_acat_skip_fail = 0;
			sec_nad_env.unlimited_loop = 0;
			sec_nad_env.max_temperature = 0;
			sec_nad_env.nad_acat_max_temperature = 0;
		}
#if defined(CONFIG_SEC_NAD_X)		
		if(sec_nad_env.nad_acat_loop_count ==  444)
		{
			pr_info("NADX test command.\n");

			strncpy(sec_nad_env.nad_extend, "NADX", 4);
			strncpy(sec_nad_env.nad_extend_result, "NULL",4);
			sec_nad_env.nad_extend_init_temperature = 0;
			sec_nad_env.nad_extend_max_temperature = 0;	
			
			sec_nad_env.nad_extend_data = 0;
			sec_nad_env.nad_extend_inform2_data = 0;
			sec_nad_env.nad_extend_inform3_data = 0;
			sec_nad_env.current_nad_status = 0;
			
			sec_nad_env.nad_extend_real_count=0;
			sec_nad_env.nad_extend_loop_count=1;
			sec_nad_env.nad_extend_skip_fail = 0;
			sec_nad_env.nad_acat_loop_count = 0;
			strncpy(sec_nad_env.nad_acat, "NONE", 4);
			strncpy(sec_nad_env.nad_acat_second, "DUMM", 4);
			sec_nad_env.nad_acat_second_running_count = 0;
			sec_nad_env.acat_fail_retry_count = 0;
			sec_nad_env.nad_acat_real_count = 0;
			sec_nad_env.nad_acat_data = 0;
			sec_nad_env.nad_acat_real_count = 0;
			sec_nad_env.current_nad_status = 0;
			sec_nad_env.nad_acat_skip_fail = 0;
			sec_nad_env.unlimited_loop = 0;
			sec_nad_env.max_temperature = 0;
			sec_nad_env.nad_acat_max_temperature = 0;

		}
#endif
		/* case 2 : DRAM test */
		if (!strncmp(nad_cmd[2], "1", 1)) {
			pr_info("DRAM test command.\n");

			strncpy(sec_nad_env.nad_dram_test_need, "DRAM", 4);
			sec_nad_env.nad_dram_test_result = 0;
			sec_nad_env.nad_dram_fail_data = 0;
			sec_nad_env.nad_dram_fail_address = 0;
		} else
			strncpy(sec_nad_env.nad_dram_test_need, "DONE", 4);

		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0) {
			pr_err("%s: write error! %d\n", __func__, ret);
			goto err_out;
		}

		return count;
	} else
		return count;
err_out:
	return ret;
}

static DEVICE_ATTR(nad_acat, S_IRUGO | S_IWUSR,  show_nad_acat, store_nad_acat);

static ssize_t show_nad_dram(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	pr_info("%s\n", __func__);

	if (sec_nad_env.nad_dram_test_result == NAD_DRAM_TEST_FAIL)
		return sprintf(buf, "NG_DRAM_0x%08llx,0x%x\n",
				sec_nad_env.nad_dram_fail_address,
				sec_nad_env.nad_dram_fail_data);
	else if (sec_nad_env.nad_dram_test_result == NAD_DRAM_TEST_PASS)
		return sprintf(buf, "OK_DRAM\n");
	else
		return sprintf(buf, "NO_DRAMTEST\n");
}
static DEVICE_ATTR(nad_dram, S_IRUGO, show_nad_dram, NULL);

static ssize_t show_nad_all(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf,
			"NAD factory : %s\n"
			"NAD result : %s\n"
			"NAD data : 0x%x\n"
			"NAD ACAT : %s\n"
			"NAD ACAT result : %s\n"
			"NAD ACAT data : 0x%x\n"
			"NAD ACAT loop count : %d\n"
			"NAD ACAT real count : %d\n"
			"NAD DRAM test need : %s\n"
			"NAD DRAM test result : 0x%x\n"
			"NAD DRAM test fail data : 0x%x\n"
			"NAD DRAM test fail address : 0x%08llx\n"
			"NAD status : %d\n"
			"NAD ACAT skip fail : %d\n"
			"NAD unlimited loop : %d\n", sec_nad_env.nad_factory,
			sec_nad_env.nad_result,
			sec_nad_env.nad_data,
			sec_nad_env.nad_acat,
			sec_nad_env.nad_acat_result,
			sec_nad_env.nad_acat_data,
			sec_nad_env.nad_acat_loop_count,
			sec_nad_env.nad_acat_real_count,
			sec_nad_env.nad_dram_test_need,
			sec_nad_env.nad_dram_test_result,
			sec_nad_env.nad_dram_fail_data,
			sec_nad_env.nad_dram_fail_address,
			sec_nad_env.current_nad_status,
			sec_nad_env.nad_acat_skip_fail,
			sec_nad_env.unlimited_loop);
}
static DEVICE_ATTR(nad_all, S_IRUGO, show_nad_all, NULL);

#if defined(CONFIG_SEC_NAD_C)
static ssize_t show_nad_c_run(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;

	if (sec_nad_env.fused_nad_custom_data.nadc_is_excuted == 1) {
		sec_nad_env.fused_nad_custom_data.nadc_is_excuted = 0;

		// clear nadc executed
		ret = sec_set_nad_param(NAD_PARAM_WRITE);
                if (ret < 0) {
                        pr_err("%s: write error! %d\n", __func__, ret);
                        return sprintf(buf, "%s\n", "RUN");
                }
		return sprintf(buf, "%s\n", "RUN");
	} else
		return sprintf(buf, "%s\n", "NORUN");

}
static DEVICE_ATTR(nad_c_run, S_IRUGO, show_nad_c_run, NULL);

static ssize_t show_nadc_fac_result(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	char vst_fail_das_string[10];
	int block;
	int max_ecc_err;
	int max_ecc_err_index;
	int temp;
	int i;
	
	temp = NAD_VST_RESULT_MASK & sec_nad_env.vst_info.vst_f_res;
	switch (temp) {
		case 0x1:
			strcpy(vst_fail_das_string,"BIG");
			break;
		case 0x2:
			strcpy(vst_fail_das_string,"MID");
			break;
		case 0x4:
			strcpy(vst_fail_das_string,"LIT");
			break;
		case 0x8:
			strcpy(vst_fail_das_string,"G3D");
			break;
		case 0x10:
			strcpy(vst_fail_das_string,"MIF");
			break;
		case 0x20:
			strcpy(vst_fail_das_string,"INT");
			break;
		case 0x40:
			strcpy(vst_fail_das_string,"FUNC");
			break;
		default:
			strcpy(vst_fail_das_string,"NULL");
			break;
	}

	max_ecc_err = 0;
	
	for (i = 0; i < sec_nad_env.nad_vector_oper_info.total_num; i++) {
		block = sec_nad_env.nad_vector_oper_info.vector_list[i].block;
		if (block == NAD_BIG || block == NAD_MIDD || block == NAD_LITT) {
			if (sec_nad_env.nad_vector_oper_info.vector_list[i].ecc_err_count > 0) {
				if (sec_nad_env.nad_vector_oper_info.vector_list[i].ecc_err_count > max_ecc_err) {
					max_ecc_err = sec_nad_env.nad_vector_oper_info.vector_list[i].ecc_err_count;
					max_ecc_err_index = i;
				}
			}
		}	
	}

	if (!strncmp(sec_nad_env.fused_nad_custom_data.nad_result, "PASS", 4))
		return sprintf(buf, "OK_5.0_L_CASE(NULL)DETAIL(NULL)\n");
	else if (!strncmp(sec_nad_env.fused_nad_custom_data.nad_result, "MAIN", 4))
		return sprintf(buf, "%s\n", "FAIL,CUSTOM_NAD");
	else if (!strncmp(sec_nad_env.fused_nad_custom_data.nad_result, "FAIL", 4)) {
		/* case 1 vst fail */
		if(( NAD_VST_RESULT_MASK & sec_nad_env.vst_info.vst_f_res) > 0) {
			return sprintf(buf, "NG_5.0_L_CASE(VST)DETAIL(%s),NADC_DAS(%s),BLOCK(%s),LEVEL(%d),VECTOR(%s)\n",
					vst_fail_das_string,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.das_string,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.block_string,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.level,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.vector_string);
		}
		/* case 2 ecc fail */
		else if (max_ecc_err != 0) {
			return sprintf(buf, "NG_5.0_L_CASE(ECC)DETAIL(%s),NADC_DAS(%s),BLOCK(%s),LEVEL(%d),VECTOR(%s)\n",
					nad_block_name[sec_nad_env.nad_vector_oper_info.vector_list[max_ecc_err_index].block],
					sec_nad_env.fused_nad_custom_data.nad_fail_info.das_string,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.block_string,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.level,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.vector_string);	
		}
		/* case 3 first nad fail */
		else if(!strncasecmp(sec_nad_env.nad_result, "FAIL", 4)){
			return sprintf(buf, "NG_5.0_L_CASE(NAD)DETAIL(%s),NADC_DAS(%s),BLOCK(%s),LEVEL(%d),VECTOR(%s)\n",
					sec_nad_env.nad_fail_info.das_string,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.das_string,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.block_string,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.level,
					sec_nad_env.fused_nad_custom_data.nad_fail_info.vector_string);
		}
		/* case 4 DEFAULT */
		else{
			/* case on_chip_cal fail*/
			for (i = 0; i < 3; i++) {
				if( (sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.big[i] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.big[i] <= 0) || 
					(sec_nad_env.nad_on_chip_cal_info.nad_cal_dat.mid[i] - sec_nad_env.nad_on_chip_cal_info.asb_otp_dat.mid[i] <= 0) ) {
					return sprintf(buf, "NG_5.0_L_CASE(ONC)DETAIL(%s),NADC_DAS(%s),BLOCK(%s),LEVEL(%d),VECTOR(%s)\n",
						sec_nad_env.nad_fail_info.das_string,
						sec_nad_env.fused_nad_custom_data.nad_fail_info.das_string,
						sec_nad_env.fused_nad_custom_data.nad_fail_info.block_string,
						sec_nad_env.fused_nad_custom_data.nad_fail_info.level,
						sec_nad_env.fused_nad_custom_data.nad_fail_info.vector_string);
				}
			}
			return sprintf(buf, "NG_5.0_L_CASE(DEF)DETAIL(DEF),NADC_DAS(%s),BLOCK(%s),LEVEL(%d),VECTOR(%s)\n",
				sec_nad_env.fused_nad_custom_data.nad_fail_info.das_string,
				sec_nad_env.fused_nad_custom_data.nad_fail_info.block_string,
				sec_nad_env.fused_nad_custom_data.nad_fail_info.level,
				sec_nad_env.fused_nad_custom_data.nad_fail_info.vector_string);
		}
	}
	else
		return sprintf(buf, "%s\n", "NONE");
}

static ssize_t store_nadc_fac_result(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int param_update = 0;
	int ret;
	int idx = 0;
	char temp[NAD_BUFF_SIZE*2];
	char nadc_cmd[NAD_CMD_LIST-1][NAD_BUFF_SIZE];
	char *nadc_ptr, *string;

	pr_info("buf : %s count : %d\n", buf, (int)count);

	/* Copy buf to nad temp */
	strncpy(temp, buf, NAD_BUFF_SIZE*2);
	string = temp;

	while (idx < NAD_CMD_LIST-1) {
		nadc_ptr = strsep(&string, ",");
		strcpy(nadc_cmd[idx++], nadc_ptr);
	}	

	/* check cmd */
	if (!strncmp(buf, "nadc", 4)) {
		/* Get NADC loop count */
		ret = sscanf(nadc_cmd[1], "%d\n", &sec_nad_env.fused_nad_custom_data.loop_count);
		if (ret != 1)
			return -EINVAL;

		/* case 1 : ACAT NAD */
		if (sec_nad_env.fused_nad_custom_data.loop_count > 0) {
			pr_info("run NADC command.\n");
			strncpy(sec_nad_env.fused_nad_custom_data.nad_name, "NADC", 4);
			strncpy(sec_nad_env.fused_nad_custom_data.nad_result, "NULL",4);
			sec_nad_env.fused_nad_custom_data.nad_init_temp = 0;
			sec_nad_env.fused_nad_custom_data.nad_max_temp = 0;
			sec_nad_env.fused_nad_custom_data.nad_inform1 = 0;
			sec_nad_env.fused_nad_custom_data.nad_inform2 = 0;
			sec_nad_env.fused_nad_custom_data.nad_inform3 = 0;
			sec_nad_env.current_nad_status = 0;
			sec_nad_env.fused_nad_custom_data.running_count=0;
			sec_nad_env.fused_nad_custom_data.skip_fail_flag = 0;
			param_update = 1;
		}
	}

	if (param_update == 1) {
		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0) {
			pr_err("%s: write error! %d\n", __func__, ret);
			return -1;
		}
	}
	return count;
}
static DEVICE_ATTR(nadc_fac_result, S_IRUGO | S_IWUSR, show_nadc_fac_result, store_nadc_fac_result);
#endif

#if defined(CONFIG_SEC_NAD_X)
static ssize_t show_nad_x_run(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;

	if (sec_nad_env.nadx_is_excuted == 1) {
		sec_nad_env.nadx_is_excuted = 0;

		// clear nadx executed
		ret = sec_set_nad_param(NAD_PARAM_WRITE);
                if (ret < 0) {
                        pr_err("%s: write error! %d\n", __func__, ret);
                        return sprintf(buf, "%s\n", "RUN");
                }
		return sprintf(buf, "%s\n", "RUN");
	} else
		return sprintf(buf, "%s\n", "NORUN");

}
static DEVICE_ATTR(nad_x_run, S_IRUGO, show_nad_x_run, NULL);

static ssize_t show_nad_fac_result(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	if (!strncmp(sec_nad_env.nad_extend_result, "PASS", 4))
		return sprintf(buf, "%s\n", sec_nad_env.nad_extend_result);
	else if (!strncmp(sec_nad_env.nad_extend_result, "MAIN", 4))
		return sprintf(buf, "%s\n", "FAIL,MAIN_NAD");
	else if (!strncmp(sec_nad_env.nad_extend_result, "FAIL", 4) && !strncasecmp(sec_nad_env.nad_extend_second_result, "FAIL", 4))
		return sprintf(buf, "FAIL,NG_NADX_DAS(%s),BLOCK(%s),LEVEL(%d),VECTOR(%s),PRE(%d,%d,%d,%d,%d,%d)\n",
					sec_nad_env.nad_extend_fail_info.das_string,
					sec_nad_env.nad_extend_fail_info.block_string,
					sec_nad_env.nad_extend_fail_info.level,
					sec_nad_env.nad_extend_fail_info.vector_string,
					sec_nad_env.nad_X_Pre_Domain_Level[0],
					sec_nad_env.nad_X_Pre_Domain_Level[1],
					sec_nad_env.nad_X_Pre_Domain_Level[2],
					sec_nad_env.nad_X_Pre_Domain_Level[3],
					sec_nad_env.nad_X_Pre_Domain_Level[4],
					sec_nad_env.nad_X_Pre_Domain_Level[5]);
	else if(!strncmp(sec_nad_env.nad_extend_result, "FAIL", 4) && !strncasecmp(sec_nad_env.nad_extend_second_result, "PASS", 4))
		return sprintf(buf, "%s\n", sec_nad_env.nad_extend_second_result);
	else
		return sprintf(buf, "%s\n", "NG");
}

static ssize_t store_nad_fac_result(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int param_update = 0;
	int ret;

	pr_info("buf : %s count = %d\n", buf, (int)count);

	/* check cmd */
	if (!strncmp(buf, "nadx", 4)) {
		pr_info("run NADX command.\n");
		strncpy(sec_nad_env.nad_extend, "NADX", 4);
		strncpy(sec_nad_env.nad_extend_result, "NULL",4);
		sec_nad_env.nad_extend_init_temperature = 0;
		sec_nad_env.nad_extend_max_temperature = 0;
		sec_nad_env.nad_extend_data = 0;
		sec_nad_env.nad_extend_inform2_data = 0;
		sec_nad_env.nad_extend_inform3_data = 0;
		sec_nad_env.current_nad_status = 0;
		sec_nad_env.nad_extend_real_count=0;
		sec_nad_env.nad_extend_loop_count=1;
		sec_nad_env.nad_extend_skip_fail = 0;
		param_update = 1;
	} else if (!strncmp(buf, "mainfail", 8)) {
		pr_info("update failed main nad result\n");
		strncpy(sec_nad_env.nad_extend_result, "MAIN", 4);
		param_update = 1;
	}

	if (param_update == 1) {
		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0) {
			pr_err("%s: write error! %d\n", __func__, ret);
			return -1;
		}
	}
	return count;
}
static DEVICE_ATTR(nad_fac_result, S_IRUGO | S_IWUSR, show_nad_fac_result, store_nad_fac_result);
#endif

static ssize_t show_nad_support(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	pr_info("%s\n", __func__);

	if (sec_nad_env.nad_support == NAD_SUPPORT_FLAG)
#if defined(CONFIG_SEC_NAD_X) && defined(CONFIG_SEC_NAD_C)
		return sprintf(buf, "SUPPORT_NADC\n");
#elif defined(CONFIG_SEC_NAD_X) && !defined(CONFIG_SEC_NAD_C)
		return sprintf(buf, "SUPPORT_NADX\n");
#else
		return sprintf(buf, "SUPPORT\n");
#endif
	else
		return sprintf(buf, "NOT_SUPPORT\n");
}
static DEVICE_ATTR(nad_support, S_IRUGO, show_nad_support, NULL);

#if defined(CONFIG_NAD_55)
static ssize_t store_nad_reboot(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{

	int ret;

	/* check commnad */
	pr_info("%s: START\n", __func__);
	if (sec_nad_param_data.nad_param_complete == true) {
		if (!strncmp(buf, "COM", 3)) {
			strncpy(sec_nad_env.nad_complete, "COM", 3);

			ret = sec_set_nad_param(NAD_PARAM_WRITE);
			if (ret < 0)
				pr_err("%s: write error! %d\n", __func__, ret);
		}
	}
	return count;
}
static DEVICE_ATTR(nad_reboot, S_IRUGO | S_IWUSR, NULL, store_nad_reboot);
#endif

static ssize_t show_nad_version(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int nad_project_index;
	unsigned int nad_project_index2;
	unsigned int nad_fw_version;
	char chipset_name[12];
	
	nad_project_index = (sec_nad_env.nad_data >> 28) & 0xF;
	nad_project_index2 = (sec_nad_env.nad_inform2_data >> 29) & 0xF;
	nad_fw_version = (sec_nad_env.nad_data >> 24) & 0xF;

	memset(chipset_name, 0x0, sizeof(chipset_name));
	pr_info("%s\n", __func__);
	
	if(sec_nad_env.nad_data != 0)
	{
	    if(nad_project_index == EXYNOS7885) {
			if(nad_project_index2 == 0) {
				strcpy(chipset_name, "EXYNOS7885");
			} 
			else if (nad_project_index2 == 1) {
				strcpy(chipset_name, "EXYNOS7884");
			} 
			else if (nad_project_index2 == 2)
			{
				strcpy(chipset_name, "EXYNOS7883");
			}
		}
		else
		{
			if( nad_project_index >= CHIPSET_MAX_COUNT)
				strcpy(chipset_name, "EXYNOSXXXX");
			else
				strcpy(chipset_name, nad_chipset_name[nad_project_index]);
		}
		return sprintf(buf,"NAD_%s_Ver.0x%x\n", chipset_name, nad_fw_version);	
			
	}
	else
		return sprintf(buf,"NAD_NO_INFORMATION\n");
	
}
static DEVICE_ATTR(nad_version, S_IRUGO, show_nad_version, NULL);

#if defined(CONFIG_SEC_NAD_API)
static void make_result_data_to_string(void)
{
	int i = 0;

	pr_info("%s : api total count(%d)\n", __func__, sec_nad_env.nad_api_total_count);

	/* Make result string if array is empty */
	if (!strlen(nad_api_result_string)) {
		for (i = 0; i < sec_nad_env.nad_api_total_count; i++) {
			pr_info("%s : name(%s) result(%d)\n", __func__,
				sec_nad_env.nad_api_info[i].name, sec_nad_env.nad_api_info[i].result);
			/* Failed gpio test */
			if (sec_nad_env.nad_api_info[i].result) {
				strcat(nad_api_result_string, sec_nad_env.nad_api_info[i].name);
				strcat(nad_api_result_string, ",");
			}
		}
		nad_api_result_string[strlen(nad_api_result_string)-1] = '\0';
	}
}

static ssize_t show_nad_api(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	pr_info("%s\n", __func__);

	/* Check nad api running status */
	if (sec_nad_env.nad_api_status == MAGIC_NAD_API_SUCCESS) {
		if (sec_nad_env.nad_api_magic == MAGIC_NAD_API_SUCCESS)
			return sprintf(buf, "OK_%d\n", sec_nad_env.nad_api_total_count);
		else {
			make_result_data_to_string();
			return sprintf(buf, "NG_%d,%s\n", sec_nad_env.nad_api_total_count, nad_api_result_string);
		}
	} else
	return sprintf(buf, "NONE\n");
}
static DEVICE_ATTR(nad_api, S_IRUGO, show_nad_api, NULL);
#endif

#if defined(CONFIG_SEC_NAD_LOG)
static ssize_t sec_nad_log_read_all(struct file *file, char __user *buf,
				size_t len, loff_t *offset)
{

	loff_t pos = *offset;
	ssize_t count;

	if (pos >= NAD_LOG_SIZE)
		return 0;

	count = min(len, (size_t)(NAD_LOG_SIZE - pos));
	if (copy_to_user(buf, nad_log_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
	
}

static const struct file_operations sec_nad_log_file_ops = {
.owner = THIS_MODULE,
.read = sec_nad_log_read_all,
};
#endif

static struct attribute *sec_nad_attrs[] = {
	&dev_attr_nad_stat.attr,
	&dev_attr_nad_erase.attr,
	&dev_attr_nad_acat.attr,
	&dev_attr_nad_dram.attr,
	&dev_attr_nad_all.attr,
	&dev_attr_nad_support.attr,
	&dev_attr_nad_version.attr,
#if defined(CONFIG_NAD_55)
	&dev_attr_nad_reboot.attr,
#endif
#if defined(CONFIG_SEC_NAD_API)
	&dev_attr_nad_api.attr,
#endif
#if defined(CONFIG_SEC_NAD_C)
	&dev_attr_nadc_fac_result.attr,
	&dev_attr_nad_c_run.attr,
#endif
#if defined(CONFIG_SEC_NAD_X)
	&dev_attr_nad_fac_result.attr,
	&dev_attr_nad_x_run.attr,
#endif
	NULL,
};

static struct attribute_group sec_nad_attr_group = {
	.attrs = sec_nad_attrs,
};
#endif

static int __init sec_nad_init(void)
{
	int ret = 0;

#if defined(CONFIG_SEC_FACTORY)

#if defined(CONFIG_SEC_NAD_LOG)
	struct proc_dir_entry *entry;
#endif

	struct device *sec_nad;

	pr_info("%s\n", __func__);

	/* Skip nad init when device goes to lp charging */
	if (lpcharge)
		return ret;

	sec_nad = sec_device_create(NULL, "sec_nad");

	if (IS_ERR(sec_nad)) {
		pr_err("%s Failed to create device(sec_nad)!\n", __func__);
		return PTR_ERR(sec_nad);
    }

	ret = sysfs_create_group(&sec_nad->kobj, &sec_nad_attr_group);
	if (ret) {
		pr_err("%s : could not create sysfs node", __func__);
		goto err_create_nad_sysfs;
	}

#if defined(CONFIG_SEC_NAD_LOG)
	entry = proc_create("sec_nad_log", 0440, NULL, &sec_nad_log_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}
	proc_set_size(entry, NAD_LOG_SIZE);
#endif

	/* Initialize nad param struct */
	sec_nad_param_data.offset = NAD_ENV_OFFSET;
	sec_nad_param_data.state = NAD_PARAM_EMPTY;
	sec_nad_param_data.retry_cnt = NAD_RETRY_COUNT;
	sec_nad_param_data.curr_cnt = 0;
	sec_nad_param_data.nad_param_complete = 0;

	INIT_WORK(&sec_nad_param_data.sec_nad_work, sec_nad_param_update);
	INIT_DELAYED_WORK(&sec_nad_param_data.sec_nad_delay_work, sec_nad_init_update);

#if defined(CONFIG_SEC_NAD_MANUAL_PARAM_READTIME)
	schedule_delayed_work(&sec_nad_param_data.sec_nad_delay_work, HZ * CONFIG_SEC_NAD_MANUAL_PARAM_READTIME);
#else
	schedule_delayed_work(&sec_nad_param_data.sec_nad_delay_work, HZ * 10);
#endif

	return 0;
err_create_nad_sysfs:
	sec_device_destroy(sec_nad->devt);
	return ret;
#else
	return ret;
#endif
}

static void __exit sec_nad_exit(void)
{
#if defined(CONFIG_SEC_FACTORY)
	cancel_work_sync(&sec_nad_param_data.sec_nad_work);
	cancel_delayed_work(&sec_nad_param_data.sec_nad_delay_work);
	pr_info("%s: exit\n", __func__);
#endif
}

module_init(sec_nad_init);
module_exit(sec_nad_exit);

