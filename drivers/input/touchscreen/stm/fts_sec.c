#ifdef SEC_TSP_FACTORY_TEST

#define BUFFER_MAX			((256 * 1024) - 16)
#define READ_CHUNK_SIZE			128 // (2 * 1024) - 16

enum {
	TYPE_RAW_DATA = 0,
	TYPE_FILTERED_DATA = 2,
	TYPE_STRENGTH_DATA = 4,
	TYPE_BASELINE_DATA = 6
};

enum {
	BUILT_IN = 0,
	UMS,
};

#ifdef FTS_SUPPORT_TOUCH_KEY
enum {
	TYPE_TOUCHKEY_RAW	= 0x34,
	TYPE_TOUCHKEY_STRENGTH	= 0x36,
	TYPE_TOUCHKEY_THRESHOLD	= 0x48,
};
#endif

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void get_x_cross_routing(void *device_data);
static void get_y_cross_routing(void *device_data);
static void get_checksum_data(void *device_data);
static void run_reference_read(void *device_data);
static void get_reference(void *device_data);
static void get_reference_all_data(void *device_data);
static void run_rawcap_read(void *device_data);
static void get_rawcap(void *device_data);
static void get_rawcap_all_data(void *device_data);
static void run_strength_read(void *device_data);
static void get_strength(void *device_data);
static void get_strength_all_data(void *device_data);
#ifdef FTS_SUPPORT_HOVER
static void run_abscap_read(void *device_data);
static void run_absdelta_read(void *device_data);
#endif
static void run_ix_data_read(void *device_data);
static void run_ix_data_read_all(void *device_data);
static void run_self_raw_read(void *device_data);
static void run_self_raw_read_all(void *device_data);
static void run_wtr_cx_data_read(void *device_data);
static void run_wtr_cx_data_read_all(void *device_data);
static void run_trx_short_test(void *device_data);
static void run_cx_data_read(void *device_data);
static void get_cx_data(void *device_data);
static void get_cx_all_data(void *device_data);
static void set_tsp_test_result(void *device_data);
static void get_tsp_test_result(void *device_data);
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
static void set_status_pureautotune(void *device_data);
static void get_status_pureautotune(void *device_data);
static void get_status_afe(void *device_data);
#endif
#ifdef FTS_SUPPORT_HOVER
static void hover_enable(void *device_data);
/* static void hover_no_sleep_enable(void *device_data); */
#endif
#ifdef CONFIG_GLOVE_TOUCH
static void glove_mode(void *device_data);
static void get_glove_sensitivity(void *device_data);
static void fast_glove_mode(void *device_data);
#endif
static void clear_cover_mode(void *device_data);
#ifdef USE_STYLUS_PEN
static void stylus_enable(void *device_data);
#endif
static void report_rate(void *device_data);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static void interrupt_control(void *device_data);
#endif
static void set_lowpower_mode(void *device_data);
static void set_deepsleep_mode(void *device_data);
static void active_sleep_enable(void *device_data);
static void set_wirelesscharger_mode(void *device_data);
#if defined(TSP_BOOSTER) && !defined(INPUT_BOOSTER_SYSFS)
static void boost_level(void *device_data);
#endif
static void second_screen_enable(void *device_data);
static void set_longpress_enable(void *device_data);
static void set_grip_detection(void *device_data);
static void set_sidescreen_x_length(void *device_data);
static void set_dead_zone(void *device_data);
static void dead_zone_enable(void *device_data);
static void select_wakeful_edge(void *device_data);
static void set_tunning_data(void *device_data);
#ifdef SMARTCOVER_COVER
static void smartcover_cmd(void *device_data);
#endif
#ifdef FTS_SUPPORT_MAINSCREEN_DISBLE
static void set_mainscreen_disable(void *device_data);
#endif
static void scrub_enable(void *device_data);
static void spay_enable(void *device_data);
static void aod_enable(void *device_data);
static void delay(void *device_data);
static void debug(void *device_data);
static void run_autotune_enable(void *device_data);
static void run_force_calibration(void *device_data);
static void set_rotation_status(void *device_data);
#ifdef FTS_SUPPORT_TOUCH_KEY
static void run_key_cx_data_read(void *device_data);
static void run_key_cm_data_read(void *device_data);
static void run_key_rawcap_read(void *device_data);
#endif
static void not_support_cmd(void *device_data);

static ssize_t fts_scrub_position(struct device *dev,
				struct device_attribute *attr, char *buf);
#ifdef FTS_SUPPORT_EDGE_POSITION
static ssize_t fts_edge_x_position(struct device *dev,
				struct device_attribute *attr, char *buf);
#endif

static struct sec_cmd stm_ft_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_config_ver", get_config_ver),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("module_off_slave", not_support_cmd),},
	{SEC_CMD("module_on_slave", not_support_cmd),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_x_cross_routing", get_x_cross_routing),},
	{SEC_CMD("get_y_cross_routing", get_y_cross_routing),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("run_reference_read", run_reference_read),},
	{SEC_CMD("get_reference", get_reference),},
	{SEC_CMD("get_reference_all_data", get_reference_all_data),},
	{SEC_CMD("run_rawcap_read", run_rawcap_read),},
	{SEC_CMD("get_rawcap", get_rawcap),},
	{SEC_CMD("get_rawcap_all_data", get_rawcap_all_data),},
	{SEC_CMD("run_strength_read", run_strength_read),},
	{SEC_CMD("get_strength", get_strength),},
	{SEC_CMD("get_strength_all_data", get_strength_all_data),},
#ifdef FTS_SUPPORT_HOVER
	{SEC_CMD("run_abscap_read" , run_abscap_read),},
	{SEC_CMD("run_absdelta_read", run_absdelta_read),},
#endif
	{SEC_CMD("run_ix_data_read", run_ix_data_read),},
	{SEC_CMD("run_ix_data_read_all", run_ix_data_read_all),},
	{SEC_CMD("get_ix_all_data", run_ix_data_read_all),},
	{SEC_CMD("run_self_raw_read", run_self_raw_read),},
	{SEC_CMD("run_self_raw_read_all", run_self_raw_read_all),},
	{SEC_CMD("get_self_raw_all_data", run_self_raw_read_all),},
	{SEC_CMD("run_wtr_cx_data_read", run_wtr_cx_data_read),},
	{SEC_CMD("run_wtr_cx_data_read_all", run_wtr_cx_data_read_all),},
	{SEC_CMD("get_wtr_cx_all_data", run_wtr_cx_data_read_all),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("run_cx_data_read", run_cx_data_read),},
	{SEC_CMD("get_cx_data", get_cx_data),},
	{SEC_CMD("get_cx_all_data", get_cx_all_data),},
	{SEC_CMD("set_tsp_test_result", set_tsp_test_result),},
	{SEC_CMD("get_tsp_test_result", get_tsp_test_result),},
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
	{SEC_CMD("set_status_pureautotune", set_status_pureautotune),},
	{SEC_CMD("get_status_pureautotune", get_status_pureautotune),},
	{SEC_CMD("get_status_afe", get_status_afe),},
#endif
#ifdef FTS_SUPPORT_HOVER
	{SEC_CMD("hover_enable", hover_enable),},
#endif
#ifdef CONFIG_GLOVE_TOUCH
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("get_glove_sensitivity", get_glove_sensitivity),},
	{SEC_CMD("fast_glove_mode", fast_glove_mode),},
#endif
	{SEC_CMD("clear_cover_mode", clear_cover_mode),},
#ifdef USE_STYLUS_PEN
	{SEC_CMD("stylus_enable", stylus_enable),},
#endif
	{SEC_CMD("report_rate", report_rate),},
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	{SEC_CMD("interrupt_control", interrupt_control),},
#endif
	{SEC_CMD("set_lowpower_mode", set_lowpower_mode),},
	{SEC_CMD("set_deepsleep_mode", set_deepsleep_mode),},
	{SEC_CMD("active_sleep_enable", active_sleep_enable),},
	{SEC_CMD("set_wirelesscharger_mode", set_wirelesscharger_mode),},
	{SEC_CMD("select_wakeful_edge", select_wakeful_edge),},
#if defined(TSP_BOOSTER) && !defined(INPUT_BOOSTER_SYSFS)
	{SEC_CMD("boost_level", boost_level),},
#endif
	{SEC_CMD("second_screen_enable", second_screen_enable),},
	{SEC_CMD("set_longpress_enable", set_longpress_enable),},
	{SEC_CMD("set_grip_detection", set_grip_detection),},
	{SEC_CMD("set_sidescreen_x_length", set_sidescreen_x_length),},
	{SEC_CMD("set_dead_zone", set_dead_zone),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("set_tunning_data", set_tunning_data),},
#ifdef SMARTCOVER_COVER
	{SEC_CMD("smartcover_cmd", smartcover_cmd),},
#endif
#ifdef FTS_SUPPORT_MAINSCREEN_DISBLE
	{SEC_CMD("set_mainscreen_disable", set_mainscreen_disable),},
#endif
	{SEC_CMD("scrub_enable", scrub_enable),},
	{SEC_CMD("spay_enable", spay_enable),},
	{SEC_CMD("aod_enable", aod_enable),},
	{SEC_CMD("delay", delay),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("run_autotune_enable", run_autotune_enable),},
	{SEC_CMD("run_force_calibration", run_force_calibration),},
	{SEC_CMD("set_rotation_status", set_rotation_status),},
#ifdef FTS_SUPPORT_TOUCH_KEY
	{SEC_CMD("run_key_cx_data_read", run_key_cx_data_read),},
	{SEC_CMD("run_key_cm_data_read", run_key_cm_data_read),},
	{SEC_CMD("run_key_rawcap_read", run_key_rawcap_read),},
#endif
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static DEVICE_ATTR(scrub_pos, S_IRUGO, fts_scrub_position, NULL);
#ifdef FTS_SUPPORT_EDGE_POSITION
static DEVICE_ATTR(edge_pos, S_IRUGO, fts_edge_x_position, NULL);
#endif

static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_scrub_pos.attr,
#ifdef FTS_SUPPORT_EDGE_POSITION
	&dev_attr_edge_pos.attr,
#endif
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};

static int fts_check_index(struct fts_ts_info *info)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int node;

	if (sec->cmd_param[0] < 0
		|| sec->cmd_param[0] >= info->SenseChannelLength
		|| sec->cmd_param[1] < 0
		|| sec->cmd_param[1] >= info->ForceChannelLength) {

		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		tsp_debug_info(true, &info->client->dev, "%s: parameter error: %u,%u\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1]);
		node = -1;
		return node;
	}
	node = sec->cmd_param[1] * info->SenseChannelLength + sec->cmd_param[0];
	tsp_debug_info(true, &info->client->dev, "%s: node = %d\n", __func__, node);
	return node;
}

static ssize_t fts_scrub_position(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	if (!info) {
		printk(KERN_ERR "%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	if (!info->input_dev) {
		printk(KERN_ERR "%s: No input_dev data found\n",
				__func__);
		return -EINVAL;
	}

	tsp_debug_info(true, &info->client->dev, "%s: %d %d %d\n",
				__func__, info->scrub_id, info->scrub_x, info->scrub_y);
	snprintf(buff, sizeof(buff), "%d %d %d", info->scrub_id, info->scrub_x, info->scrub_y);

	info->scrub_id = 0;
	info->scrub_x = 0;
	info->scrub_y = 0;

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);

}

#ifdef FTS_SUPPORT_EDGE_POSITION
static ssize_t fts_edge_x_position(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int edge_position_left, edge_position_right;

	if (!info) {
		pr_err("%s: No platform data found\n",
				__func__);
		return -EINVAL;
	}

	if (!info->input_dev) {
		pr_err("%s: No input_dev data found\n",
				__func__);
		return -EINVAL;
	}

	edge_position_left = info->dt_data->grip_area;
	edge_position_right = info->dt_data->max_x + 1 - info->dt_data->grip_area;

	tsp_debug_info(true, &info->client->dev, "%s: %d,%d\n", __func__,
			edge_position_left, edge_position_right);
	snprintf(buff, sizeof(buff), "%d,%d", edge_position_left, edge_position_right);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);

}
#endif

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: \"%s(%d)\"\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[64] = { 0 };
	int retval = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	retval = fts_fw_update_on_hidden_menu(info, sec->cmd_param[0]);

	if (retval < 0) {
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		tsp_debug_info(true, &info->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		tsp_debug_info(true, &info->client->dev, "%s: success [%d]\n", __func__, retval);
	}

	return;
}

static int fts_get_channel_info(struct fts_ts_info *info)
{
	int rc = -1;
	unsigned char cmd[4] = { 0xB2, 0x00, 0x14, 0x02 };
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;

	/* Rx/Tx channel information routine is using shared buffer with touch event block.
	 * So, when get Rx/Tx channel information, disable/enable interrupt(event block) */
	fts_interrupt_set(info, INT_DISABLE);
	fts_release_all_finger(info);

	memset(data, 0x0, FTS_EVENT_SIZE);
	fts_command(info, FLUSHBUFFER);
	fts_delay(10);

	fts_write_reg(info, &cmd[0], 4);
	cmd[0] = READ_ONE_EVENT;
	while (fts_read_reg
		(info, &cmd[0], 1, (unsigned char *)data, FTS_EVENT_SIZE)) {

		if ((data[0] == EVENTID_RESULT_READ_REGISTER)
			&& (data[1] == cmd[1]) && (data[2] == cmd[2])) {
			info->SenseChannelLength = data[3];
			info->ForceChannelLength = data[4];

			rc = 0;
			break;
		}

		if (retry++ > 30) {
			rc = -1;
			tsp_debug_info(true, &info->client->dev, "Time over - wait for channel info\n");
			break;
		}
		fts_delay(5);
	}

	fts_interrupt_set(info, INT_ENABLE);

	dev_info(&info->client->dev, "%s: SenseChannel: %02d, Force Channel: %02d\n",
			__func__, info->SenseChannelLength, info->ForceChannelLength);

	return rc;
}

static void fts_print_frame(struct fts_ts_info *info, short *min, short *max)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	pStr = kzalloc(6 * (info->SenseChannelLength + 1), GFP_KERNEL);
	if (pStr == NULL) {
		tsp_debug_info(true, &info->client->dev, "FTS pStr kzalloc failed\n");
		return;
	}

	memset(pStr, 0x0, 6 * (info->SenseChannelLength + 1));
	snprintf(pTmp, sizeof(pTmp), "    ");
	strncat(pStr, pTmp, 6 * info->SenseChannelLength);

	for (i = 0; i < info->SenseChannelLength; i++) {
		snprintf(pTmp, sizeof(pTmp), "Rx%02d  ", i);
		strncat(pStr, pTmp, 6 * info->SenseChannelLength);
	}

	tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);
	memset(pStr, 0x0, 6 * (info->SenseChannelLength + 1));
	snprintf(pTmp, sizeof(pTmp), " +");
	strncat(pStr, pTmp, 6 * info->SenseChannelLength);

	for (i = 0; i < info->SenseChannelLength; i++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strncat(pStr, pTmp, 6 * info->SenseChannelLength);
	}

	tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);

	for (i = 0; i < info->ForceChannelLength; i++) {
		memset(pStr, 0x0, 6 * (info->SenseChannelLength + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", i);
		strncat(pStr, pTmp, 6 * info->SenseChannelLength);

		for (j = 0; j < info->SenseChannelLength; j++) {
			snprintf(pTmp, sizeof(pTmp), "%5d ", info->pFrame[(i * info->SenseChannelLength) + j]);

			if (i > 0) {
				if (info->pFrame[(i * info->SenseChannelLength) + j] < *min)
					*min = info->pFrame[(i * info->SenseChannelLength) + j];

				if (info->pFrame[(i * info->SenseChannelLength) + j] > *max)
					*max = info->pFrame[(i * info->SenseChannelLength) + j];
			}
			strncat(pStr, pTmp, 6 * info->SenseChannelLength);
		}
		tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);
	}

	kfree(pStr);
}

static int fts_read_frame(struct fts_ts_info *info, unsigned char type, short *min,
		 short *max)
{
	unsigned char pFrameAddress[8] = { 0xD0, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00 };
	unsigned int FrameAddress = 0;
	unsigned int writeAddr = 0;
	unsigned int start_addr = 0;
	unsigned int end_addr = 0;
	unsigned int totalbytes = 0;
	unsigned int remained = 0;
	unsigned int readbytes = 0xFF;
	unsigned int dataposition = 0;
	unsigned char *pRead = NULL;
	int rc = 0;
	int ret = 0;
	int i = 0;
	pRead = kzalloc(BUFFER_MAX, GFP_KERNEL);

	if (pRead == NULL) {
		tsp_debug_info(true, &info->client->dev, "FTS pRead kzalloc failed\n");
		rc = 1;
		goto ErrorExit;
	}

	pFrameAddress[2] = type;
	totalbytes = info->SenseChannelLength * info->ForceChannelLength * 2;
	ret = fts_read_reg(info, &pFrameAddress[0], 3, pRead, pFrameAddress[3]);

	if (ret >= 0) {
		if (info->digital_rev == FTS_DIGITAL_REV_1)
			FrameAddress = pRead[0] + (pRead[1] << 8);
		else if ((info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3))
			FrameAddress = pRead[1] + (pRead[2] << 8);

		start_addr = FrameAddress+info->SenseChannelLength * 2;
		end_addr = start_addr + totalbytes;
	} else {
		tsp_debug_info(true, &info->client->dev, "FTS read failed rc = %d \n", ret);
		rc = 2;
		goto ErrorExit;
	}

#ifdef DEBUG_MSG
	tsp_debug_info(true, &info->client->dev,
			"FTS FrameAddress = %X\n", FrameAddress);
	tsp_debug_info(true, &info->client->dev,
			"FTS start_addr = %X, end_addr = %X\n", start_addr, end_addr);
#endif

	remained = totalbytes;
	for (writeAddr = start_addr; writeAddr < end_addr; writeAddr += READ_CHUNK_SIZE) {
		pFrameAddress[1] = (writeAddr >> 8) & 0xFF;
		pFrameAddress[2] = writeAddr & 0xFF;

		if (remained >= READ_CHUNK_SIZE)
			readbytes = READ_CHUNK_SIZE;
		else
			readbytes = remained;

		memset(pRead, 0x0, readbytes);

#ifdef DEBUG_MSG
		tsp_debug_info(true, &info->client->dev, "FTS %02X%02X%02X readbytes=%d\n",
			   pFrameAddress[0], pFrameAddress[1],
			   pFrameAddress[2], readbytes);

#endif
		if (info->digital_rev == FTS_DIGITAL_REV_1) {
			fts_read_reg(info, &pFrameAddress[0], 3, pRead, readbytes);
			remained -= readbytes;

			for (i = 0; i < readbytes; i += 2) {
				info->pFrame[dataposition++] =
				pRead[i] + (pRead[i + 1] << 8);
			}
		} else if ((info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3)) {
			fts_read_reg(info, &pFrameAddress[0], 3, pRead, readbytes + 1);
			remained -= readbytes;

			for (i = 1; i < (readbytes+1); i += 2) {
				info->pFrame[dataposition++] =
				pRead[i] + (pRead[i + 1] << 8);
			}
		}
	}
	kfree(pRead);

#ifdef DEBUG_MSG
	tsp_debug_info(true, &info->client->dev,
		   "FTS writeAddr = %X, start_addr = %X, end_addr = %X\n",
		   writeAddr, start_addr, end_addr);
#endif

	switch (type) {
	case TYPE_RAW_DATA:
		tsp_debug_info(true, &info->client->dev, "FTS [Raw Data : 0x%X%X]\n", pFrameAddress[0],
			FrameAddress);
		break;
	case TYPE_FILTERED_DATA:
		tsp_debug_info(true, &info->client->dev, "FTS [Filtered Data : 0x%X%X]\n",
			pFrameAddress[0], FrameAddress);
		break;
	case TYPE_STRENGTH_DATA:
		tsp_debug_info(true, &info->client->dev, "FTS [Strength Data : 0x%X%X]\n",
			pFrameAddress[0], FrameAddress);
		break;
	case TYPE_BASELINE_DATA:
		tsp_debug_info(true, &info->client->dev, "FTS [Baseline Data : 0x%X%X]\n",
			pFrameAddress[0], FrameAddress);
		break;
	}
	fts_print_frame(info, min, max);

ErrorExit:
	return rc;
}

static int fts_panel_ito_test(struct fts_ts_info *info)
{
	unsigned char cmd = READ_ONE_EVENT;
	unsigned char data[FTS_EVENT_SIZE];
	unsigned char regAdd[4] = {0xB0, 0x03, 0x60, 0xFB};
	int retry = 0;
	int result = -1;

	info->fts_systemreset(info);
	info->fts_wait_for_ready(info);

	if (info->stm_ver != STM_VER7) {
		fts_command(info, SLEEPOUT);
		fts_delay(20);
	}

	fts_interrupt_set(info, INT_DISABLE);
	fts_irq_enable(info, false);
	fts_release_all_finger(info);
	if (info->stm_ver != STM_VER7)
		fts_write_reg(info, &regAdd[0], 4);

	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	fts_command(info, 0xA7);
	fts_delay(200);
	memset(data, 0x0, FTS_EVENT_SIZE);
	while (fts_read_reg
			(info, &cmd, 1, (unsigned char *)data, FTS_EVENT_SIZE)) {

		if ((data[0] == 0x0F) && (data[1] == 0x05)) {
			switch (data[2]) {
			case 0x00:
				result = 0;
				break;
			case 0x01:
				tsp_debug_info(true, &info->client->dev,
					"[FTS] ITO Test result : Force channel [%d] open.\n",
					data[3]);
				break;
			case 0x02:
				tsp_debug_info(true, &info->client->dev,
					"[FTS] ITO Test result : Sense channel [%d] open.\n",
					data[3]);
				break;
			case 0x03:
				tsp_debug_info(true, &info->client->dev,
					"[FTS] ITO Test result : Force channel [%d] short to GND.\n",
					data[3]);
				break;
			case 0x04:
				tsp_debug_info(true, &info->client->dev,
					"[FTS] ITO Test result : Sense channel [%d] short to GND.\n",
					data[3]);
				break;
			case 0x07:
				tsp_debug_info(true, &info->client->dev,
					"[FTS] ITO Test result : Force channel [%d] short to force.\n",
					data[3]);
				break;
			case 0x08:
				tsp_debug_info(true, &info->client->dev,
					"[FTS] ITO Test result : Sennse channel [%d] short to sense.\n",
					data[3]);
				break;
			case 0x0E:
				tsp_debug_info(true, &info->client->dev,
					"[FTS] ITO Test result : Sennse channel [%d] short to sense.\n",
					data[3]);
				break;
			default:
				tsp_debug_info(true, &info->client->dev,
					"[FTS] ITO Test result : 0x%02x, %d\n",
					data[2], data[3]);
				break;
			}

			break;
		}

		if (retry++ > 30) {
			tsp_debug_info(true, &info->client->dev,
				"Time over - wait for result of ITO test\n");
			break;
		}

		fts_delay(10);
	}

	fts_systemreset(info);

	/* wait for ready event */
	info->fts_wait_for_ready(info);

#ifdef FTS_SUPPORT_NOISE_PARAM
	fts_set_noise_param(info);
#endif
	if (info->stm_ver != STM_VER7)
		fts_command(info, SLEEPOUT);

	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	fts_command(info, SENSEON);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif
#ifdef FTS_SUPPORT_HOVER
	if (info->hover_enabled)
		fts_command(info, FTS_CMD_HOVER_ON);
#endif
	if (info->flip_enable) {
		fts_set_cover_type(info, true);
	}
#ifdef CONFIG_GLOVE_TOUCH
	else {
		if (info->glove_enabled)
			fts_command(info, FTS_CMD_GLOVE_MODE_ON);
	}
#endif
#ifdef FTS_SUPPORT_TA_MODE
	if (info->TA_Pluged)
		fts_command(info, FTS_CMD_CHARGER_PLUGGED);
#endif

	info->touch_count = 0;

	fts_interrupt_set(info, INT_ENABLE);
	fts_irq_enable(info, true);

	return result;
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, SEC_CMD_STR_LEN, "ST%02X%04X",
			info->panel_revision,
			info->fw_main_version_of_bin);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, SEC_CMD_STR_LEN, "ST%02X%04X",
			info->panel_revision,
			info->fw_main_version_of_ic);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[20] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s_ST_%04X",
		info->dt_data->model,
		info->config_version_of_ic);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned char cmd[4] = { 0xB2, 0x01, 0x62, 0x02 };
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;

	sec_cmd_set_default_result(sec);

	if (info->stm_ver == STM_VER7) {
		cmd[1] = 0x01;
		cmd[2] = 0x8D;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_interrupt_set(info, INT_DISABLE);
	fts_release_all_finger(info);

	fts_command(info, FLUSHBUFFER);
	fts_delay(10);

	fts_write_reg(info, &cmd[0], 4);

	memset(data, 0x0, FTS_EVENT_SIZE);
	cmd[0] = READ_ONE_EVENT;

	while (info->fts_read_reg(info, &cmd[0], 1, (unsigned char *)data,
		FTS_EVENT_SIZE)) {

		if ((data[0] == EVENTID_RESULT_READ_REGISTER)
			&& (data[1] == cmd[1]) && (data[2] == cmd[2])) {
			snprintf(buff, sizeof(buff), "%d",
						*(unsigned short *)&data[3]);
			tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
						(int)strnlen(buff, sizeof(buff)));
			sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
			sec->cmd_state = SEC_CMD_STATUS_OK;
			break;
		}

		if (retry++ > 30) {
			tsp_debug_err(true, info->dev,
				"%s: Time Over\n", __func__);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			break;
		}
	}

	fts_interrupt_set(info, INT_ENABLE);
}

static void module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[3] = { 0 };

	sec_cmd_set_default_result(sec);

	fts_irq_enable(info, false);
	info->fts_power_ctrl(info->client, false);

	snprintf(buff, sizeof(buff), "%s", "OK");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[3] = { 0 };

	sec_cmd_set_default_result(sec);

	fts_irq_enable(info, true);
	info->fts_power_ctrl(info->client, true);

	snprintf(buff, sizeof(buff), "%s", "OK");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	strncpy(buff, "STM", sizeof(buff));
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (strncmp(info->dt_data->project, "ZERO", 4) == 0) {
		if (info->ic_product_id)
			strncpy(buff, "FTS4BD056", sizeof(buff));
		else
			strncpy(buff, "FTS4BD062", sizeof(buff));
	} else if (strncmp(info->dt_data->project, "HERO", 4) == 0) {
		strncpy(buff, "FTS7BD50", sizeof(buff));
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", info->SenseChannelLength);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", info->ForceChannelLength);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_x_cross_routing(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_y_cross_routing(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };
/*
	int ret = 0;
*/
	sec_cmd_set_default_result(sec);

/*
	ret = strncmp(info->dt_data->model, "G935", 4);
	if (ret == 0)
		snprintf(buff, sizeof(buff), "13,14");
	else
		snprintf(buff, sizeof(buff), "NG");
*/
	snprintf(buff, sizeof(buff), "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[16] = { 0 };
	int rc;
	int retry = 0;
	unsigned char regAdd[4];
	unsigned char cmd[1];
	unsigned char buf[5] = {0};
	unsigned char data[FTS_EVENT_SIZE];

	sec_cmd_set_default_result(sec);

	fts_interrupt_set(info, INT_DISABLE);

	if (info->stm_ver != STM_VER7) {
		regAdd[0] = 0xb3;
		regAdd[1] = 0x00;
		regAdd[2] = 0x01;
		info->fts_write_reg(info, regAdd, 3);
		fts_delay(1);

		regAdd[0] = 0xb1;
		regAdd[1] = 0xEF;
		regAdd[2] = 0xFC;
		rc = info->fts_read_reg(info, regAdd, 3, buf, 5);
	} else {
		fts_release_all_finger(info);
		fts_command(info, FLUSHBUFFER);
		fts_delay(10);

		regAdd[0] = 0xb2;
		regAdd[1] = 0x07;
		regAdd[2] = 0xfc;
		regAdd[3] = 0x05;
		info->fts_write_reg(info, regAdd, 4);
		fts_delay(1);

		memset(data, 0x0, FTS_EVENT_SIZE);
		cmd[0] = READ_ONE_EVENT;

		while (info->fts_read_reg(info, &cmd[0], 1, (unsigned char *)data,
			FTS_EVENT_SIZE)) {
			if ((data[0] == EVENTID_RESULT_READ_REGISTER)
				&& (data[1] == regAdd[1]) && (data[2] == regAdd[2])) {
				rc = 0;
				buf[1] = data[3];
				buf[2] = data[4];
				buf[3] = data[5];
				buf[4] = data[6];
				break;
			}

			if (retry++ > FTS_RETRY_COUNT) {
				tsp_debug_err(true, info->dev,
					"%s: Time Over\n", __func__);
				break;
			}
		}
	}

	fts_interrupt_set(info, INT_ENABLE);

	snprintf(buff, sizeof(buff), "%02X%02X%02X%02X", buf[1], buf[2], buf[3], buf[4]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_all_data(struct fts_ts_info *info, run_func_t run_func, void *data, enum data_type type)
{
	struct sec_cmd_data *sec = &info->sec;
	char mbuff[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	int node_num;
	int page_size, len;
	static int index = 0;

	sec_cmd_set_default_result(sec);
	info->get_all_data = true;

	if (!data) {
		tsp_debug_info(true, &info->client->dev, "%s: data is NULL\n",
			__func__);
		snprintf(mbuff, sizeof(mbuff), "%s", "FAIL");
		sec_cmd_set_cmd_result(sec, mbuff, strnlen(mbuff, sizeof(mbuff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto all_data_out;
	}

	if (sec->cmd_param[0] == 0) {
		run_func(sec);
		if (sec->cmd_state != SEC_CMD_STATUS_RUNNING)
			goto all_data_out;
		index = 0;
	} else {
		if (index == 0) {
			tsp_debug_info(true, &info->client->dev,
				"%s: Please do cmd_param '0' first\n",
				__func__);
			snprintf(mbuff, sizeof(mbuff), "%s", "NG");
			sec_cmd_set_cmd_result(sec, mbuff, strnlen(mbuff, sizeof(mbuff)));
			sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
			goto all_data_out;
		}
	}

	page_size = SEC_CMD_RESULT_STR_LEN - strlen(sec->cmd) - 10;
	node_num = info->ForceChannelLength * info->SenseChannelLength;
	buff = kzalloc(SEC_CMD_RESULT_STR_LEN, GFP_KERNEL);
	if (buff != NULL) {
		char *pBuf = buff;
		for (; index < node_num; index++) {
			switch(type) {
			case DATA_UNSIGNED_CHAR: {
				unsigned char *ddata = data;
				len = snprintf(pBuf, 5, "%u,", ddata[index]);
				break;}
			case DATA_SIGNED_CHAR: {
				char *ddata = data;
				len = snprintf(pBuf, 5, "%d,", ddata[index]);
				break;}
			case DATA_UNSIGNED_SHORT: {
				unsigned short *ddata = data;
				len = snprintf(pBuf, 10, "%u,", ddata[index]);
				break;}
			case DATA_SIGNED_SHORT: {
				short *ddata = data;
				len = snprintf(pBuf, 10, "%d,", ddata[index]);
				break;}
			case DATA_UNSIGNED_INT: {
				unsigned int *ddata = data;
				len = snprintf(pBuf, 15, "%u,", ddata[index]);
				break;}
			case DATA_SIGNED_INT: {
				int *ddata = data;
				len = snprintf(pBuf, 15, "%d,", ddata[index]);
				break;}
			default:
				tsp_debug_info(true, &info->client->dev,
					"%s: not defined data type\n",
					__func__);
				snprintf(mbuff, sizeof(mbuff), "%s", "NG");
				sec_cmd_set_cmd_result(sec, mbuff, strnlen(mbuff, sizeof(mbuff)));
				sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
				kfree(buff);
				goto all_data_out;
			}

			if (page_size - len < 0) {
				snprintf(pBuf, 5, "cont");
				break;
			} else {
				page_size -= len;
				pBuf += len;
			}
		}
		if (index == node_num)
			index = 0;

		sec_cmd_set_cmd_result(sec, buff, SEC_CMD_RESULT_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

		kfree(buff);
	} else {
		snprintf(mbuff, sizeof(mbuff), "%s", "kzalloc failed");
		sec_cmd_set_cmd_result(sec, mbuff, strnlen(mbuff, sizeof(mbuff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	}

all_data_out:
	info->get_all_data = false;
}

static void run_reference_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		char buff[SEC_CMD_STR_LEN] = { 0 };
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_BASELINE_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	if (!info->get_all_data) {
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_reference(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_reference_all_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	get_all_data(info, run_reference_read, info->pFrame, DATA_SIGNED_SHORT);
}

static void run_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
	//unsigned char regAdd[4] = {0xB0, 0x04, 0x49, 0x00}; // it's for Zero Prj
	//unsigned char regAdd[4] = {0xB0, 0x04, 0x48, 0x00}; // it's for Noble & Zero2 Prj
	unsigned char regAdd[4] = {0xB0, 0x03, 0x17, 0x00}; // it's for Hero2 Prj
#endif

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->run_autotune) {
		if (((info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3))
#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
			&& (info->tsp_dump_lock != 1)
#endif
			) {
			tsp_debug_info(true, &info->client->dev, "%s: set autotune\n\n", __func__);

			fts_irq_enable(info, false);
			fts_interrupt_set(info, INT_DISABLE);

			fts_command(info, SENSEOFF);
			fts_delay(50);
#ifdef FTS_SUPPORT_TOUCH_KEY
			if (info->dt_data->support_mskey)
				fts_command(info, FTS_CMD_KEY_SENSE_OFF); // Key Sensor OFF
#endif
			fts_command(info, FLUSHBUFFER);

			fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
			if (info->dt_data->support_mskey)
				fts_release_all_key(info);
#endif

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
			info->o_afe_ver = info->afe_ver;
#endif

			fts_execute_autotune(info);

			// Added mutual auto tune
			fts_command(info, CX_TUNNING);
			msleep(300);
			fts_fw_wait_for_event_D3(info, STATUS_EVENT_MUTUAL_AUTOTUNE_DONE, 0x00);

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
			//STMicro Auto-tune protection disable
			fts_write_reg(info, regAdd, 4);
			fts_delay(1);
#endif
			/* Command "SLEEPOUT" is not used from FTSD3. */
			if (info->stm_ver != STM_VER7) {
				fts_command(info, SLEEPOUT);
				fts_delay(1);
			}

			fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_WATER_MODE
			fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
			if( info->stm_ver == STM_VER7)
				fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE_D3);
			else
				fts_fw_wait_for_event (info, STATUS_EVENT_FORCE_CAL_DONE);
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
			if (info->dt_data->support_mskey)
				fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

			fts_interrupt_set(info, INT_ENABLE);
			fts_irq_enable(info, true);
		}
	}

	fts_read_frame(info, TYPE_FILTERED_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	if (!info->get_all_data) {
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_rawcap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_rawcap_all_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	get_all_data(info, run_rawcap_read, info->pFrame, DATA_SIGNED_SHORT);
}

static void run_strength_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_read_frame(info, TYPE_STRENGTH_DATA, &min, &max);
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	if (!info->get_all_data) {
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_strength(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->pFrame[node];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));
}

static void get_strength_all_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	get_all_data(info, run_strength_read, info->pFrame, DATA_SIGNED_SHORT);
}

#ifdef FTS_SUPPORT_HOVER
static void fts_read_self_frame(struct fts_ts_info *info, unsigned short oAddr)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[66] = {0, };
	short *data = 0;
	char temp[9] = {0, };
	char temp2[512] = {0, };
	int i;
	int rc;
	int retry = 1;
	unsigned char regAdd[6] = {0xD0, 0x00, 0x00, 0xD0, 0x00, 0x00};

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (!info->hover_enabled) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Hover is disabled\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP Hover disabled");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	while (!info->hover_ready) {
		if (retry++ > 100) {
			tsp_debug_info(true, &info->client->dev, "%s: [FTS] Timeout - Abs Raw Data Ready Event\n",
					  __func__);
			break;
		}
		fts_delay(10);
	}

	regAdd[1] = (oAddr >> 8) & 0xff;
	regAdd[2] = oAddr & 0xff;
	rc = info->fts_read_reg(info, &regAdd[0], 3, (unsigned char *)&buff[0], 5);
	if (!rc) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_1) {
		tsp_debug_info(true, &info->client->dev, "%s: Force Address : %02x%02x\n",
				__func__, buff[1], buff[0]);
		tsp_debug_info(true, &info->client->dev, "%s: Sense Address : %02x%02x\n",
				__func__, buff[3], buff[2]);
		regAdd[1] = buff[3];
		regAdd[2] = buff[2];
		regAdd[4] = buff[1];
		regAdd[5] = buff[0];
	} else if ((info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3)) {
		tsp_debug_info(true, &info->client->dev, "%s: Force Address : %02x%02x\n",
				__func__, buff[2], buff[1]);
		tsp_debug_info(true, &info->client->dev, "%s: Sense Address : %02x%02x\n",
				__func__, buff[4], buff[3]);
		regAdd[1] = buff[4];
		regAdd[2] = buff[3];
		regAdd[4] = buff[2];
		regAdd[5] = buff[1];
	}

	rc = info->fts_read_reg(info, &regAdd[0], 3,
				(unsigned char *)&buff[0],
				info->SenseChannelLength * 2 + 1);
	if (!rc) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_1)
		data = (short *)&buff[0];
	else
		data = (short *)&buff[1];

	memset(temp2, 0, 512);

	for (i = 0; i < info->SenseChannelLength; i++) {
		tsp_debug_info(true, &info->client->dev,
				"%s: Rx [%d] = %d\n", __func__,
				i,
				*data);
		memset(temp, 0, 9);
		snprintf(temp, SEC_CMD_STR_LEN, "%d,", *data);
		strncat(temp2, temp, 9);
		data++;
	}

	rc = info->fts_read_reg(info, &regAdd[3], 3,
				(unsigned char *)&buff[0],
				info->ForceChannelLength*2 + 1);
	if (!rc) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_1)
		data = (short *)&buff[0];
	else
		data = (short *)&buff[1];

	for (i = 0; i < info->ForceChannelLength; i++) {
		tsp_debug_info(true, &info->client->dev,
				"%s: Tx [%d] = %d\n", __func__, i, *data);
		snprintf(temp, SEC_CMD_STR_LEN, "%d,", *data);
		strncat(temp2, temp, 9);
		data++;
	}

	sec_cmd_set_cmd_result(sec, temp2, strnlen(temp2, sizeof(temp2)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void run_abscap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_self_frame(info, 0x000E);
}

static void run_absdelta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_self_frame(info, 0x0012);
}
#endif

#define FTS_F_WIX1_ADDR	0x1FE7
#define FTS_S_WIX1_ADDR	0x1FE8
#define FTS_F_WIX2_ADDR	0x18FD
#define FTS_S_WIX2_ADDR	0x1929
#define FTS_WATER_SELF_RAW_ADDR	0x1A

static void fts_read_ix_data(struct fts_ts_info *info, bool allnode)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	unsigned short max_tx_ix_sum = 0;
	unsigned short min_tx_ix_sum = 0xFFFF;

	unsigned short max_rx_ix_sum = 0;
	unsigned short min_rx_ix_sum = 0xFFFF;

	unsigned char tx_ix2[info->ForceChannelLength + 4];
	unsigned char rx_ix2[info->SenseChannelLength + 4];

	unsigned char regAdd[FTS_EVENT_SIZE];
	unsigned short tx_ix1 = 0, rx_ix1 = 0;

	unsigned short force_ix_data[info->ForceChannelLength * 2 + 1];
	unsigned short sense_ix_data[info->SenseChannelLength * 2 + 1];

	int buff_size, i = 0;
	char *mbuff = NULL;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
		       __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

//	fts_command(info, SLEEPIN); // Sleep In for INT disable

	fts_irq_enable(info, false);
	fts_interrupt_set(info, INT_DISABLE);
	fts_release_all_finger(info);

	if (info->stm_ver == STM_VER7) {
		fts_command(info, SENSEOFF);
	} else {
		fts_systemreset(info);
		fts_delay(50);
		fts_wait_for_ready(info);

		fts_command(info, SLEEPOUT);

		fts_delay(50);
	}

	#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
	}
	#endif

	fts_command(info, FLUSHBUFFER);                 // Clear FIFO
	fts_delay(50);

	if (info->stm_ver != STM_VER7) {
		unsigned short ix1_addr = FTS_F_WIX1_ADDR;
		unsigned short ix2_tx_addr = FTS_F_WIX2_ADDR;
		unsigned short ix2_rx_addr = FTS_S_WIX2_ADDR;

		unsigned char buf[FTS_EVENT_SIZE] = {0};
		unsigned char r_addr = READ_ONE_EVENT;
		int retry = 0;

		regAdd[0] = 0xB2;
		regAdd[1] = (ix1_addr >> 8)&0xff;
		regAdd[2] = (ix1_addr&0xff);
		regAdd[3] = 0x04;

		fts_write_reg(info, &regAdd[0], 4);
		fts_delay(1);

		retry = FTS_RETRY_COUNT * 3;
		do {
			if (retry < 0) {
				tsp_debug_err(true, &info->client->dev, "%s: failed to compare buf,[%x][%x][%x][%x] break1!\n",
								__func__, buf[1], buf[2], regAdd[1], regAdd[2]);
				break;
			}
			fts_delay(10);
			fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
			retry--;
		} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

		//read fifo
		tx_ix1 = (short)buf[3] * 4;
		rx_ix1 = (short)buf[4] * 4;

		tsp_debug_info(true, &info->client->dev, "%s: tx_ix1 : %d, rx_ix1 : %d (%d, %d)\n",
					__func__, tx_ix1, rx_ix1, buf[3], buf[4]);

		regAdd[0] = 0xB2;
		regAdd[1] = (ix2_tx_addr >>8)&0xff;
		regAdd[2] = (ix2_tx_addr & 0xff);

		for (i = 0; i < info->ForceChannelLength / 4 + 1; i++) {
			fts_write_reg(info, &regAdd[0], 4);
			fts_delay(1);

			retry = FTS_RETRY_COUNT * 3;
			do {
				if (retry < 0) {
					tsp_debug_err(true, &info->client->dev, "%s: failed to compare buf,[%x][%x][%x][%x] break2!\n",
								__func__, buf[1], buf[2], regAdd[1], regAdd[2]);
					break;
				}
				fts_delay(10);
				fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
				retry--;
			} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

			//read fifo
			tx_ix2[i * 4] = buf[3];
			tx_ix2[i * 4 + 1] = buf[4];
			tx_ix2[i * 4 + 2] = buf[5];
			tx_ix2[i * 4 + 3] = buf[6];

			ix2_tx_addr += 4;
			regAdd[0] = 0xB2;
			regAdd[1] = (ix2_tx_addr >> 8) & 0xff;
			regAdd[2] = (ix2_tx_addr & 0xff);
		}

		regAdd[0] = 0xB2;
		regAdd[1] = (ix2_rx_addr >> 8) & 0xff;
		regAdd[2] = (ix2_rx_addr & 0xff);

		for (i = 0; i < info->SenseChannelLength / 4 + 1;i++) {
			fts_write_reg(info, &regAdd[0], 4);
			fts_delay(1);

			retry = FTS_RETRY_COUNT * 3;
			do {
				if (retry < 0) {
					tsp_debug_err(true, &info->client->dev, "%s: failed to compare buf,[%x][%x][%x][%x] break3!\n",
								__func__, buf[1], buf[2], regAdd[1], regAdd[2]);

					break;
				}
				fts_delay(10);
				fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
				retry--;
			} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

			//read fifo
			rx_ix2[i * 4] = buf[3];
			rx_ix2[i * 4 + 1] = buf[4];
			rx_ix2[i * 4 + 2] = buf[5];
			rx_ix2[i * 4 + 3] = buf[6];

			ix2_rx_addr += 4;
			regAdd[0] = 0xB2;
			regAdd[1] = (ix2_rx_addr >> 8) & 0xff;
			regAdd[2] = (ix2_rx_addr & 0xff);
		}

		for (i = 0; i < info->ForceChannelLength; i++) {
			force_ix_data[i] = tx_ix1 + tx_ix2[i];
			if (max_tx_ix_sum < tx_ix1 + tx_ix2[i])
				max_tx_ix_sum = tx_ix1 + tx_ix2[i];
			if (min_tx_ix_sum > tx_ix1 + tx_ix2[i])
				min_tx_ix_sum = tx_ix1 + tx_ix2[i];
		}

		for (i = 0; i < info->SenseChannelLength; i++) {
			sense_ix_data[i] = rx_ix1 + rx_ix2[i];
			if (max_rx_ix_sum < rx_ix1 + rx_ix2[i])
				max_rx_ix_sum = rx_ix1 + rx_ix2[i];
			if (min_rx_ix_sum > rx_ix1 + rx_ix2[i])
				min_rx_ix_sum = rx_ix1 + rx_ix2[i];
		}
	} else {
		int comp_header_addr, comp_start_tx_addr, comp_start_rx_addr;
		unsigned int rx_num, tx_num;

		/* Request compensation data */
		regAdd[0] = 0xB8;
		regAdd[1] = 0x20;		// SELF IX
		regAdd[2] = 0x00;
		fts_write_reg(info, &regAdd[0], 3);
		fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x20, 0x00);

		/* Read an address of compensation data */
		regAdd[0] = 0xD0;
		regAdd[1] = 0x00;
		regAdd[2] = 0x50;
		fts_read_reg(info, regAdd, 3, &buff[0], 4);
		comp_header_addr = buff[1] + (buff[2] << 8);

		/* Read header of compensation area */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_header_addr >> 8) & 0xFF;
		regAdd[2] = comp_header_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], 16 + 1);
		tx_num = buff[5];
		rx_num = buff[6];

		tx_ix1 = (short) buff[10] * 4;		// Self TX Ix1
		rx_ix1 = (short) buff[11] * 2;		// Self RX Ix1

		comp_start_tx_addr = comp_header_addr + 0x10;
		comp_start_rx_addr = comp_start_tx_addr + tx_num;

		memset(tx_ix2, 0x0, tx_num);
		memset(rx_ix2, 0x0, rx_num);

		/* Read Self TX Ix2 */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_start_tx_addr >> 8) & 0xFF;
		regAdd[2] = comp_start_tx_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &tx_ix2[0], tx_num + 1);

		/* Read Self RX Ix2 */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_start_rx_addr >> 8) & 0xFF;
		regAdd[2] = comp_start_rx_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &rx_ix2[0], rx_num + 1);

		for (i = 0; i < info->ForceChannelLength; i++) {
			force_ix_data[i] = tx_ix1 + tx_ix2[i + 1];
			if(max_tx_ix_sum < force_ix_data[i])
				max_tx_ix_sum = force_ix_data[i];
			if(min_tx_ix_sum > force_ix_data[i])
				min_tx_ix_sum = force_ix_data[i];
		}

		for (i = 0; i < info->SenseChannelLength; i++) {
			sense_ix_data[i] = rx_ix1 + rx_ix2[i + 1];
			if(max_rx_ix_sum < sense_ix_data[i])
				max_rx_ix_sum = sense_ix_data[i];
			if(min_rx_ix_sum > sense_ix_data[i])
				min_rx_ix_sum = sense_ix_data[i];
		}
	}

	tsp_debug_info(true, &info->client->dev, "%s MIN_TX_IX_SUM : %d MAX_TX_IX_SUM : %d\n",
				__func__, min_tx_ix_sum, max_tx_ix_sum );
	tsp_debug_info(true, &info->client->dev, "%s MIN_RX_IX_SUM : %d MAX_RX_IX_SUM : %d\n",
				__func__, min_rx_ix_sum, max_rx_ix_sum );

	fts_systemreset(info);
	fts_wait_for_ready(info);

	if (info->stm_ver != STM_VER7) {
		fts_command(info, SLEEPOUT);
		fts_delay(1);
	}
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_WATER_MODE
	fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
	if( info->stm_ver == STM_VER7)
		fts_fw_wait_for_event(info, STATUS_EVENT_FORCE_CAL_DONE_D3);
	else
		fts_fw_wait_for_event(info, STATUS_EVENT_FORCE_CAL_DONE);
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	fts_irq_enable(info, true);
	fts_interrupt_set(info, INT_ENABLE);

	if (allnode == true) {
		buff_size = (info->ForceChannelLength + info->SenseChannelLength + 2)*5;
		mbuff = kzalloc(buff_size, GFP_KERNEL);
	}
	if (mbuff != NULL) {
		char *pBuf = mbuff;
		int num, n, a, fzero, j;
		char cnum;
		for (i = 0; i < info->ForceChannelLength; i++) {
			num =  force_ix_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--){
				n = n / 10;
				a = num / n;
				if (a) fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if (fzero) *pBuf++ = cnum;
			}
			if (!fzero) *pBuf++ = '0';
			*pBuf++ = ',';
			tsp_debug_info(true, &info->client->dev, "%d ", force_ix_data[i]);
		}
		for (i = 0; i < info->SenseChannelLength; i++) {
			num =  sense_ix_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--) {
				n = n / 10;
				a = num/n;
				if (a) fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if (fzero) *pBuf++ = cnum;
			}
			if (!fzero) *pBuf++ = '0';
			if (i < (info->SenseChannelLength-1)) *pBuf++ = ',';
			tsp_debug_info(true, &info->client->dev, "%d ", sense_ix_data[i]);
		}

		sec_cmd_set_cmd_result(sec, mbuff, buff_size);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		kfree(mbuff);
	}
	else {
		if (allnode == true) {
		   snprintf(buff, sizeof(buff), "%s", "kzalloc failed");
		   sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		} else {
			snprintf(buff, sizeof(buff), "%d,%d,%d,%d", min_tx_ix_sum, max_tx_ix_sum, min_rx_ix_sum, max_rx_ix_sum);
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	}
}

static void run_ix_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_ix_data(info, false);
}

static void run_ix_data_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_ix_data(info, true);
}

static void fts_read_self_raw_frame(struct fts_ts_info *info, unsigned short oAddr, bool allnode)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[SEC_CMD_STR_LEN * 2] = { 0 };
	unsigned char D0_offset = 1;
	unsigned char regAdd[3] = {0xD0, 0x00, 0x00};
	unsigned char ReadData[info->SenseChannelLength * 2 + 1];
	unsigned short self_force_raw_data[info->ForceChannelLength * 2 + 1];
	unsigned short self_sense_raw_data[info->SenseChannelLength * 2 + 1];
	unsigned int FrameAddress = 0;
	unsigned char count=0;
	int buff_size,i,j;
	char *mbuff = NULL;
	int num,n,a,fzero;
	char cnum;
	unsigned short min_tx_self_raw_data = 0xFFFF;
	unsigned short max_tx_self_raw_data = 0;
	unsigned short min_rx_self_raw_data = 0xFFFF;
	unsigned short max_rx_self_raw_data = 0;
	unsigned char cmd[4] = {0xB0, 0x01, 0x3D, 0x0C};	// Don't enter to IDLE for FTS7

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_systemreset(info);
	fts_delay(50);
	fts_wait_for_ready(info);

	// Added self auto tune
	fts_command(info, SELF_AUTO_TUNE);
	msleep(300);
	fts_fw_wait_for_event_D3(info, STATUS_EVENT_SELF_AUTOTUNE_DONE_D3, 0x00);

	if (info->stm_ver != STM_VER7) {
		fts_command(info, SLEEPOUT);
		fts_delay(50);
	} else {
		fts_write_reg(info, &cmd[0], 4);
		fts_delay(50);
	}

	fts_command(info, SENSEON);
	fts_delay(50);

	fts_irq_enable(info, false);
	fts_interrupt_set(info, INT_DISABLE);
	fts_release_all_finger(info);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
	}
#endif

	fts_command(info, FLUSHBUFFER);                 // Clear FIFO
	fts_delay(50);

	regAdd[1] = 0x00;
	regAdd[2] = oAddr;
	fts_read_reg(info, regAdd, 3, &ReadData[0], 4);

	FrameAddress = ReadData[D0_offset] + (ReadData[D0_offset + 1] << 8);           // D1 : DOFFSET = 0, D2 : DOFFSET : 1

	regAdd[1] = (FrameAddress >> 8) & 0xFF;
	regAdd[2] = FrameAddress & 0xFF;

	fts_read_reg(info, regAdd, 3, &ReadData[0], info->ForceChannelLength * 2 + 1);

	for (count = 0; count < info->ForceChannelLength; count++) {
		self_force_raw_data[count] = ReadData[count * 2 + D0_offset]
					+ (ReadData[count * 2 + D0_offset + 1] << 8);

		if (max_tx_self_raw_data < self_force_raw_data[count])
			max_tx_self_raw_data = self_force_raw_data[count];
		if (min_tx_self_raw_data > self_force_raw_data[count])
			min_tx_self_raw_data = self_force_raw_data[count];
	}

	regAdd[1] = 0x00;
	regAdd[2] = oAddr + 2;
	fts_read_reg(info, regAdd, 3, &ReadData[0], 4);

	FrameAddress = ReadData[D0_offset] + (ReadData[D0_offset + 1] << 8);           // D1 : DOFFSET = 0, D2 : DOFFSET : 1

	regAdd[1] = (FrameAddress >> 8) & 0xFF;
	regAdd[2] = FrameAddress & 0xFF;

	fts_read_reg(info, regAdd, 3, &ReadData[0], info->SenseChannelLength * 2 + 1);

	for (count = 0; count < info->SenseChannelLength; count++) {
		self_sense_raw_data[count] = ReadData[count * 2 + D0_offset]
					+ (ReadData[count * 2 + D0_offset + 1] << 8);

		if (max_rx_self_raw_data < self_sense_raw_data[count])
			max_rx_self_raw_data = self_sense_raw_data[count];
		if (min_rx_self_raw_data > self_sense_raw_data[count])
			min_rx_self_raw_data = self_sense_raw_data[count];
	}

	tsp_debug_info(true, &info->client->dev, "%s MIN_TX_SELF_RAW: %d MAX_TX_SELF_RAW : %d\n",
				__func__, min_tx_self_raw_data, max_tx_self_raw_data );
	tsp_debug_info(true, &info->client->dev, "%s MIN_RX_SELF_RAW : %d MIN_RX_SELF_RAW : %d\n",
				__func__, min_rx_self_raw_data, max_rx_self_raw_data );

	if (info->stm_ver != STM_VER7) {
		fts_command(info, SLEEPOUT);
		fts_delay(1);
	}

#ifdef FTS_SUPPORT_WATER_MODE
	fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	fts_irq_enable(info, true);
	fts_interrupt_set(info, INT_ENABLE);

	tsp_debug_info(true, &info->client->dev, "%s ForceChannelLength: %d, SenseChannelLength: %d\n",
			__func__, info->ForceChannelLength, info->SenseChannelLength);

	if (allnode == true) {
		buff_size = (info->ForceChannelLength + info->SenseChannelLength + 2) * 6;
		mbuff = kzalloc(buff_size, GFP_KERNEL);
	}
	if (mbuff != NULL) {
		char *pBuf = mbuff;
		for (i = 0; i < info->ForceChannelLength; i++) {
			num =  self_force_raw_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--){
				n = n / 10;
				a = num / n;
				if (a) fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if (fzero)*pBuf++ = cnum;
			}
			if (!fzero) *pBuf++ = '0';
			*pBuf++ = ',';
			tsp_debug_info(true, &info->client->dev, "%d ", self_force_raw_data[i]);
		}
		for (i = 0; i < info->SenseChannelLength; i++) {
			num =  self_sense_raw_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--){
				n = n / 10;
				a = num / n;
				if (a) fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if (fzero) *pBuf++ = cnum;
			}
			if (!fzero) *pBuf++ = '0';
			if (i < (info->SenseChannelLength-1)) *pBuf++ = ',';
			tsp_debug_info(true, &info->client->dev, "%d ", self_sense_raw_data[i]);
		}


		sec_cmd_set_cmd_result(sec, mbuff, buff_size);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		kfree(mbuff);
	}
	else {
		if (allnode == true) {
			snprintf(buff, sizeof(buff), "%s", "kzalloc failed");
			sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		} else {
			snprintf(buff, sizeof(buff), "%d,%d,%d,%d", min_tx_self_raw_data, max_tx_self_raw_data, min_rx_self_raw_data, max_rx_self_raw_data);
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	}
}

static void run_self_raw_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_self_raw_frame(info, FTS_WATER_SELF_RAW_ADDR, false);
}

static void run_self_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);
	fts_read_self_raw_frame(info, FTS_WATER_SELF_RAW_ADDR, true);
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_panel_ito_test(info);
	if (ret == 0) {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
static void set_status_pureautotune(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int rc;
	unsigned char regAdd[2] = {0xC2, 0x0E};

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_command(info, SENSEOFF);
	fts_delay(50);

	fts_irq_enable(info, false);
	fts_interrupt_set(info, INT_DISABLE);
	fts_delay(50);

	if (sec->cmd_param[0]==0)
		regAdd[0] = 0xC2;
	else
		regAdd[0] = 0xC1;

	tsp_debug_info(true, &info->client->dev, "%s: CMD[%2X]\n",__func__,regAdd[0]);

	rc = fts_write_reg(info, regAdd, 2);
	msleep(20);
	if (info->stm_ver != STM_VER7)
		fts_fw_wait_for_event(info, STATUS_EVENT_PURE_AUTOTUNE_FLAG_ERASE_FINISH);

	info->fts_command(info, FTS_CMD_SAVE_CX_TUNING);
	msleep(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);

	info->fts_systemreset(info);
	fts_wait_for_ready(info);

	if (info->stm_ver != STM_VER7) {
		info->fts_command(info, SLEEPOUT);
		msleep(50);
	}
	info->fts_command(info, SENSEON);

	fts_irq_enable(info, true);
	fts_interrupt_set(info, INT_ENABLE);

	if (!rc) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	get_PureAutotune_status(info);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_status_pureautotune(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int rc;
	unsigned char regAdd[3];
	unsigned char buf[5];
	int retVal = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	regAdd[0] = 0xd0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x58;
	if (info->stm_ver == STM_VER7)
		regAdd[2] = 0x4E;
	rc = info->fts_read_reg(info, regAdd, 3, buf, 4);
	if (!rc) {
		tsp_debug_info(true, info->dev, "%s: PureAutotune Information Read Fail!! [Data : %2X%2X]\n", __func__, buf[1], buf[2]);
	} else {
		if((buf[1] == 0xA5) && (buf[2] == 0x96))
			retVal = 1;
		info->pat = retVal;
		tsp_debug_info(true, info->dev,
			"%s: PureAutotune Information !! [Data : %2X%2X]\n", __func__, buf[1], buf[2]);
	}
	snprintf(buff, sizeof(buff), "%d", retVal);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_status_afe(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int rc = 0;
	unsigned char regAdd[3];
	unsigned char buf[5];
	int retVal = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	regAdd[0] = 0xd0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x5A;
	if (info->stm_ver == STM_VER7)
		regAdd[2] = 0x52;
	rc = info->fts_read_reg(info, regAdd, 3, buf, 3);
	if (!rc) {
		tsp_debug_info(true, info->dev, "%s: Final AFE [Data : %2X] Read Fail AFE Ver [Data : %2X]\n", __func__, buf[1], buf[2]);
	}
	if (buf[1])
		retVal = 1;
	tsp_debug_info(true, info->dev, "%s: Final AFE [Data : %2X] AFE Ver [Data : %2X]\n", __func__, buf[1], buf[2]);

	snprintf(buff, sizeof(buff), "%d", retVal);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

#define FTS_MAX_TX_LENGTH		44
#define FTS_MAX_RX_LENGTH		64

#define FTS_CX2_READ_LENGTH		4
#define FTS_CX2_ADDR_OFFSET		3
#define FTS_CX2_TX_START		0
#define FTS_CX2_BASE_ADDR		0x1000

#define FTS_WTR_TX_CX2_BASE_ADDR		0x1969
#define FTS_WTR_RX_CX2_BASE_ADDR		0x198A

static void run_cx_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char ReadData[info->ForceChannelLength][info->SenseChannelLength + FTS_CX2_READ_LENGTH];
	unsigned char regAdd[8];
	unsigned int addr, rx_num, tx_num;
	int i, j;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_interrupt_set(info, INT_DISABLE);
	fts_irq_enable(info, false);
	fts_release_all_finger(info);

	if (info->stm_ver == STM_VER7) {
		fts_command(info, SENSEOFF);
		fts_delay(50);
	} else {
		fts_systemreset(info);
		fts_delay(50);
		fts_wait_for_ready(info);

		fts_command(info, SLEEPOUT);
		fts_delay(50);
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_OFF); // Key Sensor OFF
#endif
	fts_command(info, FLUSHBUFFER);
	fts_delay(50);

	tx_num = info->ForceChannelLength;
	rx_num = info->SenseChannelLength;

	pStr = kzalloc(4 * (rx_num + 1), GFP_KERNEL);
	if (pStr == NULL) {
		tsp_debug_info(true, &info->client->dev, "FTS pStr kzalloc failed\n");
		return;
	}

	if (info->stm_ver != STM_VER7) {
		int cx_rx_length, max_tx_length, max_rx_length;
		int address_offset = 0, start_tx_offset = 0, retry = 0;
		unsigned char r_addr = READ_ONE_EVENT;
		unsigned char buf[8];

		if (info->digital_rev == FTS_DIGITAL_REV_1) {
			max_tx_length = FTS_MAX_TX_LENGTH - 4;
			max_rx_length = FTS_MAX_RX_LENGTH - 4;
		} else {
			max_tx_length = FTS_MAX_TX_LENGTH;
			max_rx_length = FTS_MAX_RX_LENGTH;
		}

		start_tx_offset = FTS_CX2_TX_START * max_rx_length / FTS_CX2_READ_LENGTH * FTS_CX2_ADDR_OFFSET;
		address_offset = max_rx_length / FTS_CX2_READ_LENGTH;

		dev_info(&info->client->dev, "%s: start\n", __func__);
		for (j = 0; j < tx_num; j++) {

			memset(pStr, 0x0, 4 * (rx_num + 1));
			snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", j);
			strncat(pStr, pTmp, 4 * rx_num);

			addr = FTS_CX2_BASE_ADDR + (j * address_offset * FTS_CX2_ADDR_OFFSET) + start_tx_offset;

			if (rx_num % FTS_CX2_READ_LENGTH != 0)
				cx_rx_length = rx_num / FTS_CX2_READ_LENGTH + 1;
			else
				cx_rx_length = rx_num / FTS_CX2_READ_LENGTH;

			for (i = 0; i < cx_rx_length; i++) {
				regAdd[0] = 0xB2;
				regAdd[1] = (addr >> 8) & 0xff;
				regAdd[2] = (addr & 0xff);
				regAdd[3] = 0x04;
				fts_write_reg(info, &regAdd[0], 4);

				retry = 100;
				do {
					if (retry < 0) {
						dev_err(&info->client->dev,
								"%s: failed to compare buf, break!\n", __func__);
						break;
					}

					fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
					retry--;
				} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

				ReadData[j][i * 4] = buf[3] & 0x3F;
				ReadData[j][i * 4 + 1] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
				ReadData[j][i * 4 + 2] = ((buf[4] & 0xF0) >> 4) | ((buf[5] & 0x03) << 4);
				ReadData[j][i * 4 + 3] = buf[5] >> 2;
				addr = addr + 3;

				snprintf(pTmp, sizeof(pTmp), "%3d%3d%3d%3d ",
					ReadData[j][i*4], ReadData[j][i*4+1], ReadData[j][i*4+2], ReadData[j][i*4+3]);
				strncat(pStr, pTmp, 4 * rx_num);

			}

			tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);

		}
	} else {
		int comp_header_addr, comp_start_addr;

		/* Request compensation data */
		regAdd[0] = 0xB8;
		regAdd[1] = 0x04;		// MUTUAL CX (LPA)
		regAdd[2] = 0x00;
		fts_write_reg(info, &regAdd[0], 3);
		fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x04, 0x00);

		/* Read an address of compensation data */
		regAdd[0] = 0xD0;
		regAdd[1] = 0x00;
		regAdd[2] = 0x50;
		fts_read_reg(info, regAdd, 3, &buff[0], 4);
		comp_header_addr = buff[1] + (buff[2] << 8);

		/* Read header of compensation area */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_header_addr >> 8) & 0xFF;
		regAdd[2] = comp_header_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], 16 + 1);
		tx_num = buff[5];
		rx_num = buff[6];
		comp_start_addr = comp_header_addr + 0x10;

		/* Read compensation data */
		for (j = 0; j < tx_num; j++) {
			memset(&ReadData[j], 0x0, rx_num);
			memset(pStr, 0x0, 4 * (rx_num + 1));
			snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", j);
			strncat(pStr, pTmp, 4 * rx_num);

			addr = comp_start_addr + (rx_num * j);
			regAdd[0] = 0xD0;
			regAdd[1] = (addr >> 8) & 0xFF;
			regAdd[2] = addr & 0xFF;
			fts_read_reg(info, regAdd, 3, &ReadData[j][0], rx_num + 1);
			for (i = 0; i < rx_num; i++) {
				snprintf(pTmp, sizeof(pTmp), "%3d", ReadData[j][i]);
				strncat(pStr, pTmp, 4 * rx_num);
			}
			tsp_debug_info(true, &info->client->dev, "FTS %s\n", pStr);
		}
	}

	if (info->cx_data) {
		for (j = 0; j < tx_num; j++) {
			for(i = 0; i < rx_num; i++)
				info->cx_data[(j * rx_num) + i] = ReadData[j][i];
		}
	}
	snprintf(buff, sizeof(buff), "%s", "OK");
	fts_irq_enable(info, true);
	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif
	kfree(pStr);

	if (!info->get_all_data) {
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_cx_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short val = 0;
	int node = 0;

	sec_cmd_set_default_result(sec);
	if (info->touch_stopped || !info->cx_data) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	node = fts_check_index(info);
	if (node < 0)
		return;

	val = info->cx_data[node];
	snprintf(buff, sizeof(buff), "%d", val);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
		(int)strnlen(buff, sizeof(buff)));

}

static void get_cx_all_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	get_all_data(info, run_cx_data_read, info->cx_data, DATA_UNSIGNED_CHAR);
}

static void fts_read_wtr_cx_data(struct fts_ts_info *info, bool allnode)
{
	struct sec_cmd_data *sec = &info->sec;
	char buff[512] = { 0 };
	unsigned char regAdd[8];
	unsigned char *result_tx, *result_rx;
	char buf[8];
	unsigned char r_addr = READ_ONE_EVENT;
	unsigned int addr;
	int ii, kk = 0, retry;
	int tx_num, rx_num;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	tsp_debug_info(true, &info->client->dev, "%s: ++\n", __func__);

	fts_interrupt_set(info, INT_DISABLE);
	fts_irq_enable(info, false);
	fts_release_all_finger(info);

	if (info->stm_ver == STM_VER7) {
		fts_command(info, SENSEOFF);
	} else {
		fts_systemreset(info);
		fts_delay(50);
		fts_wait_for_ready(info);

		fts_command(info, SLEEPOUT);
		fts_delay(50);
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
	}
#endif
	fts_command(info, FLUSHBUFFER);
	fts_delay(50);

	result_tx = kzalloc(info->ForceChannelLength + 10, GFP_KERNEL);
	if (!result_tx) {
		tsp_debug_err(true, &info->client->dev, "%s: failed to alloc mem\n",
				__func__);

		snprintf(buff, sizeof(buff), "%s", "FAIL");
		goto err_alloc_tx_mem;
	}

	result_rx = kzalloc(info->SenseChannelLength + 10, GFP_KERNEL);
	if (!result_rx) {
		tsp_debug_err(true, &info->client->dev, "%s: failed to alloc mem\n",
				__func__);

		snprintf(buff, sizeof(buff), "%s", "FAIL");
		goto err_alloc_rx_mem;
	}

	if (info->stm_ver != STM_VER7) {
		if (info->ForceChannelLength % FTS_CX2_READ_LENGTH)
			tx_num = (info->ForceChannelLength / FTS_CX2_READ_LENGTH + 1);
		else
			tx_num = info->ForceChannelLength / FTS_CX2_READ_LENGTH;

		if (info->SenseChannelLength % FTS_CX2_READ_LENGTH)
			rx_num = (info->SenseChannelLength / FTS_CX2_READ_LENGTH + 1);
		else
			rx_num = info->SenseChannelLength / FTS_CX2_READ_LENGTH;

		for (ii = 0; ii < tx_num + rx_num; ii++) {

			if (ii < tx_num)
				addr = FTS_WTR_TX_CX2_BASE_ADDR + (ii * 3);
			else
				addr = FTS_WTR_RX_CX2_BASE_ADDR + ((ii - tx_num) * 3);

			if (ii == tx_num)
				kk = 0;

			regAdd[0] = 0xB2;
			regAdd[1] = (addr >> 8) & 0xFF;
			regAdd[2] = addr & 0xFF;
			regAdd[3] = 0x04;

			fts_write_reg(info, &regAdd[0], 4);

			retry = FTS_RETRY_COUNT * 3;
			do {
				if (retry < 0) {
					tsp_debug_err(true, &info->client->dev,
							"%s: failed to compare buf, break!\n", __func__);
					break;
				}

				memset(buf, 0x0, 8);

				fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
				retry--;

			} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);

			tsp_debug_info(true, &info->client->dev,
					"%s: %d: [%04X] (%02X, %02X,) %02X, %02X, %02X, %02X, %02X, %02X\n",
					__func__, ii, addr, buf[0], buf[1], buf[2], buf[3],
					buf[4], buf[5], buf[6], buf[7]);

			if (ii < tx_num) {
				result_tx[kk++] = buf[3] & 0x3F;
				result_tx[kk++] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
				result_tx[kk++] = ((buf[4] & 0xF0)>> 4) | ((buf[5] & 0x03) << 4);
				result_tx[kk++] = buf[5] >> 2;
			} else {
				result_rx[kk++] = buf[3] & 0x3F;
				result_rx[kk++] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
				result_rx[kk++] = ((buf[4] & 0xF0)>> 4) | ((buf[5] & 0x03) << 4);
				result_rx[kk++] = buf[5] >> 2;
			}
		}
	} else {
		int comp_header_addr, comp_start_tx_addr, comp_start_rx_addr;

		/* Request compensation data */
		regAdd[0] = 0xB8;
		regAdd[1] = 0x20;		// SELF IX
		regAdd[2] = 0x00;
		fts_write_reg(info, &regAdd[0], 3);
		fts_fw_wait_for_specific_event(info, EVENTID_STATUS_REQUEST_COMP, 0x20, 0x00);

		/* Read an address of compensation data */
		regAdd[0] = 0xD0;
		regAdd[1] = 0x00;
		regAdd[2] = 0x50;
		fts_read_reg(info, regAdd, 3, &buff[0], 4);
		comp_header_addr = buff[1] + (buff[2] << 8);

		/* Read header of compensation area */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_header_addr >> 8) & 0xFF;
		regAdd[2] = comp_header_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], 16 + 1);
		tx_num = buff[5];
		rx_num = buff[6];

		comp_start_tx_addr = comp_header_addr + 0x10 + tx_num + rx_num;
		comp_start_rx_addr = comp_start_tx_addr + tx_num;

		memset(result_tx, 0x0, tx_num);
		memset(result_rx, 0x0, rx_num);

		memset(buff, 0x0, 512);
		/* Read Self TX Cx2 */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_start_tx_addr >> 8) & 0xFF;
		regAdd[2] = comp_start_tx_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], tx_num + 1);

		for (ii = 0; ii < tx_num; ii++) {
			result_tx[ii] = buff[ii + 1];
		}

		memset(buff, 0x0, 512);
		/* Read Self RX Ix2 */
		regAdd[0] = 0xD0;
		regAdd[1] = (comp_start_rx_addr >> 8) & 0xFF;
		regAdd[2] = comp_start_rx_addr & 0xFF;
		fts_read_reg(info, regAdd, 3, &buff[0], rx_num + 1);

		for (ii = 0; ii < rx_num; ii++) {
			result_rx[ii] = buff[ii + 1];
		}
	}

	if (allnode) {
		char *data = 0; char temp[8];
		memset(buff, 0x0, 512);
		data = result_tx;
		for (ii = 0; ii < info->ForceChannelLength; ii++) {
			tsp_debug_info(true, &info->client->dev,
					"%s: Tx[%d] = %d\n", __func__, ii, *data);
			sprintf(temp, "%d,", *data);
			strncat(buff, temp, 9);
			data++;
		}

		data = result_rx;
		for (ii = 0; ii < info->SenseChannelLength; ii++) {
			tsp_debug_info(true, &info->client->dev,
					"%s: Rx[%d] = %d\n", __func__, ii, *data);
			sprintf(temp, "%d,", *data);
			strncat(buff, temp, 9);
			data++;
		}
	} else {
		unsigned char min_tx = 255, max_tx = 0, min_rx = 255, max_rx = 0;

		for (ii = 0; ii < info->ForceChannelLength; ii++) {
			tsp_debug_info(true, &info->client->dev,
					"%s: Tx[%d] = %d\n", __func__, ii, result_tx[ii]);

			min_tx = min(min_tx, result_tx[ii]);
			max_tx = max(max_tx, result_tx[ii]);
		}

		for (ii = 0; ii < info->SenseChannelLength; ii++) {
			tsp_debug_info(true, &info->client->dev,
					"%s: Rx[%d] = %d\n", __func__, ii, result_rx[ii]);
			min_rx = min(min_rx, result_rx[ii]);
			max_rx = max(max_rx, result_rx[ii]);
		}

		sprintf(buff, "%d,%d,%d,%d", min_tx, max_tx, min_rx, max_rx);

	}
	
	kfree(result_rx);
err_alloc_rx_mem:
	kfree(result_tx);
err_alloc_tx_mem:
	fts_irq_enable(info, true);
	fts_interrupt_set(info, INT_ENABLE);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
	}
#endif
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);

}

static void run_wtr_cx_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);

	fts_read_wtr_cx_data(info, false);

}

static void run_wtr_cx_data_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);

	sec_cmd_set_default_result(sec);

	fts_read_wtr_cx_data(info, true);
}

#ifdef FTS_SUPPORT_TOUCH_KEY
#define USE_KEY_NUM 2
#define FTS_MKEY_ADDR	0x32
#define DOFFSET		1 //Digital rev 2

static void run_key_cx_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char key_cx2_data[2];
	unsigned char ReadData[USE_KEY_NUM * FTS_CX2_READ_LENGTH];
	unsigned char regAdd[8];
	unsigned char buf[8];
	unsigned char r_addr = READ_ONE_EVENT;
	unsigned int addr;
	int i = 0, retry = 0;

	sec_cmd_set_default_result(sec);

	if (!info->dt_data->support_mskey) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touchkey is not supported\n",
				__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_irq_enable(info, false);

	addr = FTS_CX2_BASE_ADDR;

	regAdd[0] = 0xB2;
	regAdd[1] = (addr >> 8) & 0xff;
	regAdd[2] = (addr & 0xff);
	regAdd[3] = 0x04;

	fts_write_reg(info, &regAdd[0], 4);
	fts_delay(1);

	retry = 100;
	do {
		if (retry < 0) {
			dev_err(&info->client->dev, "%s: failed to compare buf, break!\n", __func__);
			break;
		}

		fts_read_reg(info, &r_addr, 1, &buf[0], FTS_EVENT_SIZE);
		retry--;
	} while (buf[1] != regAdd[1] || buf[2] != regAdd[2]);


	ReadData[i * 4] = buf[3] & 0x3F;
	ReadData[i * 4 + 1] = (buf[3] & 0xC0) >> 6 | (buf[4] & 0x0F) << 2;
	ReadData[i * 4 + 2] = ((buf[4] & 0xF0) >> 4) | ((buf[5] & 0x03) << 4);
	ReadData[i * 4 + 3] = buf[5] >> 2;

	key_cx2_data[0] = ReadData[2]; key_cx2_data[1] = ReadData[3];

	dev_info(&info->client->dev, "%s: [Key 1:%d][Key 2:%d]\n", __func__,
			key_cx2_data[0], key_cx2_data[1]);

	//snprintf(buff, sizeof(buff), "%s", "OK");
	snprintf(buff, sizeof(buff), "%d,%d", key_cx2_data[0], key_cx2_data[1]);
	fts_irq_enable(info, true);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#define KEY_CHANNEL_LENGTH	4
#define USING_KEY_CHANNEL_LENGTH	2

static void run_key_cm_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char data[4] = { 0 };
	unsigned char addr[4] = {0xD0, 0x00, 0x32, 0x00};// key channel address is 0xD0, 0x00, 0x32
	unsigned int start_addr;
	unsigned int end_addr;
	unsigned int length;
	unsigned int len;
	unsigned char *buffer = NULL;
	unsigned char *pbuffer = NULL;
	int ii;
	unsigned int cm_value;
	unsigned int max_val = 0;
	unsigned int min_val = 32767;

	sec_cmd_set_default_result(sec);

	if (!info->dt_data->support_mskey) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touchkey is not supported\n",
				__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_irq_enable(info, false);
	fts_release_all_finger(info);

	fts_command(info, SENSEOFF);
	fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_OFF);
		fts_delay(50);
	}
#endif

	fts_command(info, FLUSHBUFFER);
	fts_delay(50);

	fts_read_reg(info, addr, 3, data, 4);
	tsp_debug_info(true, &info->client->dev, "%s: %X, %X, %X, %X\n",
				__func__, data[0], data[1], data[2], data[3]);

	fts_delay(10);

	// key channel length : 4
	start_addr = data[1] + (data[2] << 8);
	length = KEY_CHANNEL_LENGTH * 2 + 1;
	end_addr = start_addr + length;

	buffer = kzalloc(length, GFP_KERNEL);
	if (!buffer) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		goto err_key_cm_out;
	}

	tsp_debug_info(true, &info->client->dev, "%s: start: %X, end: %X, length: %X, len: %X\n",
				__func__, start_addr, end_addr, length, len);

	addr[0] = 0xD0;
	addr[1] = (start_addr >> 8) & 0xff;
	addr[2] = (start_addr & 0xff);

	memset(buffer, 0x00, length);
	pbuffer = buffer;
	fts_read_reg(info, addr, 3, buffer, length);

	tsp_debug_info(true, &info->client->dev, "%s: %X\n", __func__, *pbuffer);

	pbuffer++;

	for (ii = 0; ii < USING_KEY_CHANNEL_LENGTH; ii++) {
		cm_value = 0;

		tsp_debug_info(true, &info->client->dev, "%s: (D2) %X\n", __func__, *pbuffer);

		cm_value |= (*pbuffer & 0xFF);
		pbuffer++;

		tsp_debug_info(true, &info->client->dev, "%s: (D1) %X\n", __func__, *pbuffer);

		cm_value |= (*pbuffer << 8);
		pbuffer++;

		tsp_debug_info(true, &info->client->dev, "%s: [%d]: %d(%X)\n", __func__, ii, cm_value, cm_value);

		max_val = max(max_val, cm_value);
		min_val = min(min_val, cm_value);
	}

	tsp_debug_info(true, &info->client->dev, "max: %d, min: %d\n", max_val, min_val);

	snprintf(buff, SEC_CMD_STR_LEN, "%d,%d", max_val, min_val);

	kfree(buffer);

err_key_cm_out:
	fts_irq_enable(info, true);
	fts_command(info, SENSEON);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
		fts_delay(50);
	}
#endif

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_key_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned char regAdd[3] = {0xD0, 0x00, FTS_MKEY_ADDR};
	unsigned char ReadData[USE_KEY_NUM*2+1] = {0};
	unsigned int key_raw_data[USE_KEY_NUM] = {0};
	unsigned int FrameAddress = 0, StartAddress = 0,  TotalLength = 0; //EndAddress = 0;
	//unsigned int writeAddr = 0;
	//unsigned int col_cnt=0, row_cnt = 0;
	unsigned char count = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (!info->dt_data->support_mskey) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touchkey is not supported\n",
				__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->digital_rev == FTS_DIGITAL_REV_2) {

		fts_interrupt_set(info, INT_DISABLE);

		TotalLength = USE_KEY_NUM * 2;
		fts_read_reg(info, regAdd, 3, (unsigned char *)ReadData, FTS_EVENT_SIZE);

		 FrameAddress = ReadData[DOFFSET] + (ReadData[DOFFSET+1] << 8); // D1 : DOFFSET = 0, D2 : DOFFSET : 1

		StartAddress = FrameAddress;

		memset(&ReadData[0], 0x0, sizeof(ReadData));

		regAdd[1] = (StartAddress >> 8) & 0xFF;
		regAdd[2] = StartAddress & 0xFF;
		fts_read_reg(info, regAdd, 3, (unsigned char *)ReadData, USE_KEY_NUM * 2 + 1);

		for(count=0; count < USE_KEY_NUM; count++)
			key_raw_data[count] = ReadData[count * 2 + DOFFSET] + (ReadData[count * 2 + DOFFSET + 1] << 8);

		fts_interrupt_set(info, INT_ENABLE);
	}

	snprintf(buff, sizeof(buff), "%d,%d", key_raw_data[0], key_raw_data[1]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#endif

int fts_get_tsp_test_result(struct fts_ts_info *info)
{
	unsigned char cmd[3] = {0xD0, 0x00, 0x56};
	unsigned char buff[4] = { 0 };
	int ret;

	fts_systemreset(info);
	fts_wait_for_ready(info);

	ret = info->fts_read_reg(info, cmd, 3, &buff[0], 2);
	if (ret < 0) {
		tsp_debug_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		ret = -EINVAL;
		goto out;
	}
	info->test_result.data[0] = buff[1];

out:
	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	info->fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif
	fts_interrupt_set(info, INT_ENABLE);

	return ret;
}
EXPORT_SYMBOL(fts_get_tsp_test_result);

static void get_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (info->stm_ver != STM_VER7) {
		tsp_debug_info(true, &info->client->dev, "%s: not_support_cmd\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_get_tsp_test_result(info);
	if (ret < 0) {
		tsp_debug_info(true, &info->client->dev, "%s: failed to get result\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (info->test_result.data[0] == 0xFF) {
			tsp_debug_info(true, &info->client->dev,
				"%s: clear factory_result as zero\n",
				__func__);
			info->test_result.data[0] = 0;
		}

		snprintf(buff, sizeof(buff), "M:%s, M:%d, A:%s, A:%d",
				info->test_result.module_result == 0 ? "NONE" :
				info->test_result.module_result == 1 ? "FAIL" : "PASS",
				info->test_result.module_count,
				info->test_result.assy_result == 0 ? "NONE" :
				info->test_result.assy_result == 1 ? "FAIL" : "PASS",
				info->test_result.assy_count);

		sec_cmd_set_cmd_result(sec, buff, strlen(buff));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
}

/* FACTORY TEST RESULT SAVING FUNCTION
 * bit 3 ~ 0 : OCTA Assy
 * bit 7 ~ 4 : OCTA module
 * param[0] : OCTA module(1) / OCTA Assy(2)
 * param[1] : TEST NONE(0) / TEST FAIL(1) / TEST PASS(2) : 2 bit
 */
static int fts_set_tsp_test_result(struct fts_ts_info *info)
{
	unsigned char regAdd[3] = {0xC7, 0x01, 0x00};
	int ret;

	tsp_debug_info(true, &info->client->dev, "%s: [0x%X] M:%s, M:%d, A:%s, A:%d\n",
		__func__, info->test_result.data[0],
		info->test_result.module_result == 0 ? "NONE" :
		info->test_result.module_result == 1 ? "FAIL" : "PASS",
		info->test_result.module_count,
		info->test_result.assy_result == 0 ? "NONE" :
		info->test_result.assy_result == 1 ? "FAIL" : "PASS",
		info->test_result.assy_count);

	regAdd[2] = info->test_result.data[0];

	ret = fts_write_reg(info, &regAdd[0], 3);
	if (ret < 0) {
		tsp_debug_err(true, &info->client->dev,
			"%s failed. ret: %d\n", __func__, ret);
		return -EINVAL;
	}

//	fts_delay(100);
	fts_command(info, FTS_CMD_SAVE_CX_TUNING);

	fts_delay(230);
	fts_fw_wait_for_event(info, STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE);

	return 0;
}

static void set_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (info->stm_ver != STM_VER7) {
		tsp_debug_info(true, &info->client->dev, "%s: not_support_cmd\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	ret = fts_get_tsp_test_result(info);
	if (ret < 0) {
		tsp_debug_err(true, &info->client->dev, "%s: [ERROR] failed to get_tsp_test_result\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
	sec->cmd_state = SEC_CMD_STATUS_RUNNING;

	if (info->test_result.data[0] == 0xFF) {
		tsp_debug_info(true, &info->client->dev,
			"%s: clear factory_result as zero\n",
			__func__);
		info->test_result.data[0] = 0;
	}

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		info->test_result.assy_result = sec->cmd_param[1];
		if (info->test_result.assy_count < 3)
			info->test_result.assy_count++;

	} else if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		info->test_result.module_result = sec->cmd_param[1];
		if (info->test_result.module_count < 3)
			info->test_result.module_count++;
	}

	ret = fts_set_tsp_test_result(info);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef FTS_SUPPORT_HOVER
static void hover_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped || !(info->reinit_done) || (info->power_state & STATE_LOWPOWER)) {
		tsp_debug_info(true, &info->client->dev,
			"%s: [ERROR] Touch is stopped:%d, reinit_done:%d, power state:%d\n",
			__func__, info->touch_stopped, info->reinit_done, info->power_state);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

		if (sec->cmd_param[0] == 1) {
			info->retry_hover_enable_after_wakeup = 1;
			tsp_debug_info(true, &info->client->dev, "%s: retry_hover_on_after_wakeup\n", __func__);
		}

		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		int enables;
		enables = sec->cmd_param[0];
		if (enables) {
			unsigned char regAdd[4] = {0xB0, 0x01, 0x29, 0x41};
			unsigned char Dly_regAdd[4] = {0xB0, 0x01, 0x72, 0x04};

			fts_write_reg(info, &Dly_regAdd[0], 4);
			fts_write_reg(info, &regAdd[0], 4);
			fts_command(info, FTS_CMD_HOVER_ON);
			info->hover_ready = false;
			info->hover_enabled = true;
		} else {
			unsigned char Dly_regAdd[4] = {0xB0, 0x01, 0x72, 0x08};
			fts_write_reg(info, &Dly_regAdd[0], 4);
			fts_command(info, FTS_CMD_HOVER_OFF);
			info->hover_enabled = false;
			info->hover_ready = false;
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

/*
static void hover_no_sleep_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned char regAdd[4] = {0xB0, 0x01, 0x18, 0x00};
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]) {
			regAdd[3]=0x0F;
		} else {
			regAdd[3]=0x08;
		}
		fts_write_reg(info, &regAdd[0], 4);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
*/
#endif

#ifdef CONFIG_GLOVE_TOUCH
static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->glove_enabled = sec->cmd_param[0];

		if (!info->touch_stopped && info->reinit_done) {
			if (info->glove_enabled)
				fts_command(info, FTS_CMD_GLOVE_MODE_ON);
			else
				fts_command(info, FTS_CMD_GLOVE_MODE_OFF);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void get_glove_sensitivity(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned char cmd[4] = { 0xB2, 0x01, 0xC6, 0x02 };
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		char buff[SEC_CMD_STR_LEN] = { 0 };
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	fts_interrupt_set(info, INT_DISABLE);
	fts_release_all_finger(info);

	fts_command(info, FLUSHBUFFER);
	fts_delay(10);

	fts_write_reg(info, &cmd[0], 4);

	memset(data, 0x0, FTS_EVENT_SIZE);
	cmd[0] = READ_ONE_EVENT;

	while (info->fts_read_reg(info, &cmd[0], 1, (unsigned char *)data,
		FTS_EVENT_SIZE)) {

		if ((data[0] == EVENTID_RESULT_READ_REGISTER)
			&& (data[1] == cmd[1]) && (data[2] == cmd[2])) {
			snprintf(buff, sizeof(buff), "%d",
						*(unsigned short *)&data[3]);
			tsp_debug_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buff,
						(int)strnlen(buff, sizeof(buff)));
			sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
			sec->cmd_state = SEC_CMD_STATUS_OK;
			break;
		}

		if (retry++ > 30) {
			tsp_debug_err(true, info->dev,
				"%s: Time Over\n", __func__);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			break;
		}
	}

	fts_interrupt_set(info, INT_ENABLE);

}

static void fast_glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->fast_glove_enabled = sec->cmd_param[0];

		if (!info->touch_stopped && info->reinit_done) {
			if (info->fast_glove_enabled)
				fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
			else
				fts_command(info, FTS_CMD_SET_NOR_GLOVE_MODE);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 1) {
			info->flip_enable = true;
			info->cover_type = sec->cmd_param[1];
		} else {
			info->flip_enable = false;
		}
		if (!info->touch_stopped && info->reinit_done) {
			if (info->flip_enable) {
#ifdef CONFIG_GLOVE_TOUCH
				if (info->glove_enabled)
					fts_command(info, FTS_CMD_GLOVE_MODE_OFF);
#endif
				fts_set_cover_type(info, true);
			} else {
				fts_set_cover_type(info, false);
#ifdef CONFIG_GLOVE_TOUCH
				if (info->fast_glove_enabled)
					fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
				else if (info->glove_enabled)
					fts_command(info, FTS_CMD_GLOVE_MODE_ON);
#endif
			}
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

#ifdef USE_STYLUS_PEN
static void stylus_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);


	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {

		info->use_stylus = sec->cmd_param[0];

		dev_info(&info->client->dev, "%s: stylus %s\n",
				__func__, info->use_stylus ? "enable" : "disable");

		sec->cmd_state = SEC_CMD_STATUS_OK;

	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static void report_rate(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);


	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {

		if (sec->cmd_param[0] == REPORT_RATE_90HZ) {
			ret = info->fts_change_scan_rate(info, FTS_CMD_FAST_SCAN);
		} else if (sec->cmd_param[0] == REPORT_RATE_60HZ) {
			ret = info->fts_change_scan_rate(info, FTS_CMD_SLOW_SCAN);
		} else if (sec->cmd_param[0] == REPORT_RATE_30HZ) {
			/* will be fixed 30hz report rate command */
			ret = info->fts_change_scan_rate(info, FTS_CMD_USLOW_SCAN);
		}
		if (ret < 0) {
			dev_err(&info->client->dev, "%s: can not change report rate, %d\n", __func__, ret);
			snprintf(buff, sizeof(buff), "%s", "FAIL");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "%s", "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static void interrupt_control(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		int enables;
		enables = sec->cmd_param[0];
		if (enables)
			fts_irq_enable(info, true);
		else
			fts_irq_enable(info, false);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static bool check_lowpower_mode(struct fts_ts_info *info, unsigned char flag)
{
	bool ret = FTS_DISABLE;
	flag = flag & 0xFF;

	if (flag)
		ret = FTS_ENABLE;

	tsp_debug_info(true, &info->client->dev,
		"%s: lowpower_mode flag : %d, ret:%d\n", __func__, flag, ret);

	if (flag & FTS_LOWP_FLAG_2ND_SCREEN)
		tsp_debug_info(true, &info->client->dev, "%s: 2nd screen on\n", __func__);
	if (flag & FTS_LOWP_FLAG_BLACK_UI)
		tsp_debug_info(true, &info->client->dev, "%s: AOD double tap on\n", __func__);
	if (flag & FTS_LOWP_FLAG_SPAY)
		tsp_debug_info(true, &info->client->dev, "%s: spay cmd on\n", __func__);
	if (flag & FTS_LOWP_FLAG_TEMP_CMD)
		tsp_debug_info(true, &info->client->dev, "%s: known cmd on\n", __func__);

	return ret;

}

static void set_lowpower_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0])
			info->lowpower_mode = FTS_ENABLE;
		else
			info->lowpower_mode = FTS_DISABLE;

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void set_deepsleep_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->deepsleep_mode = sec->cmd_param[0];

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void set_wirelesscharger_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x10};
	int rc;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->wirelesscharger_mode = sec->cmd_param[0];

		if (info->wirelesscharger_mode ==0)
			regAdd[0] = 0xC2;
		else
			regAdd[0] = 0xC1;

		tsp_debug_info(true, &info->client->dev, "%s: CMD[%2X]\n",__func__,regAdd[0]);

		rc = fts_write_reg(info, regAdd, 2);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;

out:
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void active_sleep_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
	/* To do here */
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

#if defined(TSP_BOOSTER) && !defined(INPUT_BOOSTER_SYSFS)
static void boost_level(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;
	int stage;

	sec_cmd_set_default_result(sec);

	if (!info->tsp_booster) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		tsp_debug_err(true, &info->client->dev,
				"%s: booster is NULL.\n", __func__);

		goto boost_out;
	}

	stage = 1 << sec->cmd_param[0];
	if (!(info->tsp_booster->dvfs_stage & stage)) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		tsp_debug_err(true, &info->client->dev,
				"%s: %d is not supported(%04x != %04x).\n",
				__func__, sec->cmd_param[0],
				stage, info->tsp_booster->dvfs_stage);

		goto boost_out;
	}

	info->tsp_booster->dvfs_boost_mode = stage;

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (info->tsp_booster->dvfs_boost_mode == DVFS_STAGE_NONE) {
		retval = info->tsp_booster->dvfs_off(info->tsp_booster);
		if (retval < 0) {
			tsp_debug_err(true, &info->client->dev,
					"%s: booster stop failed(%d).\n",
					__func__, retval);
			snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		}
	}

boost_out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static void second_screen_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0])
			info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_2ND_SCREEN;
		else
			info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_2ND_SCREEN);

		info->lowpower_mode = check_lowpower_mode(info, info->lowpower_flag);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void longpress_grip_enable_mode(struct fts_ts_info *info, bool on)
{
	int ret;
	unsigned char regAdd[4] = {0xB0, 0x07, 0x10, 0x03};

	if (on)
		regAdd[3] = 0x03;	// default
	else
		regAdd[3] = 0x02;	// can long press

	ret = fts_write_reg(info, regAdd, 4);

	if (ret < 0)
		dev_err(&info->client->dev, "%s failed. ret: %d\n", __func__, ret);
	else
		dev_info(&info->client->dev, "%s: reg:%d, ret: %d\n", __func__, on, ret);

	fts_delay(1);
}

static void set_longpress_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0])
			longpress_grip_enable_mode(info, 1);
		else
			longpress_grip_enable_mode(info, 0);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};


static void grip_check_enable_mode(struct fts_ts_info *info, bool on)
{
	int ret;
	unsigned char regAdd[4] = {0xB0, 0x07, 0x11, 0x7F};

	if (on) {
		regAdd[3] = 0x7F;	//  default
		info->edge_grip_mode = true;
	} else {
		regAdd[3] = 0x7E;	// don't check grip
		info->edge_grip_mode = false;
	}

	ret = fts_write_reg(info, regAdd, 4);

	if (ret < 0)
		dev_err(&info->client->dev, "%s failed. ret: %d\n", __func__, ret);
	else
		dev_info(&info->client->dev, "%s: reg:%d, ret: %d\n", __func__, on, ret);
	fts_delay(1);
}

static void set_grip_detection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]) {
			longpress_grip_enable_mode(info, 1);		// default
			grip_check_enable_mode(info, 1);		// default
		} else {
			longpress_grip_enable_mode(info, 0);
			grip_check_enable_mode(info, 0);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};

static void set_sidescreen_x_length(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	/* TB Side screen area length  */
	unsigned char regAdd[4] = {0xB0, 0x07, 0x1C, 0xA0}; //default Side screen x length setting
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 0xA0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		regAdd[3] = sec->cmd_param[0]; // Change Side screen x length

		ret = fts_write_reg(info, regAdd, 4);
		if (ret < 0)
			dev_err(&info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			dev_info(&info->client->dev, "%s: x length:%d, ret: %d\n", __func__, regAdd[3], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};


static void set_dead_zone(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	/*  long press  */
	unsigned char regAdd[2] = {0xC4, 0x00};
	int ret;
	/* long press */

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 6) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 1)
			regAdd[1] = 0x01;	/* side edge top */
		else if (sec->cmd_param[0] == 2)
			regAdd[1] = 0x02;	/* side edge bottom */
		else if (sec->cmd_param[0] == 3)
			regAdd[1] = 0x03;	/* side edge All On */
		else if (sec->cmd_param[0] == 4)
			regAdd[1] = 0x04;	/* side edge Left Off */
		else if (sec->cmd_param[0] == 5)
			regAdd[1] = 0x05;	/* side edge Right Off */
		else if (sec->cmd_param[0] == 6)
			regAdd[1] = 0x06;	/* side edge All Off */
		else
			regAdd[1] = 0x0;	/* none	*/

		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			dev_err(&info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			dev_info(&info->client->dev, "%s: reg:%d, ret: %d\n", __func__, sec->cmd_param[0], ret);
		fts_delay(1);
		/*  long press  */

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x0C};
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]==0) {
			regAdd[0] = 0xC1;	/* dead zone  enable*/
		} else {
			regAdd[0] = 0xC2;	/* dead zone  disable*/
		}

		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			tsp_debug_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, sec->cmd_param[0], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void select_wakeful_edge(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char regAdd[2] = {0xC2, 0x0F};
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0]==0) {
			regAdd[0] = FTS_CMS_DISABLE_FEATURE;	/* right - C2, 0F */
			info->wakeful_edge_side = EDGEWAKE_RIGHT;
		} else {
			regAdd[0] = FTS_CMS_ENABLE_FEATURE;		/* left - C1, 0F */
			info->wakeful_edge_side = EDGEWAKE_LEFT;
		}
		ret = fts_write_reg(info, regAdd, 2);

		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s failed. ret: %d\n", __func__, ret);
		else
			tsp_debug_info(true, &info->client->dev, "%s: reg:%d, ret: %d\n", __func__, sec->cmd_param[0], ret);

		fts_delay(1);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void set_tunning_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned char regAdd[3] = {0xC6, 0x00, 0x00};
	int i = 0, ret = 0;

	sec_cmd_set_default_result(sec);

	/* ToDo */
	tsp_debug_info(true, &info->client->dev, "%s: %d, %d, %d, %d\n",
				__func__, sec->cmd_param[0], sec->cmd_param[1],
				sec->cmd_param[2], sec->cmd_param[3]);

	mutex_lock(&info->device_mutex);

	for (i = 0; i < 4; i++) {
		if (sec->cmd_param[i]) {
			if (sec->cmd_param[i] > info->dt_data->max_x)
				continue;

			if (i == 0)
				regAdd[1] = 0x02;
			else if (i == 1)
				regAdd[1] = 0x03;
			else if (i == 2)
				regAdd[1] = 0x05;
			else/*if (i == 3)*/
				regAdd[1] = 0x07;

			regAdd[2] = sec->cmd_param[i];

			ret = fts_write_reg(info, regAdd, 3);
			if (ret < 0) {
				tsp_debug_err(true, &info->client->dev,
							"%s (%d)failed. ret: %d\n", __func__, i, ret);
				goto out_set_tunning_data;

			}
			fts_delay(5);
		}
	}

out_set_tunning_data:
	mutex_unlock(&info->device_mutex);

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "FAIL");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef FTS_SUPPORT_MAINSCREEN_DISBLE
static void set_mainscreen_disable_cmd(struct fts_ts_info *info, bool on)
{
	int ret;
	unsigned char regAdd[2] = {0xC2, 0x07};

	if (on) {
		regAdd[0] = 0xC1;				// main screen disable
		info->mainscr_disable = true;
	} else {
		regAdd[0] = 0xC2;				// enable like normal
		info->mainscr_disable = false;
	}

	ret = fts_write_reg(info, regAdd, 2);

	if (ret < 0)
		dev_err(&info->client->dev, "%s failed. ret: %d\n", __func__, ret);
	else
		dev_info(&info->client->dev, "%s: reg:%d, ret: %d\n", __func__, on, ret);
	fts_delay(1);
}

static void set_mainscreen_disable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 2) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 1)
			set_mainscreen_disable_cmd(info, 1);
		else
			set_mainscreen_disable_cmd(info, 0);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};
#endif

static void scrub_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (!info->dt_data->support_string_lib) {
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (info->touch_stopped) {
		dev_info(&info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (info->stm_ver != STM_VER7) {
		tsp_debug_info(true, &info->client->dev, "%s: not_support_cmd\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_SCRUB;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_BLACK_UI;

	} else {
		info->fts_mode &= ~FTS_MODE_SCRUB;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_BLACK_UI);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:

	info->lowpower_mode = check_lowpower_mode(info, info->lowpower_flag);
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef SMARTCOVER_COVER
static void change_smartcover_table(struct fts_ts_info *info)
{
	u8 i, j, k, h, temp, temp_sum;

	for (i = 0; i < MAX_BYTE; i++)
		for(j = 0; j < MAX_TX; j++)
			info->changed_table[j][i] = info->smart_cover[i][j];

#if 1 // debug
	tsp_debug_info(true, &info->client->dev, "%s smart_cover value\n", __func__);
	for (i = 0; i < MAX_BYTE; i++){
		pr_info("[fts] ");
		for (j = 0; j < MAX_TX; j++)
			pr_cont("%d ",info->smart_cover[i][j]);
		pr_cont("\n");
	}

	tsp_debug_info(true, &info->client->dev, "%s changed_table value\n", __func__);
	for (j = 0; j < MAX_TX; j++){
		pr_info("[fts] ");
		for (i = 0; i < MAX_BYTE; i++)
			pr_cont("%d ",info->changed_table[j][i]);
		pr_cont("\n");
	}
#endif

	tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);

	for (i = 0; i < MAX_TX; i++)
		for (j = 0; j < 4; j++)
			info->send_table[i][j] = 0;
		tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);

	for (i = 0; i < MAX_TX; i++){
		temp = 0;
		for (j = 0; j < MAX_BYTE; j++)
			temp += info->changed_table[i][j];
		if (temp == 0) continue;

		for (k = 0; k < 4; k++){
			temp_sum = 0;
			for (h = 0; h < 8; h++){
				temp_sum += ((u8)(info->changed_table[i][h+8*k])) << (7-h);
			}
			info->send_table[i][k] = temp_sum;
		}

		tsp_debug_info(true, &info->client->dev, "i:%2d, %2X %2X %2X %2X\n",
			i, info->send_table[i][0], info->send_table[i][1],
			info->send_table[i][2], info->send_table[i][3]);
	}
	tsp_debug_info(true, &info->client->dev, "%s %d\n", __func__, __LINE__);


}

static void set_smartcover_mode(struct fts_ts_info *info, bool on)
{
	int ret;
	unsigned char regMon[2] = {0xC1, 0x0A};
	unsigned char regMoff[2] = {0xC2, 0x0A};

	if (on == 1) {
		ret = fts_write_reg(info, regMon, 2);
		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s mode on failed. ret: %d\n", __func__, ret);
	} else {
		ret = fts_write_reg(info, regMoff, 2);
		if (ret < 0)
			tsp_debug_err(true, &info->client->dev, "%s mode off failed. ret: %d\n", __func__, ret);
	}
}

static void set_smartcover_clear(struct fts_ts_info *info)
{
	int ret;
	unsigned char regClr[6] = {0xC5, 0xFF, 0x00, 0x00, 0x00, 0x00};

	ret = fts_write_reg(info, regClr, 6);
	if (ret < 0)
		tsp_debug_err(true, &info->client->dev, "%s data clear failed. ret: %d\n", __func__, ret);
}

static void set_smartcover_data(struct fts_ts_info *info)
{
	int ret;
	u8 i, j;
	u8 temp=0;
	unsigned char regData[6] = {0xC5, 0x00, 0x00, 0x00, 0x00, 0x00};


	for (i = 0; i < MAX_TX; i++){
		temp = 0;
		for (j = 0; j < 4; j++)
			temp += info->send_table[i][j];
		if (temp == 0) continue;

		regData[1] = i;

		for (j = 0; j < 4; j++)
			regData[2 + j] = info->send_table[i][j];

		tsp_debug_info(true, &info->client->dev, "i:%2d, %2X %2X %2X %2X \n",
			regData[1], regData[2], regData[3], regData[4], regData[5]);

		// data write
		ret = fts_write_reg(info, regData, 6);
		if (ret < 0)
			tsp_debug_err(true, &info->client->dev,
				"%s data write[%d] failed. ret: %d\n", __func__, i, ret);


	}

}

/* ####################################################
	func : smartcover_cmd [0] [1] [2] [3]
	index 0
		vlaue 0 : off (normal)
		vlaue 1 : off (globe mode)
		vlaue 2 :  X
		vlaue 3 : on
				clear -> data send(send_table value) ->  mode on
		vlaue 4 : clear smart_cover value
		vlaue 5 : data save to smart_cover value
			index 1 : tx channel num
			index 2 : data 0xFF
			index 3 : data 0xFF
		value 6 : table value change, smart_cover -> changed_table -> send_table

	ex)
	// clear
	echo smartcover_cmd,4 > cmd
	// data write (hart)
	echo smartcover_cmd,5,3,16,16 > cmd
	echo smartcover_cmd,5,4,56,56 > cmd
	echo smartcover_cmd,5,5,124,124 > cmd
	echo smartcover_cmd,5,6,126,252 > cmd
	echo smartcover_cmd,5,7,127,252 > cmd
	echo smartcover_cmd,5,8,63,248 > cmd
	echo smartcover_cmd,5,9,31,240 > cmd
	echo smartcover_cmd,5,10,15,224 > cmd
	echo smartcover_cmd,5,11,7,192 > cmd
	echo smartcover_cmd,5,12,3,128 > cmd
	// data change
	echo smartcover_cmd,6 > cmd
	// mode on
	echo smartcover_cmd,3 > cmd

###################################################### */

static void smartcover_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 i, j, t;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 6) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {

		if (sec->cmd_param[0] == 0) {			// off

			set_smartcover_mode(info, 0);
			tsp_debug_info(true, &info->client->dev, "%s mode off, normal\n", __func__);

		} else if (sec->cmd_param[0] == 1) {	// off, globe mode

			set_smartcover_mode(info, 0);
			tsp_debug_info(true, &info->client->dev, "%s mode off, globe mode\n", __func__);

#ifdef CONFIG_GLOVE_TOUCH
			if (info->fast_glove_enabled)
				fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
			else if (info->glove_enabled)
				fts_command(info, FTS_CMD_GLOVE_MODE_ON);
#endif
		} else if (sec->cmd_param[0] == 3) {	// on

			set_smartcover_clear(info);
			set_smartcover_data(info);
			tsp_debug_info(true, &info->client->dev, "%s data send\n", __func__);
			set_smartcover_mode(info, 1);
			tsp_debug_info(true, &info->client->dev, "%s mode on\n", __func__);

		} else if (sec->cmd_param[0] == 4) {	// clear

			for (i = 0; i < MAX_BYTE; i++)
				for (j = 0; j < MAX_TX; j++)
					info->smart_cover[i][j] = 0;
			tsp_debug_info(true, &info->client->dev, "%s data clear\n", __func__);

		} else if (sec->cmd_param[0] == 5) {	// data write

			if (sec->cmd_param[1] < 0 ||  sec->cmd_param[1] >= 32){
				tsp_debug_info(true, &info->client->dev, "%s data tx size is over[%d]\n",
					__func__, sec->cmd_param[1]);
				snprintf(buff, sizeof(buff), "%s", "NG");
				sec->cmd_state = SEC_CMD_STATUS_FAIL;
				goto fail;
			}
			tsp_debug_info(true, &info->client->dev, "%s data %2X, %2X, %2X\n", __func__,
				sec->cmd_param[1], sec->cmd_param[2], sec->cmd_param[3] );

			t = sec->cmd_param[1];
			info->smart_cover[t][0] = (sec->cmd_param[2]&0x80)>>7;
			info->smart_cover[t][1] = (sec->cmd_param[2]&0x40)>>6;
			info->smart_cover[t][2] = (sec->cmd_param[2]&0x20)>>5;
			info->smart_cover[t][3] = (sec->cmd_param[2]&0x10)>>4;
			info->smart_cover[t][4] = (sec->cmd_param[2]&0x08)>>3;
			info->smart_cover[t][5] = (sec->cmd_param[2]&0x04)>>2;
			info->smart_cover[t][6] = (sec->cmd_param[2]&0x02)>>1;
			info->smart_cover[t][7] = (sec->cmd_param[2]&0x01);
			info->smart_cover[t][8] = (sec->cmd_param[3]&0x80)>>7;
			info->smart_cover[t][9] = (sec->cmd_param[3]&0x40)>>6;
			info->smart_cover[t][10] = (sec->cmd_param[3]&0x20)>>5;
			info->smart_cover[t][11] = (sec->cmd_param[3]&0x10)>>4;
			info->smart_cover[t][12] = (sec->cmd_param[3]&0x08)>>3;
			info->smart_cover[t][13] = (sec->cmd_param[3]&0x04)>>2;
			info->smart_cover[t][14] = (sec->cmd_param[3]&0x02)>>1;
			info->smart_cover[t][15] = (sec->cmd_param[3]&0x01);

		} else if(sec->cmd_param[0] == 6) {	//  data change

			change_smartcover_table(info);
			tsp_debug_info(true, &info->client->dev, "%s data change\n", __func__);

		} else {

			tsp_debug_info(true, &info->client->dev, "%s cmd[%d] not use\n", __func__, sec->cmd_param[0]);
			snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto fail;

		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
fail:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
};
#endif

static void set_rotation_status(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		int status = sec->cmd_param[0] % 2;

		if (status)
			fts_enable_feature(info, FTS_FEATURE_DUAL_SIDE_GUSTURE, true);
		else
			fts_enable_feature(info, FTS_FEATURE_DUAL_SIDE_GUSTURE, false);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned short addr = FTS_CMD_STRING_ACCESS;

	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (!info->dt_data->support_string_lib) {
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (info->stm_ver != STM_VER7) {
		tsp_debug_info(true, &info->client->dev, "%s: not_support_cmd\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_SPAY;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_SPAY;
	} else {
		info->fts_mode &= ~FTS_MODE_SPAY;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_SPAY);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		tsp_debug_info(true, &info->client->dev, "%s: write failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	info->lowpower_mode = check_lowpower_mode(info, info->lowpower_flag);
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (!info->dt_data->support_string_lib) {
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (info->stm_ver != STM_VER7) {
		tsp_debug_info(true, &info->client->dev, "%s: not_support_cmd\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	if (sec->cmd_param[0]) {
		info->fts_mode |= FTS_MODE_AOD_TRIGGER;
		info->lowpower_flag = info->lowpower_flag | FTS_LOWP_FLAG_BLACK_UI;
	} else {
		info->fts_mode &= ~FTS_MODE_AOD_TRIGGER;
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_BLACK_UI);
	}

	ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
	if (ret < 0) {
		tsp_debug_info(true, &info->client->dev, "%s: write failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

out:
	info->lowpower_mode = check_lowpower_mode(info, info->lowpower_flag);
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
void fts_ts_run_rawdata_all(struct fts_ts_info *info)
{
	short min = 10000;
	short max = 0;

	tsp_debug_err(true, &info->client->dev, "%s, start ##\n", __func__);

	fts_read_frame(info, TYPE_FILTERED_DATA, &min, &max);
	tsp_debug_err(true, &info->client->dev, "%s, ## run Raw Cap data ##, max:%d,min:%d\n", __func__, max,min);
	
	//run_cx_data_read((void *)t_sec);
	get_all_data(info, run_cx_data_read, info->cx_data, DATA_UNSIGNED_CHAR);
	tsp_debug_err(true, &info->client->dev, "%s, ## run CX data ##, %d\n", __func__, __LINE__);
	
	tsp_debug_err(true, &info->client->dev, "%s, ## Done ##, %d\n", __func__, __LINE__);

}
#endif

static void delay(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->delay_time = sec->cmd_param[0];

	dev_info(&info->client->dev, "%s: delay time is %d\n", __func__, info->delay_time);
	snprintf(buff, sizeof(buff), "%d", info->delay_time);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->temp = sec->cmd_param[0];

	dev_info(&info->client->dev, "%s: command is %d\n", __func__, info->temp);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_autotune_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->run_autotune = sec->cmd_param[0];

	tsp_debug_info(true, &info->client->dev, "%s: command is %s\n",
			__func__, info->run_autotune ? "ENABLE" : "DISABLE");
	
	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_force_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_info *info = container_of(sec, struct fts_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	bool touch_on = false;

	sec_cmd_set_default_result(sec);

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",
			__func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
	if(info->rawdata_read_lock == 1){
		tsp_debug_info(true, &info->client->dev,
			"%s: ramdump mode is runing, %d\n", __func__, info->rawdata_read_lock);
		goto autotune_fail;
	}
#endif
	if (info->touch_count > 0) {
		touch_on = true;
		tsp_debug_info(true, info->dev, "%s: finger on touch(%d)\n", __func__, info->touch_count);
	}

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
	sec->cmd_param[0] = 0;
	set_status_pureautotune(device_data);
	sec_cmd_set_default_result(sec);
#endif

	fts_irq_enable(info, false);

	if ((info->digital_rev == FTS_DIGITAL_REV_2) || (info->digital_rev == FTS_DIGITAL_REV_3)) {
		fts_interrupt_set(info, INT_DISABLE);
		fts_release_all_finger(info);

		fts_command(info, SENSEOFF);
		fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->dt_data->support_mskey) {
			fts_command(info, FTS_CMD_KEY_SENSE_OFF);
		}
#endif
		fts_command(info, FLUSHBUFFER);
		fts_delay(10);

		fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->dt_data->support_mskey)
			fts_release_all_key(info);
#endif
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
		fts_command(info, FTS_CMD_FORCE_AUTOTUNE);
		info->o_afe_ver = info->afe_ver;
#endif

		if (touch_on) {
			tsp_debug_info(true, info->dev, "%s: finger! do not run autotune\n", __func__);
		} else {
			tsp_debug_info(true, info->dev, "%s: run autotune\n", __func__);
			fts_execute_autotune(info);
		}

		if (info->stm_ver != STM_VER7) {
			fts_command(info, SLEEPOUT);
			fts_delay(1);
		}
		fts_command(info, SENSEON);

#ifdef FTS_SUPPORT_WATER_MODE
		fts_fw_wait_for_event(info, STATUS_EVENT_WATER_SELF_DONE);
#else
		if (info->stm_ver == STM_VER7)
			fts_fw_wait_for_event(info, STATUS_EVENT_FORCE_CAL_DONE_D3);
		else
			fts_fw_wait_for_event(info, STATUS_EVENT_FORCE_CAL_DONE);
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->dt_data->support_mskey)
			fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

		fts_interrupt_set(info, INT_ENABLE);
	} else {
		fts_irq_enable(info, true);
		tsp_debug_info(true, &info->client->dev, "%s: digital_rev not matched, %d\n", __func__, info->digital_rev);
		goto autotune_fail;
	}

	fts_irq_enable(info, true);
	if (touch_on) {
		snprintf(buff, sizeof(buff), "NG_FINGER_ON");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
		sec->cmd_param[0] = 1;
		set_status_pureautotune(device_data);
#else
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
#endif
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

autotune_fail:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	tsp_debug_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

#ifdef FTS_SUPPORT_TOUCH_KEY
static int read_touchkey_data(struct fts_ts_info *info, unsigned char type, unsigned int keycode)
{
	unsigned char pCMD[3] = { 0xD0, 0x00, 0x00};
	unsigned char buf[9] = { 0 };
	int i;
	int ret = 0;

	if (!info->dt_data->support_mskey) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touchkey is not supported\n",
				__func__);
		return -ENODEV;
	}

	pCMD[2] = type;

	ret = fts_read_reg(info, &pCMD[0], 3, buf, 3);
	if (ret >= 0) {
		if (info->digital_rev == FTS_DIGITAL_REV_1) {
			pCMD[1] = buf[1];
			pCMD[2] = buf[0];
		} else {
			pCMD[1] = buf[2];
			pCMD[2] = buf[1];
		}
	} else {
		return -1;
	}

	ret = fts_read_reg(info, &pCMD[0], 3, buf, 9);
	if (ret < 0)
		return -2;

	for (i = 0 ; i < info->dt_data->num_touchkey ; i++)
		if (info->dt_data->touchkey[i].keycode == keycode) {
			if (info->digital_rev == FTS_DIGITAL_REV_1)
				return *(short *)&buf[(info->dt_data->touchkey[i].value - 1) * 2];
			else
				return *(short *)&buf[(info->dt_data->touchkey[i].value - 1) * 2 + 1];
		}

	return -3;
}

static ssize_t touchkey_recent_strength(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", value);
	}

	value = read_touchkey_data(info, TYPE_TOUCHKEY_STRENGTH, KEY_RECENT);

	return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", value);
}

static ssize_t touchkey_back_strength(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", value);
	}

	value = read_touchkey_data(info, TYPE_TOUCHKEY_STRENGTH, KEY_BACK);

	return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", value);
}

static ssize_t touchkey_recent_raw(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", value);
	}

	value = read_touchkey_data(info, TYPE_TOUCHKEY_RAW, KEY_RECENT);

	return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", value);
}

static ssize_t touchkey_back_raw(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int value = 0;

	if (info->touch_stopped) {
		tsp_debug_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", value);
	}

	value = read_touchkey_data(info, TYPE_TOUCHKEY_RAW, KEY_BACK);

	return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", value);
}

static ssize_t touchkey_threshold(struct device *dev,
				       struct device_attribute *attr, char *buf) {
	struct fts_ts_info *info = dev_get_drvdata(dev);
	unsigned char pCMD[3] = { 0xD0, 0x00, 0x00};
	int value;
	int ret = 0;

	value = -1;
	pCMD[2] = TYPE_TOUCHKEY_THRESHOLD;
	ret = fts_read_reg(info, &pCMD[0], 3, buf, 3);
	if (ret >= 0) {
		if (info->digital_rev == FTS_DIGITAL_REV_1)
			value = *(unsigned short *)&buf[0];
		else
			value = *(unsigned short *)&buf[1];
	}

	info->touchkey_threshold = value;
	return snprintf(buf, SEC_CMD_STR_LEN, "%d\n", info->touchkey_threshold);
}

static ssize_t fts_touchkey_led_control(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int data, ret;

	ret = sscanf(buf, "%d", &data);
	tsp_debug_dbg(true, &info->client->dev, "%s, %d\n", __func__, data);

	if (ret != 1) {
		tsp_debug_err(true, &info->client->dev, "%s, %d err\n",
			__func__, __LINE__);
		return size;
	}

	if (data != 0 && data != 1) {
		tsp_debug_err(true, &info->client->dev, "%s wrong cmd %x\n",
			__func__, data);
		return size;
	}

	if (info->led_power)
		ret = info->led_power(info, data);

	if (ret) {
		tsp_debug_err(true, &info->client->dev, "%s: Error turn on led %d\n",
			__func__, ret);

		goto out;
	}
	msleep(30);

out:
	return size;
}

#ifdef TKEY_BOOSTER
static ssize_t boost_level_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int val, retval;
	int stage;

	if (!info->tkey_booster) {
		tsp_debug_err(true, &info->client->dev,
			"%s: booster is NULL\n", __func__);
		return count;
	}

	tsp_debug_info(true, &info->client->dev, "%s\n", __func__);
	sscanf(buf, "%d", &val);

	stage = 1 << val;

	if (!(info->tkey_booster->dvfs_stage & stage)) {
		tsp_debug_err(true, &info->client->dev,
			"%s: wrong cmd %d\n", __func__, val);
		return count;
	}

	info->tkey_booster->dvfs_boost_mode = stage;
	tsp_debug_info(true, &info->client->dev,
			"%s: dvfs_boost_mode = 0x%X\n",
			__func__, info->tkey_booster->dvfs_boost_mode);

	if (info->tkey_booster->dvfs_boost_mode == DVFS_STAGE_DUAL) {
		info->tkey_booster->dvfs_freq = MIN_TOUCH_LIMIT_SECOND;
		tsp_debug_info(true, &info->client->dev,
			"%s: boost_mode DUAL, dvfs_freq = %d\n",
			__func__, info->tkey_booster->dvfs_freq);
	} else if (info->tkey_booster->dvfs_boost_mode == DVFS_STAGE_SINGLE) {
		info->tkey_booster->dvfs_freq = MIN_TOUCH_LIMIT;
		tsp_debug_info(true, &info->client->dev,
			"%s: boost_mode SINGLE, dvfs_freq = %d\n",
			__func__, info->tkey_booster->dvfs_freq);
	} else if (info->tkey_booster->dvfs_boost_mode == DVFS_STAGE_NONE) {
		retval = info->tkey_booster->dvfs_off(info->tkey_booster);
		if (retval < 0) {
			tsp_debug_err(true, &info->client->dev,
					"%s: booster stop failed(%d).\n",
					__func__, retval);
		}
	}
	return count;
}
#endif

static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL, fts_touchkey_led_control);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, touchkey_recent_strength, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_back_strength, NULL);
static DEVICE_ATTR(touchkey_recent_raw, S_IRUGO, touchkey_recent_raw, NULL);
static DEVICE_ATTR(touchkey_back_raw, S_IRUGO, touchkey_back_raw, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold, NULL);
#ifdef TKEY_BOOSTER
static DEVICE_ATTR(boost_level, S_IWUSR | S_IWGRP, NULL, boost_level_store);
#endif

static struct attribute *sec_touchkey_factory_attributes[] = {
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent_raw.attr,
	&dev_attr_touchkey_back_raw.attr,
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_brightness.attr,
#ifdef TKEY_BOOSTER
	&dev_attr_boost_level.attr,
#endif
	NULL,
};

static struct attribute_group sec_touchkey_factory_attr_group = {
	.attrs = sec_touchkey_factory_attributes,
};
#endif

#endif


