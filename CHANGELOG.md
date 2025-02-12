1.67 (2025-02-12)
=================

 * run `lsblk` with `--nodeps` where appropriate
 * `machines/MNT Pocket Reform with RCORE RK3588 Module.conf`: update
   `BOOTARGS` with `fbcon=font:TER16x32`
 * `sbin/reform-boot-config`:
    - delete the contents of the old `/boot`
    - add `--force` option to proceed without user interaction
 * `sbin/reform-setup-encrypted-nvme`: add `--force` option to run
   `reform-migrate` and put `/boot` on eMMC if allowed
 * `sbin/reform-display-config`: always write out
   `/etc/u-boot-menu/conf.d/reform.conf`
 * `bin/reform-check`:
    - be more verbose in error messages
    - make sure that `/etc/u-boot-menu/conf.d/reform.conf` exists with the
      right contents

1.66 (2025-02-03)
=================

  * Replace remaining uses of blkid by lsblk.
    Thanks to Chris Hofstaedtler <zeha@debian.org>
  * machines/* with i.MX8MP Module.conf: update to u-boot 2025-01-12
  * Makefile: install files in /usr/sbin with 755
  * sbin/reform-emmc-bootstrap: only run reform-flash-uboot
    if EMMC_BOOT != false
  * sbin/reform-emmc-bootstrap:
     - allow execution when actual root is on nvme
     - ensure emmc is not being used

1.65 (2025-01-11)
=================

   * machines/* with BPI-CM4 Module.conf: update u-boot to 2024-12-23
   * add reform-emmc-bootstrap
   * sbin/reform-flash-uboot:
      - restore support for --offline switch
      - instead of finding the end of the first free space, find the beginning
        of the first partition
      - add --force
   * sbin/reform-boot-config:
      - reset MOUNTROOT after umounting, so that the right partition gets
        mounted afterward
      - make sure that OLDMOUNTBOOT actually is a mount point as a sanity check
      - run mountpoint with --quiet
      - do not update, but create the initramfs
   * sbin/reform-setup-encrypted-nvme: fix incorrectly placed double quote
   * install usr/lib/sddm/sddm.conf.d/10-wayland.conf to make sddm use wayland
     by default instead of Xorg
   * add x-initrd.attach to /etc/crypttab
   * move 99-reform.rules -> 99-reform-audio.rules
   * imx8mq: replace writing 'enabled' to /sys path in reform-hw-setup and
     reform-standby by udev rule
   * Makefile:
      - let help2man error out in case of problems
      - expect all tools to start with reform- prefix
   * reform-mcu-tool: allow USBErrorPipe on reset

1.64 (2024-12-06)
=================

   * add Makefile
   * add CHANGELOG.md
   * sbin/reform-flash-uboot: do not download anything if flash.bin is
     up-to-date
   * bin/reform-check:
      - synchronize with reform-system-image:mkimage.sh
      - warn about reform-qcacld2 on bookworm
      - improve modprobe.d/reform.conf wording
   * bin/reform-compstat: use read_time instead of read_bytes to determine disk
     activity percent
   * make everything shellcheck-clean
   * reform-flash-uboot: require using shorthands
   * add dracut/20-pocket-reform.conf
   * reform2_lpc: add backlight support for pocket reform display v2
   * reform2_lpc: convert camelCase to conform to kernel coding style

1.63 (2024-11-20)
=================

   * kernel/zz-reform-tools: turn error messages into warnings, they are not
     fatal
   * machines/* with RCORE RK3588 Module.conf: set EMMC_BOOT=warn
   * sbin/reform-flash-uboot: add support for rk3588 eMMC which is not using the
     boot0 partition for u-boot
   * bin/reform-compstat:
      - add copyright header
      - add --percpu
      - use four six-per-em space U+2006 characters to create an empty box
      - let 98% CPU utilization already print the full bar
   * Add reform-mcu-tool

1.62 (2024-11-18)
=================

   * lpc/reform2_lpc.c: fixup ifdef

1.61 (2024-11-14)
=================

   * sbin/reform-hw-setup: print error if gpiod is too old
   * new script: reform-handbook
   * machines/* with RCORE RK3588 Module.conf: update u-boot to 2024-11-13b
   * machines/* with i.MX8MP Module.conf: update u-boot to 2024-11-14
   * lpc/reform2_lpc.c: add abs_diff implementation for linux (<< 6.6)

1.60 (2024-11-06)
=================

   * machines: make EMMC_BOOT a tri-state option
   * move MNT icon font to reform-branding package
   * Add postinst script to set font on upgrade
   * Do not set hardcoded terminal font

1.59 (2024-10-30)
=================

   * debian/reform-tools.postinst: do not remove the world-readable bit from
     files in /etc/skel. The bash package also installs files into /etc/skel
     with mode 644

1.58 (2024-10-30)
=================

   * bin/reform-check: add more checks for wrong permissions in /etc
   * Revert "initramfs: Load font early on Pocket Reform"
   * Clean up old /etc/modprobe.d/reform.conf conffile

1.57 (2024-10-29)
=================

   * fix permission 666 -> 644 for reform apt sources.list files
   * machines/* with RCORE RK3588 Module.conf: set EMMC_USE=true because even
     when u-boot was borked, rk3588 will prefer u-boot from sd-card if present
   * bin/reform-check: check for wrong permissions of apt sources.list
   * sbin/reform-{boot-config,migrate}: check if $BOOTPART exists before
     starting to operate

1.56 (2024-10-24)
=================

   * reform-hw-setup: add eth reset workaround and audio setup for rk3588

1.55 (2024-10-11)
=================

   * reform2_lpc: Run through clang-format using the Linux config
   * reform2_lpc: Set battery technology to Unknown
   * reform2_lpc: Detect battery reading glitches
   * reformat shell scripts with shfmt
   * machines/*RK3588 Module.conf: update to 2024-10-11b

1.54 (2024-10-08)
=================

   * bump rk3588 u-boot to 2024-10-08
   * add missing dependency of reform-wallpaper as Recommends
   * initramfs: Load font early on Pocket Reform
   * reform-hw-setup: fixup a311d ethernet re-probing

1.53 (2024-09-06)
=================

   * sbin/reform-flash-uboot: fix functionality that running without arguments
     just updates /boot/flash.bin
   * kernel/zz-reform-tools:
      - make sure /usr/lib/linux-image-* exists
      - print error if there was no dtb found
      - print warning for irregular dtb paths
   * install /usr/share/glib-2.0/schemas/20_reform.gschema.override setting the
     default theme and font
   * add libexec/reform-tools/reform-wallpaper.py

1.52 (2024-08-22)
=================

   * sbin/reform-hw-setup: do not run init_qca9377_wifi if qcacld2 is already
     loaded
   * sbin/reform-hw-setup: run insmod with -f or otherwise it will fail with
     'Invalid module format' even if the version is correct

1.51 (2024-08-22)
=================

   * kernel/zz-reform-tools: fix variable name mode -> action
   * sbin/reform-hw-setup: make init_qca9377_wifi failing non-fatal

1.50 (2024-08-22)
=================

   * Add Recommends on bmap-tools
   * machines/MNT * with RCORE RK3588 Module.conf: bump u-boot version
   * add /usr/share/kernel/postinst.d/zz-reform-tools
   * sbin/reform-flash-uboot (Thanks to deianara)
      - fix typo MMC_BOOT -> EMMC_BOOT.
      - run parted with --script.
   * sbin/reform-display-config: support for u-boot-menu
   * sbin/reform-hw-setup:
      - run with set -e
      - prefix messages with type indicator
      - add support for pocket reform and reform next with rk3588
   * sbin/reform-hw-setup: Change how the qcacld2 driver is loaded. Try to load
     exact match otherwise try every other available option

1.49 (2024-08-09)
=================

   * initramfs-tools/hooks/reform: add rk3588 for next and pocket
   * add machines/MNT Pocket Reform with RCORE RK3588 Module.conf
   * reform-check: stop warning about missing policykit-1
   * reform-check: stop warning about missing /etc/modprobe.d/reform.conf

1.48 (2024-08-07)
=================

   * sbin/reform-flash-uboot: allow flashing to emmc on platforms with
     SD_BOOT set to false
   * disallow flashing u-boot to emmc on i.MX8MP to avoid accidentally
     soft-bricking it
   * The u-boot offset for i.MX8MP is zero bytes
   * remove reform-synaptic
   * add .gitlab-ci.yml

1.47 (2024-08-02)
=================

   * stop shipping files in /etc
   * move modprobe.d/reform.conf from /etc to /usr/lib
   * bin/reform-check:
      - fix deprecated kernel detection
      - respect xz-compressed dkms kernel modules
   * add machine/*.conf files for RCORE RK3588

1.46 (2024-08-01)
=================

   * add --help output and man pages for all tools
   * replace Iosevka Term font with JetBrains Mono Regular
   * Stop shipping the following files:
      - /etc/reform-help
      - /etc/reform-desktop-help
      - /etc/reform-root-help
      - /sbin/reform-init
      - /usr/bin/reform-desktop-help
   * machines/MNT Reform 2.conf: update to u-boot 2024-07-19
   * machines/MNT * with BPI-CM4 Module.conf: u-boot 2024-08-01 (nvme support)
   * instead of explicitly running foot, run x-terminal-emulator which defaults
     to foot in the system images. Change the default terminal by running
         $ sudo update-alternatives --config x-terminal-emulator
   * bin/reform-check:
      - download uboot with dtbpath prefix
      - do not fail script if u-boot cannot be downloaded
      - allow --mirror=reform.debian.net
      - fix FLASHBIN_OFFSET computation
      - update package list
   * bin/reform-compstat: format with black
   * bin/reform-help: point out that reform-display-config is only for imx8mq
   * sbin/reform-flash-rescue
      - use bmaptool if available
      - resize root partition to fill remaining space on emmc
   * sbin/reform-boot-config: do not offer unmounting /boot twice
   * flash-kernel: put markers around preboot.d and ubootenv.d snippets
   * only inform about the dip-switch position on imx8mq
   * systemd/*.service: add Documentation field
   * add "sudo" to root commands in reform-help
   * Rename battery under /sys from 8xlifepo4 to BAT0

1.45 (2024-06-30)
=================

   * reform-hw-setup: a311d: rebind wifi sdio after booting to make sure it
     works
   * a311d: disable wifi powersaving to make wifi more reliable
   * sbin/reform-boot-config: add missing space
   * place default-wifi-powersave-off.conf into /usr/lib/NetworkManager/conf.d/
   * machines/*.conf: update u-boot tags and hashes for reform with imx8mq,
     a311d and ls1028a
   * sbin/reform-flash-uboot: download flash.bin by dtb basename

1.44 (2024-06-10)
=================

   * imx8mp: add missing initramfs drivers; fixes display, pcie and usb in
     initramfs
   * sbin/reform-setup-encrypted-nvme: make /boot on eMMC the default for
     reform-boot-config
   * sbin/reform-boot-config: allow interactive umount
   * initramfs-tools/scripts/reform: run 'dmesg -n 7' in the initramfs on
     ls1028 as workaround for dwc3 unreliability
   * sbin/reform-setup-encrypted-nvme: if reform-boot-config fails, print steps
     how to re-run it

1.43 (2024-06-03)
=================

   * reform-check: adjustments for new kernel flavour mnt-reform-arm64 in 6.8
   * machines: on imx8mp, mmc is mmcblk2 and sd is mmcblk0
   * sbin/reform-hw-setup: adjust for different /sys path in linux 6.8
   * sbin/reform-hw-setup: add loglevel=7 workaround for dwc3 module on ls1028a

1.42 (2024-05-07)
=================

   * reform-setup-cleanup.sh and reform-setup-sway-config are now shipped by
     reform-setup-wizard
   * imx8mp machines: add u-boot sha1 and tag
   * machines: add BOOTARGS
   * reform-check: check for abnormalities in kernel cmdline

1.41 (2024-05-06)
=================

   * etc/profile.d/reform.sh: remove harmful QT_QPA_PLATFORM=wayland and
     outdated ETNA_MESA_DEBUG=nir

1.40 (2024-04-17)
=================

   * wayfire: allow brightness ctrl also by super+f1/f2

1.39 (2024-04-16)
=================

   * sbin/reform-flash-uboot: fixup MMCDEV -> DEV_MMC
   * bin/reform-check: print additional information about the current system
   * machines/*: introduce SD_BOOT which is false on i.MX8MP
   * reform-hw-setup: add loader for qca9377 wifi on imx8mp platforms
   * firefox syspref.js: remove ads on new tab page
   * modprobe: block ath10k_sdio for imx8mp
   * reform-tray: include reform-help in reform-desktop-help
   * reform-hw-setup: add audio init for pocket reform

1.38 (2024-03-24)
=================

   * libexec/reform-tools/reform-tray.py: make GUI utilities optional
   * bin/reform-check: more reform2_lpc.ko and linux-headers package check
   * sway+wayfire: properly rotate and scale the pocket reform display
   * sway+wayfire: use hyper f1/f2 for brightness, use percentages, use 10% steps
   * refactor reform-hw-setup a bit and set big console font on pocket reform

1.37 (2024-03-20)
=================

   * install /usr/share/reform-setup-wizard/cleanup.d/reform-setup-cleanup.sh
   * install /usr/share/reform-setup-wizard/reform-setup-sway-config

1.36 (2024-03-19)
=================

   * reform-tray: update icon to view-more-symbolic (3 dots)
   * waybar: add mnt logo, add tooltip and instructions for launcher, fix
     battery icons
   * reform-tray: change icon, reorder menu to be more logical, open handbook
     on 'Help'
   * sbin/reform-hw-setup: do not run with set -e

1.35 (2024-03-19)
=================

   * Add support for four new setups:
      - MNT Pocket Reform with i.MX8MP Module
      - MNT Pocket Reform with BPI-CM4 Module
      - MNT Reform 2 with i.MX8MP Module
      - MNT Reform 2 with RCORE RK3588 Module
   * bin/reform-check: check if reform2_lpc is loaded
   * bin/reform-check: check whether dtb and symlinks to it are set up correctly
   * bin/reform-check: instruct to run 'apt update' first
   * sbin/reform-boot-config: do not fail if /boot is not mounted
   * sbin/reform-boot-config: make --help output dependent on the platform
   * sbin/reform-flash-uboot: forbid flashing u-boot to emmc on A311D and
     LS1028A
   * sbin/reform-migrate: give approximate numbers that have to be copied
   * initramfs-tools/hooks/reform: warn instead of error if platform unknown
   * initramfs-tools/scripts/reform: disable manual modprobing
   * flash-kernel/ubootenv.d/00reform2_ubootenv, u-boot-menu/reform.conf: set
     loglevel=3
   * remove X11 settings for etnaviv -- this removes /etc/X11/xorg.conf as well
     as /usr/share/X11/xorg.conf.d/10-reform-etnaviv.conf

1.34 (2024-01-02)
=================

   * etc/skel/.config/sway/config: replace dmps on/off with brightnessctl
     --save and brightnessctl --restore
   * reform-hw-setup: add comment explaining a311d amixer settings
   * reform-boot-config: fix /boot message for a311d
   * reform-boot-config: allow unmount /boot after prompt if current system is
     rescue sd-card
   * reform-boot-config: remove the rescue system disk label if necessary
   * reform-flash-uboot: inform about the DIP switch on imx8mq
   * reform-check: support for reform.debian.net
   * reform-flash-rescue: support for flashing reform.d.n images to emmc
   * stop installing /usr/lib/systemd/system-shutdown/reform-poweroff now that
     the reform-lpc module does the right thing
   * use symlinks to install systemd services to let dh_installsystemd choose
     whether to install into /lib or into /usr/lib
   * etc/reform-help: instruct to run reform-config with sudo
   * lpc/reform2_lpc.c: send poweroff
   * skel: fix qt5 icon theme and dialogs for KDE
   * skel: prevent autostarting of waybar, blueman, pasystray, lxpolkit in
     non-wlroots DEs
   * skel: don't set QT_QPA_PLATFORMTHEME as it interferes with KDE
   * skel: set adwaita cursor theme for gtk apps in sway and wayfire

1.33 (2023-10-16)
=================

   * bin/reform-check: support skipping the first X bytes of flash.bin when
     comparing
   * add multiplatform support in sysimage-v4
   * move remaining scripts from bash to posix shell
   * sbin/reform-setup-encrypted-nvme: compute swap space depending on total
     memory
   * update to latest u-boot 2023-10-10 on all platforms
   * set cma=512M on imx and cma=512M@3G on ls1028a

1.32 (2023-10-10)
=================

   * do not restart reform-sleep.service on upgrades
   * sbin/reform-standby: make 'Device or resource busy' and 'No such device'
     non-fatal
   * run wrap-and-sort -astb
   * update to u-boot 2023-10-10
   * sbin/reform-flash-uboot: skip checks on disks without a partition table
   * install /usr/share/u-boot-menu/conf.d/reform.conf respecting ${bootargs}

1.31 (2023-10-04)
=================

   * bin/reform-check:
      - warn about /etc/flash-kernel/preboot.d/00reform2_preboot and
        /etc/flash-kernel/ubootenv.d/00reform2_ubootenv
      - allow /etc/flash-kernel/machine to not exist on platforms other than
        imx8mq
      - update default content of /etc/default/flash-kernel
      - update list of packages from sysimage-v4
      - make checking for latest u-boot platform dependant
      - warn about missing non-free-firmware on a311d
   * flash-kernel/ubootenv.d/00reform2_ubootenv:
      - respect ${bootargs} again
      - put console=tty1 at the end of the cmdline to make sure that luks
        passphrase prompt shows up there and not on the serial line
   * stop installing /usr/share/u-boot-menu/conf.d/reform.conf as it does not
     allow for platform specific settings -- use /etc/default/u-boot instead

1.30 (2023-09-28)
=================

   * reform-hw-setup: a311d: reload ethernet modules to complete phy reset
   * Update 00reform2_ubootenv
   * reform-tray: add missing dependencies, change reboot/poweroff to systemctl
   * flash-kernel/uboot: add missing reform board variant

1.29 (2023-09-24)
=================

   * flash-kernel/ubootenv.d/00reform2_ubootenv: set bootargs for each platform

1.28 (2023-09-23)
=================

   * initramfs-tools/hooks/reform: also support 'MNT Reform 2 HDMI'
   * flash-kernel: first stab at setting cpu specific kernel command lines in
     ubootenv
   * initramfs-tools: add missing modules for ls1028a display
   * reform-hw-setup: add LS1028A usb workaround and default scaling_governor

1.27 (2023-09-21)
=================

   * copy /boot/ls1028a-mhdpfw.bin into initramfs
   * sbin/reform-flash-rescue: check all partitions of /dev/mmcblk0
   * let reform-tools activate update-initramfs and flash-kernel triggers
   * sbin/reform-migrate: check whether partition for /boot is mounted before
     proceeding

1.26 (2023-09-11)
=================

   * sbin/reform-flash-rescue: change disk labels after dd

1.25 (2023-07-12)
=================

   * updates for sysimage-v4
      - switch default sway terminal to foot and include a nice foot config
      - sway: modernize config, switch to wofi
      - update reform-help
      - update waybar config
      - add wallpaper
      - add default wayfire.ini
      - update reform-flash-rescue's image download URL
      - update reform-desktop-help content
      - skel: add configs for gtk3, qt5ct, dunst
   * let imx8mq specific tools exit early on the wrong platform
   * add a311d support to reform-hw-setup
   * install /usr/share/initramfs-tools/modules.d/reform.conf replacing
     /etc/initramfs-tools/modules
   * new script: reform-synaptic
   * add dependency on parted needed by reform-flash-uboot
   * add Protected: yes to make it really hard to remove
   * add --help option to reform-flash-uboot

1.24 (2023-07-11)
=================

   * sbin/reform-flash-uboot: adjust checksum for u-boot 2023-07-04

1.23 (2023-06-13)
=================

   * reform2-lpc-dkms: add missing header for linux 6.4

1.22 (2023-07-04)
=================

   * bump uboot version to 2023-07-04

1.21 (2023-05-30)
=================

   * reform-check: check for old /etc/flash-kernel/preboot.d/00reform2_preboot
     overwriting /usr/share/flash-kernel/preboot.d/00reform2_preboot

1.20 (2023-04-08)
=================

   * add reform-flash-uboot

1.19 (2023-02-18)
=================

   * reform2-lpc-dkms.dkms: AUTOINSTALL="yes"

1.18 (2023-01-27)
=================

   * install /usr/share/u-boot-menu/conf.d/reform.conf

1.17 (2023-01-27)
=================

   * bump uboot version to 2023-01-25

1.16 (2022-12-18)
=================

   * add reform-check utility
   * etc/profile.d/reform.sh: remove call to unicode_start as its functionality
     is provided automatically by console-setup
   * etc/motd-rescue: adjust message to not specifically talk about eMMC, SDcard
     or NVMe anymore
   * etc/motd: use reform-tools date in message
   * etc/profile.d/reform.sh: export MOZ_ENABLE_WAYLAND and
     _JAVA_AWT_WM_NONREPARENTING
   * sbin/reform-flash-rescue: check all mmcblk0 partitions in a loop before
     proceeding
   * sbin/reform-migrate: expand usage message
   * sbin/reform-setup-encrypted-nvme: mount to tmpdir instead of /mnt
   * sbin/reform-migrate: mount to tmpdir instead of /mnt

1.15 (2022-12-03)
=================

   * rework reform-boot-config
      - support all combinations of:
         * new root partition is already monted as /
         * new root partition lives elsewhere
         * new /boot partition is the same as the already configured one
         * new /boot is already mounted
         * new /boot device doesn't exist
      - error out if anything is still mounted that shouldn't be mounted
      - only mount when necessary
      - only use chroot if new root device is not /
      - use traps to always unmount everything and clean tmpfiles upon
        failure
      - consult /etc/fstab of new root for location of old /boot
      - copy over contents of old /boot to new one if necessary

1.14 (2022-11-06)
=================

   * Disable the raid456 kernel module to prevent losing wm8960audio

1.13 (2022-11-06)
=================

   * install flash-kernel boot scripts to /usr/share instead of /etc
   * flash-kernel/preboot.d/00reform2_preboot: even if ${fdtfile} is set, use
     dtb-${fk_kvers} if it exists

1.12 (2022-10-12)
=================

   * install /usr/share/initramfs-tools/scripts/init-top/reform as a temporary
     workaround to mxsfb producing a blank screen on roughly 50% of boots

1.11 (2022-09-13)
=================

   * reform-{boot-config,setup-encrypted-nvme}: add more cleanup code to exit
     traps and be verbose about script failure
   * reform-boot-config: allow messing up the currently running system by using
     an interactive prompt

1.10 (2022-09-12)
=================

   * add reform2-lpc-dkms package with the reform2_lpc module


1.9 (2022-09-07)
================

   * install /usr/share/X11/xorg.conf.d/10-reform-etnaviv.conf

1.8 (2022-07-09)
================

   * etc/skel/.config/mpv/mpv.conf: set maximum resolution of ytdl to 1080p
   * sway: fix KDE cursor appearing in sway
   * sbin/reform-{flash-rescue,boot-config,migrate,display-config,
     setup-encrypted-nvme}: check if devices are in use before proceeding
   * sbin/reform-boot-config: allow passing the device name explicitly
   * sbin/reform-migrate: add --emmc option
   * sbin/reform-{boot-config,display-config,migrate}: inform about the DIP
     switch if --emmc option is used
   * sbin/reform-setup-encrypted-nvme: replicate what reform-migrate does to
     avoid modifying the currently running system and ask whether /boot should
     be on emmc or sd-card
   * sbin/reform-{boot,display}-config: output meaningful error if
     /dev/mmcblk0p1 is missing
   * sbin/reform-{boot,display}-config: inform about the DIP switch always
   * sbin/reform-{display-config,migrate,setup-encrypted-nvme}: umount in a
     shell trap

1.7 (2022-06-17)
================

   * sbin/reform-flash-rescue: download from main branch instead of old sysimage-v3

1.6 (2022-06-08)
================

   * add etc/skel/.local/bin/kde for KDE Plasma desktop with wayland

1.5 (2022-04-22)
================

   * sbin/reform-boot-config: add option to boot with rootfs on eMMC and
     document ROOTPART envvar
   * sbin/reform-flash-rescue: extend from one-liner to interactive flasher and
     downloader to eMMC

1.4 (2022-03-30)
================

   * sbin/reform-migrate: reuse reform-boot-config to reduce code duplication
   * sbin/reform-setup-encrypted-nvme: use same crypttab options as
     debian-installer
   * sbin/reform-setup-encrypted-nvme: offer to run write resume, crypttab and
     fstab and run reform-migrate
   * sbin/reform-boot-config,reform-migrate: do not overwrite swap settings
   * sbin/reform-display-config: use findmnt instead of grepping mount output
   * sbin/reform-boot-config: allow operating on rootfs other than the currently
     running one
   * sbin/reform-migrate: use here-document to prevent globbing and word
     splitting of $TARGET
   * sbin/reform-migrate: prefixing with x is no longer necessary since bash 2.0
     released in 1996
   * sbin/reform-migrate: abort early if something is mounted on /mnt already
   * sbin/reform-migrate: add instructions how to partition NVMe

1.3 (2022-03-30)
================

   * waybar/config: pass arguments to reform-compstat
   * remove reform-connman-gtk

1.2 (2022-02-20)
================

   * rename etc/motd -> etc/motd-full to avoid conflict with existing /etc/motd
   * reform-hw-setup: some bits got lost when moving from reform-system-image to
     reform-tools
   * choose our pulse profile set when udev finds wm8960audio
   * enable systemd services after installation
   * fixup scripts to work with sysimage-v3

1.1 (2022-02-20)
================

   * Initial release for sysimage-v3
