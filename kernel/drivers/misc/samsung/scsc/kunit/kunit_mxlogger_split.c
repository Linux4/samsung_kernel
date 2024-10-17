#include <kunit/test.h>

#define miframman_alloc(ram, nbytes, align, tag)	kunit_miframman_alloc(ram, nbytes, align, tag)
#define mxmgmt_transport_send(args...)			((void *)0)

#define MEM_LAYOUT_CHECK() (0)
#define SCALE_DOWN			0.1
#define MX_DRAM_SIZE_SECTION_LOG	(8 * 1024 * 1024) * SCALE_DOWN
#define MX_DRAM_SIZE_SECTION_WLAN	(6 * 1024 * 1024) * SCALE_DOWN
#define MX_DRAM_SIZE_SECTION_WPAN	(2 * 1024 * 1024) * SCALE_DOWN
#define MXLOGGER_RSV_WLAN_SZ		(2 * 1024 * 1024) * SCALE_DOWN

struct mxlogger_channel *test_alloc_mxlogger_channel(struct kunit *test)
{
	struct mxlogger_channel *k_mxlogger_channel;

	k_mxlogger_channel = kunit_kzalloc(test, sizeof(*k_mxlogger_channel), GFP_KERNEL);

	return k_mxlogger_channel;
}

struct mxlogger *test_alloc_mxlogger(struct kunit *test)
{
	struct mxlogger *k_mxlogger;
	struct mxlogger_config_area *k_cfg_wlan, *k_cfg_wpan;
	int i;

	k_mxlogger = kunit_kzalloc(test, sizeof(*k_mxlogger), GFP_KERNEL);
	k_cfg_wlan = kunit_kzalloc(test, sizeof(*k_cfg_wlan), GFP_KERNEL);
	k_cfg_wpan = kunit_kzalloc(test, sizeof(*k_cfg_wpan), GFP_KERNEL);

	/* It's blocked because of mxmgmt_transport_send in __mxlogger_generate_sync_record()
	  char dram_addr1[16] = "ABCDAAAAsmxfAAAA";
	  char dram_addr2[16] = "ABCDAAAAsmxfBBBB";

	  k_mxlogger->chan[MXLOGGER_CHANNEL_WLAN].enabled = true;
	  k_mxlogger->chan[MXLOGGER_CHANNEL_WLAN].target = SCSC_MIF_ABS_TARGET_WLAN;
	  k_mxlogger->chan[MXLOGGER_CHANNEL_WLAN].sync_buffer_index = 0;
	  k_mxlogger->chan[MXLOGGER_CHANNEL_WLAN].mem_sync_buf = dram_addr1;

	  k_mxlogger->chan[MXLOGGER_CHANNEL_WPAN].enabled = true;
	  k_mxlogger->chan[MXLOGGER_CHANNEL_WPAN].target = SCSC_MIF_ABS_TARGET_WPAN;
	  k_mxlogger->chan[MXLOGGER_CHANNEL_WPAN].sync_buffer_index = 0;
	  k_mxlogger->chan[MXLOGGER_CHANNEL_WLAN].mem_sync_buf = dram_addr2;
	*/

	k_mxlogger->chan[MXLOGGER_CHANNEL_WLAN].cfg = k_cfg_wlan;
	k_mxlogger->chan[MXLOGGER_CHANNEL_WPAN].cfg = k_cfg_wpan;
	for (i = MXLOGGER_FIRST_FIXED_SZ; i <= MXLOGGER_NUM_BUFFERS; i++) {
		k_mxlogger->chan[MXLOGGER_CHANNEL_WLAN].cfg->bfds[i].size = 0x4;
		//k_mxlogger->chan[MXLOGGER_CHANNEL_WPAN].cfg->bfds[i].size = 0x4;
	}
	return k_mxlogger;
}

struct log_msg_packet *test_alloc_log_msg_packet(struct kunit *test)
{
	struct log_msg_packet *k_log_msg_packet;

	k_log_msg_packet = kunit_kzalloc(test, sizeof(*k_log_msg_packet), GFP_KERNEL);

	return k_log_msg_packet;
}

static int mxlogger_force_to_host_set_param_cb(const char *val, const struct kernel_param *kp);
int (*fp_mxlogger_force_to_host_set_param_cb)(const char *val, const struct kernel_param *kp) = &mxlogger_force_to_host_set_param_cb;

static int mxlogger_force_to_host_get_param_cb(char *buffer, const struct kernel_param *kp);
int (*fp_mxlogger_force_to_host_get_param_cb)(char *buffer, const struct kernel_param *kp) = &mxlogger_force_to_host_get_param_cb;

static void mxlogger_message_handler(const void *message, void *data);
void (*fp_mxlogger_message_handler)(const void *message, void *data) = &mxlogger_message_handler;

static void mxlogger_message_handler_wpan(const void *message, void *data);
void (*fp_mxlogger_message_handler_wpan)(const void *message, void *data) = &mxlogger_message_handler_wpan;

static void mxlogger_wait_for_msg_reinit_completion(struct mxlogger_channel *chan);
void (*fp_mxlogger_wait_for_msg_reinit_completion)(struct mxlogger_channel *chan) = &mxlogger_wait_for_msg_reinit_completion;

static bool mxlogger_wait_for_msg_reply(struct mxlogger_channel *chan);
bool (*fp_mxlogger_wait_for_msg_reply)(struct mxlogger_channel *chan) = &mxlogger_wait_for_msg_reply;

static int mxlogger_collect_init(struct scsc_log_collector_client *collect_client);
int (*fp_mxlogger_collect_init)(struct scsc_log_collector_client *collect_client) = &mxlogger_collect_init;

static int mxlogger_collect_init_wpan(struct scsc_log_collector_client *collect_client);
int (*fp_mxlogger_collect_init_wpan)(struct scsc_log_collector_client *collect_client) = &mxlogger_collect_init_wpan;

static int mxlogger_collect(struct scsc_log_collector_client *collect_client, size_t size);
int (*fp_mxlogger_collect)(struct scsc_log_collector_client *collect_client, size_t size) = &mxlogger_collect;

static int mxlogger_collect_wpan(struct scsc_log_collector_client *collect_client, size_t size);
int (*fp_mxlogger_collect_wpan)(struct scsc_log_collector_client *collect_client, size_t size) = &mxlogger_collect_wpan;

static int mxlogger_collect_end(struct scsc_log_collector_client *collect_client);
int (*fp_mxlogger_collect_end)(struct scsc_log_collector_client *collect_client) = &mxlogger_collect_end;

static int mxlogger_collect_end_wpan(struct scsc_log_collector_client *collect_client);
int (*fp_mxlogger_collect_end_wpan)(struct scsc_log_collector_client *collect_client) = &mxlogger_collect_end_wpan;

static void mxlogger_enable_channel(struct mxlogger *mxlogger, bool enable, u8 channel);
void (*fp_mxlogger_enable_channel)(struct mxlogger *mxlogger, bool enable, u8 channel) = &mxlogger_enable_channel;

//static int kunit_mxlogger_send_config(struct mxlogger *mxlogger, u8 channel)
//{
//	return 0;
//}

static struct kunit *gtest;
void set_test_in_mxlogger(struct kunit *test)
{
	gtest = test;
}

static void *kunit_miframman_alloc(struct miframman *ram, size_t nbytes, size_t align, int tag)
{
	return kunit_kzalloc(gtest, nbytes, GFP_KERNEL);
}
