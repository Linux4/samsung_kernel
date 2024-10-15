#if 0
static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 msi, u8 cpu)
{
        struct platform_mif *platform = platform_mif_from_mif_abs(interface);

        return pcie_mif_set_affinity_cpu(platform->pcie, msi, cpu);
}
#endif
