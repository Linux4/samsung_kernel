# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y := -Wformat
obj-$(CONFIG_TCPC_CLASS)			+= tcpc_class_module.o
tcpc_class_module-y				+= tcpci_core.o tcpci_typec.o tcpci_alert.o \
						   tcpci_timer.o tcpm.o tcpm_legacy.o tcpm_block.o
tcpc_class_module-$(CONFIG_PD_DBG_INFO)		+= pd_dbg_info.o
tcpc_class_module-$(CONFIG_USB_POWER_DELIVERY)	+= tcpci_event.o \
						   pd_core.o pd_policy_engine.o pd_process_evt.o pd_dpm_core.o \
						   pd_process_evt_snk.o pd_process_evt_src.o pd_process_evt_vdm.o \
						   pd_process_evt_drs.o pd_process_evt_prs.o pd_process_evt_vcs.o \
						   pd_policy_engine_src.o pd_policy_engine_snk.o pd_policy_engine_ufp.o \
						   pd_policy_engine_vcs.o pd_dpm_uvdm.o pd_process_evt_dbg.o \
						   pd_policy_engine_dbg.o pd_policy_engine_dfp.o pd_policy_engine_dr.o \
						   pd_policy_engine_drs.o pd_policy_engine_prs.o pd_dpm_alt_mode_dp.o \
						   pd_dpm_pdo_select.o pd_dpm_sec.o
tcpc_class_module-$(CONFIG_PDIC_POLICY)		+= tcpci_typec_class.o
tcpc_class_module-$(CONFIG_SEC_PD)		+= tcpci_pd.o