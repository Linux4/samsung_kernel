// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/mm.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/audit.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/freezer.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/tracepoint.h>
#include "mtk_selinux_warning.h"
#include <avc.h>
#include <avc_ss.h>
#include <classmap.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#define PRINT_BUF_LEN   100
#define MOD		"SELINUX"
#define SCONTEXT_FILTER
#define AV_FILTER
#define NE_FILTER
/* #define ENABLE_CURRENT_NE_CORE_DUMP */


static const char *aee_filter_list[AEE_FILTER_NUM] = {
	"u:r:bootanim:s0",
	"u:r:system_server:s0",
	"u:r:zygote:s0",
	"u:r:surfaceflinger:s0",
	"u:r:netd:s0",
	"u:r:servicemanager:s0",
	"u:r:hwservicemanager:s0",
	"u:r:hal_graphics_composer_default:s0",
	"u:r:hal_graphics_allocator_default:s0",
	"u:r:priv_app:s0",
};
#ifdef NEVER
static const char * const aee_filter_unused[] = {
	"u:r:bluetooth:s0",
	"u:r:binderservicedomain:s0",
	"u:r:dex2oat:s0",
	"u:r:dhcp:s0",
	"u:r:dnsmasq:s0",
	"u:r:dumpstate:s0",
	"u:r:gpsd:s0",
	"u:r:healthd:s0",
	"u:r:hci_attach:s0",
	"u:r:hostapd:s0",
	"u:r:inputflinger:s0",
	"u:r:isolated_app:s0",
	"u:r:keystore:s0",
	"u:r:lmkd:s0",
	"u:r:mdnsd:s0",
	"u:r:logd:s0",
	"u:r:mtp:s0",
	"u:r:nfc:s0",
	"u:r:ppp:s0",
	"u:r:racoon:s0",
	"u:r:recovery:s0",
	"u:r:rild:s0",
	"u:r:runas:s0",
	"u:r:sdcardd:s0",
	"u:r:shared_relro:s0",
	"u:r:tee:s0",
	"u:r:uncrypt:s0",
	"u:r:watchdogd:s0",
	"u:r:wpa:s0",
	"u:r:ueventd:s0",
	"u:r:vold:s0",
	"u:r:vdc:s0",
};
#endif

#define AEE_AV_FILTER_NUM 5
static const char *aee_av_filter_list[AEE_AV_FILTER_NUM] = {
	"map",
	"ioctl"
};

#define SKIP_PATTERN_NUM 5
static const char *skip_pattern[SKIP_PATTERN_NUM] = {
	"scontext=u:r:untrusted_app"
};

static const char * const ne_list[] = {
};

static struct tracepoint *satp;

static int mtk_check_filter(char *scontext);
static int mtk_get_scontext(char *data, char *buf);
static char *mtk_get_process(char *in);
static int mtk_check_filter(char *scontext)
{
	int i = 0;

	/*check whether scontext in filter list */
	for (i = 0; i < AEE_FILTER_NUM && aee_filter_list[i] != NULL; i++) {
		if (strcmp(scontext, aee_filter_list[i]) == 0)
			return i;
	}

	return -1;
}

static bool mtk_check_skip_pattern(char *data)
{
	int i = 0;

	/* check whether the log contains specific pattern*/
	for (i = 0; i < SKIP_PATTERN_NUM && skip_pattern[i] != NULL; i++) {
		if (strstr(data, skip_pattern[i]) != NULL)
			return true;
	}

	return false;
}

#define AV_LEN 30
static void mtk_check_av(char *data)
{
	char *start = NULL;
	char *end = NULL;
	char av_buf[AV_LEN] = { '\0' };
	char scontext[AEE_FILTER_LEN] = { '\0' };
	char printbuf[PRINT_BUF_LEN] = { '\0' };
	char *pname = scontext;
	char *iter;
	int i;

	if (!mtk_get_scontext(data, scontext))
		return;

	pname = mtk_get_process(scontext);
	if (pname == 0)
		return;

	start = strstr(data, "denied  { ");
	end = strstr(data, "}");
	if (start == NULL || end == NULL || end < start)
		return;

	start = start+10;

	iter = start;
	while (iter < end) {
		if (*iter == ' ' && iter-start > 0 && iter-start < AV_LEN) {
			strncpy(av_buf, start, iter-start);
			for (i = 0;
				i < AEE_AV_FILTER_NUM &&
				aee_av_filter_list[i] != NULL;
				++i) {

				if (strcmp(av_buf,
					aee_av_filter_list[i]) == 0) {

					if (mtk_check_skip_pattern(data))
						return;

					memset(printbuf, '\0', PRINT_BUF_LEN);

					snprintf(printbuf, PRINT_BUF_LEN-1,
						"[%s][WARNING]\nCR_DISPATCH_PROCESSNAME:%s\n",
						MOD, pname);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
					aee_kernel_warning_api(
							__FILE__, __LINE__,
							DB_OPT_DEFAULT  | DB_OPT_NATIVE_BACKTRACE,
							printbuf, data);
#endif
				}

			}

			start = iter+1;
		}
		iter++;
	}
}

static int mtk_get_scontext(char *data, char *buf)
{
	char *t1;
	char *t2;
	int diff = 0;

	t1 = strstr(data, "scontext=");

	if (t1 == NULL)
		return 0;

	t1 += 9;
	t2 = strchr(t1, ' ');

	if (t2 == NULL)
		return 0;

	diff = t2 - t1;
	if (diff >= AEE_FILTER_LEN)
		return 0;

	strncpy(buf, t1, diff);
	return 1;
}

static int mtk_get_pname(char *scontext, char *buf)
{
	char *t1 = scontext, *t2;
	int diff, i;

	/* Omit two ':' */
	for (i = 0; i < 2; i++) {
		t1 = strchr(t1, ':');
		if (!t1)
			return 0;
		t1 = t1 + 1;
	}

	t2 = strchr(t1, ':');
	if (!t2)
		return 0;

	diff = t2 - t1;
	if (diff >= AEE_FILTER_LEN)
		return 0;

	strscpy(buf, t1, diff);
	return 1;
}

static char *mtk_get_process(char *in)
{
	char *out = in;
	char *tmp;
	int i;

	/*Omit two ':' */
	for (i = 0; i < 2; i++) {
		out = strchr(out, ':');
		if (out == NULL)
			return 0;
		out = out + 1;
	}

	tmp = strchr(out, ':');

	if (tmp == NULL)
		return 0;

	*tmp = '\0';
	return out;
}

void mtk_audit_hook(char *data)
{
#ifdef SCONTEXT_FILTER
	char scontext[AEE_FILTER_LEN] = { '\0' };
	char *pname = scontext;
	int ret = 0;

	/*get scontext from avc warning */
	ret = mtk_get_scontext(data, scontext);
	if (!ret)
		return;

	/*check scontext is in warning list */
	ret = mtk_check_filter(scontext);
	if (ret >= 0) {
		pr_debug("[%s], In AEE Warning List scontext: %s\n",
			MOD, scontext);

		if (!IS_ENABLED(CONFIG_MTK_AEE_FEATURE))
			return;

		pname = mtk_get_process(scontext);

		if (pname != 0) {
			char printbuf[PRINT_BUF_LEN] = { '\0' };


			snprintf(printbuf, PRINT_BUF_LEN-1,
				"[%s][WARNING]\nCR_DISPATCH_PROCESSNAME:%s\n",
				MOD, pname);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT  | DB_OPT_NATIVE_BACKTRACE,
					printbuf, data);
#endif
		}
	}
#endif
#ifdef AV_FILTER
	mtk_check_av(data);
#endif
}

static void trigger_ne(void)
{
	/*
	 * delay after signal sent
	 * block current thread
	 * let other thread to handle stack
	 */
	send_sig(SIGABRT, current, 0);
	mdelay(1000);
}

static bool mtk_check_ne(char *scontext)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ne_list); i++) {
		if (!strcmp(scontext, ne_list[i]))
			return true;
	}
	return false;
}

/* reference avc_audit_pre_callback */
static size_t gen_avc_audit_pre_log(char *data, size_t size, struct selinux_audit_data *sad)
{
	const char *const *perms;
	int i, perm;
	u32 av = sad->audited;
	size_t len = 0;

	len += scnprintf(data, size, "avc: denied ");

	if (!av) {
		len += scnprintf(data + len, size - len, "null");
		return len;
	}

	len += scnprintf(data + len, size - len, " {");
	perms = secclass_map[sad->tclass - 1].perms;
	for (i = 0, perm = 1; i < (sizeof(av) * 8); i++, perm <<= 1) {
		if (!(perm & av) || !perms[i])
			continue;

		len += scnprintf(data + len, size - len, " %s", perms[i]);
		av &= ~perm;
	}

	if (av)
		len += scnprintf(data + len, size - len, " 0x%x", av);

	len += scnprintf(data + len, size - len, " }");
	return len;
}

static void selinux_aee(struct selinux_audit_data *sad, char *scontext,
			char *tcontext, const char *tclass)
{
	char pname[AEE_FILTER_LEN];
	char printbuf[PRINT_BUF_LEN];
	char data[PRINT_BUF_LEN];
	size_t len;

	if (!mtk_get_pname(scontext, pname))
		return;

	len = gen_avc_audit_pre_log(data, sizeof(data), sad);

	snprintf(data + len, sizeof(data) - len,
		 " scontext=%s tcontext=%s tclass=%s\n",
		 scontext, tcontext, tclass);

	snprintf(printbuf, sizeof(printbuf),
		 "[%s][WARNING]\nCR_DISPATCH_PROCESSNAME:%s\n",
		 MOD, pname);

	aee_kernel_warning_api(__FILE__, __LINE__,
			       DB_OPT_DEFAULT, printbuf, data);
}

static bool scontext_filter(char *scontext)
{
#ifdef SCONTEXT_FILTER
	int ret;

	/*check scontext is in warning list */
	ret = mtk_check_filter(scontext);
	if (ret >= 0) {
		pr_debug("[%s], In AEE Warning List scontext: %s\n",
			 MOD, scontext);
		return true;
	}
#endif
	return false;
}

static bool av_filter(struct selinux_audit_data *sad)
{
#ifdef AV_FILTER
	const char *const *perms;
	int i, j, perm;
	u32 av = sad->audited;

	if (!av)
		return false;

	perms = secclass_map[sad->tclass - 1].perms;

	/* reference avc_audit_pre_callback */
	for (i = 0, perm = 1; i < (sizeof(av) * 8); i++, perm <<= 1) {
		if (!(perm & av) || !perms[i])
			continue;

		for (j = 0; j < AEE_AV_FILTER_NUM && aee_av_filter_list[j];
		     j++) {
			if (!strcmp(perms[i], aee_av_filter_list[j]))
				return true;
		}
	}
#endif
	return false;
}

static void probe_selinux_audited(void *ignore, struct selinux_audit_data *sad,
				  char *scontext, char *tcontext,
				  const char *tclass)
{
	bool aee = false;

	if (!sad || !sad->denied || !enforcing_enabled(sad->state))
		return;
	if (!scontext)
		return;

	aee = scontext_filter(scontext) ||
	      (!strstr(scontext, "u:r:untrusted_app") && av_filter(sad));
	if (aee)
		selinux_aee(sad, scontext, tcontext, tclass);

	if (mtk_check_ne(scontext))
		trigger_ne();
}

static void register_tracepoint(struct tracepoint *tp, void *ignore)
{
	if (!strcmp(tp->name, "selinux_audited"))
		satp = tp;
}

static int __init selinux_init(void)
{
	int ret;

	for_each_kernel_tracepoint(register_tracepoint, NULL);
	ret = tracepoint_probe_register(satp, probe_selinux_audited, NULL);
	if (ret)
		return ret;
	pr_info("[SELinux] MTK SELinux init done\n");

	return 0;
}

static void __exit selinux_exit(void)
{
	tracepoint_probe_unregister(satp, probe_selinux_audited, NULL);
	tracepoint_synchronize_unregister();
	pr_info("[SELinux] MTK SELinux func exit\n");
}

module_init(selinux_init);
module_exit(selinux_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SELINUX Driver");
MODULE_AUTHOR("Kuan-Hsin Lee <kuan-hsin.lee@mediatek.com>");
