reform-tools
============

This package contains a collection of scripts and configuration files for
GNU/Linux operating systems running on the MNT Reform, MNT Pocket Reform and
MNT Reform Next with i.MX8MQ, i.MX8MPlus, Banana PI CM4, LS1028A or RK3588
processor modules. It contains system utilities to manage your installation as
well as configuration files for initramfs, flash-kernel, pulseaudio, alsa,
u-boot-menu, udev and NetworkManager.

reform2-lpc-dkms
----------------

The reform2_lpc kernel module allows for interaction with the NXP LPC11U24
Cortex-M0 MCU system controller in the Reform 2 open hardware laptop. It
controls an analog monitor chip for the eight battery cells as well as the
charger. It is connected to the Processor Module via SPI, and has GPIO lines to
the main power rail switchers in the system. It has a UART (SYSCTL) that the
keyboard can talk to directly for issuing power on/off commands and battery
status queries. The reform2_lpc module provides battery status information and
is necessary to completely shut down the system when powering it off via
userspace.

License
-------

Copyright 2020-2025 Lukas F. Hartmann <lukas@mntre.com>
Copyright 2022-2025 Johannes Schauer Marin Rodrigues <josch@mister-muffin.de>

Unless otherwise stated, reform-tools can be redistributed and/or modified
under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

