#
# Makefile for ADI Authenticate IC
#
obj-$(CONFIG_BATTERY_AUTH_DS28E30) += sec-auth-ds28e30.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += sec_auth_ds28e30.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += bignum.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += deep_cover_coproc_SW.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += ds28e30.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += ecc_generate_key.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += ecdsa_generic_api.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += ecdsa_high.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += sha256_stone.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += ucl_rng.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += ucl_sha256.o
sec-auth-ds28e30-$(CONFIG_BATTERY_AUTH_DS28E30) += ucl_sys.o

ifdef CONFIG_ARCH_EXYNOS
sec-auth-ds28e30-$(CONFIG_ARCH_EXYNOS) += 1wire_protocol_exynos.o
else ifdef CONFIG_ARCH_MEDIATEK
sec-auth-ds28e30-$(CONFIG_ARCH_MEDIATEK) += 1wire_protocol_mtk.o
else ifdef CONFIG_ARCH_QCOM
sec-auth-ds28e30-$(CONFIG_ARCH_QCOM) += 1wire_protocol_qcom.o
endif

ifdef CONFIG_EXYNOS_PM_QOS
sec-auth-ds28e30-$(CONFIG_EXYNOS_PM_QOS) += sec_auth_qos_devfreq_setting.o
else ifdef CONFIG_EXYNOS_PM_QOS_MODULE
sec-auth-ds28e30-$(CONFIG_EXYNOS_PM_QOS_MODULE) += sec_auth_qos_devfreq_setting.o
endif
