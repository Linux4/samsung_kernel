#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/path.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "wlan_common.h"

static int hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;

	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return -1;
}

static int hex2byte(const char *hex)
{
	int a, b;

	a = hex2num(*hex++);
	if (a < 0)
		return -1;

	b = hex2num(*hex++);
	if (b < 0)
		return -1;

	return (a << 4) | b;
}

/**
 * hexstr2bin - Convert ASCII hex string into binary data
 * @hex: ASCII hex string (e.g., "01ab")
 * @buf: Buffer for the binary data
 * @len: Length of the text to convert in bytes (of buf); hex will be double
 * this size
 * Returns: 0 on success, -1 on failure (invalid hex string)
 */
static int hexstr2bin(const char *hex, u8 *buf, size_t len)
{
	size_t i;
	int a;
	const char *ipos = hex;

	u8 *opos = buf;
	for (i = 0; i < len; i++) {
		a = hex2byte(ipos);
		if (a < 0)
			return -1;
		*opos++ = a;
		ipos += 2;
	}

	return 0;
}

static struct hostap_conf *hostap_conf_create(void)
{
	struct hostap_conf *conf = NULL;
	conf = kmalloc(sizeof(struct hostap_conf), GFP_KERNEL);

	return conf;
}

static int fp_size(struct file *f)
{
	int error = -EBADF;
	struct kstat stat;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	error = vfs_getattr(&f->f_path, &stat);
#else
	error = vfs_getattr(f->f_path.mnt, f->f_path.dentry, &stat);
#endif

	if (error == 0) {
		return stat.size;
	} else {
		pr_err("get hostapd conf file stat error\n");
		return error;
	}
}

static int hostap_conf_read(char *filename, char **buf)
{
	struct file *fp;
	mm_segment_t fs;
	int size = 0;
	loff_t pos = 0;

	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("open %s file error\n", filename);
		goto end;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	size = fp_size(fp);
	if (size <= 0) {
		pr_err("load file:%s error\n", filename);
		goto error;
	}

	*buf = kzalloc(size + 1, GFP_KERNEL);
	vfs_read(fp, *buf, size, &pos);

error:
	filp_close(fp, NULL);
	set_fs(fs);
end:
	return size;
}

static char *get_line(char *buf, char *end)
{
	if (buf == NULL || buf >= end)
		return NULL;
	while (buf != end && *buf != '\n')
		buf++;

	return buf;
}

static unsigned char hostap_conf_parse(char *buf, int size,
				       struct hostap_conf *conf)
{
	unsigned char ret = 0;
	char *spos = buf, *epos = NULL, *pos = NULL;
	int line = 0, errors = 0;

	if (!buf || !conf)
		return 0;

	for (; (epos = get_line(spos, buf + size)); (spos = epos + 1)) {
		line++;
		if (spos[0] == '#')
			continue;
		pos = spos;
		while (*pos != '\0' && pos <= epos) {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}

		if (spos[0] == '\0')
			continue;

		pos = strchr(spos, '=');
		if (pos == NULL) {
			pr_err("Line %d: invalid line '%s'", line, spos);
			errors++;
			continue;
		}

		*pos = '\0';
		pos++;

		if (strcmp(spos, "wpa_psk") == 0) {
			strlcpy(conf->wpa_psk, pos, sizeof(conf->wpa_psk));
			conf->len = strlen(pos);
		} else {
			continue;
		}
	}

	return ret;
}

int hostap_conf_load(char *filename, unsigned char *key_val)
{
	struct hostap_conf *conf = NULL;
	char *buf = NULL;
	int size = 0;

	if (filename == NULL)
		filename = HOSTAP_CONF_FILE_NAME;

	size = hostap_conf_read(filename, &buf);
	if (size > 0) {
		conf = hostap_conf_create();
		if (conf == NULL) {
			kfree(buf);
			pr_err("create hostap_conf struct error.\n");
			return -EINVAL;
		}

		hostap_conf_parse(buf, size, conf);
		if (conf->len > 64) {
			pr_err("wpa_psk len is error.(%d)\n", conf->len);
			return -EINVAL;
		}
		hexstr2bin(conf->wpa_psk, key_val, conf->len / 2);
		kfree(buf);
	}

	return 0;
}

