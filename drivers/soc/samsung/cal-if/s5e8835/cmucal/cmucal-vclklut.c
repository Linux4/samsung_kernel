#include "../../cmucal.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> LUT Parameter List */
unsigned int cmucal_VDDI_NM_lut_params[] = {
	0, 0,
};
unsigned int cmucal_VDDI_UD_lut_params[] = {
	1, 0,
};
unsigned int cmucal_VDDI_SUD_lut_params[] = {
	3, 0,
};
unsigned int cmucal_VDDI_UUD_lut_params[] = {
	4, 0,
};
unsigned int cmucal_VDD_ALIVE_SOD_lut_params[] = {
52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,52000,52000,393000,52000,52000,52000,400000,52000,52000,393000,52000,52000,52000,400000,4000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,
};
unsigned int cmucal_VDD_ALIVE_OD_lut_params[] = {
52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,52000,52000,393000,52000,52000,52000,400000,52000,52000,393000,52000,52000,52000,400000,4000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,
};
unsigned int cmucal_VDD_ALIVE_NM_lut_params[] = {
52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,52000,52000,393000,52000,52000,52000,400000,52000,52000,393000,52000,52000,52000,400000,4000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,
};
unsigned int cmucal_VDD_ALIVE_UD_lut_params[] = {
52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,52000,52000,393000,52000,52000,52000,400000,52000,52000,393000,52000,52000,52000,400000,4000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,
};
unsigned int cmucal_VDD_ALIVE_SUD_lut_params[] = {
52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,52000,52000,393000,52000,52000,52000,400000,52000,52000,393000,52000,52000,52000,400000,4000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,
};
unsigned int cmucal_VDD_ALIVE_UUD_lut_params[] = {
52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,393000,52000,52000,400000,52000,52000,393000,52000,52000,400000,52000,52000,52000,52000,52000,52000,393000,52000,52000,52000,400000,52000,52000,393000,52000,52000,52000,400000,4000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,26000,26000,52000,52000,26000,26000,52000,52000,26000,
};
unsigned int cmucal_VDD_CAM_SOD_lut_params[] = {
167000,666000,167000,666000,167000,52000,52000,52000,52000,800000,52000,800000,52000,
};
unsigned int cmucal_VDD_CAM_OD_lut_params[] = {
167000,666000,167000,666000,167000,52000,52000,52000,52000,800000,52000,800000,52000,
};
unsigned int cmucal_VDD_CAM_NM_lut_params[] = {
167000,666000,167000,666000,167000,52000,52000,52000,52000,800000,52000,800000,52000,
};
unsigned int cmucal_VDD_CAM_UD_lut_params[] = {
167000,666000,167000,666000,167000,52000,52000,52000,52000,800000,52000,800000,52000,
};
unsigned int cmucal_VDD_CAM_SUD_lut_params[] = {
167000,333000,167000,333000,167000,52000,52000,52000,52000,400000,52000,400000,52000,
};
unsigned int cmucal_VDD_CAM_UUD_lut_params[] = {
167000,333000,167000,333000,167000,52000,52000,52000,52000,400000,52000,400000,52000,
};
unsigned int cmucal_VDD_CHUB_SOD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,
};
unsigned int cmucal_VDD_CHUB_OD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,
};
unsigned int cmucal_VDD_CHUB_NM_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,
};
unsigned int cmucal_VDD_CHUB_UD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,
};
unsigned int cmucal_VDD_CHUB_SUD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,
};
unsigned int cmucal_VDD_CHUB_UUD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,
};
unsigned int cmucal_VDD_ICPU_CSILINK_SOD_lut_params[] = {
1334000,1334000,334000,1334000,1334000,334000,
};
unsigned int cmucal_VDD_ICPU_CSILINK_OD_lut_params[] = {
1334000,1334000,334000,1334000,1334000,334000,
};
unsigned int cmucal_VDD_ICPU_CSILINK_NM_lut_params[] = {
1334000,1334000,334000,1334000,1334000,334000,
};
unsigned int cmucal_VDD_ICPU_CSILINK_UD_lut_params[] = {
1334000,1334000,334000,1334000,1334000,334000,
};
unsigned int cmucal_VDD_ICPU_CSILINK_SUD_lut_params[] = {
1334000,1334000,334000,1334000,1334000,334000,
};
unsigned int cmucal_VDD_ICPU_CSILINK_UUD_lut_params[] = {
1334000,1334000,334000,1334000,1334000,334000,
};
unsigned int cmucal_VDD_CPUCL0_SOD_lut_params[] = {
2000000,1000000,667000,500000,1800000,900000,600000,450000,52000,52000,1800000,1800000,900000,
};
unsigned int cmucal_VDD_CPUCL0_OD_lut_params[] = {
1700000,850000,567000,425000,1500000,750000,500000,375000,52000,52000,1500000,1500000,750000,
};
unsigned int cmucal_VDD_CPUCL0_NM_lut_params[] = {
1500000,750000,500000,375000,1200000,600000,400000,300000,52000,52000,1200000,1200000,1200000,
};
unsigned int cmucal_VDD_CPUCL0_UD_lut_params[] = {
1000000,500000,334000,250000,800000,400000,267000,200000,52000,52000,800000,800000,800000,
};
unsigned int cmucal_VDD_CPUCL0_SUD_lut_params[] = {
450000,225000,150000,113000,400000,200000,134000,100000,52000,52000,400000,400000,400000,
};
unsigned int cmucal_VDD_CPUCL0_UUD_lut_params[] = {
200000,100000,67000,50000,150000,75000,50000,38000,52000,52000,150000,150000,150000,
};
unsigned int cmucal_VDD_G3D_SOD_lut_params[] = {
951000,951000,238000,
};
unsigned int cmucal_VDD_G3D_OD_lut_params[] = {
951000,951000,238000,
};
unsigned int cmucal_VDD_G3D_NM_lut_params[] = {
951000,951000,238000,
};
unsigned int cmucal_VDD_G3D_UD_lut_params[] = {
751000,751000,188000,
};
unsigned int cmucal_VDD_G3D_SUD_lut_params[] = {
551000,551000,138000,
};
unsigned int cmucal_VDD_G3D_UUD_lut_params[] = {
221000,221000,56000,
};
unsigned int cmucal_VDD_NPU_SOD_lut_params[] = {
52000,52000,13000,800000,800000,200000,
};
unsigned int cmucal_VDD_NPU_OD_lut_params[] = {
52000,52000,13000,800000,800000,200000,
};
unsigned int cmucal_VDD_NPU_NM_lut_params[] = {
52000,52000,13000,800000,800000,200000,
};
unsigned int cmucal_VDD_NPU_UD_lut_params[] = {
52000,52000,13000,800000,800000,200000,
};
unsigned int cmucal_VDD_NPU_SUD_lut_params[] = {
52000,52000,13000,400000,400000,100000,
};
unsigned int cmucal_VDD_NPU_UUD_lut_params[] = {
52000,52000,13000,800000,800000,200000,
};
/* SPECIAL VCLK -> LUT Parameter List */
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_PERI_IP_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_USB_NOC_SOD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_OD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_NM_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_UD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_SUD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_UUD_lut_params[] = {
    533,
};

unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_SOD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_OD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_NM_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_UD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_SUD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_UUD_lut_params[] = {
    533,
};

unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_OD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_NM_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_UD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_SUD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_UUD_lut_params[] = {
    666,
};

unsigned int cmucal_DIV_CLKCMU_CSIS_OIS_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_OIS_OD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_OIS_NM_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_OIS_UD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_OIS_SUD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_OIS_UUD_lut_params[] = {
    666,
};

unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_OD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_NM_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_UD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_SUD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_UUD_lut_params[] = {
    666,
};

unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_AP2GNSS_CLK_default_lut_params[] = {
    52,
};
unsigned int cmucal_AP2GNSS_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_AP2GNSS_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_AP2GNSS_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_AP2GNSS_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_AP2GNSS_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_AP2GNSS_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_default_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_OD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_NM_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_UD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_default_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_OD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_NM_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_UD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_DIV_CLK_ALIVE_CHUBVTS_NOC_default_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUBVTS_NOC_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUBVTS_NOC_OD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUBVTS_NOC_NM_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUBVTS_NOC_UD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUBVTS_NOC_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUBVTS_NOC_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_CLKIN_PMU_SUB_default_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_CLKIN_PMU_SUB_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_CLKIN_PMU_SUB_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_CLKIN_PMU_SUB_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_CLKIN_PMU_SUB_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_CLKIN_PMU_SUB_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_CLKIN_PMU_SUB_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_IPCLK_default_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_IPCLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_IPCLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_IPCLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK_default_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_USI0_IPCLKPORT_CLK_default_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_USI0_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_USI0_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_USI0_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_USI0_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_USI0_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_USI0_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_i_ipclk_default_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_i_ipclk_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_i_ipclk_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_i_ipclk_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_i_ipclk_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_i_ipclk_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_i_ipclk_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_DIV_CLK_ALIVE_DBGCORE_NOC_default_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_DBGCORE_NOC_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_DBGCORE_NOC_OD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_DBGCORE_NOC_NM_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_DBGCORE_NOC_UD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_DBGCORE_NOC_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_DBGCORE_NOC_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_DIV_CLK_ALIVE_CHUB_PERI_default_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_PERI_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_PERI_OD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_PERI_NM_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_PERI_UD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_PERI_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_PERI_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_TIMER_IPCLKPORT_CLK_default_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_TIMER_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_TIMER_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_TIMER_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_TIMER_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_TIMER_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_ALIVE_UID_RSTnSYNC_SR_CLK_ALIVE_TIMER_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};


unsigned int cmucal_CLK_BLK_NOCL0_UID_RSTnSYNC_CLK_NOCL0_GIC_IPCLKPORT_CLK_SOD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_RSTnSYNC_CLK_NOCL0_GIC_IPCLKPORT_CLK_OD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_RSTnSYNC_CLK_NOCL0_GIC_IPCLKPORT_CLK_NM_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_RSTnSYNC_CLK_NOCL0_GIC_IPCLKPORT_CLK_UD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_RSTnSYNC_CLK_NOCL0_GIC_IPCLKPORT_CLK_SUD_lut_params[] = {
    134,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_RSTnSYNC_CLK_NOCL0_GIC_IPCLKPORT_CLK_UUD_lut_params[] = {
    54,
};

unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_CLK_MIF_NOCD_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_SOD_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_OD_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_NM_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_UD_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_SUD_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_UUD_lut_params[] = {
    167,
};

unsigned int cmucal_CLK_BLK_CSIS_UID_QE_CSIS3_IPCLKPORT_PCLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_CSIS3_IPCLKPORT_PCLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_CSIS3_IPCLKPORT_PCLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_CSIS3_IPCLKPORT_PCLK_UD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_CSIS3_IPCLKPORT_PCLK_SUD_lut_params[] = {
    333,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_CSIS3_IPCLKPORT_PCLK_UUD_lut_params[] = {
    333,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI0_IPCLKPORT_CLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI0_IPCLKPORT_CLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI0_IPCLKPORT_CLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI0_IPCLKPORT_CLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI0_IPCLKPORT_CLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI0_IPCLKPORT_CLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI0_IPCLKPORT_CLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI1_IPCLKPORT_CLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI1_IPCLKPORT_CLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI1_IPCLKPORT_CLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI1_IPCLKPORT_CLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI1_IPCLKPORT_CLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI1_IPCLKPORT_CLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI1_IPCLKPORT_CLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI2_IPCLKPORT_CLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI2_IPCLKPORT_CLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI2_IPCLKPORT_CLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI2_IPCLKPORT_CLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI2_IPCLKPORT_CLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI2_IPCLKPORT_CLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI2_IPCLKPORT_CLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_I2C_IPCLKPORT_CLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_I2C_IPCLKPORT_CLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_I2C_IPCLKPORT_CLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_I2C_IPCLKPORT_CLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_I2C_IPCLKPORT_CLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_I2C_IPCLKPORT_CLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_I2C_IPCLKPORT_CLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI5_IPCLKPORT_CLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI5_IPCLKPORT_CLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI5_IPCLKPORT_CLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI5_IPCLKPORT_CLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI5_IPCLKPORT_CLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI5_IPCLKPORT_CLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_RSTnSYNC_SR_CLK_CMGP_USI5_IPCLKPORT_CLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_IPCLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_IPCLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_IPCLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_IPCLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI1_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI1_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI1_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI1_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI1_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI1_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_IPCLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_IPCLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_IPCLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    52,
};


unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_I2C_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_I2C_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_I2C_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_I2C_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_I2C_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_I2C_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_VTS_UID_AHB2AXI_VTS_IPCLKPORT_aclk_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_VTS_UID_AHB2AXI_VTS_IPCLKPORT_aclk_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_VTS_UID_AHB2AXI_VTS_IPCLKPORT_aclk_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_VTS_UID_AHB2AXI_VTS_IPCLKPORT_aclk_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_VTS_UID_AHB2AXI_VTS_IPCLKPORT_aclk_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_VTS_UID_AHB2AXI_VTS_IPCLKPORT_aclk_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_VTS_UID_AHB2AXI_VTS_IPCLKPORT_aclk_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CHUBVTS_UID_S2PC_CHUBVTS_IPCLKPORT_PCLK_default_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CHUBVTS_UID_S2PC_CHUBVTS_IPCLKPORT_PCLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CHUBVTS_UID_S2PC_CHUBVTS_IPCLKPORT_PCLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CHUBVTS_UID_S2PC_CHUBVTS_IPCLKPORT_PCLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CHUBVTS_UID_S2PC_CHUBVTS_IPCLKPORT_PCLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CHUBVTS_UID_S2PC_CHUBVTS_IPCLKPORT_PCLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_CHUBVTS_UID_S2PC_CHUBVTS_IPCLKPORT_PCLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK_SOD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK_OD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK_NM_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK_UD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK_SUD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK_UUD_lut_params[] = {
    267,
};

unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_SOD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_OD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_NM_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_UD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_SUD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_UUD_lut_params[] = {
    267,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU0_SW_RESET_IPCLKPORT_CLK_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU0_SW_RESET_IPCLKPORT_CLK_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU0_SW_RESET_IPCLKPORT_CLK_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU0_SW_RESET_IPCLKPORT_CLK_UD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU0_SW_RESET_IPCLKPORT_CLK_SUD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU0_SW_RESET_IPCLKPORT_CLK_UUD_lut_params[] = {
    1200,
};

unsigned int cmucal_CLK_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM_UD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM_SUD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM_UUD_lut_params[] = {
    1200,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_SOD_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_OD_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_NM_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_UD_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_SUD_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_UUD_lut_params[] = {
    50,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK_NM_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK_UD_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK_SUD_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK_UUD_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK_SOD_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK_OD_lut_params[] = {
    38,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_UD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_SUD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_UUD_lut_params[] = {
    300,
};

unsigned int cmucal_CLK_BLK_AUD_UID_SERIAL_LIF_AUD_IPCLKPORT_CCLK_SOD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SERIAL_LIF_AUD_IPCLKPORT_CCLK_OD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SERIAL_LIF_AUD_IPCLKPORT_CCLK_NM_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SERIAL_LIF_AUD_IPCLKPORT_CCLK_UD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SERIAL_LIF_AUD_IPCLKPORT_CCLK_SUD_lut_params[] = {
    100,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SERIAL_LIF_AUD_IPCLKPORT_CCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_UD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_SUD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_UUD_lut_params[] = {
    300,
};

unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_default_lut_params[] = {
    52,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_OD_lut_params[] = {
    52,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_NM_lut_params[] = {
    52,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_UD_lut_params[] = {
    52,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_SOD_lut_params[] = {
    1250,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_OD_lut_params[] = {
    1000,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_NM_lut_params[] = {
    850,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_UD_lut_params[] = {
    550,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_SUD_lut_params[] = {
    250,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_UUD_lut_params[] = {
    250,
};

unsigned int cmucal_CLK_BLK_YUVP_UID_D_TZPC_YUVP_IPCLKPORT_PCLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_D_TZPC_YUVP_IPCLKPORT_PCLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_D_TZPC_YUVP_IPCLKPORT_PCLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_D_TZPC_YUVP_IPCLKPORT_PCLK_UD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_D_TZPC_YUVP_IPCLKPORT_PCLK_SUD_lut_params[] = {
    333,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_D_TZPC_YUVP_IPCLKPORT_PCLK_UUD_lut_params[] = {
    333,
};

unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_SR_CLK_CSTAT_NOCP_IPCLKPORT_CLK_default_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_SR_CLK_CSTAT_NOCP_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_SR_CLK_CSTAT_NOCP_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_SR_CLK_CSTAT_NOCP_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_SR_CLK_CSTAT_NOCP_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_SR_CLK_CSTAT_NOCP_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_SR_CLK_CSTAT_NOCP_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_M2M_UID_PPMU_D1_M2M_IPCLKPORT_PCLK_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_M2M_UID_PPMU_D1_M2M_IPCLKPORT_PCLK_OD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_M2M_UID_PPMU_D1_M2M_IPCLKPORT_PCLK_NM_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_M2M_UID_PPMU_D1_M2M_IPCLKPORT_PCLK_UD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_M2M_UID_PPMU_D1_M2M_IPCLKPORT_PCLK_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_M2M_UID_PPMU_D1_M2M_IPCLKPORT_PCLK_UUD_lut_params[] = {
    80,
};

unsigned int cmucal_CLK_BLK_MFC_UID_PPMU_D_MFC_IPCLKPORT_PCLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_MFC_UID_PPMU_D_MFC_IPCLKPORT_PCLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_MFC_UID_PPMU_D_MFC_IPCLKPORT_PCLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_MFC_UID_PPMU_D_MFC_IPCLKPORT_PCLK_UD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_MFC_UID_PPMU_D_MFC_IPCLKPORT_PCLK_SUD_lut_params[] = {
    333,
};
unsigned int cmucal_CLK_BLK_MFC_UID_PPMU_D_MFC_IPCLKPORT_PCLK_UUD_lut_params[] = {
    84,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI01_USI_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI01_USI_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI01_USI_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI01_USI_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI01_USI_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI01_USI_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI02_USI_IPCLKPORT_CLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI02_USI_IPCLKPORT_CLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI02_USI_IPCLKPORT_CLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI02_USI_IPCLKPORT_CLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI02_USI_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI02_USI_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI03_USI_IPCLKPORT_CLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI03_USI_IPCLKPORT_CLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI03_USI_IPCLKPORT_CLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI03_USI_IPCLKPORT_CLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI03_USI_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI03_USI_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_UART_DBG_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_UART_DBG_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_UART_DBG_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_UART_DBG_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_UART_DBG_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_UART_DBG_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI_I2C_IPCLKPORT_CLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI_I2C_IPCLKPORT_CLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI_I2C_IPCLKPORT_CLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI_I2C_IPCLKPORT_CLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI_I2C_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI_I2C_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_OIS_IPCLKPORT_CLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_OIS_IPCLKPORT_CLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_OIS_IPCLKPORT_CLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_OIS_IPCLKPORT_CLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_OIS_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_OIS_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_OIS_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_OIS_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_OIS_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_OIS_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_OIS_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_OIS_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};


unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D1_DPU_IPCLKPORT_PCLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D1_DPU_IPCLKPORT_PCLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D1_DPU_IPCLKPORT_PCLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D1_DPU_IPCLKPORT_PCLK_UD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D1_DPU_IPCLKPORT_PCLK_SUD_lut_params[] = {
    333,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D1_DPU_IPCLKPORT_PCLK_UUD_lut_params[] = {
    84,
};

unsigned int cmucal_CLK_BLK_DBGCORE_UID_LH_AXI_SI_D_DBGCORE_INT_IPCLKPORT_I_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DBGCORE_UID_LH_AXI_SI_D_DBGCORE_INT_IPCLKPORT_I_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DBGCORE_UID_LH_AXI_SI_D_DBGCORE_INT_IPCLKPORT_I_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DBGCORE_UID_LH_AXI_SI_D_DBGCORE_INT_IPCLKPORT_I_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DBGCORE_UID_LH_AXI_SI_D_DBGCORE_INT_IPCLKPORT_I_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DBGCORE_UID_LH_AXI_SI_D_DBGCORE_INT_IPCLKPORT_I_CLK_UUD_lut_params[] = {
    52,
};


unsigned int cmucal_CLK_BLK_RGBP_UID_RSTnSYNC_CLK_RGBP_NOCP_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_RSTnSYNC_CLK_RGBP_NOCP_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_RSTnSYNC_CLK_RGBP_NOCP_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_RSTnSYNC_CLK_RGBP_NOCP_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_RSTnSYNC_CLK_RGBP_NOCP_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_RSTnSYNC_CLK_RGBP_NOCP_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};
unsigned int div_clk_hsi_ufs_embd_1776_params[] = {
	2, 2,
};
unsigned int div_clk_peri_400_lut_params[] = {
	0, 1,
};
unsigned int div_clk_peri_200_lut_params[] = {
	1, 1,
};
unsigned int div_clk_peri_133_lut_params[] = {
	2, 1,
};
unsigned int div_clk_peri_100_lut_params[] = {
	3, 1,
};
unsigned int div_clk_peri_80_lut_params[] = {
	4, 1,
};
unsigned int div_clk_peri_66_lut_params[] = {
	5, 1,
};
unsigned int div_clk_peri_57_lut_params[] = {
	6, 1,
};
unsigned int div_clk_peri_50_lut_params[] = {
	7, 1,
};
unsigned int div_clk_peri_44_lut_params[] = {
	8, 1,
};
unsigned int div_clk_peri_40_lut_params[] = {
	9, 1,
};
unsigned int div_clk_peri_36_lut_params[] = {
	10, 1,
};
unsigned int div_clk_peri_33_lut_params[] = {
	11, 1,
};
unsigned int div_clk_peri_30_lut_params[] = {
	12, 1,
};
unsigned int div_clk_peri_28_lut_params[] = {
	13, 1,
};
unsigned int div_clk_peri_25_lut_params[] = {
	15, 1,
};
unsigned int div_clk_peri_26_lut_params[] = {
	0, 0,
};
unsigned int div_clk_peri_13_lut_params[] = {
	1, 0,
};
unsigned int div_clk_peri_8_lut_params[] = {
	2, 0,
};
unsigned int div_clk_peri_6_lut_params[] = {
	3, 0,
};
unsigned int div_clk_peri_5_lut_params[] = {
	4, 0,
};
unsigned int div_clk_peri_4_lut_params[] = {
	5, 0,
};
unsigned int div_clk_peri_3_lut_params[] = {
	6, 0,
};
