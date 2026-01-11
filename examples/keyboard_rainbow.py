#!/usr/bin/env python3
# Copyright 2025 Giraut
# SPDX-License-Identifier: MIT
#
# https://github.com/Giraut/mnt_reform_keyboard_backlight_rainbow

### Parameters
default_backlight_intensity = 50  # %
mnt_keyboard4_hidraw_device = "/dev/hidraw0"


### Modules
import sys
import argparse
from time import time, sleep

try:
    from setproctitle import setproctitle
except ModuleNotFoundError:
    setproctitle = lambda a: True


### Defines
rainbow_rgb = (
    (255, 0, 0),  # Red
    (255, 63, 0),
    (255, 127, 0),  # Orange
    (255, 191, 0),
    (255, 255, 0),  # Yellow
    (127, 255, 0),
    (0, 255, 0),  # Green
    (0, 127, 127),
    (0, 0, 255),  # Blue
    (37, 0, 192),
    (111, 0, 170),
    (148, 0, 211),  # Indigo
)

nb_led_rows = 6
nb_led_cols = 14


### Routines
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

    nb_colors = len(rainbow_rgb)

    # Set the color of the backlight LEDs then exit, or run the animation
    while True:
        # Set the LEDs
        for seq in range(nb_colors):
            start_backlight_change_tstamp = time()

            row_bgr = sum(
                [
                    list(reversed(rainbow_rgb[(seq + i) % nb_colors]))
                    for i in range(nb_led_cols)
                ],
                [],
            )

            row = 0
            while row < nb_led_rows:
                try:
                    with open(mnt_keyboard4_hidraw_device, "wb") as k:
                        k.write(
                            b"xXRGB"
                            + bytes(
                                [row] + [int(v * args.intensity / 100) for v in row_bgr]
                            )
                        )
                    row += 1
                except:
                    row = 0  # Redo the entire refresh in case of error

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
