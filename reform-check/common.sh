# SPDX-License-Identifier: GPL-3.0+
# shellcheck shell=sh

have_cmd() {
  command -v "$1" >/dev/null 2>&1
}

# Returns 0 if version $1 is greater than or equal to version $2.
version_ge() {
  [ "$1" = "$2" ] || [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n1)" = "$1" ]
}

detect_backend_from_os_release() {
  if [ ! -r /etc/os-release ]; then
    echo "E: unable to detect backend because /etc/os-release is missing" >&2
    return 1
  fi

  # shellcheck disable=SC1091
  . /etc/os-release

  case "${ID:-}" in
    debian)
      printf '%s\n' debian
      return 0
      ;;
    arch | archlinux | archarm)
      printf '%s\n' arch
      return 0
      ;;
  esac

  case " ${ID_LIKE:-} " in
    *" debian "*)
      printf '%s\n' debian
      return 0
      ;;
    *" arch "*)
      printf '%s\n' arch
      return 0
      ;;
  esac

  echo "E: unsupported operating system in /etc/os-release: ID=${ID:-unknown} ID_LIKE=${ID_LIKE:-}" >&2
  return 1
}

read_nul_terminated_file() {
  awk 'BEGIN { RS = "\0" } NR == 1 { printf "%s", $0 }' "$1"
}

worddiff() {
  for word in $1; do
    case $2 in
      "$word" | "$word "* | *" $word" | *" $word "*) : ;;
      *) echo "$word" ;;
    esac
  done
}

check_expected_modules() {
  for mod in "$@"; do
    if [ ! -e "/usr/lib/modules/$(uname -r)/$mod" ] \
      && [ ! -e "/usr/lib/modules/$(uname -r)/${mod}.xz" ]; then
      echo "E: /usr/lib/modules/$(uname -r)/$mod{.xz} does not exist" >&2
    fi
  done
}

download_url_with_sha1() {
  url=$1
  output=$2
  expected_sha1=$3

  if [ -x /usr/lib/apt/apt-helper ]; then
    /usr/lib/apt/apt-helper download-file "$url" "$output" "SHA1:$expected_sha1" >/dev/null
    return
  fi

  if ! have_cmd curl || ! have_cmd sha1sum; then
    echo "E: curl and sha1sum are required to download and verify $url" >&2
    return 1
  fi

  curl -fsSL --output "$output" "$url" || return 1
  actual_sha1="$(sha1sum "$output" | awk '{print $1}')"
  [ "$actual_sha1" = "$expected_sha1" ]
}

parse_uboot_version() {
  version=$1
  current_model="${2:-$(cat /proc/device-tree/model)}"

  # MNT u-boot version starts with upstream u-boot version
  case $version in
    [0-9][0-9][0-9][0-9].[0-9][0-9]" MNT "*) : ;;
    *)
      echo "E: expected u-boot version string to start with upstream version but got: $version" >&2
      return
      ;;
  esac

  # Strip off everything following the first space for the upstream version
  upstream_version=${version%% *}
  echo "I: Version of upstream U-Boot: $upstream_version" >&2

  # Remove the upstream version to parse the remainder
  version=${version#[0-9][0-9][0-9][0-9].[0-9][0-9] }

  # After the upstream version comes the machine name
  # The part after the machine name does not contain any spaces, so we can
  # split the version again here
  machine=${version% *}
  case $machine in
    "MNT Pocket Reform with BPI-CM4 Module" | \
      "MNT Pocket Reform with i.MX8MP Module" | \
      "MNT Pocket Reform with RCORE RK3588 Module" | \
      "MNT Reform 2" | \
      "MNT Reform 2 HDMI" | \
      "MNT Reform 2 with BPI-CM4 Module" | \
      "MNT Reform 2 with i.MX8MP Module" | \
      "MNT Reform 2 with LS1028A Module" | \
      "MNT Reform 2 with QUASAR 8550 Module" | \
      "MNT Reform 2 with RCORE-DSI RK3588 Module" | \
      "MNT Reform 2 with RCORE RK3588 Module" | \
      "MNT Reform Next with RCORE RK3588 Module") : ;;
    *)
      echo "E: unknown machine name in u-boot version string: $machine"
      return
      ;;
  esac

  if [ "$machine" != "$current_model" ]; then
    case "$current_model" in
      "MNT Reform 2" | "MNT Reform 2 HDMI")
        case $machine in
          "MNT Reform 2" | "MNT Reform 2 HDMI") : ;;
          *)
            echo "E: machine name in u-boot version does not match system: $machine != $current_model"
            return
            ;;
        esac
        ;;
      *)
        echo "E: machine name in u-boot version does not match system: $machine != $current_model"
        return
        ;;
    esac
  fi

  version=${version##* }

  # The remaining version was generated from a "git describe" from the MNT
  # u-boot project together with "git describe" from upstream u-boot.
  # The latter will likely be suffixed with "-dirty" because we applied
  # patches, strip that off
  version=${version%-dirty}

  # The remaining version should start with the latest MNT u-boot git tag
  # which is a ISO8601 date
  case $version in
    [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9])
      # In case of i.MX8MQ u-boot, upstream u-boot is not a sub-project and thus,
      # there is no additional content after the date
      # In this case, we are done.
      echo "$version"
      return
      ;;
    [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]-*) : ;;
    *)
      echo "E: unable to parse U-Boot version git suffix: $version" >&2
      return
      ;;
  esac

  year=${version%%-*}
  version=${version#*-}
  month=${version%%-*}
  version=${version#*-}
  day=${version%%-*}
  version=${version#*-}

  # If the remaining version contains no hyphen, then that should be the
  # abbreviated git object name and should start with a g.
  case $version in
    - | -* | *- | *-*)
      # If there is a hyphen in the remainder, then the U-Boot build is not
      # from a tag but contains changes
      echo "W: U-Boot version not from tagged commit but with local changes: $version" >&2
      ;;
    g*) : ;;
    *)
      echo "E: Unable to parse U-Boot version git suffix: $version" >&2
      return
      ;;
  esac

  # Finally, if successful, print the MNT U-Boot version for the caller
  echo "$year-$month-$day"
}
