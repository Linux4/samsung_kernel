#include <kunit/test.h>
#include "../pmu_host_if.h"
#include "../mxman.h"
#include "../fwhdr_if.h"

struct mxman *test_alloc_mxman(struct kunit *test)
{
	struct mxman *mx;

	mx = kunit_kzalloc(test, sizeof(*mx), GFP_KERNEL);
	return mx;
}

struct work_struct *test_alloc_work(struct kunit *test)
{
	struct work_struct *work;

	work = kunit_kzalloc(test, sizeof(*work), GFP_KERNEL);
	return work;
}

void whdr_crc_wq_stop(struct fwhdr_if *interface)
{
}

struct fwhdr_if *test_alloc_fwif(struct kunit *test)
{
	struct fwhdr_if *interface;

	interface = kunit_kzalloc(test, sizeof(*interface), GFP_KERNEL);
	interface->crc_wq_stop = whdr_crc_wq_stop;

	return interface;
}

u32 whdr_get_fw_rt_len(struct fwhdr_if *interface)
{
	return 1;
}

u32 whdr_get_fw_offset(struct fwhdr_if *interface)
{
	return 1024;
}

int mock_platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	return 0;
}

void mock_platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
}

int mock_platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	return 0;
}

int mock_platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	return 0;
}

int mock_platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	return 0;
}

int mock_platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	return 0;
}

#define START_DRAM 0x00000010
void *mock_platform_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	return (void *)START_DRAM;
}

void mock_platform_unmap(struct scsc_mif_abs *interface, void *mem)
{
}

void mock_platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
}

void mock_platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
}

void mock_platform_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
}

void mock_platform_irq_unreg_reset_request_handler(struct scsc_mif_abs *interfacee)
{
}

void mock_platform_irq_unreg_handler(struct scsc_mif_abs *interfacee)
{
}

void mock_platform_irq_unreg_handler_wpan(struct scsc_mif_abs *interfacee)
{
}

void mock_platform_recovery_disabled_unreg(struct scsc_mif_abs *interface)
{
}

void mock_platform_mif_get_mif_device(struct scsc_mif_abs *interface)
{
}

int mock_platform_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	return 0;
}

int mock_platform_get_mifram_ptr(struct scsc_mif_abs *interface,  scsc_mifram_ref *ref)
{
	return 0;
}

int mock_platform_get_mifram_phy_ptr(struct scsc_mif_abs *interface,  scsc_mifram_ref *ref)
{
	return 0;
}

char *mock_platform_get_uid(struct scsc_mif_abs *interface)
{
	return "0";
}

void mock_platform_suspend_unreg_handler(struct scsc_mif_abs *interface)
{
}

void mock_platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
}

int mock_platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface)
{
	return 0;
}

int mock_platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	return PMU_AP_MSG_SUBSYS_ERROR_WPAN;
}

int mock_platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	return 0;
}

void mock_platform_mif_dump_registers(struct scsc_mif_abs *interface)
{
}

int mock_platform_mif_wlbt_phandle_property_read_u32(struct scsc_mif_abs *interface, const char *phandle_name,
					 const char *propname, u32 *out_value, size_t size)
{
	return 0;
}

int mock_platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	return 1;
}

bool mock_platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname)
{
	return 0;
}

int mock_platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
					      const char *propname, u8 *out_value, size_t size)
{
	return 0;
}

int mock_platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
					       const char *propname, u16 *out_value, size_t size)
{
	return 0;
}

int mock_platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
					       const char *propname, u32 *out_value, size_t size)
{
	return 0;
}

int mock_platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
						  const char *propname, char **out_value, size_t size)

{
	return 0;
}

void mock_platform_mif_irq_pmu_bit_unmask(struct scsc_mif_abs *interface)
{

}

void mock_platform_mif_irqdump(struct scsc_mif_abs *interface)
{

}

void mock_platform_wlbt_regdump(struct scsc_mif_abs *interface)
{

}

struct scsc_mif_abs kmif = {
	.irq_get = mock_platform_mif_irq_get,
	.irq_bit_set = mock_platform_mif_irq_bit_set,
	.irq_bit_clear =  mock_platform_mif_irq_bit_clear,
	.irq_bit_mask =  mock_platform_mif_irq_bit_mask,
	.irq_bit_unmask =  mock_platform_mif_irq_bit_unmask,
	.irq_bit_mask_status_get = mock_platform_mif_irq_bit_mask_status_get,
	.map = mock_platform_map,
	.unmap = mock_platform_unmap,
	.irq_reg_handler = mock_platform_mif_irq_reg_handler,
	.irq_reg_handler_wpan = mock_platform_mif_irq_reg_handler_wpan,
	.irq_reg_reset_request_handler = mock_platform_irq_reg_reset_request_handler,
	.irq_unreg_reset_request_handler = mock_platform_irq_unreg_reset_request_handler,
	.irq_unreg_handler = mock_platform_irq_unreg_handler,
	.irq_unreg_handler_wpan = mock_platform_irq_unreg_handler_wpan,
	.recovery_disabled_unreg = mock_platform_recovery_disabled_unreg,
	.get_mifram_ref = mock_platform_get_mifram_ref,
	.get_mif_device = mock_platform_mif_get_mif_device,
	.get_mifram_ptr = mock_platform_get_mifram_ptr,
	.get_mifram_phy_ptr = mock_platform_get_mifram_phy_ptr,
	.get_uid = mock_platform_get_uid,
	.suspend_unreg_handler = mock_platform_suspend_unreg_handler,
	.get_mbox_pmu = mock_platform_mif_get_mbox_pmu,
	.set_mbox_pmu = mock_platform_mif_set_mbox_pmu,
	.get_mbox_ptr = mock_platform_mif_get_mbox_ptr,
	.irq_reg_pmu_handler = mock_platform_mif_irq_reg_pmu_handler,
	.mif_dump_registers = mock_platform_mif_dump_registers,
	.wlbt_phandle_property_read_u32 = mock_platform_mif_wlbt_phandle_property_read_u32,
	.reset = mock_platform_mif_reset,
	.irq_pmu_bit_unmask = mock_platform_mif_irq_pmu_bit_unmask,
	.wlbt_irqdump = mock_platform_mif_irqdump,
	.wlbt_regdump = mock_platform_wlbt_regdump,
	.mif_dump_registers = mock_platform_wlbt_regdump,
};

struct scsc_mif_abs *get_mif(void)
{
	return &kmif;
}

struct scsc_mif_abs *test_alloc_mif(struct kunit *test)
{
	struct scsc_mif_abs *mif;

	mif = kunit_kzalloc(test, sizeof(*mif), GFP_KERNEL);

	memcpy(mif, &kmif, sizeof(struct scsc_mif_abs));

	return mif;
}
