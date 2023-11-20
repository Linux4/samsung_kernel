#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <video/isp_drv_kernel.h>
#include "compat_isp_drv_kernel.h"

struct compat_isp_irq_param {
	compat_uint_t  isp_irq_val;
	compat_uint_t  dcam_irq_val;
	compat_uint_t  irq_val;
	compat_int_t   ret_val;
};

struct compat_isp_reg_bits {
	compat_ulong_t reg_addr;
	compat_ulong_t reg_value;
} ;

struct compat_isp_reg_param {
	compat_ulong_t reg_param;
	uint32_t       counts;
} ;


#define COMPAT_ISP_IO_READ       _IOR(ISP_IO_MAGIC, 1, struct compat_isp_reg_param)
#define COMPAT_ISP_IO_WRITE      _IOW(ISP_IO_MAGIC, 2, struct compat_isp_reg_param)
#define COMPAT_ISP_IO_LNC_PARAM  _IOW(ISP_IO_MAGIC, 8, struct compat_isp_reg_param)
#define COMPAT_ISP_IO_LNC        _IOW(ISP_IO_MAGIC, 9, struct compat_isp_reg_param)
#define COMPAT_ISP_IO_ALLOC      _IOW(ISP_IO_MAGIC, 10, struct compat_isp_reg_param)


static int compat_get_rw_param(
			struct compat_isp_reg_param __user *data32,
			struct isp_reg_param __user *data)
{
	int                               err = 0;
	compat_ulong_t                    addr;
	uint32_t                          cnt;

	err = get_user(addr, &data32->reg_param);
	err |= put_user(addr, &data->reg_param);
	err |= get_user(cnt, &data32->counts);
	err |= put_user(cnt, &data->counts);

	//printk("compat_get_rw_param, addr: 0x%x data32->reg_param: 0x%x data->reg_param: 0x%lx cnt: %d\n",
	//	addr, data32->reg_param, data->reg_param, cnt);

	return err;
}

static int compat_get_rw_param_detail(struct isp_reg_param __user *data)
{
	struct compat_isp_reg_bits __user *data32;
	struct isp_reg_bits __user	*reg_data;
	int            err = 0;
	int            i = 0;
	int            cnt = data->counts;
	compat_ulong_t addr;
	compat_ulong_t val;
	struct isp_reg_bits __user *data_tmp;

	data32 = compat_ptr(data->reg_param);
//	printk("compat_get_rw_param_detail param: %p cnt %d\n", data32, cnt);
	reg_data = (struct isp_reg_bits __user*)((unsigned long)data + sizeof(struct isp_reg_param));
	data->reg_param = reg_data;
	if (reg_data == NULL) {
		printk("compat_isp_k failed to compat_alloc_user_space \n");
		return -EFAULT;
	}
//	printk("compat_get_rw_param_detail param: data %p \n", reg_data);
	data_tmp = reg_data;
	for (i=0 ; i< cnt; i++) {
		err = get_user(addr, &data32->reg_addr);
		err |= put_user(addr, &data_tmp->reg_addr);
		err |= get_user(val, &data32->reg_value);
		err |= put_user(val, &data_tmp->reg_value);
		if (err) {
			printk("compat_isp_k, read_param_detail failed cnt %d ret %d\n", cnt, err);
			break;
		}
		data32++;
		data_tmp++;
	}
	return err;
}

static int compat_put_read_param(
			struct compat_isp_reg_param __user *data32,
			struct isp_reg_param __user *data)

{
	int                               err = 0;
	compat_ulong_t                    val;
	uint32_t                          cnt;
	int                               i = 0;
	struct compat_isp_reg_bits __user *reg_data32;
	struct isp_reg_bits __user        *reg_data;

	cnt = data->counts;
	reg_data32 = (struct compat_isp_reg_bits __user *)data32->reg_param;
	reg_data = (struct isp_reg_bits __user *)data->reg_param;
	for (i=0 ; i<cnt; i++) {
		err = get_user(val, &reg_data->reg_value);
		err |= put_user(val, &reg_data32->reg_value);
		if (err) {
			printk("compat_isp_k, compat_put_read_param ret %d\n", err);
			break;
		}
		reg_data32++;
		reg_data++;
	}

	return err;
}

static int compat_get_alloc_param(
			struct compat_isp_reg_param __user *data32,
			struct isp_reg_param __user *data)
{
	int                               err = 0;
	compat_ulong_t                    addr;
	uint32_t                          cnt;
	struct compat_isp_reg_bits __user *reg_data32;
	struct isp_reg_bits __user        *reg_data;

	err = get_user(addr, &data32->reg_param);
	err |= put_user(addr, &data->reg_param);
	err |= get_user(cnt, &data32->counts);
	err |= put_user(cnt, &data->counts);

	return err;
}

static int compat_put_alloc(
			struct compat_isp_reg_param __user *data32,
			struct isp_reg_param __user *data)

{
	int                               err = 0;
	compat_ulong_t                    addr;
	uint32_t                          cnt;
	struct compat_isp_reg_bits __user *reg_data32;
	struct isp_reg_bits __user        *reg_data;

	err = get_user(addr, &data->reg_param);
	err |= put_user(addr, &data32->reg_param);
	err |= get_user(cnt, &data->counts);
	err |= put_user(cnt, &data32->counts);

	return err;
}




long compat_isp_kernel_ioctl( struct file *fl, unsigned int cmd, unsigned long param)
{
	long        ret;
	int         err = 0;
	void __user *up = compat_ptr(param);

	if(ISP_IO_IRQ == cmd) {
		ret = fl->f_op->unlocked_ioctl(fl, cmd, (unsigned long)up);
	} else {
		switch (cmd) {
		case COMPAT_ISP_IO_READ:
		{
			struct compat_isp_reg_param __user *data32;
			struct isp_reg_param __user *data;
			uint32_t cnt;


			data32 = compat_ptr(param);
			err = get_user(cnt, &data32->counts);

//			printk("COMPAT_ISP_IO_WRITE param: %p \n", data32);
			data = compat_alloc_user_space(sizeof(struct isp_reg_param)+cnt*sizeof(struct isp_reg_bits));
			if (data == NULL) {
				printk("compat_isp_k failed to compat_alloc_user_space \n");
				return -EFAULT;
			}
//			printk("@@1 %p", data);
			err = compat_get_rw_param(data32, data);
			if (err) {
				return err;
			}
			err = compat_get_rw_param_detail(data);
			if (err) {
				printk("compat_isp_k error %d\n", err);
				return err;
			}
			err = fl->f_op->unlocked_ioctl(fl, ISP_IO_READ, (unsigned long)data);
			err = compat_put_read_param(data32, data);
			ret = ret ? ret : err;
			break;
		}
		case COMPAT_ISP_IO_WRITE:
		{
			struct compat_isp_reg_param __user *data32;
			struct isp_reg_param __user *data;
			uint32_t cnt;


			data32 = compat_ptr(param);
			err = get_user(cnt, &data32->counts);

//			printk("COMPAT_ISP_IO_WRITE param: %p \n", data32);
			data = compat_alloc_user_space(sizeof(struct isp_reg_param)+cnt*sizeof(struct isp_reg_bits));
			if (data == NULL) {
				printk("compat_isp_k failed to compat_alloc_user_space \n");
				return -EFAULT;
			}
//			printk("@@1 %p", data);
			err = compat_get_rw_param(data32, data);
			if (err) {
				return err;
			}
			err = compat_get_rw_param_detail(data);
			if (err) {
				printk("compat_isp_k error %d\n", err);
				return err;
			}
			err = fl->f_op->unlocked_ioctl(fl, ISP_IO_WRITE, (unsigned long)data);
			break;
		}
		case COMPAT_ISP_IO_LNC_PARAM:
		{
			struct compat_isp_reg_param __user *data32;
			struct isp_reg_param __user *data;
			char __user  *lnc_data;


			data32 = compat_ptr(param);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL) {
				printk("compat_isp_k failed to compat_alloc_user_space \n");
				return -EFAULT;
			}
			err = compat_get_rw_param(data32, data);
			if (err) {
				printk("compat_isp_k COMPAT_ISP_IO_LNC_PARAM error\n");
				return err;
			}
			/*lnc_data = compat_alloc_user_space(sizeof(*lnc_data)*data->counts);
			data->reg_param = (unsigned long)lnc_data;
			ret = copy_from_user((void*)data->reg_param, (void*)data32->reg_param, data->counts);
			if (ret) {
				return ret;
			}*/
//			printk("test 123\n");
			ret = fl->f_op->unlocked_ioctl(fl, ISP_IO_LNC_PARAM, (unsigned long)data);
//			printk("test 123 end\n");
			break;
		}
		case COMPAT_ISP_IO_LNC:
		{
			struct compat_isp_reg_param __user *data32;
			struct isp_reg_param __user *data;
			uint32_t cnt;


			data32 = compat_ptr(param);
			err = get_user(cnt, &data32->counts);

//			printk("COMPAT_ISP_IO_WRITE param: %p \n", data32);
			data = compat_alloc_user_space(sizeof(struct isp_reg_param)+cnt*sizeof(struct isp_reg_bits));
			if (data == NULL) {
				printk("compat_isp_k failed to compat_alloc_user_space \n");
				return -EFAULT;
			}
//			printk("@@1 %p", data);
			err = compat_get_rw_param(data32, data);
			if (err) {
				return err;
			}
			err = compat_get_rw_param_detail(data);
			if (err) {
				printk("compat_isp_k error %d\n", err);
				return err;
			}
	//		printk("test ISP_IO_LNC\n");
			err = fl->f_op->unlocked_ioctl(fl, ISP_IO_LNC, (unsigned long)data);
	//		printk("test ISP_IO_LNC end\n");
			break;
		}
		case COMPAT_ISP_IO_ALLOC:
		{
			struct compat_isp_reg_param __user *data32;
			struct isp_reg_param __user *data;

			data32 = compat_ptr(param);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL) {
				printk("compat_isp_k failed to compat_alloc_user_space \n");
				return -EFAULT;
			}
			err = compat_get_alloc_param(data32, data);
			if (err) {
				return err;
			}
			ret = fl->f_op->unlocked_ioctl(fl, ISP_IO_ALLOC, (unsigned long)data);
			if (ret) {
				printk("compat_isp_k,failed to ISP_IO_ALLOC %ld", ret);
				goto exit;
			}
			err = compat_put_alloc(data32, data);
			ret = err;
			break;
		}

		case ISP_IO_RST:
		case ISP_IO_SETCLK:
		case ISP_IO_STOP:
		case ISP_IO_INT:
		case ISP_IO_DCAM_INT:
		{
			ret = fl->f_op->unlocked_ioctl(fl, cmd, (unsigned long)up);
			break;
		}
		default:
			printk("compat_isp_k, don't support cmd 0x%x", cmd);
			break;
		}
	}
exit:
	return ret;
}
