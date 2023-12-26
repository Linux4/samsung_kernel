/*
  *
  **************************************************************************
  **                        STMicroelectronics				 **
  **************************************************************************
  **                        marco.cali@st.com				*
  **************************************************************************
  *                                                                        *
  *                     Utilities published in /proc/fts		*
  *                                                                        *
  **************************************************************************
  **************************************************************************
  *
  */

/*!
  * \file fts_proc.c
  * \brief contains the function and variables needed to publish a file node in
  * the file system which allow to communicate with the IC from userspace
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "stm_dev.h"

static struct stm_ts_data *proc_ts;

char tag[12] = "[fts_touch]\0";

#define FTS_TS_DRV_VER		0x05021300	/* driver version u32 format */

/* FIRST LEVEL ERROR CODE */
/** @defgroup first_level	First Level Error Code
  * @ingroup error_codes
  * Errors related to low level operation which are not under control of driver,
  * such as: communication protocol (I2C/SPI), timeout, file operations ...
  * @{
  */
#define OK				((int)0x00000000)	/* /< No ERROR */
#define ERROR_ALLOC		((int)0x80000001)	/* /< allocation of memory failed */
#define ERROR_BUS_R		((int)0x80000002)	/* /< i2c/spi read failed */
#define ERROR_BUS_W		((int)0x80000003)	/* /< i2c/spi write failed */
#define ERROR_BUS_WR	((int)0x80000004)	/* /< i2c/spi write/read failed */
#define ERROR_BUS_O		((int)0x80000005)	/* /< error during opening an i2c device */
#define ERROR_OP_NOT_ALLOW	((int)0x80000006)	/* /< operation not allowed */
#define ERROR_TIMEOUT		((int)0x80000007)	/* /< timeout expired! exceed the max number of retries or the max waiting time */

#define DRIVER_TEST_FILE_NODE		"driver_test"
#define CHUNK_PROC				1024

#define MAX_PRINT_SIZE			4096

/** @}*/

/** @defgroup scriptless Scriptless Protocol
  * @ingroup proc_file_code
  * Scriptless Protocol allows ST Software (such as FingerTip Studio etc) to
  * communicate with the IC from an user space.
  * This mode gives access to common bus operations (write, read etc) and
  * support additional functionalities. \n
  * The protocol is based on exchange of binary messages included between a
  * start and an end byte
  * @{
  */

#define CMD_DRIVER_VERSION			0x00
#define CMD_WRITE					0x01
#define CMD_WRITEREAD				0x02

/**
  * Struct used to contain info of the message received by the host in
  * Scriptless mode
*/

static int limit;	/* /< store the amount of data to print into the shell*/
static int chunk;	/* /< store the chuk of data that should be printed in this iteration */
static int printed;	/* /< store the amount of data already printed in the shell */
static struct proc_dir_entry *fts_dir;	/* /< reference to the directory fts under /proc */
static u8 test_print_buff[MAX_PRINT_SIZE] = { 0 };

char buf_chunk[CHUNK_PROC] = { 0 };	/* /< buffer used to store the message info received */

/************************ SEQUENTIAL FILE UTILITIES **************************/
/**
  * This function is called at the beginning of the stream to a sequential file
  * or every time into the sequential were already written PAGE_SIZE bytes and
  * the stream need to restart
  * @param s pointer to the sequential file on which print the data
  * @param pos pointer to the offset where write the data
  * @return NULL if there is no data to print or the pointer to the beginning of
  * the data that need to be printed
  */
static void *fts_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos != 0)
		*pos += chunk - 1;

	if (*pos >= limit) {
		return NULL;
	}

	chunk = CHUNK_PROC;
	if (limit - *pos < CHUNK_PROC)
		chunk = limit - *pos;

	memset(buf_chunk, 0, CHUNK_PROC);
	memcpy(buf_chunk, &test_print_buff[(int)*pos], chunk);

	return buf_chunk;
}

/**
  * This function actually print a chunk amount of data in the sequential file
  * @param s pointer to the sequential file where to print the data
  * @param v pointer to the data to print
  * @return 0
  */
static int fts_seq_show(struct seq_file *s, void *v)
{
	seq_write(s, (u8 *)v, chunk);
	printed += chunk;
	return 0;
}

/**
  * This function update the pointer and the counters to the next data to be
  * printed
  * @param s pointer to the sequential file where to print the data
  * @param v pointer to the data to print
  * @param pos pointer to the offset where write the next data
  * @return NULL if there is no data to print or the pointer to the beginning of
  * the next data that need to be printed
  */
static void *fts_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos) += chunk;/* increase my position counter */
	chunk = CHUNK_PROC;
	
	if (*pos >= limit)
		return NULL;
	else if (limit - *pos < CHUNK_PROC)
		chunk = limit - *pos;
	memset(buf_chunk, 0, CHUNK_PROC);
	memcpy(buf_chunk, &test_print_buff[(int)*pos], chunk);
	return buf_chunk;
}

/**
  * This function is called when there are no more data to print  the stream
  *need to be terminated or when PAGE_SIZE data were already written into the
  *sequential file
  * @param s pointer to the sequential file where to print the data
  * @param v pointer returned by fts_seq_next
  */
static void fts_seq_stop(struct seq_file *s, void *v)
{
	if (v) {

	} else {

		limit = 0;
		chunk = 0;
		printed = 0;
	}
}

/**
  * Struct where define and specify the functions which implements the flow for
  *writing on a sequential file
  */
static const struct seq_operations fts_seq_ops = {
	.start = fts_seq_start,
	.next = fts_seq_next,
	.stop = fts_seq_stop,
	.show = fts_seq_show
};

/**
  * This function open a sequential file
  * @param inode Inode in the file system that was called and triggered this
  * function
  * @param file file associated to the file node
  * @return error code, 0 if success
  */
static int fts_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &fts_seq_ops);
};

/*****************************************************************************/

/**************************** DRIVER TEST ************************************/

/** @addtogroup proc_file_code
  * @{
  */

/**
  * Receive the OP code and the inputs from shell when the file node is called,
  * parse it and then execute the corresponding function
  * echo cmd+parameters > /proc/fts/driver_test to execute the select command
  * cat /proc/fts/driver_test			to obtain the result into the
  * shell \n
  * the string returned in the shell is made up as follow: \n
  * { = start byte \n
  * the answer content and format strictly depend on the cmd executed. In
  * general can be: an HEX string or a byte array (e.g in case of 0xF- commands)
  * \n
  * } = end byte \n
  */
static ssize_t fts_driver_test_write(struct file *file, const char __user *buf,
									 size_t count, loff_t *pos)
{
	int res = OK;
	char *pbuf = NULL;
	char *p = NULL;
	u32 *func_to_test = NULL;
	u8 *cmd = NULL;
	int number_param = 0;
	u8 *read_buf = NULL;
	u16 to_read = 0;
	int index = 0;
	int size = 0;
	int dummy = 0;
	int i = 0;

	pbuf = (u8 *)kmalloc(count * sizeof(u8), GFP_KERNEL);
	if (pbuf == NULL) {
		pr_info("%s Error allocating memory\n", tag);
		res = ERROR_ALLOC;
		goto goto_end;
	}

	cmd = (u8 *)kmalloc(count * sizeof(u8), GFP_KERNEL);
	if (cmd == NULL) {
		pr_info("%s Error allocating memory\n", tag);
		res = ERROR_ALLOC;
		goto goto_end;
	}
	func_to_test = (u32 *)kmalloc(((count + 1) / 3) * sizeof(u32),
			GFP_KERNEL);
	if (func_to_test == NULL) {
		pr_info("%s Error allocating memory\n", tag);
		res = ERROR_ALLOC;
		goto goto_end;
	}
	if (access_ok(buf, count) < OK ||
		copy_from_user(pbuf, buf, count) != 0) {
		res = ERROR_ALLOC;
		goto goto_end;
	}
	p = pbuf;
	if (((count + 1) / 3) >= 1) {
		if (sscanf(p, "%02X ", &func_to_test[0]) == 1) {
			p += 3;
			cmd[0] = (u8)func_to_test[0];
			number_param = 1;
		}
	} else {
		res = ERROR_OP_NOT_ALLOW;
		goto goto_end;
	}

	for (; number_param < (count + 1) / 3; number_param++) {
		if (sscanf(p, "%02X ",
			&func_to_test[number_param]) == 1) {
			p += 3;
			cmd[number_param] =
				(u8)func_to_test[number_param];			
		}
	}

	if (number_param >= 1) {
		switch (func_to_test[0]) {
		case CMD_DRIVER_VERSION:
			to_read = sizeof(u32);
			read_buf = (u8 *)kmalloc(to_read *
				sizeof(u8), GFP_KERNEL);
			if (read_buf == NULL) {
				pr_info("%s Error allocating memory\n", tag);
				to_read = 0;
				res = ERROR_ALLOC;
				break;
			}

			read_buf[0] = (u8)(FTS_TS_DRV_VER >> 24);
			read_buf[1] = (u8)(FTS_TS_DRV_VER >> 16);
			read_buf[2] = (u8)(FTS_TS_DRV_VER >> 8);
			read_buf[3] = (u8)(FTS_TS_DRV_VER >> 0);

			pr_info("%s %s: Version = 0x%02X%02X%02X%02X\n", tag, __func__, read_buf[0], read_buf[1], read_buf[2], read_buf[3]);
				
			size += (to_read * sizeof(u8));
			break;

			case CMD_WRITE:
				if (number_param >= 1)
					res = proc_ts->stm_ts_write(proc_ts, &cmd[1], number_param - 1, NULL, 0);
				else
				{
					pr_info("%s Wrong number of parameters!\n", tag);
					res = ERROR_OP_NOT_ALLOW;
				}
				break;

			case CMD_WRITEREAD:
				if (number_param >= 1)
				{
					dummy = cmd[number_param - 1];
					to_read = (u16)(((cmd[number_param - 3 + 0] & 0x00FF) << 8) + (cmd[number_param - 3 + 1] & 0x00FF));
					
					read_buf = (u8 *)kmalloc((to_read + dummy + 1) *
												 sizeof(u8),
											 GFP_KERNEL);
					if (read_buf == NULL)
					{
						pr_info("%s Error allocating memory\n", tag);
						to_read = 0;
						res = ERROR_ALLOC;
						break;
					}					
					res = proc_ts->stm_ts_read(proc_ts, &cmd[1], number_param - 4,
											   &read_buf[1], to_read + dummy);
					if (res >= OK)
						size += (to_read * sizeof(u8));
				}
				else
				{
					pr_info("%s wrong number of parameters\n", tag);
					res = ERROR_OP_NOT_ALLOW;
				}
				break;
			default:
				pr_info("%s COMMAND ID NOT VALID!!!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
				break;
			}
	}
	else
	{
		pr_info("%s %s NO COMMAND SPECIFIED!!! do: 'echo [cmd_code] [args] > stm_fts_cmd' before looking for result!\n", tag, __func__);
		res = ERROR_OP_NOT_ALLOW;
	}

 goto_end:

	size += (sizeof(u32)); /*for error code of 4 bytes*/
	size *= 2; /*for each chars*/
	size += 5; /*for "{", " ", " ", "}", "\n" */
		
	snprintf(&test_print_buff[index], 3, "{ ");
	index += 2;
	snprintf(&test_print_buff[index], 9, "%08X", res);
	index += 8;
	if (res >= OK) {
		switch (func_to_test[0]) {
		case CMD_DRIVER_VERSION:
		case CMD_WRITEREAD:
			if (dummy == 1)
				i = 1;
			else
				i = 0;
			for (; i < (to_read + dummy); i++) {
				snprintf(&test_print_buff[index],
				3, "%02X", read_buf[i]);
				index += 2;
			}
		break;
		}
	}
	snprintf(&test_print_buff[index], 4, " }\n");
	limit = size;
	printed = 0;

	number_param = 0;
	if (read_buf != NULL) {
		kfree(read_buf);
		read_buf = NULL;
	}

	if (pbuf != NULL) {
		kfree(pbuf);
		pbuf = NULL;
	}
	if (cmd != NULL) {
		kfree(cmd);
		cmd = NULL;
	}

	if (func_to_test != NULL) {
		kfree(func_to_test);
		func_to_test = NULL;
	}
	return count;
}

/** @}*/

/**
  * file_operations struct which define the functions for the canonical
  *operation on a device file node (open. read, write etc.)
  */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops fts_driver_test_ops = {
	.proc_open	= fts_open,
	.proc_read	= seq_read,
	.proc_write	= fts_driver_test_write,
	//.unlocked_ioctl = driver_ioctl,
	.proc_lseek	= seq_lseek,
	.proc_release	= seq_release
};
#else
static const struct file_operations fts_driver_test_ops = {
	.open		= fts_open,
	.read		= seq_read,
	.write		= fts_driver_test_write,
	//.unlocked_ioctl = driver_ioctl,
	.llseek		= seq_lseek,
	.release	= seq_release
};
#endif

/**
  * This function is called in the probe to initialize and create the directory
  * proc/fts and the driver test file node DRIVER_TEST_FILE_NODE into the /proc
  * file system
  * @return OK if success or an error code which specify the type of error
  */
int stm_ts_tool_proc_init(struct stm_ts_data *ts)
{
	struct proc_dir_entry *entry;
	int retval = 0;

	proc_ts = (struct stm_ts_data *)ts;

	fts_dir = proc_mkdir_data("fts", 0777, NULL, NULL);
	if (fts_dir == NULL) {	/* directory creation failed */
		retval = -ENOMEM;
		goto out;
	}

	entry = proc_create(DRIVER_TEST_FILE_NODE, 0777, fts_dir, &fts_driver_test_ops);

	if (entry)
		pr_info("%s %s: proc entry CREATED!\n", tag, __func__);
	else {
		pr_info("%s %s: error creating proc entry!\n", tag, __func__);
		retval = -ENOMEM;
		goto badfile;
	}
	return OK;
badfile:
	remove_proc_entry("fts", NULL);
out:
	return retval;
}

/**
  * Delete and Clean from the file system, all the references to the driver test
  * file node
  * @return OK
  */
int stm_ts_tool_proc_remove(void)
{
	remove_proc_entry(DRIVER_TEST_FILE_NODE, fts_dir);
	remove_proc_entry("fts", NULL);
	return OK;
}
