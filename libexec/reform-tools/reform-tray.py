#!/usr/bin/env python3

import signal
import os
import subprocess
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Notify', '0.7')
gi.require_version('AyatanaAppIndicator3', '0.1')
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import Notify
from gi.repository import AyatanaAppIndicator3 as AppIndicator3

APPID = "reform-tray"
ICON = "start-here-symbolic"

def handle_exit_item(question, command):
    dialog = Gtk.MessageDialog(
        flags=0,
        message_type=Gtk.MessageType.WARNING,
        text=question,
    )
    dialog.format_secondary_text(
        "Make sure to save your work first."
    )
    dialog.add_buttons(
        Gtk.STOCK_OK, Gtk.ResponseType.OK,
        Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
    )
    response = dialog.run()

    if response == Gtk.ResponseType.OK:
        subprocess.run(command)

    dialog.destroy()

def handle_help_item(self):
    subprocess.run(["foot", "-W", "100x80", "bash", "-c", "reform-desktop-help; bash"])

def handle_brt_high_item(self):
    subprocess.run(["brightnessctl", "set", "100%"])

def handle_brt_low_item(self):
    subprocess.run(["brightnessctl", "set", "10%"])

def handle_vol_item(self):
    subprocess.run(["pavucontrol"])

def handle_term_item(self):
    subprocess.run(["foot"])

def handle_files_item(self):
    subprocess.run(["thunar", os.environ["HOME"]])

def handle_sysmon_item(self):
    subprocess.run(["gnome-system-monitor"])

def handle_software_item(self):
    subprocess.run(["reform-synaptic"])

def handle_logout_item(self):
    handle_exit_item("Are you sure you want to exit the desktop session?", ["pkill", "(wayfire|sway)"])

def handle_reboot_item(self):
    handle_exit_item("Are you sure you want to reboot the computer?", ["pkexec", "/sbin/reboot", "now"])

def handle_shutdown_item(self):
    handle_exit_item("Are you sure you want to shut down the computer?", ["pkexec", "/sbin/shutdown", "now"])

menu = Gtk.Menu()

mi_help = Gtk.ImageMenuItem(label="Help", visible=True)
img = Gtk.Image()
img.set_from_icon_name("help-about-symbolic", -1)
mi_help.set_image(img)
mi_help.set_always_show_image(True)
mi_help.connect("activate", handle_help_item)

mi_brt_inc = Gtk.ImageMenuItem(label="High Brightness", visible=True)
img = Gtk.Image()
img.set_from_icon_name("weather-clear-symbolic", -1)
mi_brt_inc.set_image(img)
mi_brt_inc.set_always_show_image(True)
mi_brt_inc.connect("activate", handle_brt_high_item)

mi_brt_dec = Gtk.ImageMenuItem(label="Low Brightness", visible=True)
img = Gtk.Image()
img.set_from_icon_name("weather-clear-night-symbolic", -1)
mi_brt_dec.set_image(img)
mi_brt_dec.set_always_show_image(True)
mi_brt_dec.connect("activate", handle_brt_low_item)

mi_vol = Gtk.ImageMenuItem(label="Volume", visible=True)
img = Gtk.Image()
img.set_from_icon_name("audio-volume-high-symbolic", -1)
mi_vol.set_image(img)
mi_vol.set_always_show_image(True)
mi_vol.connect("activate", handle_vol_item)

mi_term = Gtk.ImageMenuItem(label="Terminal", visible=True)
img = Gtk.Image()
img.set_from_icon_name("utilities-terminal-symbolic", -1)
mi_term.set_image(img)
mi_term.set_always_show_image(True)
mi_term.connect("activate", handle_term_item)

mi_files = Gtk.ImageMenuItem(label="File Manager", visible=True)
img = Gtk.Image()
img.set_from_icon_name("folder-symbolic", -1)
mi_files.set_image(img)
mi_files.set_always_show_image(True)
mi_files.connect("activate", handle_files_item)

mi_sysmon = Gtk.ImageMenuItem(label="System Monitor", visible=True)
img = Gtk.Image()
img.set_from_icon_name("utilities-system-monitor-symbolic", -1)
mi_sysmon.set_image(img)
mi_sysmon.set_always_show_image(True)
mi_sysmon.connect("activate", handle_sysmon_item)

mi_software = Gtk.ImageMenuItem(label="Software", visible=True)
img = Gtk.Image()
img.set_from_icon_name("system-software-install-symbolic", -1)
mi_software.set_image(img)
mi_software.set_always_show_image(True)
mi_software.connect("activate", handle_software_item)

mi_logout = Gtk.ImageMenuItem(label="Logout", visible=True)
img = Gtk.Image()
img.set_from_icon_name("system-log-out-symbolic", -1)
mi_logout.set_image(img)
mi_logout.set_always_show_image(True)
mi_logout.connect("activate", handle_logout_item)

mi_reboot = Gtk.ImageMenuItem(label="Reboot", visible=True)
img = Gtk.Image()
img.set_from_icon_name("system-restart-symbolic", -1)
mi_reboot.set_image(img)
mi_reboot.set_always_show_image(True)
mi_reboot.connect("activate", handle_reboot_item)

mi_shutdown = Gtk.ImageMenuItem(label="Shutdown", visible=True)
img = Gtk.Image()
img.set_from_icon_name("system-shutdown-symbolic", -1)
mi_shutdown.set_image(img)
mi_shutdown.set_always_show_image(True)
mi_shutdown.connect("activate", handle_shutdown_item)

menu.append(mi_help)
menu.append(mi_brt_inc)
menu.append(mi_brt_dec)
menu.append(mi_vol)
menu.append(mi_term)
menu.append(mi_files)
menu.append(mi_sysmon)
menu.append(mi_software)
menu.append(mi_logout)
menu.append(mi_reboot)
menu.append(mi_shutdown)

class TrayIcon:

    def __init__(self, appid, icon, menu):
        self.menu = menu

        self.ind = AppIndicator3.Indicator.new(
            appid, icon,
            AppIndicator3.IndicatorCategory.HARDWARE)
        self.ind.set_status(AppIndicator3.IndicatorStatus.ACTIVE)
        self.ind.set_secondary_activate_target(mi_shutdown)
        self.ind.set_menu(self.menu)

signal.signal(signal.SIGINT, signal.SIG_DFL)
icon = TrayIcon(APPID, ICON, menu)
Notify.init(APPID)

Gtk.main()
