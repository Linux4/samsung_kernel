obj-$(CONFIG_VIDEO_EXYNOS_MFC) := exynos_mfc.o
#Header include
ccflags-y += -I$(srctree)/drivers/media/platform/exynos/mfc

#Dev interface layer
exynos_mfc-y += mfc.o mfc_dec_v4l2.o mfc_dec_vb2.o mfc_enc_v4l2.o mfc_enc_vb2.o
#Dev control layer
exynos_mfc-y += mfc_rm.o mfc_ctx_ctrl.o mfc_meerkat.o mfc_debugfs.o
#Core interface layer
exynos_mfc-y += mfc_core.o mfc_core_ops.o mfc_core_isr.o
#Core control layer
exynos_mfc-y += mfc_core_hwlock.o mfc_core_intlock.o mfc_core_nal_q.o mfc_core_run.o
exynos_mfc-y += mfc_core_pm.o mfc_core_otf.o mfc_core_meerkat.o
exynos_mfc-y += mfc_core_sync.o mfc_core_sched_rr.o mfc_core_sched_prio.o
#Core HW access layer
exynos_mfc-y += mfc_core_enc_param.o mfc_core_buf_ctrl.o mfc_core_cmd.o mfc_core_perf_measure.o
exynos_mfc-y += mfc_core_hw_reg_api.o mfc_core_reg_api.o
#Plugin interface layer
exynos_mfc-y += plugin/mfc_plugin.o plugin/mfc_plugin_ops.o plugin/mfc_plugin_isr.o
#Plugin control layer
exynos_mfc-y += plugin/mfc_plugin_hwlock.o plugin/mfc_plugin_run.o plugin/mfc_plugin_sync.o
exynos_mfc-y += plugin/mfc_plugin_pm.o plugin/mfc_plugin_meerkat.o
#Plugin HW access layer
exynos_mfc-y += plugin/mfc_plugin_perf_measure.o plugin/mfc_plugin_reg_api.o
#Common base layer
exynos_mfc-y += base/mfc_rate_calculate.o base/mfc_queue.o base/mfc_utils.o
exynos_mfc-y += base/mfc_buf.o base/mfc_meminfo.o base/mfc_mem.o
exynos_mfc-y += base/mfc_qos.o base/mfc_memlog.o base/mfc_sysevent.o base/mfc_llc.o
