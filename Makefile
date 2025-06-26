#!/usr/bin/make -f
# SPDX-License-Identifier: GPL-3.0+
# Copyright 2024-2025 Johannes Schauer Marin Rodrigues <josch@mister-muffin.de>

SHELL = /bin/sh
INSTALL = /usr/bin/install
INSTALLDATA = /usr/bin/install -m 644

prefix = /usr
datadir = $(prefix)/share
bindir = $(prefix)/bin
libexecdir = $(prefix)/libexec
libdir = $(prefix)/lib
statedir = /var

BINPROGS=$(wildcard bin/*)
MAN1=$(patsubst bin/%,man/%.1,$(BINPROGS))

.PHONY: all
all: $(MAN1)

man/%.1: bin/%
	set -e;                                                               \
	mkdir -p man;                                                         \
	tool2whatis () { case $${1#reform-} in                                \
		boot-config)    echo "choose rootfs to boot from" ;;          \
		chat)           echo "chat on #mnt-reform" ;;                 \
		check)          echo "check your setup" ;;                    \
		compstat)       echo "system statistics for waybar" ;;        \
		config)         echo "select keyboard and timezone" ;;        \
		display-config) echo "select single/dual display" ;;          \
		emmc-bootstrap) echo "set up /boot on emmc for / on nvme" ;;  \
		flash-rescue)   echo "flash rescue image to eMMC" ;;          \
		flash-uboot)    echo "download and flash u-boot" ;;           \
		gnome-config)   echo "apply default config to GNOME" ;;       \
		handbook)       echo "show Reform Handbook" ;;                \
		pocket-reform-handbook) echo "show Pocket Reform Handbook" ;; \
		help)           echo "help with MNT Reform" ;;                \
		hw-setup)       echo "perform hardware tweaks" ;;             \
		migrate)        echo "copy rootfs to device" ;;               \
		mcu-tool)       echo "manage microcontrollers" ;;             \
		pavucontrol)    echo "kill and restart pavucontrol" ;;        \
		setup-encrypted-disk) echo "setup encrypted disk" ;;          \
		setup-encrypted-nvme) echo "use setup-encrypted-disk" ;;      \
		standby)        echo "suspend/wakeup tweaks" ;;               \
		*) echo "unknown tool: $$1" 2>&1; exit 1 ;;                   \
	esac; };                                                              \
	whatis="$$(tool2whatis "$*")";                                        \
	version=$$(head -c4 CHANGELOG.md);                                    \
	case $$version in 1.[0-9][0-9]) : ;; *) echo invalid;exit 1;; esac;   \
	env "PATH=./bin:$$PATH" help2man                                      \
		--section=1                                                   \
		--name="$$whatis"                                             \
		--no-info --version-string="$$version"                        \
		--no-discard-stderr "$*"                                      \
		--output="$@";                                                \

.PHONY: install
install: $(MAN1)
	$(INSTALL)     -d $(DESTDIR)$(libdir)/NetworkManager/conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/NetworkManager/conf.d NetworkManager/default-wifi-powersave-off.conf
	$(INSTALL)     -d $(DESTDIR)$(libdir)/udev/rules.d
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/udev/rules.d audio/99-reform-audio.rules
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/udev/rules.d udev/reform-ttymxc-wakeup.rules
	$(INSTALL)     -d $(DESTDIR)$(datadir)/pulseaudio/alsa-mixer/paths
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/pulseaudio/alsa-mixer/paths audio/analog-input-reform.conf
	$(INSTALL)     -d $(DESTDIR)$(statedir)/lib/alsa
	$(INSTALLDATA) -t $(DESTDIR)$(statedir)/lib/alsa audio/asound.state
	$(INSTALL)     -d $(DESTDIR)$(datadir)/pulseaudio/alsa-mixer/profile-sets
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/pulseaudio/alsa-mixer/profile-sets audio/reform.conf
	$(INSTALL)     -d $(DESTDIR)$(bindir)
	$(INSTALL)     -t $(DESTDIR)$(bindir) $(BINPROGS)
	$(INSTALL)     -d $(DESTDIR)$(datadir)/flash-kernel/preboot.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/flash-kernel/preboot.d flash-kernel/preboot.d/00reform2_preboot
	$(INSTALL)     -d $(DESTDIR)$(datadir)/flash-kernel/ubootenv.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/flash-kernel/ubootenv.d flash-kernel/ubootenv.d/00reform2_ubootenv
	$(INSTALL)     -d $(DESTDIR)$(datadir)/initramfs-tools/hooks
	$(INSTALL)     -t $(DESTDIR)$(datadir)/initramfs-tools/hooks initramfs-tools/hooks/reform
	$(INSTALL)     -d $(DESTDIR)$(datadir)/initramfs-tools/modules.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/initramfs-tools/modules.d initramfs-tools/reform.conf
	$(INSTALL)     -d $(DESTDIR)$(datadir)/initramfs-tools/scripts/init-top
	$(INSTALL)     -t $(DESTDIR)$(datadir)/initramfs-tools/scripts/init-top initramfs-tools/scripts/reform
	$(INSTALL)     -d $(DESTDIR)$(datadir)/kernel/postinst.d
	$(INSTALL)     -t $(DESTDIR)$(datadir)/kernel/postinst.d kernel/zz-reform-tools
	$(INSTALL)     -d $(DESTDIR)$(libexecdir)/reform-tools
	$(INSTALL)     -t $(DESTDIR)$(libexecdir)/reform-tools libexec/reform-tools/reform-tray.py
	$(INSTALL)     -t $(DESTDIR)$(libexecdir)/reform-tools libexec/reform-tools/reform-wallpaper.py
	$(INSTALL)     -d $(DESTDIR)$(datadir)/reform-tools/machines
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/reform-tools/machines machines/*
	$(INSTALL)     -d $(DESTDIR)$(libdir)/modprobe.d
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/modprobe.d modprobe.d/reform.conf
	$(INSTALL)     -d $(DESTDIR)$(datadir)/glib-2.0/schemas
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/glib-2.0/schemas schemas/20_reform.gschema.override
	$(INSTALL)     -d $(DESTDIR)$(datadir)/gdm/dconf
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/gdm/dconf share/gdm/dconf/95-mnt-reform-settings
	$(INSTALL)     -d $(DESTDIR)$(datadir)/backgrounds
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/backgrounds share/backgrounds/reform-mountains.jpg
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/backgrounds share/backgrounds/mnt-reform-next-y2k.jpg
	$(INSTALL)     -d $(DESTDIR)$(libdir)/systemd/sleep.conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/systemd/sleep.conf.d systemd/reform-sleep.conf
	$(INSTALL)     -d $(DESTDIR)$(libdir)/systemd/system
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/systemd/system systemd/reform-hw-setup.service systemd/reform-sleep.service
	$(INSTALL)     -d $(DESTDIR)$(datadir)/u-boot-menu/conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/u-boot-menu/conf.d u-boot-menu/reform.conf
	$(INSTALL)     -d $(DESTDIR)$(libdir)/dracut/dracut.conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/dracut/dracut.conf.d dracut/20-pocket-reform.conf
	$(INSTALL)     -d $(DESTDIR)$(libdir)/sddm/sddm.conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/sddm/sddm.conf.d sddm/10-wayland.conf
	$(INSTALL)     -d $(DESTDIR)$(datadir)/man/man1
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/man/man1 $(MAN1)
	$(INSTALL)     -d $(DESTDIR)$(datadir)/ucm2/conf.d/rk3588-tlv320ai/rk3588-tlv320aic3100
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/ucm2/conf.d/rk3588-tlv320ai/rk3588-tlv320aic3100 share/ucm2/conf.d/rk3588-tlv320ai/rk3588-tlv320aic3100/rk3588-tlv320aic3100.conf
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/ucm2/conf.d/rk3588-tlv320ai/rk3588-tlv320aic3100 share/ucm2/conf.d/rk3588-tlv320ai/rk3588-tlv320aic3100/HiFi.conf

.PHONY: clean
clean:
	rm -f man/*.1

.PHONY: lint
lint:
	clang-format lpc/reform2_lpc.c | diff -u lpc/reform2_lpc.c -
	shfmt --posix --simplify --binary-next-line --case-indent --indent 2 --diff \
		bin kernel/* initramfs-tools/*/* flash-kernel/*/*
	black --check --diff bin/reform-compstat libexec/reform-tools/reform-tray.py libexec/reform-tools/reform-wallpaper.py
	black --line-length 120 --check --diff bin/reform-mcu-tool
	shellcheck bin/* kernel/* initramfs-tools/*/* flash-kernel/*/*

test:
	# check the validity of gschema overrides
	glib-compile-schemas --dry-run --strict schemas
