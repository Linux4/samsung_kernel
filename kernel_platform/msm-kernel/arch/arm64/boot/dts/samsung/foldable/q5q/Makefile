dtb-y += kalamap-v2-q5q_eur_openx_w00_r09.dtb
kalamap-v2-q5q_eur_openx_w00_r09-dtbs += ../../../vendor/qcom/kalamap-v2.dtb q5q_eur_openx_w00_r09.dtbo
dtb-y += kalamap-q5q_eur_openx_w00_r09.dtb
kalamap-q5q_eur_openx_w00_r09-dtbs += ../../../vendor/qcom/kalamap.dtb q5q_eur_openx_w00_r09.dtbo
dtb-y += kalama-v2-q5q_eur_openx_w00_r09.dtb
kalama-v2-q5q_eur_openx_w00_r09-dtbs += ../../../vendor/qcom/kalama-v2.dtb q5q_eur_openx_w00_r09.dtbo
dtb-y += kalama-q5q_eur_openx_w00_r09.dtb
kalama-q5q_eur_openx_w00_r09-dtbs += ../../../vendor/qcom/kalama.dtb q5q_eur_openx_w00_r09.dtbo
kalama-overlays-dtb-y += q5q_eur_openx_w00_r09.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtb-y += kalamap-v2-q5q_eur_openx_w00_r06.dtb
kalamap-v2-q5q_eur_openx_w00_r06-dtbs += ../../../vendor/qcom/kalamap-v2.dtb q5q_eur_openx_w00_r06.dtbo
dtb-y += kalamap-q5q_eur_openx_w00_r06.dtb
kalamap-q5q_eur_openx_w00_r06-dtbs += ../../../vendor/qcom/kalamap.dtb q5q_eur_openx_w00_r06.dtbo
dtb-y += kalama-v2-q5q_eur_openx_w00_r06.dtb
kalama-v2-q5q_eur_openx_w00_r06-dtbs += ../../../vendor/qcom/kalama-v2.dtb q5q_eur_openx_w00_r06.dtbo
dtb-y += kalama-q5q_eur_openx_w00_r06.dtb
kalama-q5q_eur_openx_w00_r06-dtbs += ../../../vendor/qcom/kalama.dtb q5q_eur_openx_w00_r06.dtbo
kalama-overlays-dtb-y += q5q_eur_openx_w00_r06.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
dtb-y += kalamap-v2-q5q_eur_openx_w00_r04.dtb
kalamap-v2-q5q_eur_openx_w00_r04-dtbs += ../../../vendor/qcom/kalamap-v2.dtb q5q_eur_openx_w00_r04.dtbo
dtb-y += kalamap-q5q_eur_openx_w00_r04.dtb
kalamap-q5q_eur_openx_w00_r04-dtbs += ../../../vendor/qcom/kalamap.dtb q5q_eur_openx_w00_r04.dtbo
dtb-y += kalama-v2-q5q_eur_openx_w00_r04.dtb
kalama-v2-q5q_eur_openx_w00_r04-dtbs += ../../../vendor/qcom/kalama-v2.dtb q5q_eur_openx_w00_r04.dtbo
dtb-y += kalama-q5q_eur_openx_w00_r04.dtb
kalama-q5q_eur_openx_w00_r04-dtbs += ../../../vendor/qcom/kalama.dtb q5q_eur_openx_w00_r04.dtbo
kalama-overlays-dtb-y += q5q_eur_openx_w00_r04.dtbo # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
kalama-overlays-dtb-y += ../../../vendor/qcom/kalama.dtb ../../../vendor/qcom/kalama-v2.dtb ../../../vendor/qcom/kalamap.dtb ../../../vendor/qcom/kalamap-v2.dtb # ADDED BY LEGO AUTOMATICALLY: DO NOT SUBMIT
# SPDX-License-Identifier: GPL-2.0


# add-overlay defines the target with following naming convention:
# <base>-<board>-dtbs = base.dtb board.dtbo
#
# Combined dtb target is also generated using the fdt_overlay tool.
# dtb-y += <base>-<board>.dtb

add-overlays = $(foreach o,$1,$(foreach b,$2,$(eval $(basename $(notdir $b))-$(basename $o)-dtbs = $b $o) $(basename $(notdir $b))-$(basename $o).dtb))

# kalama-dtb-y is list of DTBs to install when doing non-overlay build
# kalama-overlays-dtb-y is list of DTBs and DTBOs to install when doing overlay-supported build
# see DTB_TYPES from scripts/Makefile.dtbinst and install_dtbs from build.config.msm.com

# KALAMA BASE DTB
#QCOM_DTB := ../../../vendor/qcom
#SEC_KALAMA_BASE_DTB := $(QCOM_DTB)/kalama.dtb $(QCOM_DTB)/kalamap.dtb

# Q5Q
#SEC_Q5Q_BOARDS := \
#	kalama-sec.q5q.overlay.00.dtbo

#kalama-dtb-$(CONFIG_ARCH_KALAMA) += \
#    $(call add-overlays, $(SEC_Q5Q_BOARDS),$(SEC_KALAMA_BASE_DTB))
#kalama-overlays-dtb-$(CONFIG_ARCH_KALAMA) += $(SEC_Q5Q_BOARDS) $(SEC_KALAMA_BASE_DTB)
#dtb-y += $(kalama-dtb-y)

always-y        := $(dtb-y)
subdir-y        := $(dts-dirs)

clean-files := *.dtb *.reverse.dts *.dtbo
