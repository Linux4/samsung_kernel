/*
 * ** (C) Copyright 2009 Marvell International Ltd.
 * **  		All Rights Reserved
 *
 * ** This software file (the "File") is distributed by Marvell International Ltd.
 * ** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
 * ** You may use, redistribute and/or modify this File in accordance with the terms and
 * ** conditions of the License, a copy of which is available along with the File in the
 * ** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * ** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http:www.gnu.org/licenses/gpl.txt.
 * ** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
 * ** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
 * ** The License provides additional details about this warranty disclaimer.
 * */

#include <linux/version.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "pxpmu.h"
#include "common.h"
#include <asm/irq_regs.h>

#ifdef PX_SOC_ARMADAXP
#include <linux/interrupt.h>
//extern int pmu_request_irq(int irq, irq_handler_t handler);
//extern void pmu_free_irq(int irq);

typedef int (* pmu_request_irq_t)(int irq, irq_handler_t handler);
typedef int (* pmu_free_irq_t)(int irq);

static pmu_request_irq_t pmu_request_irq_func;
static pmu_free_irq_t pmu_free_irq_func;

#endif

#ifdef PRM_SUPPORT
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
	#include <mach/prm.h>
#else
	#include <asm/arch/prm.h>
#endif

#define PRM_ALLOC_RES_COUNT	6
static char* prm_client_name = "Marvell Performance Data Collector";
int prm_client_id = -1;
static prm_resource_id prm_resource[PRM_ALLOC_RES_COUNT] =
{
	PRM_CCNT,
	PRM_PMN0,
	PRM_PMN1,
	PRM_PMN2,
	PRM_PMN3,
	PRM_COP
};
#else /* PRM_SUPPORT */

#include <linux/interrupt.h>
#include <linux/errno.h>

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34))
#include <asm/pmu.h>

#endif

#endif  /* PRM_SUPPORT */


#ifndef PRM_SUPPORT
static struct platform_device * pmu_dev;

typedef int (* irq_set_affinity_t)(unsigned int irq, const struct cpumask *m);

static irq_set_affinity_t irq_set_affinity_func;

bool raw_allocate_pmu;
#endif

//#if !defined(LINUX_VERSION_CODE) || (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34))
//int g_pmu_irq_num = PX_IRQ_PMU;
#ifndef PRM_SUPPORT
//static void * pmu_dev_id = NULL;
#endif
//#endif

#ifdef PRM_SUPPORT
static int free_pmu_prm(void)
{
	if (prm_client_id != -1)
	{
		prm_free_resources(prm_client_id, 0);
		prm_close_session(prm_client_id);
		prm_client_id = -1;
	}

	return 0;
}
#else /* PRM_SUPPORT */
static int raw_free_pmu_non_prm(void)
{
	if (!IS_ERR(pmu_dev) && (pmu_dev != NULL))
	{
		platform_device_del(pmu_dev);
		platform_device_put(pmu_dev);
	}

        pmu_dev = NULL;

	return 0;
}

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0))
static int new_kernel_free_pmu_non_prm(void)
{
	if (!IS_ERR(pmu_dev) && (pmu_dev != NULL))
	{
		release_pmu(ARM_PMU_DEVICE_CPU);
		raw_free_pmu_non_prm();
	}

        pmu_dev = NULL;

	return 0;
}

#else

static int free_pmu_non_prm(void)
{
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34))

	if (!IS_ERR(pmu_dev) && (pmu_dev != NULL))
	{
		int ret;
		
		ret = release_pmu(pmu_dev);
		
		if (ret != 0)
			return ret;
	}

        pmu_dev = NULL;

	return 0;
#else
	return 0;
#endif
}
#endif
#endif /* PRM_SUPPORT */

/*
 * Free PMU resources and unhook the PMU ISR
 */
int free_pmu(void)
{
#ifdef PRM_SUPPORT
	return free_pmu_prm();
#else

/* In the new kernel version, PMU related code has been merge into perf, 
 * we need implement this code and call it directly  */
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0))
	return new_kernel_free_pmu_non_prm();
#else

	if (raw_allocate_pmu)
	{
		return raw_free_pmu_non_prm();
	}
	else
	{
		return free_pmu_non_prm();
	}
#endif
#endif
}

#ifdef PRM_SUPPORT
static int allocate_pmu_prm(void)
{
	int ret;
	int i;

	ret = prm_open_session(PRI_VTUNE, prm_client_name, NULL, NULL);

	if (ret < 0)
	{
		printk(KERN_ERR "[CPA] failed to open prm open session\n");
		return ret;
	}

	prm_client_id = ret;

	for (i=0; i<PRM_ALLOC_RES_COUNT; i++)
	{
		ret = prm_allocate_resource(prm_client_id, prm_resource[i], 0);

		if (ret != 0)
		{
			printk(KERN_ERR "[CPA] failed to allocate prm resource %d, ret = %d\n",
					prm_resource[i], ret);

			goto alloc_pmu_err;
		}
	}

	if ((ret = prm_commit_resources(prm_client_id, 0)) != 0)
	{
		printk(KERN_ERR "[CPA] failed to commit prm resource\n");
		goto alloc_pmu_err;
	}

	return 0;

alloc_pmu_err:
	free_pmu();

	return ret;
}
#else /* PRM_SUPPORT */

static int set_irq_affinity(int irq, unsigned int cpu)
{
#ifdef CONFIG_SMP
	if (irq_set_affinity_func != NULL)
		return irq_set_affinity_func(irq, cpumask_of(cpu));
	else
		return 0;
#else
	return 0;
#endif
}

int set_cpu_irq_affinity(unsigned int cpu)
{
	return set_irq_affinity(platform_get_irq(pmu_dev, cpu), cpu);
}

#ifdef PX_PMU_IRQ_STR

#define IRQ_STR_SEP ","

void change_default_pmu_irq(void)
{
	unsigned int * pmu_irq;
	unsigned int pmu_irq_num;
	const char * pmu_irq_string = PX_PMU_IRQ_STR;
	
	pmu_irq_num = split_string_to_long(pmu_irq_string, IRQ_STR_SEP, NULL);
	
	if (pmu_irq_num == 0)
	{
		return;
	}
	
	pmu_irq = kzalloc(pmu_irq_num * sizeof(unsigned int), GFP_ATOMIC);
	
	split_string_to_long(pmu_irq_string, IRQ_STR_SEP, pmu_irq);
	
	set_pmu_resource(pmu_irq_num, pmu_irq);
	
	kfree(pmu_irq);
}
#endif

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0))
static int raw_allocate_pmu_non_prm(void)
{
	int i;
	int ret;
	struct resource * pmu_resource;
	unsigned int pmu_resource_num;

	irq_set_affinity_func = (irq_set_affinity_t)kallsyms_lookup_name_func("irq_set_affinity");

	pmu_dev = platform_device_alloc("cpa-pmu", 0 /* ARM_PMU_DEVICE_CPU */);
	
	if (pmu_dev == NULL)
	{
		return -ENOMEM;
	}

	/* if the PMU IRQ is defined by the user */
#ifdef PX_PMU_IRQ_STR
	change_default_pmu_irq();
#endif
	get_pmu_resource(&pmu_resource, &pmu_resource_num);
	
	if ((ret = platform_device_add_resources(pmu_dev, pmu_resource, pmu_resource_num)) != 0)
	{
		goto err1;
	}
	
	if ((ret = platform_device_add(pmu_dev)) != 0)
	{
		goto err1;
	}
	
	for (i = 0; i < pmu_dev->num_resources; ++i)
	{
		ret = set_irq_affinity(platform_get_irq(pmu_dev, i), i);
		
		if (ret)
		{
			goto err2;
		}
	}

	return 0;

err2:
	if (!IS_ERR(pmu_dev))
	{
		platform_device_del(pmu_dev);
	}
err1:
	if (!IS_ERR(pmu_dev))
	{
		platform_device_put(pmu_dev);
	}

	pmu_dev = NULL;

	return ret;	
}
#endif

static int new_kernel_allocate_pmu_non_prm(void)
{
	int i;
	int ret, err;
	struct resource * pmu_resource;
	unsigned int pmu_resource_num;

	err = reserve_pmu(ARM_PMU_DEVICE_CPU);
	if (err) {
		printk(KERN_ALERT "[CPA] warning unable to reserve PMU: error = %d\n", err);
	    return err;
	}

	irq_set_affinity_func = (irq_set_affinity_t)kallsyms_lookup_name_func("irq_set_affinity");

	pmu_dev = platform_device_alloc("cpa-pmu", 0 /* ARM_PMU_DEVICE_CPU */);

	if (pmu_dev == NULL)
	{
		return -ENOMEM;
	}

	/* if the PMU IRQ is defined by the user */
#ifdef PX_PMU_IRQ_STR
	change_default_pmu_irq();
#endif
	get_pmu_resource(&pmu_resource, &pmu_resource_num);

	if ((ret = platform_device_add_resources(pmu_dev, pmu_resource, pmu_resource_num)) != 0)
	{
		goto err1;
	}

	if ((ret = platform_device_add(pmu_dev)) != 0)
	{
		goto err1;
	}

	for (i = 0; i < pmu_dev->num_resources; ++i)
	{
		/* If core is offline, no need to set irq affinity for it*/
		if(!cpu_online(i)) continue;

		ret = set_irq_affinity(platform_get_irq(pmu_dev, i), i);

		if (ret)
		{
			goto err2;
		}
	}

	return 0;

err2:
	if (!IS_ERR(pmu_dev))
	{
		platform_device_del(pmu_dev);
	}
err1:
	if (!IS_ERR(pmu_dev))
	{
		platform_device_put(pmu_dev);
	}

	pmu_dev = NULL;

	return ret;
}

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0))
static int allocate_pmu_non_prm(void)
{
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0))
        pmu_dev = reserve_pmu(ARM_PMU_DEVICE_CPU);
        if (IS_ERR(pmu_dev) || (pmu_dev == NULL))
	{
                printk(KERN_ALERT "[CPA] warning: unable to reserve PMU: error = %ld\n", PTR_ERR(pmu_dev));
		return -ENODEV;
        }

        init_pmu(ARM_PMU_DEVICE_CPU);

        if (pmu_dev->num_resources < 1)
	{
                printk(KERN_ALERT "[CPA] no irqs for PMUs defined\n");
                return -ENODEV;
        }

	return 0;
#else
	return -ENODEV;
#endif
}
#endif
#endif /* PRM_SUPPORT */

/*
 * Allocate PMU resources and hook the PMU ISR
 */
int allocate_pmu(void)
{
#ifdef PRM_SUPPORT
	return allocate_pmu_prm();
#else

/* In the new kernel version, PMU related code has been merge into perf, 
 * we need implement this code and call it directly  */
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0))
	return new_kernel_allocate_pmu_non_prm();
#else

	int ret;
	
	ret = allocate_pmu_non_prm();
	
	if (ret != 0)
	{
		raw_allocate_pmu = true;
		/* It is possible that pmu device is not registered by PMU 
		 * or CONFIG_CPU_HAS_PMU is not enabled
		 * so in such case, we need to do the things by ourselves
		 */
		ret = raw_allocate_pmu_non_prm();
	}
	else
	{
		raw_allocate_pmu = false;
	}
	
	return ret;
#endif
#endif
}

#ifndef PRM_SUPPORT
#if 0
static int raw_unregister_pmu_isr(int irq_num, void * dev_id)
{
	free_irq(irq_num, dev_id);

	return 0;
}

static int raw_register_pmu_isr(int irq_num, pmu_isr_t px_pmu_isr, void * dev_id)
{
	int ret;

	/* directly install ISR for PMU */
	if ((ret = request_irq(irq_num/*IRQ_PMU*/,
			       px_pmu_isr,
			       0,//IRQF_DISABLED
			       "CPA PMU",
			       dev_id)) != 0)
	{
		printk("[CPA] Fail to request PMU IRQ %d, errno = %d\n", irq_num, ret);
		return ret;
	}
	else
	{
		printk("[CPA] Succeed to request PMU IRQ %d\n", irq_num);
	}

	return 0;
}
#endif
#endif

#ifdef PRM_SUPPORT
static int register_pmu_isr_prm(pmu_isr_t px_pmu_isr)
{
	int ret;

	if ((ret = pmu_register_isr(prm_client_id, px_pmu_isr, 0)) != 0)
	{
		return ret;
	}

	return 0;
}

#else /* PRM_SUPPORT */

static int register_pmu_isr_non_prm(pmu_isr_t px_pmu_isr)
{
//#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34))
	int i;
	int irq;
	int err = -ENODEV;

	if (IS_ERR(pmu_dev) || (pmu_dev == NULL))
	{
		//return raw_register_pmu_isr(g_pmu_irq_num, px_pmu_isr, pmu_dev_id);
		return -EFAULT;
	}

	for (i = 0; i < pmu_dev->num_resources; ++i)
	{
		irq = platform_get_irq(pmu_dev, i);

		if (irq < 0)
			continue;

#ifdef PX_SOC_ARMADAXP

       pmu_request_irq_func = (pmu_request_irq_t)kallsyms_lookup_name_func("pmu_request_irq");

       err = pmu_request_irq_func(irq, px_pmu_isr);

/* Add IRQF_PERCPU to avoid PMU interrupts migrations when hotplug events happened */
#elif defined(PX_SOC_PXA1088) || defined(PX_SOC_PXAEDEN)

		err = request_irq(irq, px_pmu_isr,
		                  //IRQF_DISABLED | IRQF_NOBALANCING,
		                  IRQF_NOBALANCING | IRQF_PERCPU,
		                  "cpa-pmu",
		                  NULL);

#else

		err = request_irq(irq, px_pmu_isr,
		                  //IRQF_DISABLED | IRQF_NOBALANCING,
		                  IRQF_NOBALANCING,
		                  "cpa-pmu",
		                  NULL);

#endif

		if (err)
		{
			printk(KERN_ALERT "[CPA] unable to request PMU IRQ %d, errno = %d\n", irq, err);
			break;
		}
		else
		{
			printk(KERN_ALERT "[CPA] successfully allocate PMU IRQ %d\n", irq);
		}
	}
	
	return 0;
//#else
//	return raw_register_pmu_isr(g_pmu_irq_num, px_pmu_isr, pmu_dev_id);
//#endif
}
#endif /* PRM_SUPPORT */

int register_pmu_isr(pmu_isr_t px_pmu_isr)
{
#ifdef PRM_SUPPORT
	return register_pmu_isr_prm(px_pmu_isr);
#else
	return register_pmu_isr_non_prm(px_pmu_isr);
#endif
}

#ifdef PRM_SUPPORT

static int unregister_pmu_isr_prm(void)
{
	return pmu_unregister_isr(prm_client_id);
}

#else /* PRM_SUPPORT */

static int unregister_pmu_isr_non_prm(void)
{
//#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34))
	int i;
	int irq;

	if (IS_ERR(pmu_dev))
	{
		//return raw_unregister_pmu_isr(g_pmu_irq_num, pmu_dev_id);
		return -EFAULT;
	}

	for (i = pmu_dev->num_resources - 1; i >= 0; --i)
	{
		irq = platform_get_irq(pmu_dev, i);
		if (irq >= 0) {
			#ifdef PX_SOC_ARMADAXP
			      	pmu_free_irq_func = (pmu_free_irq_t)kallsyms_lookup_name_func("pmu_free_irq");
                		pmu_free_irq_func(irq);
			#else
				free_irq(irq, NULL);
			#endif
		}
	}

	return 0;
//#else
//	raw_unregister_pmu_isr(g_pmu_irq_num, pmu_dev_id);

//	return 0;
//#endif
}
#endif /* PRM_SUPPORT */

int unregister_pmu_isr(void)
{
#ifdef PRM_SUPPORT
	return unregister_pmu_isr_prm();
#else
	return unregister_pmu_isr_non_prm();
#endif
}
