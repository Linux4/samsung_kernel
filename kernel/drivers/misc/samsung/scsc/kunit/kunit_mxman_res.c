int ret_tr_init;
int ret_mlogger_init;
int ret_gdb_init;
int ret_mlog_init;
int mbox;

#define mxmgmt_transport_init(args...)			((int)ret_tr_init)
#define mxlogger_init_transport_channel(args...)	((int)ret_mlogger_init)
#define gdb_transport_init(args...)			((int)ret_gdb_init)
#define mxlog_transport_init(args...)			((int)ret_mlog_init)
#define mxlog_transport_init_wpan(args...)		((int)ret_mlog_init)
#define gdb_transport_release(args...)			((void)0)
#define mxmgmt_transport_release(args...)		((void)0)
#define mxlog_transport_release(args...)		((void)0)
#define mifmboxman_get_mbox_ptr(args...)		((void *)&mbox)

static void mxman_res_mbox_init_wlan(struct mxman *mxman, u32 firmware_entry_point);
void (*fp_mxman_res_mbox_init_wlan)(struct mxman *mxman, u32 firmware_entry_point) = &mxman_res_mbox_init_wlan;

