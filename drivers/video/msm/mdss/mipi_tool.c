#include <linux/lcd.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include "mdss_dsi.h"
const char *FILE_NAME = "/data/log/mipi_set";
static int original_values[6];
static long atoi(const char *name, int *l)
{
	long val = 0;
	while((*name!='\0' && *name!= '\n') && (*name >'9' || *name <'0')) {
		(*l)++;
		name++;
	}
	if(*name == '\0' || *name == '\n')
		return -1;
	for (;; name++) {
		(*l)++;
		switch (*name) {
		case '0' ... '9':
			val = 10*val+(*name-'0');
			break;
		case ' ':
			pr_debug("%s: val is: %ld\n", __func__, val);
			return val;
			break;
		case '\n':
			pr_debug("%s: val is: %ld\n", __func__, val);
			return val;
			break;
		case '\0':
			pr_debug("%s: val is: %ld\n", __func__, val);
			return val;
			break;
		default:
			return -1;
		}
	}
}
int writeBack(struct mdss_dsi_ctrl_pdata * ctrl_pdata) {
	struct file *fp = NULL;
	mm_segment_t fs;
	int ret = 0;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	u32 arr[6];
	char *buf = NULL;
	long l;
	loff_t pos;
	fs = get_fs();
	set_fs(get_ds());
	fp = filp_open(FILE_NAME, O_WRONLY, 0);
	if(IS_ERR(fp)) {
		pr_err("%s: Can't write %d\n", __func__, __LINE__);
		set_fs(fs);
		return -1;
	}
	if(ctrl_pdata == NULL) {
		pr_err("%s: Wrong panel data\n", __func__);
		filp_close(fp, current->files);
		set_fs(fs);
		return -2;
	}
	pdata = &ctrl_pdata->panel_data;
	pinfo = &(ctrl_pdata->panel_data.panel_info);
	l = 80;
	buf = kmalloc(l, GFP_KERNEL);
	if(buf==NULL) {
		pr_err("%s: Unable to allocate buffer %d\n", __func__, __LINE__);
		filp_close(fp, current->files);
		set_fs(fs);
		return -3;
	}
	memset(buf, 0, l);
	pos = 0;
	arr[0] = pinfo->lcdc.h_front_porch;
	arr[1] = pinfo->lcdc.h_pulse_width;
	arr[2] = pinfo->lcdc.h_back_porch;
	arr[3] = pinfo->lcdc.v_front_porch;
	arr[4] = pinfo->lcdc.v_pulse_width;
	arr[5] = pinfo->lcdc.v_back_porch;
	snprintf(buf, l, "c %d %d %d %d %d %d %d %d %d\n",pdata->panel_info.clk_rate, ctrl_pdata->pclk_rate, ctrl_pdata->byte_clk_rate, arr[0], arr[1], arr[2], arr[3], arr[4], arr[5]);
	ret = vfs_write(fp, (const char __user *) buf, l, &pos);
	if(ret!=l) {
		pr_err("%s: Can not write properly %d\n", __func__, __LINE__);
		kfree(buf);
		filp_close(fp, current->files);
		set_fs(fs);
		return -4;
	}
	kfree(buf);
	filp_close(fp, current->files);
	set_fs(fs);
	return 0;
}

int readFromSD(struct mdss_dsi_ctrl_pdata * ctrl_pdata) {
	struct file *fp = NULL;
	mm_segment_t fs;
	char id;
	int ret = 0;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	struct mipi_panel_info *mipi = NULL;
	u32 cnt=0, i, arr[6];
	char *buf = NULL;
	long l, val;
	loff_t pos;
	fs = get_fs();
	set_fs(get_ds());
	fp = filp_open(FILE_NAME, O_RDONLY, 0);
	if(IS_ERR(fp)) {
		pr_err("%s: Can't read %d\n", __func__, __LINE__);
		set_fs(fs);
		return -1;
	}
	if(ctrl_pdata == NULL) {
		pr_err("%s: Wrong panel data\n", __func__);
		filp_close(fp, current->files);
		set_fs(fs);
		return -2;
	}
	pdata = &ctrl_pdata->panel_data;
	pinfo = &(ctrl_pdata->panel_data.panel_info);
	mipi = &(pinfo->mipi);
	l = fp->f_path.dentry->d_inode->i_size;
	buf = kmalloc(l+10, GFP_KERNEL);
	if(buf==NULL) {
		pr_err("%s: Unable to allocate buffer\n", __func__);
		filp_close(fp, current->files);
		set_fs(fs);
		return -3;
	}
	memset(buf, 0, l);
	pos = 0;
	ret = vfs_read(fp, (char __user *)buf, l, &pos);
	if(ret!=l) {
		pr_err("%s: Can not read properly %s %d\n", __func__, buf, __LINE__);
		kfree(buf);
		filp_close(fp, current->files);
		set_fs(fs);
		return -4;
	}
	id = buf[0];
	cnt = 2;
	if(id=='c') {
		val = atoi(buf, &cnt);
		if(val==-1) {
			pr_err("%s: Invalid value read\n", __func__);
			goto InvalidRead;
		}
		pdata->panel_info.clk_rate = val;
		cnt = mdss_dsi_clk_div_config(pinfo, mipi->frame_rate);
		if (cnt) {
			pr_err("%s: unable to set the clk dividers\n", __func__);
		}
		ctrl_pdata->pclk_rate = mipi->dsi_pclk_rate;
		ctrl_pdata->byte_clk_rate = pinfo->clk_rate / 8;
	}
	else if(id=='p') {
		val = atoi(buf+cnt, &cnt);
		val = atoi(buf+cnt, &cnt);
		val = atoi(buf+cnt, &cnt);
		arr[0] = (int)atoi(buf+cnt, &cnt);
		if(arr[0] == 0) {
			arr[0] = original_values[0];
			arr[1] = original_values[1];
			arr[2] = original_values[2];
			arr[3] = original_values[3];
			arr[4] = original_values[4];
			arr[5] = original_values[5];
			goto update;
		}
		for(i=1; i<6; i++) {
			arr[i] = (int)atoi(buf+cnt, &cnt);
		}
		for(i=0; i<6; i++) {
			if(arr[i] == -1)
				break;
		}
		if(i<6) {
			pr_err("%s: Invalid value entered %d\n", __func__, __LINE__);
			goto InvalidRead;
		}
	update:
		pinfo->lcdc.h_front_porch= arr[0];
		pinfo->lcdc.h_pulse_width= arr[1];
		pinfo->lcdc.h_back_porch= arr[2];
		pinfo->lcdc.v_front_porch= arr[3];
		pinfo->lcdc.v_pulse_width= arr[4];
		pinfo->lcdc.v_back_porch= arr[5];
		pinfo->clk_rate = 0;
		ret = mdss_dsi_clk_div_config(pinfo, mipi->frame_rate);
		if (ret) {
			pr_err("%s: unable to set the clk dividers\n", __func__);
		}
		ctrl_pdata->pclk_rate = mipi->dsi_pclk_rate;
		ctrl_pdata->byte_clk_rate = pinfo->clk_rate / 8;
	}
InvalidRead:
	kfree(buf);
	filp_close(fp, current->files);
	set_fs(fs);
	return ret;
}
int tune_file_create(struct mdss_dsi_ctrl_pdata * ctrl_pdata) {
	struct file *fp = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	u32 arr[6];
	char *buf = NULL;
	long l, ret;
	loff_t pos;
	mm_segment_t fs;
	l = 80;
	if(ctrl_pdata == NULL) {
		pr_err("%s: Wrong panel data %d\n", __func__, __LINE__);
		return 1;
	}
	fs = get_fs();
	set_fs(get_ds());
	fp = filp_open(FILE_NAME, O_CREAT | O_RDWR, 0666);
	sys_chmod(FILE_NAME, 0777);
	if(IS_ERR(fp)) {
		pr_err("%s: Unable to create file a %d\n", __func__, __LINE__);
		set_fs(fs);
		return -1;
	}
	pdata = &ctrl_pdata->panel_data;
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	buf = kmalloc(l, GFP_KERNEL);
	if(buf==NULL) {
		pr_err("%s: Unable to allocate buffer %d\n", __func__, __LINE__);
		filp_close(fp, current->files);
		set_fs(fs);
		return -2;
	}
	memset(buf, 0, l);
	pos = 0;
	arr[0] = pinfo->lcdc.h_front_porch;
	arr[1] = pinfo->lcdc.h_pulse_width;
	arr[2] = pinfo->lcdc.h_back_porch;
	arr[3] = pinfo->lcdc.v_front_porch;
	arr[4] = pinfo->lcdc.v_pulse_width;
	arr[5] = pinfo->lcdc.v_back_porch;
	snprintf(buf, l, "c %d %d %d %d %d %d %d %d %d\n",pdata->panel_info.clk_rate, ctrl_pdata->pclk_rate, ctrl_pdata->byte_clk_rate, arr[0], arr[1], arr[2], arr[3], arr[4], arr[5]);
	ret = vfs_write(fp, (const char __user *) buf, l, &pos);
	if(ret!=l) {
		pr_err("%s: Can not write properly %d\n", __func__, __LINE__);
		kfree(buf);
		filp_close(fp, current->files);
		set_fs(fs);
		return -3;
	}
	kfree(buf);
	filp_close(fp, current->files);
	set_fs(fs);
	original_values[0] = pinfo->lcdc.h_front_porch;
	original_values[1] = pinfo->lcdc.h_pulse_width;
	original_values[2] = pinfo->lcdc.h_back_porch;
	original_values[3] = pinfo->lcdc.v_front_porch;
	original_values[4] = pinfo->lcdc.v_pulse_width;
	original_values[5] = pinfo->lcdc.v_back_porch;
	return 0;
}
