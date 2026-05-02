# SPDX-License-Identifier: GPL-3.0+
# shellcheck shell=sh
# shellcheck disable=SC1091

if [ -n "${MIRROR:-}" ]; then
  case "$MIRROR" in
    mntre.com | reform.debian.net) BACKEND=debian ;;
    archlinuxarm.org) BACKEND=arch ;;
    *)
      echo "E: unsupported value for --mirror: $MIRROR" >&2
      return 1
      ;;
  esac
else
  BACKEND="$(detect_backend_from_os_release)"
fi

case "$BACKEND" in
  debian | arch) : ;;
  *)
    echo "E: unsupported backend: $BACKEND" >&2
    return 1
    ;;
esac

DISTRO_BACKEND_FILE="$DISTRO_LIB_DIR/distro/$BACKEND.sh"
if [ ! -r "$DISTRO_BACKEND_FILE" ]; then
  echo "E: backend file is missing or unreadable for backend '$BACKEND'" >&2
  return 1
fi

# shellcheck source=../reform-check/distro/debian.sh
. "$DISTRO_BACKEND_FILE"
