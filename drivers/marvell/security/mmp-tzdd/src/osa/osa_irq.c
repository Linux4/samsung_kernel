/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 * Filename     : osa_irq.c
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the source code of irq-related functions in osa.
 *
 */

/*
 ******************************
 *          HEADERS
 ******************************
 */

#include <osa.h>

/*
 ******************************
 *          MACROS
 ******************************
 */

#define INIT_IRQ_HANDLE_MAGIC(ih)               \
do {                                            \
	((struct _irq_handle *)ih)->magic[0] = 'I'; \
	((struct _irq_handle *)ih)->magic[1] = 'r'; \
	((struct _irq_handle *)ih)->magic[2] = 'Q'; \
	((struct _irq_handle *)ih)->magic[3] = 'h'; \
} while (0)

#define IS_IRQ_HANDLE_VALID(ih)                 \
	('I' == ((struct _irq_handle *)ih)->magic[0] && \
	'r' == ((struct _irq_handle *)ih)->magic[1] && \
	'Q' == ((struct _irq_handle *)ih)->magic[2] && \
	'h' == ((struct _irq_handle *)ih)->magic[3])

#define CLEAN_IRQ_HANDLE_MAGIC(ih)              \
do {                                            \
	((struct _irq_handle *)ih)->magic[0] = 0;   \
	((struct _irq_handle *)ih)->magic[1] = 0;   \
	((struct _irq_handle *)ih)->magic[2] = 0;   \
	((struct _irq_handle *)ih)->magic[3] = 0;   \
} while (0)

/*
 ******************************
 *          TYPES
 ******************************
 */

struct _irq_handle {
	uint8_t magic[4];

	int32_t irq;
	void (*isr) (void *);
	void (*dsr) (void *);
	void *arg;

	struct tasklet_struct tl;
};

/*
 ******************************
 *          FUNCTIONS
 ******************************
 */

/*
 * Name:        osa_disable_irq
 *
 * Description: disable the irqs
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_disable_irq(void)
{
	local_irq_disable();
}
OSA_EXPORT_SYMBOL(osa_disable_irq);

/*
 * Name:        osa_enable_irq
 *
 * Description: enable the irqs
 *
 * Params:      none
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_enable_irq(void)
{
	local_irq_enable();
}
OSA_EXPORT_SYMBOL(osa_enable_irq);

/*
 * Name:        osa_save_irq
 *
 * Description: save the irq flags and disable the irqs
 *
 * Params:      flags - the irq flags to be saved
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_save_irq(uint32_t *flags)
{
	if (flags)
		local_irq_save(*((ulong_t *)(flags)));
}
OSA_EXPORT_SYMBOL(osa_save_irq);

/*
 * Name:        osa_restore_irq
 *
 * Description: restore the irq flags
 *
 * Params:      flags - the irq flags to be restored
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_restore_irq(uint32_t *flags)
{
	OSA_ASSERT(flags);

	local_irq_restore(*(ulong_t *)flags);
}
OSA_EXPORT_SYMBOL(osa_restore_irq);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))

static irqreturn_t _isr_shell(int irq, void *dev_id, struct pt_regs *regs)
{
	struct _irq_handle *irq_handle = (struct _irq_handle *)dev_id;

	if (irq_handle->isr)
		irq_handle->isr(irq_handle->arg);

	/*
	 * FIXME:
	 * here is a bug.
	 * isr should return the status whether the irq is handled.
	 * that means isr needs a return value.
	 */

	if (irq_handle->dsr)
		tasklet_schedule(&irq_handle->tl);

	return IRQ_HANDLED;
}

#else

static irqreturn_t _isr_shell(int irq, void *dev_id)
{
	struct _irq_handle *irq_handle = (struct _irq_handle *)dev_id;

	if (irq_handle->isr)
		irq_handle->isr(irq_handle->arg);

	/*
	 * FIXME:
	 * here is a bug.
	 * isr should return the status whether the irq is handled.
	 * that means isr needs a return value.
	 */

	if (irq_handle->dsr)
		tasklet_schedule(&irq_handle->tl);

	return IRQ_HANDLED;
}

#endif

/*
 * Name:        osa_hook_irq
 *
 * Description: request the irq and hook the callback isr
 *
 * Params:      irq - irq number to be hooked
 *              isr - the interrupt service routine of the irq
 *              dsr - the delayed service routine of the irq, no use in linux
 *              arg - the argument of the isr/dsr when called
 *              param       - the os-specific param of irq
 *              param->flag - status flag of irq
 *                SA_SHIRQ         interrupt is shared
 *                SA_INTERRUPT     disable local interrupts while processing
 *                SA_SAMPLE_RANDOM the interrupt can be used for entropy
 *
 * Returns:     osa_irq_t - the irq-hooking handle
 *
 * Notes:       none
 */
osa_irq_t osa_hook_irq(int32_t irq,
		       void (*isr) (void *), void (*dsr) (void *),
		       void *arg, struct osa_irq_param *param)
{
	struct _irq_handle *ret = NULL;
	int32_t res;

	OSA_ASSERT((irq >= 0) && (irq < NR_IRQS));
	OSA_ASSERT(isr || dsr);
	OSA_ASSERT(param);

	ret = kmalloc(sizeof(struct _irq_handle), GFP_KERNEL);
	if (!ret) {
		osa_dbg_print(DBG_ERR, "ERROR - failed to create irq handle\n");
		return NULL;
	}

	INIT_IRQ_HANDLE_MAGIC(ret);

	ret->irq = irq;
	ret->isr = isr;
	ret->dsr = dsr;
	ret->arg = arg;

	if (dsr) {
		ret->tl.next = NULL;
		ret->tl.state = 0;
		atomic_set(&ret->tl.count, 0);
		ret->tl.func = (void (*)(ulong_t))dsr;
		ret->tl.data = (ulong_t) arg;
	}

	res = request_irq(irq,
			  _isr_shell,
			  (ulong_t) param->flag, "osa", (void *)ret);
	if (0 == res) {
		return (osa_irq_t) ret;
	} else {
		osa_dbg_print(DBG_ERR, "ERROR - failed to hook irq %d\n", irq);
		CLEAN_IRQ_HANDLE_MAGIC(ret);
		kfree(ret);
		return NULL;
	}
}
OSA_EXPORT_SYMBOL(osa_hook_irq);

/*
 * Name:        osa_free_irq
 *
 * Description: free the irq-hooking
 *
 * Params:      handle - the handle of irq-hooking
 *
 * Returns:     osa_err_t - the status of freeing irq, always OK.
 *
 * Notes:       none
 */
osa_err_t osa_free_irq(osa_irq_t handle)
{
	struct _irq_handle *irq_handle = handle;

	OSA_ASSERT(IS_IRQ_HANDLE_VALID(irq_handle));
	/*
	 * to avoid the condition of freeing irq when isr is called,
	 * the irq will be disabled and then enabled in free_irq.
	 * so what we need to do here is to free_irq and then
	 * to free irq handle.
	 */
	free_irq(irq_handle->irq, irq_handle);

	CLEAN_IRQ_HANDLE_MAGIC(irq_handle);
	kfree(irq_handle);

	return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_free_irq);

/*
 * Name:        osa_mask_irq
 *
 * Description: mask the irq
 *
 * Params:      irq - the irq number to be masked
 *
 * Returns:     none
 *
 * Notes:       no implementation in linux currently
 */
void osa_mask_irq(uint32_t irq)
{
	OSA_ASSERT((irq >= 0) && (irq < NR_IRQS));
	disable_irq(irq);
}
OSA_EXPORT_SYMBOL(osa_mask_irq);

/*
 * Name:        osa_unmask_irq
 *
 * Description: unmask the irq
 *
 * Params:      irq - the irq number to be unmasked
 *
 * Returns:     none
 *
 * Notes:       no implementation in linux currently
 */
void osa_unmask_irq(uint32_t irq)
{
	OSA_ASSERT((irq >= 0) && (irq < NR_IRQS));
	enable_irq(irq);
}
OSA_EXPORT_SYMBOL(osa_unmask_irq);
