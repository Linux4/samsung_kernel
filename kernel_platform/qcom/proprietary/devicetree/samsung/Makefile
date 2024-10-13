# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

dtstree         := $(srctree)/$(src)

dtstree_boards      += $(patsubst $(dtstree)/%.dts,%.dtb, $(wildcard $(dtstree)/*.dts))

dtstree_boards      += $(patsubst $(dtstree)/%.dts,%.dtbo, $(wildcard $(dtstree)/*.dts))
dtb-y    += $(dtstree_boards)

