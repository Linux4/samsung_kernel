#ifndef __ASV_EXYNOS3830_H__
#define __ASV_EXYNOS3830_H__

#include <linux/io.h>

#define ASV_TABLE_BASE	(0x10009000)
#define ID_TABLE_BASE	(0x10000000)

struct asv_tbl_info {
	unsigned reserved_0:4;
	int      reserved_1:4;
	unsigned littlecpu_asv_group:4;
	unsigned g3d_asv_group:4;
	unsigned mif_asv_group:4;
	unsigned int_asv_group:4;
	unsigned cam_disp_asv_group:4;
	unsigned cp_asv_group:4;
	int      reserved_2:32;
	unsigned reserved_3:4;
	int      reserved_4:4;
	unsigned littlecpu_modify_group:4;
	unsigned g3d_modify_group:4;
	unsigned mif_modify_group:4;
	unsigned int_modify_group:4;
	unsigned cam_disp_modify_group:4;
	unsigned cp_modify_group:4;
	int      asv_table_version:7;
	int      asv_group_type:1;
	int      reserved_5:24;
};

struct id_tbl_info {
	unsigned reserved_0:16;
	unsigned reserved_1:16;

	unsigned reserved_2:16;
	unsigned reserved_3:16;

	unsigned reserved_4:10;
	unsigned product_line:2;
	unsigned reserved_5:4;
	unsigned char ids_cpu:8;
	unsigned char ids_g3d:8;

	unsigned char ids_others:8; /* int, cam, idsp, intcam, mfc, aud */
	unsigned asb_version:8;
	unsigned reserved_7:16;

	unsigned reserved_8:16;
	unsigned short sub_rev:4;
	unsigned short main_rev:4;
	unsigned reserved_9:8;
};

#define ASV_INFO_ADDR_CNT	(sizeof(struct asv_tbl_info) / 4)
#define ID_INFO_ADDR_CNT	(sizeof(struct id_tbl_info) / 4)

static struct asv_tbl_info asv_tbl;
static struct id_tbl_info id_tbl;

int asv_get_grp(unsigned int id)
{
	int grp = -1;

	switch (id) {
	case MIF:
		grp = asv_tbl.mif_asv_group + asv_tbl.mif_modify_group;
		break;
	case INT:
		grp = asv_tbl.int_asv_group + asv_tbl.int_modify_group;
		break;
	case CPUCL0:
	case CPUCL1:
		grp = asv_tbl.littlecpu_asv_group + asv_tbl.littlecpu_modify_group;
		break;
	case G3D:
		grp = asv_tbl.g3d_asv_group + asv_tbl.g3d_modify_group;
		break;
	case CAM:
	case DISP:
		grp = asv_tbl.cam_disp_asv_group + asv_tbl.cam_disp_modify_group;
		break;
	case CP:
		grp = asv_tbl.cp_asv_group + asv_tbl.cp_modify_group;
		break;
	default:
		pr_info("Un-support asv grp %d\n", id);
	}

	return grp;
	return 0;
}

int asv_get_ids_info(unsigned int id)
{
	int ids = 0;

	switch (id) {
	case CPUCL1:
	case CPUCL0:
		ids = id_tbl.ids_cpu;
		break;
	case G3D:
		ids = id_tbl.ids_g3d;
		break;
	case INT:
	case MIF:
	case DISP:
	case CP:
	case CAM:
		ids = id_tbl.ids_others;
		break;
	default:
		pr_info("Un-support ids info %d\n", id);
	}

	return ids;
}

int asv_get_table_ver(void)
{
	return asv_tbl.asv_table_version;
}

void id_get_rev(unsigned int *main_rev, unsigned int *sub_rev)
{
	*main_rev = id_tbl.main_rev;
	*sub_rev =  id_tbl.sub_rev;
}

int id_get_product_line(void)
{
	return id_tbl.product_line;
}

int id_get_asb_ver(void)
{
	return id_tbl.asb_version;
}

int asv_table_init(void)
{
	int i;
	unsigned int *p_table;
	unsigned int *regs;
	unsigned long tmp;

	p_table = (unsigned int *)&asv_tbl;

	for (i = 0; i < ASV_INFO_ADDR_CNT; i++) {
		exynos_smc_readsfr((unsigned long)(ASV_TABLE_BASE + 0x4 * i), &tmp);
		*(p_table + i) = (unsigned int)tmp;
	}

	p_table = (unsigned int *)&id_tbl;

	regs = (unsigned int *)ioremap(ID_TABLE_BASE, ID_INFO_ADDR_CNT * sizeof(int));
	for(i = 0; i < ID_INFO_ADDR_CNT; i++)
		*(p_table + i) = (unsigned int)regs[i];

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("asv_table_version : %d\n", asv_tbl.asv_table_version);
	pr_info("  littlecpu grp : %d\n", asv_tbl.littlecpu_asv_group);
	pr_info("  g3d grp : %d\n", asv_tbl.g3d_asv_group);
	pr_info("  mif grp : %d\n", asv_tbl.mif_asv_group);
	pr_info("  int grp : %d\n", asv_tbl.int_asv_group);
	pr_info("  cam_disp grp : %d\n", asv_tbl.cam_disp_asv_group);
	pr_info("  cp grp : %d\n", asv_tbl.cp_asv_group);
#endif

	return asv_tbl.asv_table_version;
}
#endif
