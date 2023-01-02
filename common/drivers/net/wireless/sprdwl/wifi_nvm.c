#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/namei.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include "wifi_nvm.h"

#define MAX_BOARD_TYPE_LEN 32
#ifndef FALSE
#define FALSE       0
#endif
#ifndef TRUE
#define TRUE        1
#endif

/*static char *WIFI_CONFIG_FILE = "/system/etc/connectivity_configure.ini";*/
static char *WIFI_CALI_FILE_PRODUCT = "/productinfo/connectivity_calibration.ini";
static char *WIFI_CALI_FILE_SYSTEM = "/system/etc/connectivity_calibration.ini";


static struct wifi_nvm_data g_wifi_nvm_data;

struct wifi_nvm_data *get_gwifi_nvm_data(void)
{
	return &g_wifi_nvm_data;
}

static int find_type(char key)
{
	if ((key >= 'a' && key <= 'w')
	    || (key >= 'y' && key <= 'z')
	    || (key >= 'A' && key <= 'W')
	    || (key >= 'Y' && key <= 'Z')
	    || ('_' == key))
		return 1;
	if ((key >= '0' && key <= '9')
	    || ('-' == key))
		return 2;
	if (('x' == key)
	    || ('X' == key)
	    || ('.' == key))
		return 3;
	if ((key == '\0')
	    || ('\r' == key)
	    || ('\n' == key)
	    || ('#' == key))
		return 4;
	return 0;
}

static void get_cmd_par(unsigned char *str, struct nvm_cali_cmd *cmd)
{
	int i, j, buf_type, c_type, flag;
	char tmp[128];
	char c;
	buf_type = -1;
	c_type = 0;
	flag = 0;
	memset(cmd, 0, sizeof(struct nvm_cali_cmd));
	for (i = 0, j = 0;; i++) {
		c = str[i];
		c_type = find_type(c);
		if ((1 == c_type) || (2 == c_type) || (3 == c_type)) {
			tmp[j] = c;
			j++;
			if (-1 == buf_type) {
				if (2 == c_type)
					buf_type = 2;
				else
					buf_type = 1;
			} else if (2 == buf_type) {
				if (1 == c_type)
					buf_type = 1;
			}
			continue;
		}
		if (-1 != buf_type) {
			tmp[j] = '\0';

			if ((1 == buf_type) && (0 == flag)) {
				strcpy(cmd->sprdwl, tmp);
				flag = 1;
			} else {
				cmd->par[cmd->num] =
				    simple_strtol(tmp, NULL, 0);
				cmd->num++;
			}
			buf_type = -1;
			j = 0;
		}
		if (0 == c_type)
			continue;
		if (4 == c_type)
			return;
	}
	return;
}

static int wifi_nvm_set_cmd(struct nvm_name_table *p_table,
			    struct nvm_cali_cmd *cmd)
{
	int i;
	unsigned char *p;
	if ((1 != p_table->type) && (2 != p_table->type)
	    && (4 != p_table->type))
		return -1;
	p = (unsigned char *)(&g_wifi_nvm_data) + p_table->mem_offset;
	pr_debug("###[g_table]%s, offset:%d, num:%d, value: %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d\n",
		 p_table->sprdwl, p_table->mem_offset, cmd->num,
		 cmd->par[0], cmd->par[1], cmd->par[2],
		 cmd->par[3], cmd->par[4], cmd->par[5],
		 cmd->par[6], cmd->par[7], cmd->par[8],
		 cmd->par[9], cmd->par[10], cmd->par[11]);
	for (i = 0; i < cmd->num; i++) {
		if (1 == p_table->type)
			*((unsigned char *)p + i) =
			    (unsigned char)(cmd->par[i]);
		else if (2 == p_table->type)
			*((unsigned short *)p + i) =
			    (unsigned short)(cmd->par[i]);
		else if (4 == p_table->type)
			*((unsigned int *)p + i) = (unsigned int)(cmd->par[i]);
		else
			pr_err("%s, type err\n", __func__);
	}
	return 0;
}

static struct nvm_name_table *wifi_nvm_table_match(struct nvm_cali_cmd *cmd)
{
	int i;
	struct nvm_name_table *p_table = NULL;
	int len = sizeof(g_nvm_table) / sizeof(struct nvm_name_table);
	if (NULL == cmd->sprdwl)
		return NULL;
	for (i = 0; i < len; i++) {
		if (NULL == g_nvm_table[i].sprdwl)
			continue;
		if (0 != strcmp(g_nvm_table[i].sprdwl, cmd->sprdwl))
			continue;
		p_table = &g_nvm_table[i];
		break;
	}
	return p_table;
}

static int wifi_nvm_buf_operate(unsigned char *p_buf, int file_len)
{
	int i, p;
	struct nvm_cali_cmd cmd;
	struct nvm_name_table *p_table = NULL;
	if ((NULL == p_buf) || (0 == file_len))
		return -1;
	for (i = 0, p = 0; i < file_len; i++) {
		if (('\n' == *(p_buf + i)) || ('\r' == *(p_buf + i))
		    || ('\0' == *(p_buf + i))) {
			if (5 <= (i - p)) {
				get_cmd_par((p_buf + p), &cmd);
				p_table = wifi_nvm_table_match(&cmd);
				if (NULL != p_table)
					wifi_nvm_set_cmd(p_table, &cmd);
			}
			p = i + 1;
		}

	}
	return 0;
}

int wifi_nvm_parse(const char *path)
{
	struct file *filp = NULL;
	struct inode *inode = NULL;
	unsigned char *p_buf = NULL;
	unsigned short len = 0;
	mm_segment_t oldfs;

	pr_info("%s()...\n", __func__);

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDONLY, S_IRUSR);
	if (IS_ERR(filp)) {
		pr_err("%s: file %s filp_open error\n", __func__, path);
		goto nvm_read_err;
	}
	if (!filp->f_op) {
		pr_err("%s: File Operation Method Error\n", __func__);
		goto nvm_read_err;
	}
	inode = filp->f_path.dentry->d_inode;
	if (!inode) {
		pr_err("%s: Get inode from filp failed\n", __func__);
		goto nvm_read_err;
	}
	len = i_size_read(inode->i_mapping->host);
	if (!(len > 0 && len < 62 * 1024)) {
		pr_err("%s file size error\n", __func__);
		goto file_close;
	}
	p_buf = kmalloc(len, GFP_KERNEL);
	if (!p_buf) {
		pr_err("%s alloctation memory failed\n", __func__);
		goto file_close;
	}
	if (len != filp->f_op->read(filp, p_buf, len, &filp->f_pos)) {
		pr_err("%s file read error\n", __func__);
		goto free_buf;
	}
	filp_close(filp, NULL);
	set_fs(oldfs);

	pr_info("%s read %s data_len:0x%x\n", __func__, path, len);
	wifi_nvm_buf_operate(p_buf, len);
	kfree(p_buf);
	pr_info("%s(), ok!\n", __func__);
	return 0;

free_buf:
	kfree(p_buf);
file_close:
	filp_close(filp, NULL);
nvm_read_err:
	set_fs(oldfs);
	pr_err("%s(), err!\n", __func__);
	return -1;
}

bool wifi_cali_file_check(const char *path)
{
	struct file *filp;
	struct file *filp_2;
	const char *path_2 = "/system/etc/connectivity_calibration.ini";
	unsigned char *p_buf = NULL;
	unsigned short len = 0;
	loff_t pos;

	mm_segment_t fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(path, O_RDONLY, S_IRUSR);
	if (IS_ERR(filp)) {
		pr_err("%s,Unable to load '%s' err=%ld.\n", __func__, path, PTR_ERR(filp));
		filp =
		    filp_open(path, O_RDWR | O_CREAT | O_TRUNC,
			      S_IRUSR | S_IWUSR);
		if (IS_ERR(filp)) {
			pr_err("%s,Unable to Creat '%s' err=%ld.\n",
			       __func__, path, PTR_ERR(filp));
			goto file_create_error;
		}
	} else {
		pr_err("%s,connectivity_calibration.ini check oK.\n", __func__);
		goto file_check_done;
	}

	filp_2 = filp_open(path_2, O_RDONLY, S_IRUSR);
	if (IS_ERR(filp)) {
		pr_err("%s,Unable to load '%s'.\n", __func__, path_2);
		goto file2_open_error;
	}

	len = i_size_read(file_inode(filp_2));
	if (!(len > 0 && len < 62 * 1024)) {
		pr_err("%s file size error %d\n", __func__, len);
		goto file_close;
	}
	p_buf = kmalloc(len, GFP_KERNEL);
	if (p_buf == NULL) {
		pr_err("%s Out of memory loading '%s'.\n", __func__, path);
		goto file_close;
	}
	pos = 0;
	if (vfs_read(filp_2, p_buf, len, &pos) != len) {
		pr_err("%s Failed to read '%s'.\n", __func__, path);
		kfree(p_buf);
		goto file_close;
	}

	vfs_write(filp, p_buf, len, &filp->f_pos);
	kfree(p_buf);
	filp_close(filp_2, NULL);
file_check_done:
	filp_close(filp, NULL);
	set_fs(fs);
	pr_info("%s Done  '%s'.\n", __func__, path);
	return TRUE;

file_close:
	filp_close(filp_2, NULL);
file2_open_error:
	filp_close(filp, NULL);
file_create_error:
	set_fs(fs);
	return FALSE;
}

void sprdwl_nvm_init(void)
{
	int ret = 0;
	int len = 0;
	int board_type = 0;
	char board_type_str[MAX_BOARD_TYPE_LEN] = {0};

#if (defined CONFIG_MACH_SP7715EA) \
	|| (defined CONFIG_MACH_SP7715EAOPENPHONE) \
	|| (defined CONFIG_MACH_SP7715EATRISIM) \
	|| (defined CONFIG_MACH_SP7715GA) \
	|| (defined CONFIG_MACH_SP7715GATRISIM) \
	|| (defined CONFIG_MACH_SP8815GA) \
	|| (defined CONFIG_MACH_SP8815GAOPENPHONE) \
	|| (defined CONFIG_MACH_SPX35EC) \
	|| (defined CONFIG_MACH_SP8830GA) \
	|| (defined CONFIG_MACH_SP8835EB) \
	|| (defined CONFIG_MACH_KANAS_W) \
	|| (defined CONFIG_MACH_KANAS_TD) \
	|| (defined CONFIG_MACH_SP5735C1EA) \
	|| (defined CONFIG_MACH_SP5735EA)
	char *WIFI_CONFIG_FILE[] = {
		"/system/etc/connectivity_configure_hw100.ini",
		"/system/etc/connectivity_configure_hw102.ini",
		"/system/etc/connectivity_configure_hw104.ini"
	};
#else
	char *WIFI_CONFIG_FILE[] = {
		"/system/etc/connectivity_configure.ini"
	};
#endif

#if (defined CONFIG_MACH_SP7715EA) \
	|| (defined CONFIG_MACH_SP7715EAOPENPHONE) \
	|| (defined CONFIG_MACH_SP7715EATRISIM) \
	|| (defined CONFIG_MACH_SP7715GA) \
	|| (defined CONFIG_MACH_SP7715GATRISIM) \
	|| (defined CONFIG_MACH_SP8815GA) \
	|| (defined CONFIG_MACH_SP8815GAOPENPHONE) \
	|| (defined CONFIG_MACH_SPX35EC) \
	|| (defined CONFIG_MACH_SP8830GA) \
	|| (defined CONFIG_MACH_SP8835EB) \
	|| (defined CONFIG_MACH_KANAS_W) \
	|| (defined CONFIG_MACH_KANAS_TD) \
	|| (defined CONFIG_MACH_SP5735C1EA) \
	|| (defined CONFIG_MACH_SP5735EA)

	sprd_kernel_get_board_type(board_type_str, MAX_BOARD_TYPE_LEN);
	if (strstr(board_type_str, "1.0.0"))
		board_type = 0;
	else if (strstr(board_type_str, "1.0.2"))
		board_type = 1;
	else
		board_type = 2;	/* default is 1.0.4 */
#endif
	pr_info("#### %s get2 board type len %d %s type %d ####\n",
		__func__, len, board_type_str, board_type);
	ret = wifi_nvm_parse(WIFI_CONFIG_FILE[board_type]);
	if (0 != ret) {
		pr_err("%s(),parse:%s, err!\n", __func__,
		       WIFI_CONFIG_FILE[board_type]);
		return;
	}

	if (wifi_cali_file_check(WIFI_CALI_FILE_PRODUCT) == TRUE) {
		pr_info("%s(),read %s success\n",__func__, WIFI_CALI_FILE_PRODUCT);
		pr_info("%s(),load %s \n",__func__, WIFI_CALI_FILE_PRODUCT);
		ret = wifi_nvm_parse(WIFI_CALI_FILE_PRODUCT);
		if (0 != ret) {
		    pr_err("%s(),parse:%s, err!\n", __func__, WIFI_CALI_FILE_PRODUCT);
		    return;
		}
	} else {/* read file from product failed */
		pr_err("%s(),read %s fail\n",__func__, WIFI_CALI_FILE_PRODUCT);
		pr_err("%s(),load %s \n",__func__, WIFI_CALI_FILE_SYSTEM);
		ret = wifi_nvm_parse(WIFI_CALI_FILE_SYSTEM);
		if (0 != ret) {
		    pr_err("%s(),parse:%s, err!\n", __func__, WIFI_CALI_FILE_SYSTEM);
		    return;
		}
	}

	g_wifi_nvm_data.data_init_ok = 1;
	pr_info("%s(), ok!\n", __func__);
	return;
}
