/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <scsc/scsc_logring.h>
#include "mifproc.h"
#include "scsc_mif_abs.h"

static struct proc_dir_entry *procfs_dir;

/* WARNING --- SINGLETON FOR THE TIME BEING */
/* EXTEND PROC ENTRIES IF NEEDED!!!!! */
struct scsc_mif_abs *mif_global;

static int mifprocfs_open_file_generic(struct inode *inode, struct file *file)
{
	file->private_data = MIF_PDE_DATA(inode);
	return 0;
}

MIF_PROCFS_RW_FILE_OPS(mif_dump);
MIF_PROCFS_RW_FILE_OPS(mif_writemem);
MIF_PROCFS_RW_FILE_OPS(mif_reg);
/*
MIF_PROCFS_SEQ_FILE_OPS(mif_dbg);
*/

static ssize_t mifprocfs_mif_reg_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	/* Avoid unused parameter error */
	(void)file;

	pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "OK");

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}


static ssize_t mifprocfs_mif_writemem_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	/* Avoid unused parameter error */
	(void)file;

	pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "OK");

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t mifprocfs_mif_dump_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	/* Avoid unused parameter error */
	(void)file;

	pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "OK");

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t mifprocfs_mif_writemem_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	char         *sptr, *token;
	unsigned int len = 0, pass = 0;
	u32          value = 0, address = 0;
	int          match = 0;
	void         *mem;

	/* Avoid unused parameter error */
	(void)file;
	(void)ppos;


	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	while ((token = strsep(&sptr, " ")) != NULL) {
		switch (pass) {
		/* register */
		case 0:
			if ((token[0] == '0') && (token[1] == 'x')) {
				if (kstrtou32(token, 16, &address)) {
					SCSC_TAG_INFO(MIF, "Wrong format: <address> <value (hex)>\n");
					SCSC_TAG_INFO(MIF, "Example: \"0xaaaabbbb 0xcafecafe\"\n");
					goto error;
				}
			} else {
				SCSC_TAG_INFO(MIF, "Wrong format: <address> <value (hex)>\n");
				SCSC_TAG_INFO(MIF, "Example: \"0xaaaabbbb 0xcafecafe\"\n");
				goto error;
			}
			break;
		/* value */
		case 1:
			if ((token[0] == '0') && (token[1] == 'x')) {
				if (kstrtou32(token, 16, &value)) {
					SCSC_TAG_INFO(MIF, "Wrong format: <address> <value (hex)>\n");
					SCSC_TAG_INFO(MIF, "Example: \"0xaaaabbbb 0xcafecafe\"\n");
					goto error;
				}
			} else {
				SCSC_TAG_INFO(MIF, "Wrong format: <address> <value (hex)>\n");
				SCSC_TAG_INFO(MIF, "Example: \"0xaaaabbbb 0xcafecafe\"\n");
				goto error;
			}
			break;
		}
		pass++;
	}
	if (pass != 2 && !match) {
		SCSC_TAG_INFO(MIF, "Wrong format: <address> <value (hex)>\n");
		SCSC_TAG_INFO(MIF, "Example: \"0xaaaabbbb 0xcafecafe\"\n");
		goto error;
	}

	/* Get memory offset */
	mem = mif_global->get_mifram_ptr(mif_global, 0);
	if (!mem) {
		SCSC_TAG_INFO(MIF, "Mem not allocated\n");
		goto error;
	}

	SCSC_TAG_INFO(MIF, "Setting value 0x%x at address 0x%x offset\n", value, address);


	*((u32 *)(mem + address)) = value;
error:
	return count;
}

static ssize_t mifprocfs_mif_dump_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	char         *sptr, *token;
	unsigned int len = 0, pass = 0;
	u32          address = 0;
	u32          size;
	u8           unit;
	void         *mem;

	(void)file;
	(void)ppos;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	while ((token = strsep(&sptr, " ")) != NULL) {
		switch (pass) {
		/* address */
		case 0:
			if ((token[0] == '0') && (token[1] == 'x')) {
				if (kstrtou32(token, 16, &address)) {
					SCSC_TAG_INFO(MIF, "Incorrect format,,,address should start by 0x\n");
					SCSC_TAG_INFO(MIF, "Example: \"0xaaaabbbb 256 8\"\n");
					goto error;
				}
				SCSC_TAG_INFO(MIF, "address %d 0x%x\n", address, address);
			} else {
				SCSC_TAG_INFO(MIF, "Incorrect format,,,address should start by 0x\n");
				SCSC_TAG_INFO(MIF, "Example: \"0xaaaabbbb 256 8\"\n");
				goto error;
			}
			break;
		/* size */
		case 1:
			if (kstrtou32(token, 0, &size)) {
				SCSC_TAG_INFO(MIF, "Incorrect format,,, for size\n");
				goto error;
			}
			SCSC_TAG_INFO(MIF, "size: %d\n", size);
			break;

		/* unit */
		case 2:
			if (kstrtou8(token, 0, &unit)) {
				SCSC_TAG_INFO(MIF, "Incorrect format,,, for unit\n");
				goto error;
			}
			if ((unit != 8) && (unit != 16) && (unit != 32)) {
				SCSC_TAG_INFO(MIF, "Unit %d should be 8/16/32\n", unit);
				goto error;
			}
			SCSC_TAG_INFO(MIF, "unit: %d\n", unit);
			break;
		}
		pass++;
	}
	if (pass != 3) {
		SCSC_TAG_INFO(MIF, "Wrong format: <start_address> <size> <unit>\n");
		SCSC_TAG_INFO(MIF, "Example: \"0xaaaabbbb 256 8\"\n");
		goto error;
	}

	mem = mif_global->get_mifram_ptr(mif_global, 0);
	if (!mem) {
		SCSC_TAG_INFO(MIF, "Mem not allocated\n");
		goto error;
	}

	/* Add offset */
	mem = mem + address;

	SCSC_TAG_INFO(MIF, "Phy addr :%p ref addr :%x\n", mem, address);
	SCSC_TAG_INFO(MIF, "------------------------------------------------------------------------\n");
	print_hex_dump(KERN_WARNING, "ref addr offset: ", DUMP_PREFIX_OFFSET, 16, unit/8, mem, size, 1);
	SCSC_TAG_INFO(MIF, "------------------------------------------------------------------------\n");
error:
	return count;
}

static ssize_t mifprocfs_mif_reg_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	void         *mem;

	if (!mif_global) {
		SCSC_TAG_INFO(MIF, "Endpoint not registered\n");
		return 0;
	}

	mem = mif_global->get_mifram_ptr(mif_global, 0);

	SCSC_TAG_INFO(MIF, "Phy addr :%p\n", mem);

	mif_global->mif_dump_registers(mif_global);

	return count;
}
/*
 * TODO: Add here any debug message should be exported
static int mifprocfs_mif_dbg_show(struct seq_file *m, void *v)
{
	(void)v;

	if (!mif_global) {
		seq_puts(m, "endpoint not registered");
		return 0;
	}
	return 0;
}
*/

static const char *procdir = "driver/mif_ctrl";

#define MIF_DIRLEN 128


int mifproc_create_proc_dir(struct scsc_mif_abs *mif)
{
	char                  dir[MIF_DIRLEN];
	struct proc_dir_entry *parent;

	/* WARNING --- SINGLETON FOR THE TIME BEING */
	/* EXTEND PROC ENTRIES IF NEEDED!!!!! */
	if (mif_global)
		return -EBUSY;

	(void)snprintf(dir, sizeof(dir), "%s", procdir);
	parent = proc_mkdir(dir, NULL);
	if (parent) {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 4, 0))
		parent->data = NULL;
#endif
		procfs_dir = parent;

		MIF_PROCFS_ADD_FILE(NULL, mif_writemem, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		MIF_PROCFS_ADD_FILE(NULL, mif_dump, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		MIF_PROCFS_ADD_FILE(NULL, mif_reg, parent, S_IRUSR | S_IRGRP);
		/*
		MIF_PROCFS_SEQ_ADD_FILE(NULL, mif_dbg, parent, S_IRUSR | S_IRGRP | S_IROTH);
		*/
	} else {
		SCSC_TAG_INFO(MIF, "failed to create /proc dir\n");
		return -EINVAL;
	}

	mif_global = mif;

	return 0;
/*
err:
	return -EINVAL;
*/
}

void mifproc_remove_proc_dir(void)
{
	if (procfs_dir) {
		char dir[MIF_DIRLEN];

		MIF_PROCFS_REMOVE_FILE(mif_writemem, procfs_dir);
		MIF_PROCFS_REMOVE_FILE(mif_dump, procfs_dir);
		MIF_PROCFS_REMOVE_FILE(mif_reg, procfs_dir);
		/*
		MIF_PROCFS_REMOVE_FILE(mif_dbg, procfs_dir);
		*/

		(void)snprintf(dir, sizeof(dir), "%s", procdir);
		remove_proc_entry(dir, NULL);
		procfs_dir = NULL;
	}

	mif_global = NULL;
}
