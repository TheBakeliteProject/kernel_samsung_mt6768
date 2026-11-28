#
# arch/arm64/boot/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2012, ARM Ltd.
# Author: Will Deacon <will.deacon@arm.com>
#
# Based on the ia64 boot/Makefile.
#

include $(srctree)/arch/arm64/boot/dts/Makefile

OBJCOPYFLAGS_Image :=-O binary -R .note -R .note.gnu.build-id -R .comment -S

targets := Image Image.bz2 Image.gz Image.lz4 Image.lzma Image.lzo Image-dtb Image.gz-dtb

quiet_cmd_cat = CAT     $@
      cmd_cat = (cat $(filter-out FORCE,$^) > $@) || (rm -f $@; false)

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif
DTB_OBJS := $(addprefix $(obj)/dts/,$(DTB_LIST))

# Samsung's board files are DT overlays.  Turn each selected overlay into a
# complete DTB before appending it to the kernel image so the bootloader can
# select a standalone tree for the current model and hardware revision.
MERGED_DTB_NAMES := $(filter mediatek/merged/%,$(DTB_NAMES))
MERGED_DTB_STEMS := $(patsubst mediatek/merged/%,%,$(MERGED_DTB_NAMES))
MERGED_DTB_OBJS := $(addprefix $(obj)/dts/mediatek/merged/,$(addsuffix .dtb,$(MERGED_DTB_STEMS)))
# Intermediate overlay blobs, built in this directory because the dts/
# sub-make only builds the base mediatek/mt6768.dtb.
OVERLAY_DTB_OBJS := $(addprefix $(obj)/dts/mediatek/,$(addsuffix .dtb,$(MERGED_DTB_STEMS)))

targets += $(patsubst $(obj)/%,%,$(MERGED_DTB_OBJS))
targets += $(patsubst $(obj)/%,%,$(OVERLAY_DTB_OBJS))

quiet_cmd_merge_dtbo = DTBMERGE $@
cmd_merge_dtbo = mkdir -p $(dir $@); \
	$(srctree)/scripts/dtc/ufdt_apply_overlay \
	$(word 1,$(filter-out FORCE,$^)) \
	$(word 2,$(filter-out FORCE,$^)) $@

# Build each overlay .dts to .dtb with the standard DTC overlay flags.
# Use an explicit per-file rule (instead of a %-pattern) so we never
# shadow the base mt6768.dtb rule or the merged output rule.
define overlay_dtbo_rule
$(obj)/dts/mediatek/$(1).dtb: $(srctree)/arch/arm64/boot/dts/mediatek/$(1).dts $(DTC) FORCE
	$$(call if_changed_dep,dtc)
endef

$(foreach dtb,$(MERGED_DTB_STEMS),$(eval $(call overlay_dtbo_rule,$(dtb))))

define merge_dtbo_rule
$(obj)/dts/mediatek/merged/$(1).dtb: \
		$(obj)/dts/mediatek/mt6768.dtb \
		$(obj)/dts/mediatek/$(1).dtb \
		$(srctree)/scripts/dtc/ufdt_apply_overlay FORCE
	$$(call if_changed,merge_dtbo)
endef

$(foreach dtb,$(MERGED_DTB_STEMS),$(eval $(call merge_dtbo_rule,$(dtb))))

$(obj)/Image: vmlinux FORCE
	$(call if_changed,objcopy)

$(obj)/Image.bz2: $(obj)/Image FORCE
	$(call if_changed,bzip2)

$(obj)/Image-dtb: $(obj)/Image $(DTB_OBJS) FORCE
	$(call if_changed,cat)

$(obj)/Image.gz: $(obj)/Image FORCE
	$(call if_changed,gzip)

$(obj)/Image.lz4: $(obj)/Image FORCE
	$(call if_changed,lz4)

$(obj)/Image.lzma: $(obj)/Image FORCE
	$(call if_changed,lzma)

$(obj)/Image.lzo: $(obj)/Image FORCE
	$(call if_changed,lzo)

$(obj)/Image.gz-dtb: $(obj)/Image.gz $(DTB_OBJS) FORCE
	$(call if_changed,cat)

install:
	$(CONFIG_SHELL) $(srctree)/$(src)/install.sh $(KERNELRELEASE) \
	$(obj)/Image System.map "$(INSTALL_PATH)"

zinstall:
	$(CONFIG_SHELL) $(srctree)/$(src)/install.sh $(KERNELRELEASE) \
	$(obj)/Image.gz System.map "$(INSTALL_PATH)"
