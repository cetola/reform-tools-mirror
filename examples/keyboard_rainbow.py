#!/usr/bin/env python3
# Copyright 2025 Giraut
# Copyright 2026 Johannes Schauer Marin Rodrigues
# SPDX-License-Identifier: MIT
#
# https://github.com/Giraut/mnt_reform_keyboard_backlight_rainbow


### Constants
keycap_widths = {
    1: 17.5,
    1.25: 17.5 * 1.25 + 0.1,  # 21.975
    1.5: 17.5 * 1.5 + 0.25,  # 26.5
    1.75: 31.3,
    2: 17.5 * 2 + 0.5,  # 35.5
}

classic_kbd_width = 260.65
classic_sizes = [
    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5],
    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5],
    [1.5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
    [1.75, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.75],
    [1.25, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.25],
    [1.25, 1.5, 1.5, 1.5, 2, 1.5, 1, 1, 1, 1, 1.25],
]

# ortholinear pocket reform
# yes, the pocket keyboard only has 5 rows and 12 columns but we need to send
# the full matrix of the big keyboard
pocket_sizes = [[1] * 14] * 6
# this is not the real pocket keyboard width but we make our live easier by
# re-using the default keycap sizes this way
pocket_kbd_width = 251.65


### Parameters
default_backlight_intensity = 50  # %

### Modules
import sys
import argparse
from time import time, sleep
import glob

try:
    from setproctitle import setproctitle
except ModuleNotFoundError:
    setproctitle = lambda a: True


### Routines
def flatten(xss):
    return [x for xs in xss for x in xs]


def hsv_to_bgr(h: float, s: float, v: float) -> list:
    if not s:
        return [v, v, v]
    if h == 1.0:
        h = 0.0
    i = int(h * 6.0)
    f = h * 6.0 - i

    w = int(v * (1.0 - s) * 255)
    q = int(v * (1.0 - s * f) * 255)
    t = int(v * (1.0 - s * (1.0 - f)) * 255)
    v = int(255 * v)

    match i:
        case 0:
            return [w, t, v]
        case 1:
            return [w, v, q]
        case 2:
            return [t, v, w]
        case 3:
            return [v, q, w]
        case 4:
            return [v, w, t]
        case 5:
            return [q, w, v]


def argparse_intensity_type(value):
    """Argparse type for a valid backlight intensity"""

    try:
        v = float(value)
    except:
        raise argparse.ArgumentTypeError("{}: invalid intensity".format(value))

    if v < 0:
        raise argparse.ArgumentTypeError("{}: intensity too low (min. 0%)".format(v))

    if v > 100:
        raise argparse.ArgumentTypeError("{}: intensity too high (max. 100%)".format(v))

    return v


def argparse_delay_type(value):
    """Argparse type for a valid backlight refresh delay"""

    try:
        v = float(value)
    except:
        raise argparse.ArgumentTypeError("{}: invalid delay".format(value))

    if v < 1:
        raise argparse.ArgumentTypeError("{}: delay too low (min. 1s)".format(v))

    return v


### Main routine
def main():
    setproctitle("mnt_reform_keyboard_backlight_rainbow")

    # Process the command line arguments
    argparser = argparse.ArgumentParser()

    argparser.add_argument(
        "-i",
        "--intensity",
        type=argparse_intensity_type,
        help="Backlight intensity (default: {}%%)".format(default_backlight_intensity),
        default=default_backlight_intensity,
    )

    argparser.add_argument(
        "-r",
        "--refresh-every-sec",
        type=argparse_delay_type,
        help="Number of seconds between refreshes "
        "(default: no refresh - set the pattern and exit)",
    )

    args = argparser.parse_args()

    mnt_keyboard4_hidraw_device = None
    # try hidraw devices until one matches
    for name, sizes, kbd_width, g in [
        (
            "MNT Keyboard v4",
            classic_sizes,
            classic_kbd_width,
            "/dev/input/by-id/usb-MNT_Research_MNT_Reform_Keyboard_4.0_*-hidraw",
        ),
        (
            "MNT Pocket Reform Keyboard",
            pocket_sizes,
            pocket_kbd_width,
            "/dev/input/by-id/usb-MNT_Pocket_Reform_Input_1.0_*-hidraw",
        ),
    ]:
        matches = glob.glob(g)
        if len(matches) != 1:
            continue
        mnt_keyboard4_hidraw_device = matches[0]
        print(f"Found {name}")
        break

    if mnt_keyboard4_hidraw_device is None:
        print("unable to find hidraw device")
        exit(1)

    offsets = []
    for row in sizes:
        row_offsets = []
        offset_x = -17.5 / 2
        for u in row:
            offset_x += keycap_widths[u] / 2
            row_offsets.append(round(offset_x, 3))
            offset_x += keycap_widths[u] / 2
            offset_x += 1.1
        assert 0 <= offset_x <= kbd_width, offset_x
        offsets.append(row_offsets)

    max_num_retries = 5

    # Set the color of the backlight LEDs then exit, or run the animation
    while True:
        # Set the LEDs
        num_steps = 1000
        for seq in range(num_steps):
            start_backlight_change_tstamp = time()

            num_retries = 0
            row = 0
            while row < len(offsets):
                row_offsets = offsets[row]
                data = b"xXRGB" + bytes(
                    [row]
                    + flatten(
                        hsv_to_bgr(
                            (((offset - 17.5 / 4 * row) / kbd_width) + seq / num_steps)
                            % 1,
                            1.0,
                            args.intensity / 100,
                        )
                        for offset in row_offsets
                    )
                )
                try:
                    with open(mnt_keyboard4_hidraw_device, "wb") as k:
                        k.write(data)
                    row += 1
                    num_retries = 0
                except PermissionError:
                    print(
                        f"Unable to open {mnt_keyboard4_hidraw_device} for writing: permission denied"
                    )
                    exit(1)
                except BrokenPipeError:
                    print(
                        f"Unable to write to {mnt_keyboard4_hidraw_device}: broken pipe -- wrong device?"
                    )
                    num_retries += 1
                    if num_retries > max_num_retries:
                        print("failed too often")
                        exit(1)

                sleep(0.15)  # Give the keyboard controller a chance to do its thing

            # Exit if we're not doing an animation
            if args.refresh_every_sec is None:
                return 0

            # Sleep until the next refresh
            sleep(
                max(
                    0, args.refresh_every_sec - (time() - start_backlight_change_tstamp)
                )
            )


### Main program
if __name__ == "__main__":
    sys.exit(main())
