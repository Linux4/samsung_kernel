#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <mt-plat/mtk_devinfo.h>

#define CTRL_SBC_EN (0x1U << 1)
#define CTRL_DAA_EN (0x1U << 2)
#define CTRL_SW_JTAG_EN (0x1U << 6)
#define LOCK_PUBK_EN (0x1U << 0)
#define SELF_BLOW_DISABLE (0x1U << 0)

#define FUSE_OK 0x00000000
#define FUSE_SBC_FAIL 0x0000000F
#define FUSE_DAA_FAIL 0x000000F0
#define FUSE_SW_JTAG_FAIL 0x00000F00
#define FUSE_LOCK_PUBK_FAIL 0x0000F000
#define FUSE_SELF_BLOW_DISABLE_FAIL 0x000F0000

//Bug 618260,guosanwei1.wt,modify,20210109,modify secboot fuse check statues
static int secboot_proc_show(struct seq_file *m, void *v)
{
        u32 secure_ctrl_value;
        u32 secure_lock_value;
        u32 c_ctrlm_value;
        u32 result_value=0x0;

        secure_ctrl_value = get_devinfo_with_index(27);
        secure_lock_value = get_devinfo_with_index(22);
        c_ctrlm_value = get_devinfo_with_index(88);

        result_value = (secure_ctrl_value & CTRL_SBC_EN)? FUSE_OK : FUSE_SBC_FAIL;
        result_value |= (secure_ctrl_value & CTRL_DAA_EN)? FUSE_OK : FUSE_DAA_FAIL;
        result_value |= (secure_ctrl_value & CTRL_SW_JTAG_EN)? FUSE_OK : FUSE_SW_JTAG_FAIL;
        result_value |= (secure_lock_value & LOCK_PUBK_EN)? FUSE_OK : FUSE_LOCK_PUBK_FAIL;
        result_value |= (c_ctrlm_value & SELF_BLOW_DISABLE)? FUSE_OK : FUSE_SELF_BLOW_DISABLE_FAIL;

        seq_printf(m, "0x%x\n",result_value);
        return 0;
}

static int secboot_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, secboot_proc_show, NULL);
}

static const struct file_operations secboot_proc_fops = {
        .open           = secboot_proc_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int __init proc_secboot_init(void)
{
        proc_create("secboot_fuse_reg", 0, NULL, &secboot_proc_fops);
        return 0;
}
fs_initcall(proc_secboot_init);

