/*
 * sprd trusted key storage driver
 *
 * Copyright (C)
 *
 * This driver is for ali trusted key storage
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <asm/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ion.h>
#include <linux/ioport.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <soc/sprd/sci.h>
#include <uapi/linux/sprd_tks.h>

#ifdef CONFIG_OF
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif

#define TSP_TKS_SHM_REQ_ADDR	0x2005
#define TSP_TKS			0x2010

/*from arm trusted firmware */
#define TSP_STD_FID(fid)	((fid) | 0x72000000 | (0 << 31))
#define TSP_FAST_FID(fid)	((fid) | 0x72000000 | (1 << 31))

/*c6e7c860-9299-455c-b5eb-68686d2c739c*/
#define SEC_SHAREMEM_MAX_SIZE	4096

#define TKS_KEY_LEN(p)	(sizeof(enum key_type)+sizeof(int)+(p)->keylen)
#define PACK_DATA_LEN(p)	(sizeof(int)+(p)->len)

#define KEYLEN_OFF	(offsetof(struct tks_key_cal, keylen))
#define DATA_OFF	(offsetof(struct tks_key_cal, keydata))
/*#define GEN_KEYPARI_LEN(p) (TKS_KEY_LEN(&((p)->rsaprv)) + \
		TKS_KEY_LEN((&(p)->out_rsapub)))
#define DEC_SIMPLE_LEN(p) (TKS_KEY_LEN(&((p)->encryptkey)) + \
		PACK_DATA_LEN(&((p)->data)))*/

#define SHM_SIZE	(4096ul)
#define ERRNO_BASE	(1000)
#define ARM7_CFG_BUS_OFFSET	(0x0124)
#define SLEEP_STATUS_OFFSET	(0x00d4)
#define SLEEP_STATUS_MASK	(0xf << 20)
#define SLEEP_STATUS_VAL	(0x6 << 20)

static unsigned long *shm_vaddr;
static dma_addr_t phy_addr;
static ulong arm7_cfg_bus;
static ulong sleep_status;
static ulong reg_val = 0;

__packed struct shm_mem_view {
	u32 cmd;
	u8  cmd_data[SEC_SHAREMEM_MAX_SIZE-sizeof(u32)];
};


static inline void readbyte(unsigned long addr, void *buffer, int count)
{
	u8 *buf = buffer;
	while (count--)
		*buf++ = __raw_readb((void __iomem *)addr++);
}

static inline void writebyte(unsigned long addr, const void *buffer, int count)
{
	const u8 *buf = buffer;
	while (count--)
		__raw_writeb(*buf++, (void __iomem *)addr++);
}

static noinline int __invoke_tks_fn_smc(u64 function_id, u64 arg0, u64 arg1,
								u64 arg2)
{
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"smc    #0\n"
			: "+r" (function_id)
			: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}


static int (*invoke_tks_fn)(u64, u64, u64, u64);

static int wake_secure_ap(void)
{
	ulong state = 0;
	int loop = 1000;

	reg_val = sci_glb_read(arm7_cfg_bus, 0x1);
	sci_glb_clr(arm7_cfg_bus, BIT(0));
	while (loop--) {
		state = sci_glb_read(sleep_status, SLEEP_STATUS_MASK);
		if (state == SLEEP_STATUS_VAL)
			break;
		cpu_relax();
		msleep(1);
	}
	pr_info("%s secure ap awake!", __func__);

	return 0;
}

static int resume_bus_status(void)
{
	return sci_glb_write(arm7_cfg_bus, reg_val, 0x1);
}

static int invoke_tks_fn_awake(u64 func_id, u64 arg0, u64 arg1, u64 arg2)
{
	wake_secure_ap();
	invoke_tks_fn(func_id, arg0, arg1, arg2);
	resume_bus_status();

	return 0;
}

/* filesystem operations */
static int sprd_tks_open(struct inode *inode, struct file *file)
{

	return nonseekable_open(inode, file);
}

static int sprd_tks_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long sprd_tks_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct shm_mem_view *buf;
	unsigned char *databuf;
	unsigned int cnt;
	unsigned int keylen;
	unsigned char * __user userdata;
	unsigned char op;
	unsigned char *bakloc1;
	int ret = -ENOTTY;
	void __user *argp = (void *)arg;

	buf = (struct shm_mem_view *)shm_vaddr;

	switch (cmd) {
	case SEC_IOC_GEN_KEYPAIR:
		buf->cmd = _IOC_NR(SEC_IOC_GEN_KEYPAIR);
		invoke_tks_fn_awake((TSP_FAST_FID(TSP_TKS)),
				SEC_IOC_GEN_KEYPAIR, 0, 0);
		invoke_tks_fn_awake((TSP_FAST_FID(TSP_TKS)),
				SEC_IOC_GEN_KEYPAIR, 0, 0);
		if (buf->cmd)
			return -(ERRNO_BASE + buf->cmd);
		databuf = buf->cmd_data;
		readbyte((unsigned long)(databuf + KEYLEN_OFF),
				&keylen, sizeof(keylen));	/*get rsaprv*/
		pr_info("%s:%d:databuf=0x%p,argp=0x%p, len1= 0x%x \n",
				__func__, __LINE__, databuf, argp, keylen);
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		if (!access_ok(VERIFY_WRITE, argp, DATA_OFF + keylen))
			return -EFAULT;
		/*can not use copy_to_user ,bc .
		  the 8 alignment fault.do it in byte copy.*/
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = put_user(*((char *)databuf + cnt),
					(char *)argp + cnt);
			if (ret)
				return -EIO;
		}
		/*get out_rsapub*/
		databuf +=  (DATA_OFF + keylen);
		argp += offsetof(struct request_gen_keypair, out_rsapub);
		/*bc. this is maybe not a alignment addr, so use byteget
		  method to get keylen.*/
		readbyte((unsigned long)(databuf + KEYLEN_OFF),
				&keylen, sizeof(keylen));
		pr_info("%s:%d:databuf=0x%p,argp=0x%p, len1= 0x%x \n",
				__func__, __LINE__, databuf, argp, keylen);
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		if (!access_ok(VERIFY_WRITE, argp, DATA_OFF + keylen))
			return -EFAULT;
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = put_user(*((char *)databuf + cnt),
					(char *)argp + cnt);
			if (ret)
				return -EIO;
		}
		break;
	case SEC_IOC_DEC_SIMPLE:
		buf->cmd = _IOC_NR(SEC_IOC_DEC_SIMPLE);
		databuf = buf->cmd_data;
		/*user view*/
		ret = get_user(keylen,
		&(((struct request_dec_simple *)argp)->encryptkey.keylen));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = get_user(databuf[cnt], (char *)argp + cnt);
			if (ret)
				return -EIO;
		}
		/*updata databuf pto pack_data*/
		databuf += cnt;
		/*get keylen  form user space.*/
		ret = get_user(keylen,
		&(((struct request_dec_simple *)argp)->data.len));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s %d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		writebyte((unsigned long)databuf, &keylen, sizeof(keylen));
		databuf += sizeof(keylen);
		/*get data pto usermemmory form userspace.*/
		ret = get_user(userdata,
		&(((struct request_dec_simple *)argp)->data.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = get_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		invoke_tks_fn_awake((TSP_FAST_FID(TSP_TKS)),
					SEC_IOC_DEC_SIMPLE, 0, 0);
		if (buf->cmd)
			return -(ERRNO_BASE + buf->cmd);
		/*rollback tothe shm packdata locate.*/
		databuf -= sizeof(keylen);
		userdata = (unsigned char * __user)
		(&(((struct request_dec_simple *)argp)->data.len));
		readbyte((unsigned long)databuf, &keylen, sizeof(keylen));
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < sizeof(keylen); cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		databuf += sizeof(keylen);
		/*get packdata.data ,the user space memmory heap*/
		/*userdata = ((struct request_dec_simple*)argp)->data.data*/
		ret = get_user(userdata,
		&(((struct request_dec_simple *)argp)->data.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		break;
	case SEC_IOC_DEC_PACK_USERPASS:
		buf->cmd = _IOC_NR(SEC_IOC_DEC_PACK_USERPASS);
		databuf = buf->cmd_data;
		/*write shm the endryptkey from user space.
		get keylen from user*/
		ret = get_user(keylen,
		&(
		((struct request_dec_pack_userpass *)argp)->encryptkey.keylen
		));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = get_user(databuf[cnt], (char *)argp + cnt);
			if (ret)
				return -EIO;
		}
		/*write shm the userpasskey from user space.*/
		databuf +=  cnt;
		/*updata userdata pos.*/
		userdata = (unsigned char * __user)
		(&(((struct request_dec_pack_userpass *)argp)->userpass));
		/*get keylen from user*/
		ret = get_user(keylen,
		&(((struct request_dec_pack_userpass *)argp)->userpass.keylen));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s %d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = get_user(databuf[cnt], (char *)userdata + cnt);
			if (ret)
				return -EIO;
		}
		invoke_tks_fn_awake((TSP_FAST_FID(TSP_TKS)),
				SEC_IOC_DEC_PACK_USERPASS, 0, 0);
		if (buf->cmd)
			return -(ERRNO_BASE + buf->cmd);
		/*databuf is remained; userdata is remianed*/
		readbyte((unsigned long)(databuf + KEYLEN_OFF),
						&keylen, sizeof(keylen));
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = put_user(*((char *)databuf + cnt),
						(char *)userdata + cnt);
			if (ret)
				return -EIO;
		}
		break;
	case SEC_IOC_CRYPTO:
		buf->cmd = _IOC_NR(SEC_IOC_CRYPTO);
		databuf = buf->cmd_data;
		ret = get_user(op, (char *)argp);	/*write op*/
		writebyte((unsigned long)databuf, &op, sizeof(op));
		/*write shm the endryptkey from user space.*/
		/*keylen = ((struct request_crypto*)argp)->encryptkey.keylen;*/
		ret = get_user(keylen,
		&(((struct request_crypto *)argp)->encryptkey.keylen));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		/*updata databuf,userdata.*/
		databuf += sizeof(op);
		userdata = (char *)argp
				+ offsetof(struct request_crypto, encryptkey);
		/*write encryptkey to shm.*/
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = get_user(databuf[cnt], (char *)userdata + cnt);
			if (ret)
				return -EIO;
		}
		/*write shm the userpasskey from user space.*/
		databuf +=  cnt; /*updata the databuf pos.*/
		/*updata userdata pos.*/
		userdata = (char *)argp +
				offsetof(struct request_crypto, userpass);
		ret = get_user(keylen,
		&(((struct request_crypto *)argp)->userpass.keylen));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = get_user(databuf[cnt], (char *)userdata + cnt);
			if (ret)
				return -EIO;
		}
		/*packdata write to shm*/
		databuf +=  cnt; /*updata the databuf pos.*/
		bakloc1 = databuf;/*backup the pointer locate of the packdata*/
		userdata = (char *)argp +
				offsetof(struct request_crypto, data);
		ret = get_user(keylen,
		&(((struct request_crypto *)argp)->data.len));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		writebyte((unsigned long)databuf, &keylen, sizeof(keylen));
		databuf += sizeof(keylen);
		ret = get_user(userdata,
		&(((struct request_crypto *)argp)->data.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = get_user(databuf[cnt], (char *)userdata + cnt);
			if (ret)
				return -EIO;
		}
		/*write signature*/
		if (op == 1) { /*0=SIGN,1=VERIFY,2=ENC,3=DEC; [in]*/
			/*write the signature to shm*/
			databuf +=  cnt; /*updata the databuf pos.*/
			/*updata userdata pos.*/
			userdata = (char *)argp +
				offsetof(struct request_crypto, signature);
			ret = get_user(keylen,
			&(((struct request_crypto *)argp)->signature.len));
			if (ret)
				return -EIO;
			if (unlikely(keylen >= SHM_SIZE)) {
				pr_err("%s:%d:tks keylen=0x%x out of range.\n",
						__func__, __LINE__, keylen);
				return -EIO;
			}
			writebyte((unsigned long)databuf, &keylen,
							sizeof(keylen));
			/*updata databuf*/
			databuf += sizeof(keylen);
			ret = get_user(userdata,
			&(((struct request_crypto *)argp)->signature.data));
			if (ret)
				return -EIO;
			for (cnt = 0; cnt < keylen; cnt++) {
				ret = get_user(databuf[cnt],
						(char *)userdata + cnt);
				if (ret)
					return -EIO;
			}
		}
		invoke_tks_fn_awake((TSP_FAST_FID(TSP_TKS)),
							SEC_IOC_CRYPTO, 0, 0);
		if (buf->cmd)
			return -(ERRNO_BASE + buf->cmd);
		/* copy pack_data form shm to user space.*/
		databuf = bakloc1; /*the shm packdata locate.*/
		/*updata userdata pos.*/
		userdata = (char *)argp + offsetof(struct request_crypto, data);
		readbyte((unsigned long)databuf, &keylen, sizeof(keylen));
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < sizeof(keylen); cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		databuf += sizeof(keylen);
		ret = get_user(userdata,
			&(((struct request_crypto *)argp)->data.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		break;
	case SEC_IOC_GEN_DHKEY:
		buf->cmd = _IOC_NR(SEC_IOC_GEN_DHKEY);
		databuf = buf->cmd_data;
		/*write user space encryptkey to shm*/
		ret = get_user(keylen,
		&(((struct request_gen_dhkey *)argp)->encryptkey.keylen));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < (keylen + DATA_OFF); cnt++) {
			ret = get_user(databuf[cnt], (char *)argp + cnt);
			if (ret)
				return -EIO;
		}
		/*write param*/
		databuf += cnt;
		userdata = (char *)argp +
				offsetof(struct request_gen_dhkey, param) +
				offsetof(struct dh_param, a);/*big_int a*/
		ret = get_user(keylen,
			&(((struct request_gen_dhkey *)argp)->param.a.len));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		/*write bit_int a keylen*/
		writebyte((unsigned long)databuf, &keylen, sizeof(keylen));
		databuf += sizeof(keylen);
		ret = get_user(userdata,
			&(((struct request_gen_dhkey *)argp)->param.a.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = get_user(databuf[cnt], (char *)userdata + cnt);
			if (ret)
				return -EIO;
		}
		/*write big_int g*/
		databuf += cnt;
		userdata = (char *)argp +
				offsetof(struct request_gen_dhkey, param) +
				offsetof(struct dh_param, g); /*big_int g*/
		ret = get_user(keylen,
			&(((struct request_gen_dhkey *)argp)->param.g.len));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		/*write bit_int a keylen*/
		writebyte((unsigned long)databuf, &keylen, sizeof(keylen));
		databuf += sizeof(keylen);
		ret = get_user(userdata,
			&(((struct request_gen_dhkey *)argp)->param.g.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = get_user(databuf[cnt], (char *)userdata + cnt);
			if (ret)
				return -EIO;
		}
		/*write big_int p*/
		databuf += cnt;
		userdata = (char *)argp +
				offsetof(struct request_gen_dhkey, param) +
				offsetof(struct dh_param, p); /*big_int g*/
		ret = get_user(keylen,
			&(((struct request_gen_dhkey *)argp)->param.p.len));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		/*write bit_int a keylen*/
		writebyte((unsigned long)databuf, &keylen, sizeof(keylen));
		databuf += sizeof(keylen);
		ret = get_user(userdata,
			&(((struct request_gen_dhkey *)argp)->param.p.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = get_user(databuf[cnt], (char *)userdata + cnt);
			if (ret)
				return -EIO;
		}

		invoke_tks_fn_awake((TSP_FAST_FID(TSP_TKS)),
						SEC_IOC_GEN_DHKEY, 0, 0);
		if (buf->cmd)
			return -(ERRNO_BASE + buf->cmd);
		databuf += cnt;
		userdata = (char *)argp +
				offsetof(struct request_gen_dhkey, b);
		readbyte((unsigned long)databuf, &keylen, sizeof(keylen));
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < sizeof(keylen); cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		databuf += sizeof(keylen);
		ret = get_user(userdata,
				&(((struct request_gen_dhkey *)argp)->b.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		/*write big_int k*/
		databuf += cnt;
		userdata = (char *)argp + offsetof(struct request_gen_dhkey, k);
		readbyte((unsigned long)databuf, &keylen, sizeof(keylen));
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < sizeof(keylen); cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		databuf += sizeof(keylen);
		ret = get_user(userdata,
				&(((struct request_gen_dhkey *)argp)->k.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		break;
	case SEC_IOC_HMAC:
		buf->cmd = _IOC_NR(SEC_IOC_HMAC);
		databuf = buf->cmd_data;
		ret = get_user(keylen,
				&(((struct request_hmac *)argp)->message.len));
		if (ret)
			return -EIO;
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		writebyte((unsigned long)databuf, &keylen, sizeof(keylen));
		databuf += sizeof(keylen);
		ret = get_user(userdata,
				&(((struct request_hmac *)argp)->message.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = get_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		invoke_tks_fn_awake((TSP_FAST_FID(TSP_TKS)),
							SEC_IOC_HMAC, 0, 0);
		if (buf->cmd)
			return -(ERRNO_BASE + buf->cmd);
		databuf = buf->cmd_data;
		userdata = (char *)argp;
		readbyte((unsigned long)databuf, &keylen, sizeof(keylen));
		if (unlikely(keylen >= SHM_SIZE)) {
			pr_err("%s:%d:tks keylen=0x%x out of range.\n",
					__func__, __LINE__, keylen);
			return -EIO;
		}
		for (cnt = 0; cnt < sizeof(keylen); cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		databuf += sizeof(keylen);
		ret = get_user(userdata,
				&(((struct request_hmac *)argp)->message.data));
		if (ret)
			return -EIO;
		for (cnt = 0; cnt < keylen; cnt++) {
			ret = put_user(databuf[cnt], userdata + cnt);
			if (ret)
				return -EIO;
		}
		break;
	case SEC_IDC_TEST_USED:
		ret = copy_from_user(buf, argp, sizeof(struct shm_mem_view));
		if (ret)
			return -EIO;
		break;
	default:
		return -ENOTTY;
	}
	return ret;
}

static ssize_t sprd_tks_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	return count;
}

static void get_cfg_addr(void)
{
	struct device_node *np;
	struct resource res;

	np = of_find_compatible_node(NULL, NULL, "sprd,aon_apb");
	if(!np)
		pr_err("sprd_tks_init: get aon_apb node failed!\n");
	of_address_to_resource(np, 0, &res);
	arm7_cfg_bus = (unsigned long)ioremap_nocache(res.start
				+ ARM7_CFG_BUS_OFFSET, 0x4);
	np = of_find_compatible_node(NULL, NULL, "sprd,pmu_apb");
	if(!np)
		pr_err("sprd_tks_init: get pmu_apb node failed!\n");
	of_address_to_resource(np, 0, &res);
	sleep_status = (unsigned long)ioremap_nocache(res.start
				+ SLEEP_STATUS_OFFSET, 0x4);
}

static const struct file_operations sprd_tks_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.unlocked_ioctl	= sprd_tks_ioctl,
	.open		= sprd_tks_open,
	.write		= sprd_tks_write,
	.release	= sprd_tks_release,
};

static struct miscdevice sprd_tks_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "sprdtks",
	.fops	= &sprd_tks_fops,
};

static int __init sprd_tks_init(void)
{
	int ret;

	get_cfg_addr();
	ret = misc_register(&sprd_tks_miscdev);
	if (ret)
		dev_err(sprd_tks_miscdev.this_device, "sprd tks reg error\n");
	invoke_tks_fn = __invoke_tks_fn_smc;
	shm_vaddr = dma_alloc_coherent(sprd_tks_miscdev.this_device, SHM_SIZE,
							&phy_addr, GFP_KERNEL);
	if (!shm_vaddr) {
		dev_err(sprd_tks_miscdev.this_device, "shm_vaddr failure\n");
		return -ENOMEM;
	}
	memset(shm_vaddr, 0, SHM_SIZE);
	/* the first argument of invoke_tks_fn_awake is the case that used to
	   initialize the share memory.*/
	invoke_tks_fn_awake((TSP_FAST_FID(TSP_TKS_SHM_REQ_ADDR)),
								phy_addr, 0, 0);
	pr_info("%s:shm_vaddr:%p, phy_addr:0x%lx\n",
					__func__, shm_vaddr, (ulong)phy_addr);

	return ret;
}

static void __exit sprd_tks_exit(void)
{
	dev_info(sprd_tks_miscdev.this_device, "sprd tks driver eixt!\n");
	dma_free_coherent(sprd_tks_miscdev.this_device, SHM_SIZE,
				shm_vaddr, phy_addr);
	misc_deregister(&sprd_tks_miscdev);
}

module_init(sprd_tks_init);
module_exit(sprd_tks_exit);


MODULE_AUTHOR("spreadtrum");
MODULE_DESCRIPTION("sprd tks driver");
MODULE_LICENSE("GPL");
