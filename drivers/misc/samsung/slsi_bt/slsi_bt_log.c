#include <linux/module.h>
#include <linux/ctype.h>

#include "slsi_bt_log.h"
#include "slsi_bt_property.h"
#include "hci_trans.h"

#include "../scsc/scsc_mx_impl.h"
#include "../scsc/mxlog.h"
#include "../scsc/mxlog_transport.h"
#include "slsi_bt_controller.h"

static struct hci_trans *bt_htr;
static struct firm_log_filter fw_filter;

static unsigned int dev_host_tr_data_log_filter;
module_param(dev_host_tr_data_log_filter, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dev_host_tr_data_log_filter, "Host transport layer log filter");

/* slsi_btlog_enables enables log options of bt fw.
 * For now, Nov 2021, It only sets at the service start. BT UART will move
 * BSMHCP to property type of command. Then, It should be update to set any
 * time.
 */
static struct kernel_param_ops slsi_btlog_enables_ops;
module_param_cb(btlog_enables, &slsi_btlog_enables_ops, NULL, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(btlog_enables,
	"Set the enables for btlog sources in Bluetooth firmware (127..0)");

/* Legacy. Recommend to use btlog_enables. */
static struct kernel_param_ops slsi_mxlog_filter_ops;
module_param_cb(mxlog_filter, &slsi_mxlog_filter_ops, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlog_filter,
	 "Set the enables for btlog sources in Bluetooth firmware (31..0)");

/**
 * MXLOG_LOG_EVENT_IND
 *
 * MXLOG via BCSP interface specification
 *
 * value encoding is:
 *
 *  | 1st |2nd|3rd|4th|5th|6th|7th|8th|9th|10th ~ 13rd|each 4bytes|...
 *  ------------------------------------------------------------------
 *  |level|   time stamp  |index of format|argument 1 |argument N |.
 *  ------------------------------------------------------------------
 *
 */
void slsi_bt_mxlog_log_event(const unsigned char len, const unsigned char *val)
{
	struct scsc_mx *mx = (struct scsc_mx *)slsi_bt_controller_get_mx();
	struct mxlog_transport *mtrans;
	const int lv = 0, msg = 1;
	const int min = 9;

	BT_DBG("Log event: mx: %s, len: %d\n", mx ? "OK" : "NULL", len);

	if (mx == NULL || len < min || val == NULL)
		return;

	BT_DBG("lv: %d, time: %02x%02x%02x%02x\n", val[lv],
		val[lv+4], val[lv+3], val[lv+2], val[lv+1]);

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	mtrans = scsc_mx_get_mxlog_transport_wpan(mx);
#else
	mtrans = scsc_mx_get_mxlog_transport(mx);
#endif
	if (mtrans) {
		unsigned int header, tmp;
		unsigned char phase, level;
		/* To get level. header_handler_fn has a table to transfer
		 * log level from fw level to mxlog level */
		header = (SYNC_VALUE_PHASE_5 << 16) | (val[lv] << 8);
		mtrans->header_handler_fn(header, &phase, &level, &tmp);
		BT_DBG("phase: %d, level: %d\n", phase, level);

		mutex_lock(&mtrans->lock);
		mtrans->channel_handler_fn(phase,
					   (void *)&val[msg],
					   (size_t)len - 1,
					   level,
					   mtrans->channel_handler_data);
		mutex_unlock(&mtrans->lock);
	} else {
		BT_WARNING("failed to get mxlog transport.\n");
	}
}

void slsi_bt_log_data_hex(const char *func, unsigned int tag,
		const unsigned char *data, size_t len)
{
	char buf[SLSI_BT_LOG_DATA_BUF_MAX + 1];
	char *tags;
	size_t idx = 0, offset = 0;
	const int head = 10,
		  tail = 10;

	if(!((dev_host_tr_data_log_filter & tag) & 0xF0))
		return;

	switch (tag) {
	case BTTR_TAG_USR_TX:
	case BTTR_TAG_USR_RX:  tags = "User"; break;
	case BTTR_TAG_H4_TX:
	case BTTR_TAG_H4_RX:   tags = "H4"; break;
	case BTTR_TAG_BCSP_TX:
	case BTTR_TAG_BCSP_RX: tags = "BCSP"; break;
	case BTTR_TAG_SLIP_TX:
	case BTTR_TAG_SLIP_RX: tags = "SLIP"; break;
	default:
		tags = "Unknown";
		break;
	}

	if (len <= head + tail) {
		while (idx < len && offset + 4 < SLSI_BT_LOG_DATA_BUF_MAX)
			offset += snprintf(buf + offset,
					SLSI_BT_LOG_DATA_BUF_MAX - offset,
					"%02x ", data[idx++]);
	} else {
		while (idx < head && offset + 4 < SLSI_BT_LOG_DATA_BUF_MAX)
			offset += snprintf(buf + offset,
					SLSI_BT_LOG_DATA_BUF_MAX - offset,
					"%02x ", data[idx++]);
		offset += snprintf(buf + offset,
					SLSI_BT_LOG_DATA_BUF_MAX - offset,
					" ..(%d bytes).. ",
					(int)len - head - tail);
		idx = len - tail;
		while (idx < len && offset + 4 < SLSI_BT_LOG_DATA_BUF_MAX)
			offset += snprintf(buf + offset,
					SLSI_BT_LOG_DATA_BUF_MAX - offset,
					"%02x ", data[idx++]);
	}

	if (offset + 5 < SLSI_BT_LOG_DATA_BUF_MAX)
		BT_DBG("%s %s-%s(%3zu): %s\n", func, tags,
			(tag & 0x1) ? "RX" : "TX", len, buf);
	else
		BT_DBG("%s %s-%s(%3zu): %s...\n", func, tags,
			(tag & 0x1) ? "RX" : "TX", len, buf);
}

/* btlog string
 *
 * The string must be null-terminated, and may also include a single
 * newline before its terminating null. The string shall be given
 * as a hexadecimal number, but the first character may also be a
 * plus sign. The maximum number of Hexadecimal characters is 32
 * (128bits)
 */
#define BTLOG_BUF_PREFIX_LEN	        (2)  // 0x
#define BTLOG_BUF_MAX_OTHERS_LEN        (2)  // plus sign, newline
#define BTLOG_BUF_MAX_HEX_LEN           (32) // 128 bit
#define BTLOG_BUF_MAX_LEN               (BTLOG_BUF_PREFIX_LEN +\
					 BTLOG_BUF_MAX_HEX_LEN +\
					 BTLOG_BUF_MAX_OTHERS_LEN)
#define BTLOG_BUF_SPLIT_LEN             (BTLOG_BUF_MAX_HEX_LEN/2 +\
					 BTLOG_BUF_MAX_HEX_LEN)

#define SCSC_BTLOG_MAX_STRING_LEN       (37)
#define SCSC_BTLOG_BUF_LEN              (19)
#define SCSC_BTLOG_BUF_MAX_CHAR_TO_COPY (16)
#define SCSC_BTLOG_BUF_PREFIX_LEN        (2)

static bool is_hex_string(const char *s)
{
	if (s[0] == '0' && tolower(s[1]) == 'x' && isxdigit(s[2]))
		return true;
	return false;
}

static int btlog_enables_set(const char *val, const struct kernel_param *kp)
{
	int ret;
	size_t len;

	if (val == NULL)
		return -EINVAL;

	len = strnlen(val, BTLOG_BUF_MAX_LEN);
	if (len >= BTLOG_BUF_MAX_LEN)
		return -ERANGE;
	else if (len <= 0)
		return -EINVAL;

	if (val[len - 1] == '\n')
		len--;
	if (val[0] == '+') {
		val++;
		len--;
	}

	if (len <= BTLOG_BUF_PREFIX_LEN || !is_hex_string(val))
		return -EINVAL;

	if (len <= BTLOG_BUF_SPLIT_LEN) {
		ret = kstrtou64(val, 0, &fw_filter.uint64[0]);
	} else {
		char split_buf[BTLOG_BUF_SPLIT_LEN + 1];
		u32 split = len - BTLOG_BUF_SPLIT_LEN;

		// the most significatn integer(u64)
		memcpy(split_buf, val, split);
		split_buf[split] = '\0';
		ret = kstrtou64(split_buf, 0, &fw_filter.uint64[1]);

		// the least significant integer(u64)
		strcpy(&split_buf[BTLOG_BUF_PREFIX_LEN], &val[split]);
		ret += kstrtou64(split_buf, 0, &fw_filter.uint64[0]);
	}

	if (ret == 0) {
#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
		slsi_bt_property_set_logmask(bt_htr, fw_filter.uint32, 4);
#else
		slsi_bt_controller_update_fw_log_filter(fw_filter.uint64);
#endif
	}


	return ret;
}


static int btlog_enables_get(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "btlog_enables = 0x%08x%08x%08x%08x\n",
			fw_filter.en1_h ,fw_filter.en1_l,
			fw_filter.en0_h, fw_filter.en0_l);
}

static struct kernel_param_ops slsi_btlog_enables_ops = {
	.set = btlog_enables_set,
	.get = btlog_enables_get,
};

static int mxlog_filter_set(const char *val, const struct kernel_param *kp)
{
	int ret;
	u32 value;

	ret = kstrtou32(val, 0, &value);
	if (!ret) {
#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
		slsi_bt_property_set_logmask(bt_htr, fw_filter.uint32, 4);
#else
		slsi_bt_controller_update_fw_log_filter(fw_filter.uint64);
#endif
	}

	return ret;
}

static int mxlog_filter_get(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "mxlog_filter=0x%08x\n", fw_filter.uint32[0]);
}

static struct kernel_param_ops slsi_mxlog_filter_ops = {
	.set = mxlog_filter_set,
	.get = mxlog_filter_get,
};

struct firm_log_filter slsi_bt_log_filter_get(void)
{
	return fw_filter;
}

void slsi_bt_log_init(void *htr)
{
	bt_htr = (struct hci_trans *)htr;
	dev_host_tr_data_log_filter = BTTR_LOG_FILTER_DEFAULT;
	slsi_bt_controller_update_fw_log_filter(fw_filter.uint64);
}

void slsi_bt_log_deinit(void)
{
	bt_htr = NULL;
}
