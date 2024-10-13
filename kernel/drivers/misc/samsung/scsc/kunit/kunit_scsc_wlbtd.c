#define wait_for_completion_timeout(args...)	(0)
#define wake_lock_init(args...)			((void *)0)

static int msg_from_wlbtd_cb(struct sk_buff *skb, struct genl_info *info);
int (*fp_msg_from_wlbtd_cb)(struct sk_buff *skb, struct genl_info *info) = &msg_from_wlbtd_cb;

static int msg_from_wlbtd_sable_cb(struct sk_buff *skb, struct genl_info *info);
int (*fp_msg_from_wlbtd_sable_cb)(struct sk_buff *skb, struct genl_info *info) = &msg_from_wlbtd_sable_cb;

static int msg_from_wlbtd_build_type_cb(struct sk_buff *skb, struct genl_info *info);
int (*fp_msg_from_wlbtd_build_type_cb)(struct sk_buff *skb, struct genl_info *info) = &msg_from_wlbtd_build_type_cb;

static int msg_from_wlbtd_write_file_cb(struct sk_buff *skb, struct genl_info *info);
int (*fp_msg_from_wlbtd_write_file_cb)(struct sk_buff *skb, struct genl_info *info) = &msg_from_wlbtd_write_file_cb;