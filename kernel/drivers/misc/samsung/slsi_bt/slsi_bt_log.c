#include <linux/module.h>
#include <linux/ctype.h>

#ifdef CONFIG_SCSC_LOG_COLLECTION
#include <scsc/scsc_log_collector.h>
#endif

#include "slsi_bt_log.h"
#include "slsi_bt_property.h"

#include "../scsc/scsc_mx_impl.h"
#include "../scsc/mxlog.h"
#include "../scsc/mxlog_transport.h"
#include "slsi_bt_controller.h"

static void *property_data;
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

	if (mx == NULL) {
		BT_WARNING("failed to get mx handler\n");
		return;
	} else if (len < min || val == NULL) {
		BT_WARNING("Invalid data\n");
		return;
	}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	mtrans = scsc_mx_get_mxlog_transport_wpan(mx);
#else
	mtrans = scsc_mx_get_mxlog_transport(mx);
#endif
	if (mtrans) {
		unsigned int header, tmp;
		unsigned char phase, level;

		mutex_lock(&mtrans->lock);
		if (mtrans->header_handler_fn == NULL ||
		    mtrans->channel_handler_fn == NULL) {
			BT_WARNING("mxlog transport is not opened.\n");
			mutex_unlock(&mtrans->lock);
			return;
		}

		header = (SYNC_VALUE_PHASE_5 << 16) | (val[lv] << 8);
		mtrans->header_handler_fn(header, &phase, &level, &tmp);
		mtrans->channel_handler_fn(phase,
					   (void *)&val[msg],
					   (size_t)len - 1,
					   level,
					   mtrans->channel_handler_data);
		mutex_unlock(&mtrans->lock);
	} else
		BT_WARNING("failed to get mxlog transport.\n");
}

void slsi_bt_log_data_hex(const char *func, unsigned int tag,
		const unsigned char *data, size_t len)
{
	char buf[SLSI_BT_LOG_DATA_BUF_MAX + 1];
	char *tags, isrx;
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

	isrx = (tag & 0x1);
	if (offset + 5 < SLSI_BT_LOG_DATA_BUF_MAX)
		TR_DATA(isrx, "%20s %s-%s(%3zu): %s\n", func, tags,
				isrx ? "RX" : "TX", len, buf);
	else
		TR_DATA(isrx, "%20s %s-%s(%3zu): %s...\n", func, tags,
				isrx ? "RX" : "TX", len, buf);
}

/* btlog string
 *
 * The string must be null-terminated, and may also include a single
 * newline before its terminating null. The string shall be given
 * as a hexadecimal number, but the first character may also be a
 * plus sign. The maximum number of Hexadecimal characters is 32
 * (128bits)
 */
static inline bool hex_prefix_check(const char *s)
{
	if (s[0] == '0' && tolower(s[1]) == 'x' && isxdigit(s[2]))
		return true;
	return false;
}

static int btlog_enables_set(const char *val, const struct kernel_param *kp)
{
	const int max_size = strlen("+0x\n")+1+32; /* 128 bit */
	const int hexprefix = strlen("0x");
	const int split_size = 16;

	unsigned long long filter[2];
	size_t len = 0, split = 0;
	int ret = 0;

	if (val == NULL)
		return -EINVAL;

	len = strnlen(val, max_size);
	if (len > 0 && len < max_size) {
		if (val[0] == '+') { /* support regacy */
			val++;
			len--;
		}
		if (val[len-1] == '\n')
			len--;
	} else if (len >= max_size)
		return -ERANGE;

	if (len <= hexprefix || !hex_prefix_check(val))
		return -EINVAL;

	if (len - hexprefix > split_size) {
		char buf[20]; /* It's bigger than split_size + hexprefix + 1 */
		split = len - split_size;
		strncpy(buf, val, split);
		ret = kstrtou64(buf, 16, &filter[1]);
		if (ret != 0)
			return ret;
	}
	ret = kstrtou64(val+split, 16, &filter[0]);
	if (ret != 0)
		return ret;

	// update confirmed filter
	memcpy(fw_filter.uint64, filter, sizeof(filter));
#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
	slsi_bt_property_set_logmask((struct hci_trans *)property_data,
				     fw_filter.uint32, 4);
#else
	slsi_bt_controller_update_fw_log_filter(fw_filter.uint64);
#endif
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
		fw_filter.uint32[0] = value;
#ifdef CONFIG_SLSI_BT_USE_UART_INTERFACE
		slsi_bt_property_set_logmask((struct hci_trans *)property_data,
					     fw_filter.uint32, 4);
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

#ifdef CONFIG_SCSC_LOG_COLLECTION
static struct bt_hcf {
	unsigned char *data;
	size_t size;
} bt_hcf = { NULL, 0 };

static int bt_hcf_collect(struct scsc_log_collector_client *client, size_t size)
{
	if (bt_hcf.data) {
		BT_DBG("Collecting BT config file\n");
		return scsc_log_collector_write(bt_hcf.data, bt_hcf.size, 1);
	}
	return 0;
}

static struct scsc_log_collector_client bt_collect_hcf_client = {
	.name = "bt_hcf",
	.type = SCSC_LOG_CHUNK_BT_HCF,
	.collect_init = NULL,
	.collect = bt_hcf_collect,
	.collect_end = NULL,
	.prv = &bt_hcf,
};

void bt_hcf_collect_store(void *hcf, size_t size)
{
	if (hcf && size > 0) {
		if (bt_hcf.data)
			kfree(bt_hcf.data);
		bt_hcf.data = kmalloc(size, GFP_KERNEL);
		memcpy(bt_hcf.data, hcf, size);
		bt_hcf.size = size;
		scsc_log_collector_register_client(&bt_collect_hcf_client);
	}
}

void bt_hcf_collect_free(void)
{
	/* Deinit HCF log collection */
	if (bt_hcf.data) {
		scsc_log_collector_unregister_client(&bt_collect_hcf_client);
		kfree(bt_hcf.data);
		bt_hcf.data = NULL;
		bt_hcf.size = 0;
	}
}

void bt_hcf_collection_request(void)
{
	scsc_log_collector_schedule_collection(
			SCSC_LOG_HOST_BT, SCSC_LOG_HOST_BT_REASON_HCI_ERROR);
}
#endif

struct firm_log_filter slsi_bt_log_filter_get(void)
{
	return fw_filter;
}

void slsi_bt_log_set_transport(void *data)
{
	property_data = data;
	if (data) {
		dev_host_tr_data_log_filter = BTTR_LOG_FILTER_DEFAULT;
		slsi_bt_controller_update_fw_log_filter(fw_filter.uint64);
	}
}
