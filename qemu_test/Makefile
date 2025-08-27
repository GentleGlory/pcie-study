# ===== User knobs =====
ARCH ?= arm64
CROSS ?= aarch64-linux-gnu-
J ?= $(shell nproc)
KVER ?= v6.16
BBVER ?= 1.36.1

# ===== Layout =====
ROOT := $(CURDIR)
BUILD := $(ROOT)/build
KDIR := $(BUILD)/linux
BBFILE := busybox-$(BBVER).tar.bz2
BBTAR := $(BUILD)/$(BBFILE)
BBDIR := $(BUILD)/busybox-$(BBVER)
IMAGE := $(KDIR)/arch/$(ARCH)/boot/Image
SRC_DTB := $(BUILD)/frank_pcie_test.dtb
DTB := $(SRC_DTB)
INITRD := $(BUILD)/rootfs.cpio.gz
SRC := $(ROOT)/src

.PHONY: all run kernel busybox rootfs menuconfig kdefconfig kconfig-min clean kfrag

all: run

$(BUILD):
	mkdir -p $@

# ---------- Kernel ----------
$(KDIR): | $(BUILD)
	git clone --depth=1 --branch $(KVER) https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git $(KDIR)

kdefconfig: $(KDIR)
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) defconfig

# Minimal config fragment merge (enables PCI host generic & e1000e, etc.)
kconfig-min: kdefconfig kfrag
	$(KDIR)/scripts/kconfig/merge_config.sh -m -O $(KDIR) $(KDIR)/.config $(ROOT)/kfrag
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) olddefconfig

kernel: pci-host-test kconfig-min
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) -j$(J) Image
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) -j$(J) dtbs
	
# Compile our custom DTB
$(SRC_DTB): $(KDIR) $(SRC)/frank_pcie_test.dts
	dtc -I dts -O dtb -o $@ src/frank_pcie_test.dts

# Compile our custom driver module
pci-host-test: $(KDIR) $(SRC)/pci-host-test.c
	cp $(SRC)/pci-host-test.c $(KDIR)/drivers/pci/controller/
	grep -q 'pci-host-test.o' $(KDIR)/drivers/pci/controller/Makefile || echo 'obj-y += pci-host-test.o' >> $(KDIR)/drivers/pci/controller/Makefile


menuconfig:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) menuconfig

# ---------- BusyBox ----------
$(BBTAR): | $(BUILD)
	cp $(SRC)/$(BBFILE) $(BBTAR)

$(BBDIR): $(BBTAR)
	tar xjf $< -C $(BUILD)

busybox: $(BBDIR)
	$(MAKE) -C $(BBDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) defconfig
	# force static busybox to avoid shared libs in initramfs
	sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' $(BBDIR)/.config
	$(MAKE) -C $(BBDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) -j$(J)
	$(MAKE) -C $(BBDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) install

# ---------- RootFS (initramfs) ----------
rootfs: busybox
	rm -rf $(BUILD)/rootfs && mkdir -p $(BUILD)/rootfs
	cp -a $(BBDIR)/_install/* $(BUILD)/rootfs/
	mkdir -p $(BUILD)/rootfs/proc $(BUILD)/rootfs/sys $(BUILD)/rootfs/dev $(BUILD)/rootfs/tmp $(BUILD)/rootfs/root $(BUILD)/rootfs/home
	printf '%s\n' '#!/bin/sh' \
	'mount -t proc none /proc' \
	'mount -t sysfs none /sys' \
	'mount -t tmpfs none /tmp' \
	'echo "Hello from minimal rootfs!"' \
	'exec /bin/sh' > $(BUILD)/rootfs/init
	chmod +x $(BUILD)/rootfs/init
	( cd $(BUILD)/rootfs && find . | cpio -H newc -o ) | gzip -9 > $(INITRD)

# ---------- Run ----------
run: kernel $(SRC_DTB) rootfs scripts/run_qemu.sh
	bash scripts/run_qemu.sh "$(IMAGE)" "$(DTB)" "$(INITRD)"

clean:
	[ -d "$(KDIR)" ] && $(MAKE) -C $(KDIR) ARCH=$(ARCH) clean || true
	[ -d "$(BBDIR)" ] && $(MAKE) -C $(BBDIR) clean || true
	rm -f $(INITRD)
	rm kfrag

distclean:
	rm kfrag
	rm -rf $(BUILD)

# ---------- Files ----------
kfrag:
	@echo 'CONFIG_PRINTK_TIME=y' > kfrag
	@echo 'CONFIG_PCI=y' >> kfrag
	@echo 'CONFIG_PCI_MSI=y' >> kfrag
	@echo 'CONFIG_PCI_HOST_GENERIC=y' >> kfrag
	@echo 'CONFIG_DEVTMPFS=y' >> kfrag
	@echo 'CONFIG_DEVTMPFS_MOUNT=y' >> kfrag
	@echo 'CONFIG_TMPFS=y' >> kfrag
	@echo 'CONFIG_BLK_DEV_INITRD=y' >> kfrag
	@echo 'CONFIG_E1000E=y' >> kfrag
	@echo '# helpful but optional:' >> kfrag
	@echo 'CONFIG_VIRTIO_BLK=y' >> kfrag
	

# Ensure helper scripts exist
scripts/run_qemu.sh:
	mkdir -p scripts
	@echo '#!/usr/bin/env bash' > $@
	@echo 'set -euo pipefail' >> $@
	@echo 'IMAGE="$${1:-}"; DTB="$${2:-}"; INITRD="$${3:-}"' >> $@
	@echo 'if [[ -z "$$IMAGE" || -z "$$DTB" || -z "$$INITRD" ]]; then' >> $@
	@echo '  echo "usage: $$0 <Image> <dtb> <rootfs.cpio.gz>" >&2; exit 1' >> $@
	@echo 'fi' >> $@
	@echo 'qemu-system-aarch64 \' >> $@
	@echo '  -M virt \' >> $@
	@echo '  -cpu cortex-a57 \' >> $@
	@echo '  -smp 2 -m 1024 \' >> $@
	@echo '  -kernel "$$IMAGE" \' >> $@
	@echo '  -initrd "$$INITRD" \' >> $@
	@echo '  -append "console=ttyAMA0" \' >> $@
	@echo '  -device e1000e,netdev=net0 \' >> $@
	@echo '  -netdev user,id=net0 \' >> $@
	@echo '  -nographic \' >> $@
	@echo '  -dtb "$$DTB"'  >> $@
	chmod +x $@

