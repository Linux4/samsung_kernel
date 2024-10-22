#ifndef _XT_MARK_H
#define _XT_MARK_H

#include <linux/types.h>

struct xt_mark_tginfo2 {
	__u32 mark, mask;
};

struct xt_mark_mtinfo1 {
	__u32 mark, mask;
	__u8 invert;
};

#ifdef CONFIG_KNOX_VPN_UID_PID

/* KNOX framework uses mark value 200 to 250 
 * when the special meta data is required
 * This will indicate to the kernel code that
 * the recipient (VPN client) expects to see
 * the values indicated in skb_meta_param structure
 */

#define META_MARK_BASE_LOWER 200
#define META_MARK_BASE_UPPER 250

/* Structure to hold metadata values
 * intended for VPN clients to make 
 * more intelligent decisions
 * when the KNOX meta mark 
 * feature is enabled
 */

struct skb_meta_param {
	uid_t uid;
	pid_t pid;
};

#endif /* CONFIG_KNOX_VPN_UID_PID */

#endif /*_XT_MARK_H*/
