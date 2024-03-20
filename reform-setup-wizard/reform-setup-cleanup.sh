#!/bin/sh
#
# This script is run by the setup wizard after the setup wizard has completed successfully.
# 1. Remove autostart of reform-setup

rm -f /etc/profile.d/reform-setup.sh

# 2. Remove root autologin

rm -f /etc/systemd/system/getty@tty1.service.d/override.conf
systemctl daemon-reload

# 3. Enable and start greetd

systemctl enable greetd
systemctl start greetd

# 4. Exit

pkill sway
exit
