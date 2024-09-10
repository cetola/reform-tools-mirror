#!/usr/bin/env python3
import subprocess
from ctypes import CDLL
import gi
CDLL('libgtk4-layer-shell.so')
gi.require_version('Gtk', '4.0')
gi.require_version('Gdk', '4.0')
gi.require_version('Gio', '2.0')
gi.require_version('Gtk4LayerShell', '1.0')
from gi.repository import Gdk, Gtk, Gio, GLib, Gtk4LayerShell

ICON_NAME = 'view-more-symbolic'
SNI_PATH = '/org/mntre/sni'
SNI_WATCHER = 'org.kde.StatusNotifierWatcher'
SNI_ITEM = 'org.kde.StatusNotifierItem'
NODE_XML = f"""
<node name='/'>
<interface name='{SNI_ITEM}'>
<property name='Category' type='s' access='read'/>
<property name='Id' type='s' access='read'/>
<property name='IconName' type='s' access='read'/>
<property name='Status' type='s' access='read'/>
<property name='Title' type='s' access='read'/>
<property name='ToolTip' type='(sa(iiay)ss)' access='read'/>
<property name='Menu' type='o' access='read'/>
<property name='ItemIsMenu' type='b' access='read'/>
<method name="Activate">
<arg type="i" direction="in" />
<arg type="i" direction="in" />
</method>
</interface>
</node>
"""


def get_volume_percent():
    to_pipe = subprocess.run(['wpctl', 'get-volume', '@DEFAULT_AUDIO_SINK@'], check=True, capture_output=True).stdout
    return 100 * float(subprocess.run(['cut', '-f', '2', '-d', ' '], input=to_pipe, check=True, capture_output=True).stdout)


def set_volume_percent(p):
    subprocess.run(['wpctl', 'set-volume', '@DEFAULT_AUDIO_SINK@', f'{p}%'])


def get_brightness_percent():
    max_ = float(subprocess.run(['brightnessctl', 'm'], check=True, capture_output=True).stdout)
    val = float(subprocess.run(['brightnessctl', 'g'], check=True, capture_output=True).stdout)
    return 100 * val / max_


def set_brightness_percent(p):
    subprocess.run(['brightnessctl', 's', f'{p}%'])


class Tray(Gtk.Application):
    def __init__(self):
        super().__init__()
        self.connect('activate', self.on_activate)

    def on_activate(self, _):
        self.gtk_window = Gtk.Window()

        Gtk4LayerShell.init_for_window(self.gtk_window)

        Gtk4LayerShell.set_layer(self.gtk_window, Gtk4LayerShell.Layer.TOP)
        Gtk4LayerShell.auto_exclusive_zone_enable(self.gtk_window)

        Gtk4LayerShell.set_keyboard_mode(self.gtk_window, Gtk4LayerShell.KeyboardMode.ON_DEMAND)

        Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.LEFT, 0)
        Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.RIGHT, 80)
        Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.TOP, 0)
        Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.BOTTOM, 0)  # 0 is default
        anchors = (False, True, True, False)
        for i in range(Gtk4LayerShell.Edge.ENTRY_NUMBER):
            Gtk4LayerShell.set_anchor(self.gtk_window, i, anchors[i])

        box = Gtk.Box.new(Gtk.Orientation.HORIZONTAL, 0)

        # brightness controller
        vbox1 = Gtk.Box.new(Gtk.Orientation.VERTICAL, 0)
        scale1 = Gtk.Scale.new_with_range(Gtk.Orientation.VERTICAL, 0, 100, 1)
        scale1.set_inverted(True)
        scale1.set_size_request(-1, 200)
        scale1.connect('value-changed', self.on_brightness_change)
        icon1 = Gtk.Image.new_from_icon_name('display-brightness-symbolic')
        vbox1.append(scale1)
        vbox1.append(icon1)
        brite = get_brightness_percent()
        scale1.set_value(brite)

        # volume controller
        vbox2 = Gtk.Box.new(Gtk.Orientation.VERTICAL, 0)
        scale2 = Gtk.Scale.new_with_range(Gtk.Orientation.VERTICAL, 0, 100, 1)
        scale2.set_inverted(True)
        scale2.set_size_request(-1, 200)
        scale2.connect('value-changed', self.on_volume_change)
        icon2 = Gtk.Image.new_from_icon_name('audio-volume-medium-symbolic')
        vbox2.append(scale2)
        vbox2.append(icon2)
        vol = get_volume_percent()
        scale2.set_value(vol)

        box.append(vbox1)
        box.append(vbox2)
        self.gtk_window.props.child = box

        # css styling
        display = Gdk.Display.get_default()
        provider = Gtk.CssProvider.new()
        provider.load_from_string('.rounded-window {border-radius: 20px; margin:20px; padding: 20px;box-shadow:0px 0px 10px 0px rgba(0,0,0,0.5);}')
        Gtk.StyleContext.add_provider_for_display(display, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.gtk_window.add_css_class('rounded-window')

        event_controller = Gtk.EventControllerKey.new()
        event_controller.connect('key_pressed', self.event_key_pressed_cb)
        self.gtk_window.add_controller(event_controller)

        # publish StatusNotifierItem to make tray icon that toggles our visibility
        bus = Gio.bus_get_sync(Gio.BusType.SESSION)
        bus.register_object(
                SNI_PATH,
                Gio.DBusNodeInfo.new_for_xml(NODE_XML).interfaces[0],
                lambda _a, _b, _c, _d, _e, _f, _g: bus.emit_signal(None, '/com/mntre/waycontrol', 'com.mntre.waycontrol', 'Show', None),
                lambda _a, _b, _c, _d, k: GLib.Variant(props[k][0], props[k][1]),
                None)
        props = {'Category': ('s', 'Hardware'),
                 'Id': ('s', 'mntre'),
                 'IconName': ('s', ICON_NAME),
                 'Status': ('s', ''),
                 'Title': ('s', ''),
                 'ToolTip': ('(sa(iiay)ss)', ('', [], '', '')),
                 'Menu': ('s', ''),
                 'ItemIsMenu': ('b', False)}
        bus.call_sync(
            SNI_WATCHER,
            '/StatusNotifierWatcher',
            SNI_WATCHER,
            'RegisterStatusNotifierItem',
            GLib.Variant('(s)', (SNI_PATH,)),
            None,
            Gio.DBusCallFlags.NONE,
            -1)

        # listen for signal to toggle visiblity
        Gio.bus_own_name(Gio.BusType.SESSION,
                         'com.mntre.waycontrol',
                         Gio.BusNameOwnerFlags.NONE,
                         self.on_bus_acquired)

        self.hold()

    def on_signal_received(self, _connection, _sender_name, _object_path, _interface_name, _signal_name, _parameters):
        self.toggle_window()

    def on_bus_acquired(self, connection, _name):
        connection.signal_subscribe(
            None,
            'com.mntre.waycontrol',
            'Show',
            '/com/mntre/waycontrol',
            None,
            Gio.DBusSignalFlags.NONE,
            self.on_signal_received)

    def toggle_window(self):
        if self.gtk_window.props.visible:
            self.gtk_window.set_visible(False)
        else:
            self.gtk_window.present()

    def on_brightness_change(self, scale):
        value = scale.get_value()
        set_brightness_percent(value)

    def on_volume_change(self, scale):
        value = scale.get_value()
        set_volume_percent(value)

    def event_key_pressed_cb(self, keyval, keycode, state, controller):
        if keycode == Gdk.KEY_Escape:
            app.quit()
            return True
        return False


app = Tray()
app.run(None)
