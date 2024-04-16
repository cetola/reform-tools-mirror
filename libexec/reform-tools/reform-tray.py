#!/usr/bin/env python3
# Copyright 2023 - 2024 Lukas F. Hartmann <lukas@mntre.com>
# Copyright 2024 Johannes Schauer Marin Rodrigues <josch@debian.org>
# SPDX-License-Identifier: GPL-3.0+

import signal
import os
import subprocess
import gi
import shutil

gi.require_version("Gtk", "3.0")
gi.require_version("Notify", "0.7")
gi.require_version("AyatanaAppIndicator3", "0.1")
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import Notify
from gi.repository import AyatanaAppIndicator3 as AppIndicator3

APPID = "reform-tray"
ICON = "view-more-symbolic"


def handle_exit_item(question, command):
    dialog = Gtk.MessageDialog(
        flags=0,
        message_type=Gtk.MessageType.WARNING,
        text=question,
    )
    dialog.format_secondary_text("Make sure to save your work first.")
    dialog.add_buttons(
        Gtk.STOCK_OK,
        Gtk.ResponseType.OK,
        Gtk.STOCK_CANCEL,
        Gtk.ResponseType.CANCEL,
    )
    response = dialog.run()

    if response == Gtk.ResponseType.OK:
        subprocess.run(command)

    dialog.destroy()


class MenuItem(Gtk.ImageMenuItem):
    def __init__(self, label, icon, args, exitmsg=None):
        Gtk.ImageMenuItem.__init__(self, label=label, visible=True)
        self.img = Gtk.Image()
        self.img.set_from_icon_name(icon, -1)
        self.set_image(self.img)
        self.set_always_show_image(True)
        if exitmsg:
            self.action = lambda _: handle_exit_item(exitmsg, args)
        else:
            self.action = lambda _: subprocess.run(args)
        self.connect("activate", self.action)
        if not shutil.which(args[0]):
            self.set_sensitive(False)


menuitems = [
    MenuItem(
        "Help",
        "help-about-symbolic",
        ["foot", "bash", "-c", "reform-desktop-help; bash"],
        None,
    ),
    MenuItem(
        "High Brightness",
        "weather-clear-symbolic",
        ["brightnessctl", "set", "100%"],
        None,
    ),
    MenuItem(
        "Low Brightness",
        "weather-clear-night-symbolic",
        ["brightnessctl", "set", "10%"],
        None,
    ),
    MenuItem("Terminal", "utilities-terminal-symbolic", ["foot"], None),
    MenuItem("File Manager", "folder-symbolic", ["thunar", os.environ["HOME"]], None),
    MenuItem(
        "System Monitor",
        "utilities-system-monitor-symbolic",
        ["gnome-system-monitor"],
        None,
    ),
    MenuItem(
        "Install Software",
        "system-software-install-symbolic",
        ["reform-synaptic"],
        None,
    ),
    MenuItem(
        "Logout",
        "system-log-out-symbolic",
        ["pkill", "(wayfire|sway)"],
        "Are you sure you want to exit the desktop session?",
    ),
    MenuItem(
        "Reboot",
        "system-restart-symbolic",
        ["systemctl", "reboot"],
        "Are you sure you want to reboot the computer?",
    ),
    MenuItem(
        "Shutdown",
        "system-shutdown-symbolic",
        ["systemctl", "poweroff"],
        "Are you sure you want to shut down the computer?",
    ),
]


class TrayIcon:
    def __init__(self, appid, icon, menuitems):
        self.menu = Gtk.Menu()
        for mi in menuitems:
            self.menu.append(mi)

        self.ind = AppIndicator3.Indicator.new(
            appid, icon, AppIndicator3.IndicatorCategory.HARDWARE
        )
        self.ind.set_status(AppIndicator3.IndicatorStatus.ACTIVE)
        self.ind.set_secondary_activate_target(mi)
        self.ind.set_menu(self.menu)


signal.signal(signal.SIGINT, signal.SIG_DFL)
icon = TrayIcon(APPID, ICON, menuitems)
Notify.init(APPID)

Gtk.main()
