//#ifdef CONFIG_USERSTACKTRACEG
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/nmi.h>
#include <linux/quicklist.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/kallsyms.h>
#include <linux/ext2_fs.h>
#include <linux/highmem.h>
#define K(x) ((x) << (PAGE_SHIFT - 10))

extern struct meminfo meminfo;
static int show_user_stack(struct seq_file *m, void *v);
//static struct proc_dir_entry* userstack_info;

static int get_pfn_from_mm(unsigned long address, struct mm_struct *mm)
{
        pgd_t *pgd;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *pte;
        int ret = 0;

        pgd = pgd_offset(mm, address);
        if (!pgd_present(*pgd))
                return ret;

        pud = pud_offset(pgd, address);
        if (!pud_present(*pud))
                return ret;

        pmd = pmd_offset(pud, address);
        if (!pmd_present(*pmd))
                return ret;

        pte = pte_offset_map(pmd, address);

        if (!pte_present(*pte))
                return 0;

        return pte_pfn(*pte);
}

static struct page *user_addr_to_page(unsigned long uaddr, struct task_struct *task)
{
        int pfn;
        pfn = get_pfn_from_mm(uaddr, task->mm);

        if (!pfn)
                return NULL;

        return pfn_to_page(pfn);
}

static int show_userstack_of_task(struct task_struct *task, struct seq_file *m, void *v)
{
	unsigned long val;
	unsigned long p;
	struct vm_area_struct *vma;
	unsigned long usersp, kusersp;
	unsigned long length = 32*10;
	unsigned long *kaddr = NULL;
	struct page *page = NULL;
	pgoff_t pgoff;

	if (!task->mm) {
		seq_printf(m, "pid:%d, it's a kthread, no userstack!\n",task->pid);
		return 0;
	}

	usersp = task_pt_regs(task)->uregs[13];

        seq_printf(m, "userstack  pid:%d sp:0x%p\n\n", task->pid, (void *)usersp);

	page = user_addr_to_page(usersp, task);
	if (!page) {
		seq_printf(m, "Sorry, can't find the stack's page!\n");
		return 0;
	}

	pgoff = usersp & 0x00000fff;
	kaddr = kmap(page);
	kusersp = (unsigned long)kaddr + pgoff;

	for (p = kusersp; p < kusersp + length; p += 4) {
		vma = NULL;
		val = *(unsigned long *)p;
		vma = find_vma(task->mm, val);
		if (vma) {
			struct file *file = vma->vm_file;
			struct mm_struct *mm = vma->vm_mm;

			if (val < vma->vm_start) {
                                seq_printf(m, "%4lx: %08lx \n", p, val);
				continue;
			}

			if (file) {
				char buf[50];
				char *pp = NULL;

				memset(buf, 0, 50);
				pp = d_path(&file->f_path, buf, 50);
				if (!IS_ERR(pp))
					mangle_path(buf, pp, "\n");

                                seq_printf(m, "%4lx: %08lx %s  0x%p\n", p, val, buf, (void *)vma->vm_start);
			} else {
				const char *name = arch_vma_name(vma);
				if (!name) {
					if (vma->vm_start <= mm->start_brk && vma->vm_end >= mm->brk)
                                                seq_printf(m, "%4lx: %08lx [heap]\n", p, val);
                                        else if (vma->vm_start <= mm->start_stack && vma->vm_end >= mm->start_stack)
                                                seq_printf(m, "%4lx: %08lx [stack]\n", p, val);
                                        else
                                                seq_printf(m, "%4lx: %08lx [vdso]\n", p, val);
				} else
					seq_printf(m, "%4lx: %08lx %s\n", p, val, name);
			}
		} else
			seq_printf(m, "%4lx: %08lx \n", p, val);
	}
        seq_printf(m, "====================================================\n");
	kunmap(page);
	return 0;
}

static int show_user_stack(struct seq_file *m, void *v)
{
	struct task_struct *g, *p;
	seq_printf(m, "=============enter ====show_user_stack===========\n");

	do_each_thread(g, p) {
		task_lock(p);
		if (p->state == TASK_UNINTERRUPTIBLE)
			show_userstack_of_task(p, m, v);
		task_unlock(p);
	}while_each_thread(g, p);

	return 0;
}

static int userstack_proc_open(struct inode* inode, struct file*  file)
{
	single_open(file, show_user_stack, NULL);
	return 0;
}


struct file_operations userstack_info_proc_fops =
{
	.open = userstack_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

