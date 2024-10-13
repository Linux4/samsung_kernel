#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <asm/uaccess.h>
#include <asm/current.h>
#include <linux/mm.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("userspace process memory info device");

struct user_process_mem_info {
	unsigned long vss;
	unsigned long rss;
	unsigned long rss_mapped;
	unsigned long pss;
	unsigned long uss;
	int task_pid;
};

static int memoryinfo_flag = 0;

static pte_t check_pte(struct mm_struct *mm, unsigned long address)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset(mm, address);
	if (!pgd_present(*pgd))
		goto check_fail;

	pud = pud_offset(pgd, address);
	if (!pud_present(*pud))
		goto check_fail;

	pmd = pmd_offset(pud, address);
	if (!pmd_present(*pmd))
		goto check_fail;
	if (pmd_trans_huge(*pmd))
		goto check_fail;

	pte = pte_offset_map(pmd, address);
	if (!pte_present(*pte))
		goto check_fail;

	return *pte;

check_fail:
	return 0;
}


static struct page *check_pfn(pte_t pte)
{
	if (!pte_pfn(pte))
		return NULL;
	else
		return pfn_to_page(pte_pfn(pte));
}

int get_userprocess_meminfo(struct task_struct* p, struct user_process_mem_info *upmeminfo)
{
	struct vm_area_struct *vma = NULL;
	if (p->mm == NULL)
		return -1;

	upmeminfo->task_pid = p->pid;
	for (vma = p->mm->mmap; vma; vma = vma->vm_next) {
		unsigned long pagesize = PAGE_SIZE;
		unsigned long step = vma->vm_start;

		for(; step < vma->vm_end; step += pagesize) {
			pte_t pte;
			int mapcount;
			struct page *page;

			/*!!! VSS in procrank equals rss, and actually wrong !!!*/
			upmeminfo->vss += pagesize; /*vss*/
			pte = check_pte(p->mm, step);
			if(!pte) continue;

			page = check_pfn(pte);
			if(!page) continue;

			/*!!! RSS in procrank equals rss_mapped, and actually wrong !!!*/
			upmeminfo->rss += pagesize; /*rss*/
			mapcount = page_mapcount(page);
			if(2 <= mapcount) {
				upmeminfo->rss_mapped += pagesize; /*rss_mapped*/
				upmeminfo->pss += pagesize / mapcount; /*pss*/
			} else if (1 == mapcount) {
				upmeminfo->rss_mapped += pagesize; /*rss_mapped*/
				upmeminfo->pss += pagesize; /*pss*/
				upmeminfo->uss += pagesize; /*uss*/
			} else if (0 == mapcount) {
				/*this is the difference between rss & rss_mapped.*/
			}
		}
	}

	return 0;
}

int user_process_meminfo_show(void)
{
	struct task_struct *p = NULL;
	struct user_process_mem_info upmeminfo;

	printk(KERN_INFO "%5s %11s %9s %11s %9s %9s %s\n",
			"pid", "vss", "rss", "rss_mapped", "pss", "uss", "name");

	printk(KERN_INFO "-----------------------------------------------------------------\n");
	for_each_process(p) {
		memset(&upmeminfo, 0, sizeof(upmeminfo));
		if (get_userprocess_meminfo(p, &upmeminfo))
			continue;

		printk(KERN_INFO "%5d %10luK %8luK %10luK %8luK %8luK %s\n",
				upmeminfo.task_pid,
				upmeminfo.vss >> 10,
				upmeminfo.rss >> 10,
				upmeminfo.rss_mapped >> 10,
				upmeminfo.pss >> 10,
				upmeminfo.uss >> 10,
				p->comm);
	}

	return 0;
}

static int proc_user_process_meminfo_show(struct seq_file *m, void *v)
{
	struct task_struct *p = NULL;
	struct user_process_mem_info upmeminfo;

	seq_printf(m, "%5s %11s %9s %11s %9s %9s %s\n",
			"pid", "vss", "rss", "rss_mapped", "pss", "uss", "name");

	seq_printf(m, "-----------------------------------------------------------------\n");
	for_each_process(p) {
		memset(&upmeminfo, 0, sizeof(upmeminfo));
		if (get_userprocess_meminfo(p, &upmeminfo))
			continue;

		seq_printf(m, "%5d %10luK %8luK %10luK %8luK %8luK %s\n",
				upmeminfo.task_pid,
				upmeminfo.vss >> 10,
				upmeminfo.rss >> 10,
				upmeminfo.rss_mapped >> 10,
				upmeminfo.pss >> 10,
				upmeminfo.uss >> 10,
				p->comm);
	}

	return 0;
}

static int proc_user_process_meminfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_user_process_meminfo_show, NULL);
}

struct file_operations user_process_meminfo_fops = {
	.open		= proc_user_process_meminfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

