#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#$(call all-subdir-src-or-makefile)
subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

obj-$(CONFIG_MTK_BASE_POWER) += base/
obj-$(CONFIG_MTK_ICE_DEBUG) += ice_debug/
obj-$(CONFIG_MTK_SYS_CIRQ) += cirq/
obj-$(CONFIG_MTK_EMI_BWL) += emi/
obj-$(CONFIG_MTK_TURNKEY_THERMAL) += thermal/
obj-$(CONFIG_MTK_IRRX) += irrx/
obj-$(CONFIG_MTK_BTIF) += btif/

# for btmtk_btif repo sync from other repo
BTIF = $(wildcard drivers/misc/mediatek/btmtk_btif/*.c)
ifneq ($(BTIF),)
obj-$(CONFIG_MTK_BTMTK_BTIF) += btmtk_btif/
endif
