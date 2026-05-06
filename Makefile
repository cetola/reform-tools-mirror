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
libdirarch = $(prefix)/lib/$(shell $(CC) --print-multiarch)
statedir = /var
sysconfdir = /etc

BINPROGS=$(wildcard bin/*)
MAN1=$(patsubst bin/%,man/%.1,$(BINPROGS))
REFORM_CHECK_INSTALLED_DISTRO ?= $(shell sh -c .\ reform-check/common.sh\;\ detect_backend_from_os_release)
REFORM_CHECK_INSTALLED_BACKEND=reform-check/distro/$(REFORM_CHECK_INSTALLED_DISTRO).sh

# Plymouth integration is optional. If the build host doesn't have
# it, skip building and installing the boot splash bits.
HAVE_PLYMOUTH := $(shell command -v pkgconf >/dev/null 2>&1 && pkgconf --exists ply-splash-core 2>/dev/null && echo yes)

.PHONY: all
all: man $(if $(HAVE_PLYMOUTH),plymouth)

.PHONY: man
man: $(MAN1)

.PHONY: plymouth
plymouth: plymouth/background.png plymouth/monobar.so

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
		flash-bootloader) echo "download and flash u-boot" ;;         \
		flash-rescue)   echo "flash rescue image to eMMC" ;;          \
		flash-uboot)    echo "use reform-flash-bootloader" ;;         \
		gnome-config)   echo "apply default config to GNOME" ;;       \
		handbook)       echo "show Reform Handbook" ;;                \
		pocket-reform-handbook) echo "show Pocket Reform Handbook" ;; \
		help)           echo "help with MNT Reform" ;;                \
		hw-setup)       echo "perform hardware tweaks" ;;             \
		migrate)        echo "copy rootfs to device" ;;               \
		mcu-tool)       echo "manage microcontrollers" ;;             \
		pavucontrol)    echo "kill and restart pavucontrol" ;;        \
		rescue-shell)   echo "rescue a system on eMMC/NVMe" ;;        \
		setup-encrypted-disk) echo "setup encrypted disk" ;;          \
		setup-encrypted-nvme) echo "use setup-encrypted-disk" ;;      \
		standby)        echo "suspend/wakeup tweaks" ;;               \
		waybar-icon-wedge) echo "launch waybar with custom icon theme" ;; \
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

plymouth/monobar.so: plymouth/monobar.c
	$(CC) $< -o $@ $(CPPFLAGS) $(CFLAGS) $(shell pkgconf --cflags --libs ply-splash-core) -Wall -fPIC -pedantic -Wextra -std=c23 -shared $(LDFLAGS)

plymouth/background.png: ./share/backgrounds/mnt-reform-next-y2k.jpg
	convert $< -resize 1920x1080 $@

.PHONY: install
install: install-indep $(if $(HAVE_PLYMOUTH),install-plymouth) $(if $(filter debian,$(REFORM_CHECK_INSTALLED_DISTRO)),install-debian-tooling)
ifneq ($(HAVE_PLYMOUTH),yes)
	@echo "W: ply-splash-core not found, plymouth integration was skipped (no boot splash)." >&2
	@echo "W: install plymouth dev files (Arch: 'plymouth'; Debian: 'libplymouth-dev') and re-run 'make install' to include it." >&2
endif

.PHONY: install-plymouth
install-plymouth: plymouth/background.png plymouth/monobar.so
	$(INSTALL)     -d $(DESTDIR)$(datadir)/initramfs-tools/hooks
	$(INSTALL)     -t $(DESTDIR)$(datadir)/initramfs-tools/hooks initramfs-tools/hooks/reform-plymouth
	$(INSTALL)     -d $(DESTDIR)$(datadir)/plymouth/themes/reform-y2k
	set -e; for f in bullet capslock entry keyboard keymap-render lock; do \
		ln --force --symbolic --no-target-directory ../spinner/$${f}.png $(DESTDIR)$(datadir)/plymouth/themes/reform-y2k/$${f}.png; \
	done
	set -e; for i in $$(seq 1 36); do \
		filename=$$(printf "animation-%04d.png" "$$i"); \
		ln --force --symbolic --no-target-directory ../spinner/$$filename $(DESTDIR)$(datadir)/plymouth/themes/reform-y2k/$$filename; \
	done
	set -e; for i in $$(seq 1 30); do \
		filename=$$(printf "throbber-%04d.png" "$$i"); \
		ln --force --symbolic --no-target-directory ../spinner/$$filename $(DESTDIR)$(datadir)/plymouth/themes/reform-y2k/$$filename; \
	done
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/plymouth/themes/reform-y2k plymouth/background.png
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/plymouth/themes/reform-y2k plymouth/reform-y2k.plymouth
	$(INSTALL)     -d $(DESTDIR)$(libdirarch)/plymouth
	$(INSTALLDATA) -t $(DESTDIR)$(libdirarch)/plymouth plymouth/monobar.so
	$(INSTALL)     -d $(DESTDIR)$(datadir)/plymouth/themes/monobar/
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/plymouth/themes/monobar/ plymouth/monobar.plymouth

.PHONY: install-indep
install-indep: $(MAN1)
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
	$(INSTALL)     -d $(DESTDIR)$(libexecdir)/reform-tools
	$(INSTALL)     -t $(DESTDIR)$(libexecdir)/reform-tools libexec/reform-tools/reform-tray.py
	$(INSTALL)     -t $(DESTDIR)$(libexecdir)/reform-tools libexec/reform-tools/reform-wallpaper.py
	$(INSTALL)     -t $(DESTDIR)$(libexecdir)/reform-tools libexec/reform-tools/reform-power-daemon
	$(INSTALL)     -d $(DESTDIR)$(datadir)/reform-tools/machines
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/reform-tools/machines machines/*
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/reform-tools reform-check/common.sh
	$(INSTALLDATA) $(REFORM_CHECK_INSTALLED_BACKEND) $(DESTDIR)$(datadir)/reform-tools/distro.sh
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
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/systemd/system systemd/reform-hw-setup.service systemd/reform-sleep.service systemd/reform-power-daemon.service
	$(INSTALL)     -d $(DESTDIR)$(libdir)/dracut/dracut.conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/dracut/dracut.conf.d dracut/20-pocket-reform.conf
	$(INSTALL)     -d $(DESTDIR)$(libdir)/sddm/sddm.conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(libdir)/sddm/sddm.conf.d sddm/10-wayland.conf
	$(INSTALL)     -d $(DESTDIR)$(datadir)/man/man1
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/man/man1 $(MAN1)
	$(INSTALL)     -d $(DESTDIR)$(datadir)/alsa/ucm2/conf.d/rk3588-tlv320ai/
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/alsa/ucm2/conf.d/rk3588-tlv320ai/ audio/ucm2.conf.d/rk3588-tlv320ai/rk3588-tlv320aic3100.conf
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/alsa/ucm2/conf.d/rk3588-tlv320ai/ audio/ucm2.conf.d/rk3588-tlv320ai/HiFi.conf
	$(INSTALL)     -d $(DESTDIR)$(datadir)/wireplumber/wireplumber.conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/wireplumber/wireplumber.conf.d audio/reform-hdmi-audio-priority.conf
	$(INSTALL)     -d $(DESTDIR)$(sysconfdir)/profile.d
	$(INSTALLDATA) -t $(DESTDIR)$(sysconfdir)/profile.d etc/profile.d/reform-kwin.sh
	$(INSTALLDATA) -t $(DESTDIR)$(sysconfdir)/profile.d etc/profile.d/reform-mali.sh
	$(INSTALL)     -d $(DESTDIR)$(datadir)/doc/reform-tools/examples
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/doc/reform-tools/examples examples/keyboard_rainbow.py
	if [ -e "reform-check/distro/unknown.sh" ]; then \
		echo "E: reform-check/distro/unknown.sh must never exist" >&2; \
		exit 1; \
	fi
	if [ "$(REFORM_CHECK_INSTALLED_DISTRO)" = "unknown" ]; then \
		echo "E: unable to detect your distro" >&2; \
		exit 1; \
	fi
	if [ ! -r "$(REFORM_CHECK_INSTALLED_BACKEND)" ]; then \
		echo "E: $(REFORM_CHECK_INSTALLED_BACKEND) is unreadable" >&2; \
		exit 1; \
	fi

# Tooling configs that only make sense on Debian-based systems:
# flash-kernel, initramfs-tools, u-boot-menu, and Debian's kernel
# postinst.d hook convention. Wired into 'install' only when the
# detected distro is debian.
.PHONY: install-debian-tooling
install-debian-tooling:
	$(INSTALL)     -d $(DESTDIR)$(datadir)/flash-kernel/preboot.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/flash-kernel/preboot.d flash-kernel/preboot.d/00reform2_preboot
	$(INSTALL)     -d $(DESTDIR)$(datadir)/flash-kernel/ubootenv.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/flash-kernel/ubootenv.d flash-kernel/ubootenv.d/00reform2_ubootenv
	$(INSTALL)     -d $(DESTDIR)$(datadir)/initramfs-tools/hooks
	$(INSTALL)     -t $(DESTDIR)$(datadir)/initramfs-tools/hooks initramfs-tools/hooks/reform
	$(INSTALL)     -t $(DESTDIR)$(datadir)/initramfs-tools/hooks initramfs-tools/hooks/reform_set_root
	$(INSTALL)     -d $(DESTDIR)$(datadir)/initramfs-tools/modules.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/initramfs-tools/modules.d initramfs-tools/reform.conf
	$(INSTALL)     -d $(DESTDIR)$(datadir)/initramfs-tools/scripts/init-top
	$(INSTALL)     -t $(DESTDIR)$(datadir)/initramfs-tools/scripts/init-top initramfs-tools/scripts/reform
	$(INSTALL)     -d $(DESTDIR)$(datadir)/kernel/postinst.d
	$(INSTALL)     -t $(DESTDIR)$(datadir)/kernel/postinst.d kernel/zz-reform-tools
	$(INSTALL)     -t $(DESTDIR)$(datadir)/kernel/postinst.d kernel/zz-reform-bootspec
	$(INSTALL)     -d $(DESTDIR)$(datadir)/u-boot-menu/conf.d
	$(INSTALLDATA) -t $(DESTDIR)$(datadir)/u-boot-menu/conf.d u-boot-menu/reform.conf

.PHONY: clean
clean:
	rm -f man/*.1 plymouth/background.png

.PHONY: lint
lint:
	clang-format lpc/reform2_lpc.c | diff -u lpc/reform2_lpc.c -
	shfmt --posix --simplify --binary-next-line --case-indent --indent 2 --diff \
		bin kernel/* initramfs-tools/*/* flash-kernel/*/* reform-check/*.sh reform-check/distro/*.sh
	shfmt --language-dialect=bash --simplify --binary-next-line --case-indent --indent 2 --diff \
		libexec/reform-tools/reform-power-daemon
	black --check --diff bin/reform-compstat libexec/reform-tools/reform-tray.py libexec/reform-tools/reform-wallpaper.py examples/keyboard_rainbow.py
	black --line-length 120 --check --diff bin/reform-mcu-tool
	shellcheck bin/* kernel/* initramfs-tools/*/* flash-kernel/*/* reform-check/*.sh reform-check/distro/*.sh libexec/reform-tools/reform-power-daemon

test:
	# check the validity of gschema overrides
	# Create a dummy placeholder file to convince glib-compile-schemas to
	# not skip processing this directory
	echo '<schemalist></schemalist>' > schemas/dummy.gschema.xml
	glib-compile-schemas --dry-run --strict schemas
	rm schemas/dummy.gschema.xml
	set -e; for backend in reform-check/distro/*.sh; do \
		backend_name=$$(basename "$$backend"); \
		backend_dir=$$(dirname "$$backend"); \
		BACKEND="$$backend" BACKEND_DIR="$$backend_dir" sh -ec '\
			. reform-check/common.sh; \
			. "$$BACKEND"; \
			# BACKEND_MAIN_FLOW_READY marks that this backend is wired into \
			# the shared reform-check flow and expected to implement the full \
			# backend interface below. Experimental backends can leave this \
			# unset or set it to no until they are ready. \
			test "$${BACKEND_MAIN_FLOW_READY:-no}" = yes; \
			for backend_fn in \
				backend_name \
				backend_validate_options \
				backend_preflight_checks \
				backend_pkg_installed \
				backend_pkg_version \
				backend_pkg_verify \
				backend_kernel_pkg_name \
				backend_kernel_headers_pkg_name \
				backend_tools_pkg_name \
				backend_fwupd_pkg_name \
				backend_jq_pkg_name \
				backend_mcu_tool_pkg_name \
				backend_running_kernel_pkg_installed \
				backend_running_kernel_headers_pkg_installed \
				backend_running_kernel_headers_pkg_name \
				backend_running_kernel_matches_main_pkg \
				backend_kernel_artifact_checks \
				backend_skel_profile_checks \
				backend_repo_checks \
				backend_repo_online_checks \
				backend_boot_tool_checks \
				backend_modules_check \
				backend_initramfs_checks \
				backend_distro_specific_checks; do \
				command -v "$$backend_fn" >/dev/null 2>&1 || { \
					echo "$$BACKEND: missing $$backend_fn" >&2; \
					exit 1; \
				}; \
			done'; \
	done
