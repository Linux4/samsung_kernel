#ifndef __INTERNAL__SEC_QC_SUMMARY_EXTERNAL_H__
#define __INTERNAL__SEC_QC_SUMMARY_EXTERNAL_H__

/* implemented @ drivers/soc/qcom/memory_dump_v2.c */
#if IS_ENABLED(CONFIG_QCOM_MEMORY_DUMP_V2)
extern void sec_qc_summary_set_msm_memdump_info(struct sec_qc_summary_data_apss *apss);
#else
static inline void sec_qc_summary_set_msm_memdump_info(struct sec_qc_summary_data_apss *apss) {}
#endif

/* implemented @ kernel/sched/walt/walt.c */
#if IS_ENABLED(CONFIG_SCHED_WALT)
extern void sec_qc_summary_set_sched_walt_info(struct sec_qc_summary_data_apss *apss);
#else
static inline void sec_qc_summary_set_sched_walt_info(struct sec_qc_summary_data_apss *apss) {}
#endif

/* implemented @ kernel/trace/msm_rtb.c */
#if IS_ENABLED(CONFIG_QCOM_RTB)
extern void sec_qc_summary_set_rtb_info(struct sec_qc_summary_data_apss *apss);
#else
static inline void sec_qc_summary_set_rtb_info(struct sec_qc_summary_data_apss *apss) {}
#endif

/* implemented @ kernel/time/sched_clock.c */
#if IS_BUILTIN(CONFIG_SEC_QC_SUMMARY)
extern atomic64_t sec_qc_summary_last_ns;
#endif

/* implemented @ kernel/module.c */
#if IS_BUILTIN(CONFIG_SEC_QC_SUMMARY)
extern void *sec_qc_summary_mod_tree;
#endif

#endif /* __INTERNAL__SEC_QC_SUMMARY_EXTERNAL_H__ */
