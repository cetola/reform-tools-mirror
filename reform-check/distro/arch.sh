#!/bin/sh
# SPDX-License-Identifier: GPL-3.0+

BACKEND_MAIN_FLOW_READY=yes

backend_validate_options() {
  if [ -n "${MIRROR:-}" ] && [ "$MIRROR" != "archlinuxarm.org" ]; then
    echo "E: invalid value for --mirror: $MIRROR -- only archlinuxarm.org is supported for Arch" >&2
    return 1
  fi
}

backend_preflight_checks() {
  :
}

backend_pkg_installed() {
  pacman -Q "$1" >/dev/null 2>&1
}

backend_pkg_version() {
  pacman -Q "$1" | awk '{print $2}'
}

backend_pkg_verify() {
  pacman -Qkk "$1"
}

backend_kernel_pkg_name() {
  if pacman -Q linux-mnt-reform >/dev/null 2>&1; then
    printf '%s\n' linux-mnt-reform
  elif pacman -Q linux-mnt-reform-bin >/dev/null 2>&1; then
    printf '%s\n' linux-mnt-reform-bin
  else
    return 1
  fi
}

backend_kernel_headers_pkg_name() {
  if pacman -Q linux-mnt-reform-headers >/dev/null 2>&1; then
    printf '%s\n' linux-mnt-reform-headers
  elif pacman -Q linux-mnt-reform-bin-headers >/dev/null 2>&1; then
    printf '%s\n' linux-mnt-reform-bin-headers
  else
    return 1
  fi
}

backend_tools_pkg_name() {
  printf '%s\n' reform-tools
}

backend_fwupd_pkg_name() {
  printf '%s\n' fwupd
}

backend_jq_pkg_name() {
  printf '%s\n' jq
}

backend_mcu_tool_pkg_name() {
  printf '%s\n' reform-tools
}

backend_running_kernel_pkg_installed() {
  [ -n "${PKG_KERNEL_MAIN:-}" ] && backend_pkg_installed "$PKG_KERNEL_MAIN"
}

backend_running_kernel_headers_pkg_installed() {
  [ -n "${PKG_KERNEL_HEADERS:-}" ] && backend_pkg_installed "$PKG_KERNEL_HEADERS"
}

backend_running_kernel_matches_main_pkg() {
  if [ -z "${PKG_KERNEL_MAIN:-}" ] || ! backend_pkg_installed "$PKG_KERNEL_MAIN"; then
    return 1
  fi
  if ! have_cmd pacman; then
    return 1
  fi
  pacman -Qqo "/usr/lib/modules/$(uname -r)" 2>/dev/null | grep -qx "$PKG_KERNEL_MAIN"
}

backend_kernel_artifact_checks() {
  :
}

backend_skel_profile_checks() {
  :
}

backend_repo_checks() {
  :
}

backend_repo_online_checks() {
  :
}

backend_boot_tool_checks() {
  HAVE_BOOTSCR=no
  if test -e /boot/boot.scr; then
    HAVE_BOOTSCR=yes
  fi
  HAVE_EXTLINUX=no
  if test -e /boot/extlinux/extlinux.conf; then
    HAVE_EXTLINUX=yes
  fi

  case "$HAVE_BOOTSCR$HAVE_EXTLINUX" in
    nono)
      echo "W: neither /boot/boot.scr nor /boot/extlinux/extlinux.conf exists" >&2
      printf '%s\n' unknown
      ;;
    noyes)
      echo "I: probably booting via /boot/extlinux/extlinux.conf (/boot/boot.scr does not exist)" >&2
      printf '%s\n' extlinux
      ;;
    yesno)
      echo "I: probably booting via /boot/boot.scr (/boot/extlinux/extlinux.conf does not exist)" >&2
      printf '%s\n' bootscr
      ;;
    yesyes)
      echo "I: probably booting via /boot/extlinux/extlinux.conf (/boot/boot.scr also exists)" >&2
      printf '%s\n' extlinux
      ;;
  esac
}

backend_modules_check() {
  check_expected_modules \
    updates/dkms/reform2_lpc.ko \
    updates/dkms/wlan.ko
}

backend_initramfs_checks() {
  BOOT_METHOD="${1:-unknown}"
  HAVE_DRACUT=no
  if have_cmd dracut; then
    HAVE_DRACUT=yes
  fi
  HAVE_MKINITCPIO=no
  if have_cmd mkinitcpio; then
    HAVE_MKINITCPIO=yes
  fi

  case "$HAVE_DRACUT$HAVE_MKINITCPIO" in
    nono) echo "W: neither dracut nor mkinitcpio is installed; initramfs generation may be misconfigured" >&2 ;;
    yesyes) echo "I: both dracut and mkinitcpio are installed" >&2 ;;
  esac

  if [ ! -d "/usr/lib/modules/$(uname -r)" ]; then
    echo "E: /usr/lib/modules/$(uname -r) does not exist for the running kernel" >&2
  fi

  HAVE_INITRAMFS=no
  for f in /boot/initramfs* /boot/initrd*; do
    if [ -s "$f" ]; then
      HAVE_INITRAMFS=yes
      break
    fi
  done
  if [ "$HAVE_INITRAMFS" = "no" ]; then
    echo "E: no non-empty initramfs/initrd artifact found in /boot" >&2
  fi

  resolve_boot_path() {
    case "$1" in
      /*) printf '/boot%s\n' "$1" ;;
      *) printf '/boot/%s\n' "$1" ;;
    esac
  }

  if [ "${BOOT_METHOD:-unknown}" = "bootscr" ]; then
    if [ ! -e /boot/boot.scr ]; then
      echo "E: /boot/boot.scr doesn't exist" >&2
    else
      if ! grep --quiet "setenv fk_kvers '$(uname -r)'" /boot/boot.scr; then
        echo "E: /boot/boot.scr doesn't reference the currently running kernel" >&2
      fi
      if have_cmd shellcheck; then
        tail -n +2 /boot/boot.scr | shellcheck --shell sh --exclude=SC2086,SC2154 -
      else
        echo "I: Install shellcheck for checking /boot/boot.scr for problems" >&2
      fi
    fi

    HAVE_KERNEL_IMAGE=no
    if [ -e /boot/Image ] \
      || [ -e /boot/Image.gz ] \
      || [ -e /boot/vmlinuz ] \
      || [ -e "/boot/vmlinuz-$(uname -r)" ] \
      || [ -e "/boot/Image-$(uname -r)" ] \
      || [ -e "/boot/Image.gz-$(uname -r)" ] \
      || [ -e /boot/Image-linux-mnt-reform ] \
      || [ -e /boot/Image-linux-mnt-reform-bin ] \
      || [ -e /boot/Image.gz-linux-mnt-reform ] \
      || [ -e /boot/Image.gz-linux-mnt-reform-bin ] \
      || [ -e /boot/vmlinuz-linux-mnt-reform ] \
      || [ -e /boot/vmlinuz-linux-mnt-reform-bin ]; then
      HAVE_KERNEL_IMAGE=yes
    fi
    if [ "$HAVE_KERNEL_IMAGE" = "no" ]; then
      echo "E: no kernel image in /boot for the currently running kernel (checked /boot/Image, /boot/Image.gz, /boot/vmlinuz, release-suffixed names, and package-named Arch variants)" >&2
    fi

    if [ ! -e "/boot/initramfs-$(uname -r).img" ] && [ ! -e "/boot/initrd.img-$(uname -r)" ]; then
      echo "E: no initramfs/initrd in /boot for the currently running kernel" >&2
    fi

    if [ ! -e "/boot/dtb-$(uname -r)" ]; then
      echo "E: no dtb-* symlink for the currently running kernel" >&2
    elif [ ! -e "/boot/dtbs/$(uname -r)/$DTBPATH" ]; then
      echo "E: device tree file $DTBPATH is missing from /boot/dtbs" >&2
    elif [ ! -s "/boot/dtbs/$(uname -r)/$DTBPATH" ]; then
      echo "E: device tree file /boot/dtbs/$DTBPATH is empty (zero bytes)" >&2
    elif [ "$(readlink "/boot/dtb-$(uname -r)")" != "dtbs/$(uname -r)/$DTBPATH" ]; then
      echo "E: /boot/dtb-$(uname -r) symlink does not reference dtbs/$(uname -r)/$DTBPATH but: $(readlink "/boot/dtb-$(uname -r)")" >&2
    fi
  elif [ "${BOOT_METHOD:-unknown}" = "extlinux" ]; then
    EXTLINUX_DEFAULT="$(awk 'tolower($1)=="default" { print $2; exit }' /boot/extlinux/extlinux.conf)"
    EXTLINUX_LINUX="$(awk -v target_label="$EXTLINUX_DEFAULT" '
      BEGIN { current="" }
      tolower($1)=="label" { current=$2; next }
      (target_label=="" || current==target_label) && tolower($1)=="linux" { print $2; exit }
    ' /boot/extlinux/extlinux.conf)"
    EXTLINUX_INITRD="$(awk -v target_label="$EXTLINUX_DEFAULT" '
      BEGIN { current="" }
      tolower($1)=="label" { current=$2; next }
      (target_label=="" || current==target_label) && tolower($1)=="initrd" { print $2; exit }
    ' /boot/extlinux/extlinux.conf)"
    EXTLINUX_FDT="$(awk -v target_label="$EXTLINUX_DEFAULT" '
      BEGIN { current="" }
      tolower($1)=="label" { current=$2; next }
      (target_label=="" || current==target_label) && tolower($1)=="fdt" { print $2; exit }
    ' /boot/extlinux/extlinux.conf)"
    EXTLINUX_FDTDIR="$(awk -v target_label="$EXTLINUX_DEFAULT" '
      BEGIN { current="" }
      tolower($1)=="label" { current=$2; next }
      (target_label=="" || current==target_label) && tolower($1)=="fdtdir" { print $2; exit }
    ' /boot/extlinux/extlinux.conf)"

    if [ -z "$EXTLINUX_LINUX" ]; then
      echo "E: /boot/extlinux/extlinux.conf has no linux entry for the default label" >&2
    else
      EXTLINUX_LINUX_ABS="$(resolve_boot_path "$EXTLINUX_LINUX")"
      if [ ! -e "$EXTLINUX_LINUX_ABS" ]; then
        echo "E: extlinux kernel image does not exist: $EXTLINUX_LINUX_ABS" >&2
      fi
    fi

    if [ -z "$EXTLINUX_INITRD" ]; then
      echo "E: /boot/extlinux/extlinux.conf has no initrd entry for the default label" >&2
    else
      EXTLINUX_INITRD_ABS="$(resolve_boot_path "$EXTLINUX_INITRD")"
      if [ ! -e "$EXTLINUX_INITRD_ABS" ]; then
        echo "E: extlinux initramfs/initrd does not exist: $EXTLINUX_INITRD_ABS" >&2
      fi
    fi

    if [ -n "$EXTLINUX_FDT" ]; then
      EXTLINUX_FDT_ABS="$(resolve_boot_path "$EXTLINUX_FDT")"
      if [ ! -e "$EXTLINUX_FDT_ABS" ]; then
        echo "E: extlinux fdt does not exist: $EXTLINUX_FDT_ABS" >&2
      elif [ ! -s "$EXTLINUX_FDT_ABS" ]; then
        echo "E: extlinux fdt is empty (zero bytes): $EXTLINUX_FDT_ABS" >&2
      fi
    elif [ -n "$EXTLINUX_FDTDIR" ]; then
      EXTLINUX_DTB_ABS="$(resolve_boot_path "$EXTLINUX_FDTDIR/$DTBPATH")"
      if [ ! -e "$EXTLINUX_DTB_ABS" ]; then
        echo "E: extlinux fdtdir does not contain expected DTB $DTBPATH: $EXTLINUX_DTB_ABS" >&2
      elif [ ! -s "$EXTLINUX_DTB_ABS" ]; then
        echo "E: expected DTB from extlinux fdtdir is empty (zero bytes): $EXTLINUX_DTB_ABS" >&2
      fi
    else
      echo "E: /boot/extlinux/extlinux.conf has neither fdt nor fdtdir entry for the default label" >&2
    fi
  fi
}
