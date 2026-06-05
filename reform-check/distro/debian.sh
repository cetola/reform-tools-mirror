# SPDX-License-Identifier: GPL-3.0+
# shellcheck shell=sh
# shellcheck disable=SC2016

# Checked by the Makefile test target after sourcing this file to verify the
# backend interface is complete.
# shellcheck disable=SC2034
BACKEND_MAIN_FLOW_READY=yes

backend_name() { printf '%s\n' debian; }

backend_validate_options() {
  if [ -n "${MIRROR:-}" ] && [ "$MIRROR" != "mntre.com" ] && [ "$MIRROR" != "reform.debian.net" ]; then
    echo "E: invalid value for --mirror: $MIRROR -- only mntre.com and reform.debian.net are supported" >&2
    return 1
  fi
}

backend_preflight_checks() {
  if [ -z "${MIRROR:-}" ] && grep --silent '^URIs: https://reform.debian.net/debian/\?$' /etc/apt/sources.list.d/reform*.sources 2>/dev/null; then
    echo "I: reform.debian.net is configured as a mirror in /etc/apt/sources.list.d" >&2
    echo "I: Assuming reform.debian.net stable mirror for this script" >&2
    echo "I: If this is incorrect, you can set the mirror to its default by re-running this script with --mirror=mntre.com"
    MIRROR="reform.debian.net"
  fi

  if [ -z "${MIRROR:-}" ]; then
    MIRROR="mntre.com"
  fi

  if [ -z "$(apt-get indextargets)" ] && [ "${OFFLINE:-}" != "yes" ]; then
    echo "E: reform-check needs a populated apt cache for some of its operation." >&2
    echo "E: re-run with --offline (disabling some checks) or choose to run 'apt update' now." >&2
    printf "Should reform-check run 'apt update' for you? [y/N] " >&2
    read -r response
    if [ "$response" != "y" ]; then
      echo "Exiting."
      return 1
    fi
    apt-get update --error-on=any
  fi
}

backend_pkg_installed() {
  dpkg-query --showformat '${db:Status-Status}\n' --show "$1" 2>/dev/null | grep -q '^installed$'
}

backend_pkg_version() {
  dpkg-query --show --showformat '${Version}' "$1"
}

backend_pkg_verify() {
  dpkg --verify "$1"
}

backend_kernel_pkg_name() {
  printf '%s\n' linux-image-mnt-reform-arm64
}

backend_kernel_headers_pkg_name() {
  printf '%s\n' linux-headers-mnt-reform-arm64
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
  backend_pkg_installed "linux-image-$(uname -r)"
}

backend_running_kernel_headers_pkg_installed() {
  backend_pkg_installed "linux-headers-$(uname -r)"
}

backend_running_kernel_headers_pkg_name() {
  printf '%s\n' "linux-headers-$(uname -r)"
}

backend_running_kernel_matches_main_pkg() {
  [ -n "${PKG_KERNEL_MAIN:-}" ] \
    && backend_pkg_installed "$PKG_KERNEL_MAIN" \
    && uname --kernel-version | grep --quiet --fixed-strings " $(backend_pkg_version "$PKG_KERNEL_MAIN") "
}

backend_kernel_artifact_checks() {
  for file in /boot/initrd.img-*-reform2-arm64 /boot/vmlinuz-*-reform2-arm64; do
    if [ ! -e "$file" ]; then
      continue
    fi
    echo "I: file from deprecated kernel flavour found: $file" >&2
  done

  for file in /boot/initrd.img-*-mnt-reform-arm64 /boot/vmlinuz-*-mnt-reform-arm64; do
    if [ ! -e "$file" ]; then
      continue
    fi
    suffix="${file##/boot/}"
    suffix="${suffix##initrd.img-}"
    suffix="${suffix##vmlinuz-}"
    if ! backend_pkg_installed "linux-image-$suffix"; then
      echo "I: $file does not belong to any installed kernel package" >&2
    fi
  done
}

backend_deprecated_kernel_pkg_checks() {
  if backend_pkg_installed "linux-image-arm64"; then
    echo "W: The deprecated meta-package linux-image-arm64 is installed." >&2
    if [ -n "$PKG_KERNEL_MAIN" ] && backend_pkg_installed "$PKG_KERNEL_MAIN"; then
      echo "W: Since the new meta-package $PKG_KERNEL_MAIN is installed," >&2
      echo "W: the old meta-package linux-image-arm64 should be removed." >&2
    else
      echo "W: Please install the new meta-package ${PKG_KERNEL_MAIN:-linux-image-mnt-reform-arm64}" >&2
      echo "W: and remove the old meta-package linux-image-arm64." >&2
    fi
    if ! backend_pkg_version "linux-image-arm64" | grep --quiet reform; then
      echo "E: the currently installed package linux-image-arm64 does not come with Reform patches" >&2
    fi
  fi
}

backend_skel_profile_checks() {
  if [ ! -e /etc/skel/.profile ]; then
    echo "E: /etc/skel/.profile doesn't exist" >&2
    echo "E: install the package bash to create it" >&2
  else
    # shellcheck disable=SC2016
    skelprofilecontent='if [ "$(whoami)" = "root" ]; then reform-help --root; elif [ -z "$WAYLAND_DISPLAY" ]; then reform-help; fi'
    if [ "$(tail -1 /etc/skel/.profile)" != "$skelprofilecontent" ]; then
      echo "E: unexpected last line in /etc/skel/.profile, should be:" >&2
      echo "$skelprofilecontent" >&2
    fi
  fi
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
  HAVE_FLASH_KERNEL=no
  if backend_pkg_installed "flash-kernel"; then
    HAVE_FLASH_KERNEL=yes
  fi
  HAVE_U_BOOT_MENU=no
  if backend_pkg_installed "u-boot-menu"; then
    HAVE_U_BOOT_MENU=yes
  fi

  case "$HAVE_BOOTSCR$HAVE_EXTLINUX" in
    nono)
      echo "W: neither /boot/boot.scr nor /boot/extlinux/extlinux.conf exists" >&2
      case "$HAVE_FLASH_KERNEL$HAVE_U_BOOT_MENU" in
        nono) echo "E: neither flash-kernel nor u-boot-menu are installed" >&2 ;;
        noyes) echo "E: /boot/extlinux/extlinux.conf should exist because u-boot-menu is installed" >&2 ;;
        yesno) echo "E: /boot/boot.scr should exist because flash-kernel is installed" >&2 ;;
        yesyes) echo "E: both files should exist as both flash-kernel and u-boot-menu are installed" >&2 ;;
      esac
      printf '%s\n' unknown
      ;;
    noyes)
      echo "I: probably booting via /boot/extlinux/extlinux.conf (/boot/boot.scr does not exist)" >&2
      if [ "$HAVE_U_BOOT_MENU" = "no" ]; then
        echo "W: /boot/extlinux/extlinux.conf is not managed by u-boot-menu (not installed)" >&2
      fi
      printf '%s\n' extlinux
      ;;
    yesno)
      echo "I: probably booting via /boot/boot.scr (/boot/extlinux/extlinux.conf does not exist)" >&2
      if [ "$HAVE_FLASH_KERNEL" = "no" ]; then
        echo "W: /boot/boot.scr is not managed by flash-kernel (not installed)" >&2
      fi
      printf '%s\n' bootscr
      ;;
    yesyes)
      echo "I: probably booting via /boot/extlinux/extlinux.conf (/boot/boot.scr also exists)" >&2
      if [ "$HAVE_U_BOOT_MENU" = "no" ]; then
        echo "W: /boot/extlinux/extlinux.conf is not managed by u-boot-menu (not installed)" >&2
      fi
      if [ "$HAVE_FLASH_KERNEL" = "no" ]; then
        echo "W: /boot/boot.scr is not managed by flash-kernel (not installed)" >&2
      fi
      printf '%s\n' extlinux
      ;;
  esac

  if [ -e "/etc/flash-kernel/machine" ] && [ -e "/proc/device-tree/model" ] && [ "$(cat /proc/device-tree/model)" != "$(cat /etc/flash-kernel/machine)" ]; then
    echo "E: your currently loaded dtb is not the one referenced by flash-kernel" >&2
    echo "E: contents of /proc/device-tree/model: $(cat /proc/device-tree/model)" >&2
    echo "E: contents of /etc/flash-kernel/machine: $(cat /etc/flash-kernel/machine)" >&2
  fi

  if [ ! -e /etc/flash-kernel/machine ]; then
    # /etc/flash-kernel/machine not existing is only a potential problem
    # on imx8mq
    case "$(cat /proc/device-tree/model)" in
      "MNT Reform 2" | "MNT Reform 2 HDMI")
        echo "E: /etc/flash-kernel/machine doesn't exist" >&2
        echo "E: It should contain either 'MNT Reform 2' (for single display) or 'MNT Reform 2 HDMI' (for dual display)." >&2
        echo "E: You can run reform-display-config as root to create a working version." >&2
        ;;
    esac
  else
    # we can check only /etc/flash-kernel/machine and not /proc/device-tree/model
    # as well because above we check (and print) if their contents diff
    case "$(cat /etc/flash-kernel/machine)" in
      "MNT Reform 2") : ;;
      "MNT Reform 2 HDMI") : ;;
      "MNT Pocket Reform with BPI-CM4 Module.conf" | \
        "MNT Pocket Reform with i.MX8MP Module.conf" | \
        "MNT Pocket Reform with RCORE RK3588 Module.conf" | \
        "MNT Reform 2 with BPI-CM4 Module.conf" | \
        "MNT Reform 2 with i.MX8MP Module.conf" | \
        "MNT Reform 2 with LS1028A Module.conf" | \
        "MNT Reform 2 with QUASAR 8550 Module" | \
        "MNT Reform 2 with RCORE-DSI RK3588 Module.conf" | \
        "MNT Reform 2 with RCORE RK3588 Module.conf" | \
        "MNT Reform Next with RCORE RK3588 Module.conf")
        echo "W: /etc/flash-kernel/machine is not needed on $(cat /etc/flash-kernel/machine). Consider removing it." >&2
        ;;
      *) echo "E: unexpected content in /etc/flash-kernel/machine" >&2 ;;
    esac
  fi

  flashkerneldefaultcontent="LINUX_KERNEL_CMDLINE=\"\"\nLINUX_KERNEL_CMDLINE_DEFAULTS=\"\"\n"
  if [ ! -e /etc/default/flash-kernel ]; then
    echo "E: /etc/default/flash-kernel doesn't exist" >&2
    echo "E: /etc/default/flash-kernel should contain the following lines:" >&2
    printf '%b' "$flashkerneldefaultcontent" >&2
  elif ! printf '%b' "$flashkerneldefaultcontent" | cmp --quiet - /etc/default/flash-kernel; then
    echo "W: unexpected content in /etc/default/flash-kernel:" >&2
    printf '%b' "$flashkerneldefaultcontent" | diff -u - /etc/default/flash-kernel || true
  fi

  if [ -e /etc/flash-kernel/preboot.d/00reform2_preboot ]; then
    if printf "# place here any u-boot commands to be executed before boot\n" | cmp --quiet - /etc/flash-kernel/preboot.d/00reform2_preboot; then
      echo "E: /etc/flash-kernel/preboot.d/00reform2_preboot contains default content overriding /usr/share/flash-kernel/preboot.d/00reform2_preboot" >&2
      echo "E: consider deleting /etc/flash-kernel/preboot.d/00reform2_preboot in favour of /usr/share/flash-kernel/preboot.d/00reform2_preboot" >&2
    else
      echo "W: your custom /etc/flash-kernel/preboot.d/00reform2_preboot is overriding /usr/share/flash-kernel/preboot.d/00reform2_preboot" >&2
    fi
  fi

  if [ -e /etc/flash-kernel/ubootenv.d/00reform2_ubootenv ]; then
    if printf '# setenv bootpart "1"\n# setenv prefix "/"\n# setenv kernel_addr_r "0x40480000"\n# setenv fdt_addr_r "0x50000000"\n# setenv ramdisk_addr_r "0x51000000"\n' | cmp --quiet - /etc/flash-kernel/ubootenv.d/00reform2_ubootenv; then
      echo "E: /etc/flash-kernel/ubootenv.d/00reform2_ubootenv contains default content overriding /usr/share/flash-kernel/ubootenv.d/00reform2_ubootenv" >&2
      echo "E: consider deleting /etc/flash-kernel/ubootenv.d/00reform2_ubootenv in favour of /usr/share/flash-kernel/ubootenv.d/00reform2_ubootenv" >&2
    else
      echo "W: your custom /etc/flash-kernel/ubootenv.d/00reform2_ubootenv is overriding /usr/share/flash-kernel/ubootenv.d/00reform2_ubootenv"
    fi
  fi

  if command -v systemctl >/dev/null 2>&1 \
    && systemctl is-active --quiet greetd \
    && test -e /etc/greetd/config.toml \
    && grep --quiet '^ *command *= *["'"'"']/usr/bin/tuigreet *' /etc/greetd/config.toml; then
    if [ -e /etc/flash-kernel/ubootenv.d/00reform2_tuigreet_loglevel ]; then
      if ! echo 'setenv bootargs "${bootargs} loglevel=3"' | cmp --quiet - /etc/flash-kernel/ubootenv.d/00reform2_tuigreet_loglevel; then
        echo "I: /etc/flash-kernel/ubootenv.d/00reform2_tuigreet_loglevel exists but with the wrong content." >&2
        echo 'I: it should contain this: setenv bootargs "${bootargs} loglevel=3"'
      fi
    else
      echo 'I: your system is booting using greetd with tuigreet. You may want to put the following line into your /etc/flash-kernel/ubootenv.d/00reform2_tuigreet_loglevel: setenv bootargs "${bootargs} loglevel=3' >&2
      echo "I: Then run 'sudo flash-kernel' to regenerate your /boot/boot.scr. This will do away with your kernel overwriting your login prompt" >&2
    fi
  elif [ -e /etc/flash-kernel/ubootenv.d/00reform2_tuigreet_loglevel ] && echo 'setenv bootargs "${bootargs} loglevel=3"' | cmp --quiet - /etc/flash-kernel/ubootenv.d/00reform2_tuigreet_loglevel; then
    echo "W: you do not seem to boot using tuigreet but the following file sets your loglevel to 3: /etc/flash-kernel/ubootenv.d/00reform2_tuigreet_loglevel -- you may consider removing it and re-running 'sudo flash-kernel'" >&2
  fi

  case "$(cat /proc/device-tree/model)" in
    "MNT Reform 2" | "MNT Reform 2 HDMI")
      if [ ! -e /etc/u-boot-menu/conf.d ]; then
        echo "W: /etc/u-boot-menu/conf.d does not exist -- create it by running reform-display-config" >&2
      fi
      if [ ! -e /etc/u-boot-menu/conf.d/reform.conf ]; then
        echo "W: /etc/u-boot-menu/conf.d/reform.conf does not exist -- create it by running reform-display-config" >&2
      else
        case "$(cat /proc/device-tree/model)" in
          "MNT Reform 2")
            reformconfcontent="# the content of this file is auto-generated by reform-display-config\nU_BOOT_FDT=/freescale/imx8mq-mnt-reform2.dtb\n"
            ;;
          "MNT Reform 2 HDMI")
            reformconfcontent="# the content of this file is auto-generated by reform-display-config\nU_BOOT_FDT=/freescale/imx8mq-mnt-reform2-hdmi.dtb\n"
            ;;
        esac
        if ! printf '%b' "$reformconfcontent" | cmp --quiet - /etc/u-boot-menu/conf.d/reform.conf; then
          echo "W: unexpected content in /etc/u-boot-menu/conf.d/reform.conf:" >&2
          printf '%b' "$reformconfcontent" | diff -u - /etc/u-boot-menu/conf.d/reform.conf || true
          echo "W: re-run reform-display-config for the corrent content" >&2
        fi
      fi
      ;;
  esac
}

backend_modules_check() {
  # The reform-debian-packages pipeline makes sure that these modules exist.
  check_expected_modules \
    updates/dkms/reform2_lpc.ko \
    kernel/drivers/gpu/drm/imx/cdns/cdns_mhdp_imx.ko \
    kernel/drivers/gpu/drm/bridge/cadence/cdns-mhdp8546.ko \
    kernel/drivers/net/mdio/mdio-mux-meson-g12a.ko
}

backend_repo_checks() {
  case "$MIRROR" in
    mntre.com) aptprefcontent="Package: *\nPin: release n=reform, l=reform\nPin-Priority: 990\n" ;;
    reform.debian.net) aptprefcontent="Package: *\nPin: origin \"reform.debian.net\"\nPin-Priority: 999\n" ;;
    *)
      echo "invalid mirror: $MIRROR" >&2
      return 1
      ;;
  esac

  if [ ! -e /etc/apt/preferences.d/reform.pref ]; then
    echo "E: /etc/apt/preferences.d/reform.pref doesn't exist" >&2
    echo "E: you should not install packages on this system unless you know what you are doing" >&2
    echo "E: /etc/apt/preferences.d/reform.pref should contain the following lines:" >&2
    printf '%b' "$aptprefcontent" >&2
  elif ! printf '%b' "$aptprefcontent" | cmp --quiet - /etc/apt/preferences.d/reform.pref; then
    echo "W: unexpected content in /etc/apt/preferences.d/reform.pref:" >&2
    printf '%b' "$aptprefcontent" | diff -u - /etc/apt/preferences.d/reform.pref || true
    echo "W: you should not install packages on this system unless you know what you are doing" >&2
  fi

  for META in reform-desktop-full reform-desktop-minimal; do
    # if meta package is installed, we don't need to check its Depends
    RELATIONS="Recommends Suggests"
    if ! backend_pkg_installed "$META"; then
      echo "I: MNT Reform Desktop meta-package is not installed: $META" >&2
      RELATIONS="Depends $RELATIONS"
    fi
    # do not give conflicting info about ezurio-qcacld-2.0-dkms or reform-qcacld2
    for REL in $RELATIONS; do
      OIFS=$IFS
      IFS=','
      for PKG in $(apt-cache show --no-all-versions $META | sed -ne 's/^'"$REL"': \(.*\)/\1/p'); do
        PKG="${PKG## }"
        if ! backend_pkg_installed "$PKG"; then
          # Avoid contradictory guidance for the mutually exclusive i.MX8MP wifi packages.
          if [ "$PKG" = "ezurio-qcacld-2.0-dkms" ] || [ "$PKG" = "reform-qcacld2" ]; then
            case "${MODEL:-$(cat /proc/device-tree/model)}" in
              "MNT Pocket Reform with i.MX8MP Module" | "MNT Reform 2 with i.MX8MP Module")
                continue
                ;;
            esac
          fi
          echo "I: $REL of $META is not installed: $PKG" >&2
        fi
      done
      IFS=$OIFS
    done
  done

  if ! backend_pkg_installed "reform-branding"; then
    case "$MIRROR" in
      mntre.com) echo "W: reform-branding (non-free) is not installed" >&2 ;;
      reform.debian.net) echo "I: reform-branding (non-free) is not installed" >&2 ;;
      *)
        echo "invalid mirror: $MIRROR" >&2
        return 1
        ;;
    esac
  fi

  if [ "$MIRROR" = "reform.debian.net" ] && backend_pkg_installed "reform-qcacld2"; then
    echo "E: Having reform-qcacld2 on a system configured to use packages for Debian stable from reform.debian.net will attempt installing the kernel package from the MNT repositories in the kernel postinstallation hook" >&2
    echo "W: Consider replacing reform-qcacld2 with ezurio-qcacld-2.0-dkms for Debian stable and stable-backports kernels from reform.debian.net" >&2
  fi

  if [ "$MIRROR" = "mntre.com" ]; then
    if backend_pkg_installed "reform-qcacld2"; then
      case "$(cat /proc/device-tree/model)" in
        "MNT Pocket Reform with i.MX8MP Module" | "MNT Reform 2 with i.MX8MP Module")
          :
          ;;
        *)
          echo "I: the reform-qcacld2 package is only required for wifi on the Pocket Reform with i.MX8MP, you can safely remove it unless you plan to go back to the imx8m+" >&2
          ;;
      esac
    else
      case "$(cat /proc/device-tree/model)" in
        "MNT Pocket Reform with i.MX8MP Module.conf")
          echo "E: For the official MNT Debian on Pocket Reform with i.MX8MP, the reform-qcacld2 package needs to be installed for working wifi" >&2
          ;;
        *)
          :
          ;;
      esac
    fi
  fi

  for f in mntre reform_bookworm reform_bookworm-backports; do
    file="/etc/apt/sources.list.d/${f}.sources"
    if [ ! -e "$file" ]; then
      continue
    fi
    if [ "$(stat -c %a "$file")" != 666 ]; then
      continue
    fi
    echo "E: $file has insecure permissions 0666, consider a chmod 644" >&2
  done
}

backend_repo_online_checks() {
  case "$MIRROR" in
    mntre.com)
      if [ "$(apt-get indextargets 'Created-By: Packages' 'Repo-URI: https://mntre.com/reform-debian-repo/' --format '$(RELEASE)' | sort -u)" != "reform" ]; then
        echo "E: the reform repository is not known to apt" >&2
        echo "E: add the following line to your /etc/apt/sources.list to fix this" >&2
        echo "deb [arch=arm64 trusted=yes] https://mntre.com/reform-debian-repo reform main" >&2
      fi
      if [ -n "$(apt-get indextargets 'Created-By: Packages' 'Repo-URI: https://reform.debian.net/debian/' --format '$(RELEASE)')" ]; then
        echo "E: you have the reform.debian.net repository enabled while also using the mntre.com mirror. Consider choosing one or the other but not both." >&2
      fi
      ;;
    reform.debian.net)
      release="$(apt-get indextargets 'Created-By: Packages' 'Repo-URI: https://reform.debian.net/debian/' --format '$(RELEASE)')"
      case "$release" in
        "") echo "E: unable to obtain release name for reform.debian.net repository" >&2 ;;
        bookworm*) : ;;
        trixie*) : ;;
        *) echo "E: unknown reform.debian.net repository release name: $release" >&2 ;;
      esac
      if [ "$(apt-get indextargets 'Created-By: Packages' 'Repo-URI: https://mntre.com/reform-debian-repo/' --format '$(RELEASE)')" = reform ]; then
        echo "E: you have the mntre.com repository enabled while also using the reform.debian.net mirror. Consider choosing one or the other but not both." >&2
      fi
      ;;
    *)
      echo "invalid mirror: $MIRROR" >&2
      return 1
      ;;
  esac

  if [ "$NEED_NONFREE" = true ] && ! apt-get indextargets 'Created-By: Packages' 'Origin: Debian' 'Repo-URI: http://deb.debian.org/debian/' --format '$(COMPONENT)' | sort -u | grep --silent non-free-firmware; then
    echo "W: you do not have non-free-firmware enabled" >&2
    echo "W: this is required to install non-free firmware blobs that are required for platforms like Banana Pi A311D" >&2
    echo "W: you can add a line like this to your /etc/apt/sources.list:" >&2
    echo "deb http://deb.debian.org/debian unstable non-free-firmware" >&2
  fi

  if backend_pkg_installed "linux-image-arm64"; then
    case "$MIRROR" in
      mntre.com)
        if ! apt-cache policy linux-image-arm64 | grep --quiet mntre.com; then
          echo "E: the linux-image-arm64 package cannot come from the MNT repos" >&2
        fi
        ;;
      reform.debian.net)
        if ! apt-cache policy linux-image-arm64 | grep --quiet /reform.debian.net/; then
          echo "E: the linux-image-arm64 package cannot come from the reform.debian.net repos" >&2
        fi
        ;;
      *)
        echo "invalid mirror: $MIRROR" >&2
        return 1
        ;;
    esac
  fi

  if [ -n "${PKG_KERNEL_MAIN:-}" ] && backend_pkg_installed "$PKG_KERNEL_MAIN"; then
    case "$MIRROR" in
      mntre.com)
        if ! apt-cache policy "$PKG_KERNEL_MAIN" | grep --quiet mntre.com; then
          echo "E: the $PKG_KERNEL_MAIN package cannot come from the MNT repos" >&2
        fi
        ;;
      reform.debian.net)
        if ! apt-cache policy "$PKG_KERNEL_MAIN" | grep --quiet /reform.debian.net/; then
          echo "E: the $PKG_KERNEL_MAIN package cannot come from the reform.debian.net repos" >&2
        fi
        ;;
      *)
        echo "invalid mirror: $MIRROR" >&2
        return 1
        ;;
    esac
  fi
}

backend_initramfs_checks() {
  initramfstoolsmodulescontent="pwm_imx27\nnwl-dsi\nti-sn65dsi86\nimx-dcss\npanel-edp\nmux-mmio\nmxsfb\nusbhid\nimx8mq-interconnect\n"
  if [ -e /etc/initramfs-tools/modules ] && printf '%b' "$initramfstoolsmodulescontent" | cmp --quiet - /etc/initramfs-tools/modules; then
    echo "W: /etc/initramfs-tools/modules contains default content superseded by /usr/share/initramfs-tools/modules.d/reform.conf" >&2
    echo "W: consider restoring /etc/initramfs-tools/modules to its original state by running:" >&2
    echo "sed '/^###/d' /usr/share/initramfs-tools/modules | sudo tee /etc/initramfs-tools/modules" >&2
  fi

  if [ -e /boot/boot.scr ]; then
    if ! grep --quiet "setenv fk_kvers '$(uname -r)'" /boot/boot.scr; then
      echo "E: /boot/boot.scr doesn't reference the currently running kernel" >&2
    fi
    if command -v shellcheck >/dev/null; then
      tail -n +2 /boot/boot.scr | shellcheck --shell sh --exclude=SC2086,SC2154 -
    else
      echo "I: Install the package shellcheck for checking /boot/boot.scr for problems" >&2
    fi
  else
    echo "E: /boot/boot.scr doesn't exist" >&2
    echo "E: run 'sudo flash-kernel' to create it" >&2
  fi

  if [ ! -e "/boot/vmlinuz-$(uname -r)" ]; then
    echo "E: no vmlinuz in /boot for the currently running kernel" >&2
  fi

  if [ ! -e "/boot/initrd.img-$(uname -r)" ]; then
    echo "E: no initrd.img in /boot for the currently running kernel" >&2
  fi

  if [ ! -e "/boot/dtb-$(uname -r)" ]; then
    echo "E: no dtb-* symlink for the currently running kernel" >&2
    echo "E: run 'sudo flash-kernel' to create it" >&2
  elif [ ! -e "/boot/dtbs/$(uname -r)/$DTBPATH" ]; then
    echo "E: device tree file $DTBPATH is missing from /boot/dtbs" >&2
  elif [ ! -s "/boot/dtbs/$(uname -r)/$DTBPATH" ]; then
    echo "E: device tree file /boot/dtbs/$DTBPATH is empty (zero bytes)" >&2
  elif [ "$(readlink "/boot/dtb-$(uname -r)")" != "dtbs/$(uname -r)/$DTBPATH" ]; then
    echo "E: /boot/dtb-$(uname -r) symlink does not reference dtbs/$(uname -r)/$DTBPATH but: $(readlink "/boot/dtb-$(uname -r)")" >&2
  fi
}

backend_image_hygiene_checks() {
  for f in . firefox firefox/syspref.js motd-full motd-rescue profile.d profile.d/reform.sh profile.d/reform-setup.sh; do
    file="/etc/$f"
    if [ ! -e "$file" ]; then
      continue
    fi
    # only process world-writable files
    if [ -z "$(find "$file" -maxdepth 0 \( \! -type l \) -perm /o+w)" ]; then
      continue
    fi
    echo "E: $file has insecure permissions (it is world-writable)" >&2
  done
  if [ -e /etc/profile.d/reform-setup.sh ]; then
    # https://source.mnt.re/reform/mnt-reform-setup-wizard/-/issues/23
    echo "W: reform-setup-wizard failed to clean up /etc/profile.d/reform-setup.sh. It can be safely removed." >&2
  fi
  if [ -e /etc/skel ] && [ "$(stat -c %a /etc/skel)" = "777" ]; then
    echo "E: /etc/skel has insecure permissions 0777, consider a chmod -R g-w,o-rw" >&2
  fi
  if [ -n "$(find /etc \( \! -type l \) -perm /o+w)" ]; then
    echo "E: the following files in /etc are world-writable, consider removing world-writable permissions:" >&2
    find /etc \( \! -type l \) -perm /o+w >&2
  fi
  if [ -n "$(find /root \( \! -type l \) -perm /o+w)" ]; then
    echo "E: the following files in /root are world-writable, consider removing world-writable permissions:" >&2
    find /root \( \! -type l \) -perm /o+w >&2
  fi
}

backend_empty_root_password_check() {
  if grep --quiet '^root::' /etc/shadow; then
    echo "E: root account has no password (maybe run passwd -l root)" >&2
  fi
}

backend_distro_specific_checks() {
  backend_deprecated_kernel_pkg_checks
  backend_empty_root_password_check
  backend_image_hygiene_checks
}
