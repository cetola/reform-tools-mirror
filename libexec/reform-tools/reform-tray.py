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

SLIDER_CHANGE_INTERVAL_MS = 250

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

SLIDER_WIDTH = 100
ICON_SPACING = 10
MARGIN_ROUNDED_WINDOW = 20
PADDING_ROUNDED_WINDOW = 20
PADDING_MENU_BUTTON = 10
SPACE_FOR_SUBMENUS = MARGIN_ROUNDED_WINDOW + PADDING_ROUNDED_WINDOW + 2 * PADDING_MENU_BUTTON
CSS = '''
.rounded-window {
    border-radius: 20px;
    margin: ''' + str(MARGIN_ROUNDED_WINDOW) + '''px;
    padding: ''' + str(PADDING_ROUNDED_WINDOW) + '''px;
    box-shadow: 0px 0px 10px 0px rgba(0,0,0,0.5);
}
.menu-button {
    border-radius: 0px;
    margin: 0px;
    padding: ''' + str(PADDING_MENU_BUTTON) + '''px;
}
'''


class MenuItem:

    def __init__(self, title, icon_name, items, on_click=lambda x: print(f'CLICKED: {x}')):
        self.title = title
        self.icon_name = icon_name
        self.items = items
        self.on_click = on_click


MENU_ITEMS = [
    MenuItem(
        'Help', 'helb-about-symbolic', [
            MenuItem('Handbook', 'system-help-symbolic', []),
            MenuItem('Keyboard Shortcuts', 'preferences-desktop-keyboard-shortcuts-symbolic', []),
            ]),
    MenuItem(
        'Settings', 'emblem-system-symbolic', [
            MenuItem('Audio', 'multimedia-volume-control-symbolic', []),
            MenuItem('Display', 'video-display-symbolic', []),
            MenuItem('Keyboard', 'input-keyboard-symbolic', []),
            MenuItem('Mouse', 'input-mouse-symbolic', []),
            MenuItem('Wallpaper', 'preferences-desktop-wallpaper-symbolic', []),
            MenuItem('Devices', 'media-floppy-symbolic', []),
            ]),
    MenuItem(
        'Power', 'system-shutdown-symbolic', [
            MenuItem('LockScreen', 'system-lock-screen-symbolic', []),
            MenuItem('Logout', 'application-exit-symbolic', []),
            MenuItem('Reboot', 'system-reboot-symbolic', []),
            MenuItem('Shutdown', 'system-shutdown-symbolic', []),
            ]),
]


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
        Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.RIGHT, SPACE_FOR_SUBMENUS)
        anchors = (False, True, True, False)
        for i in range(Gtk4LayerShell.Edge.ENTRY_NUMBER):
            Gtk4LayerShell.set_anchor(self.gtk_window, i, anchors[i])
        box = Gtk.Box.new(Gtk.Orientation.VERTICAL, 0)
        self.gtk_window.props.child = box

        # sliders
        def make_slider(getter, on_change, icon1_name, icon2_name):
            vbox = Gtk.Box.new(Gtk.Orientation.HORIZONTAL, 0)
            scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
            scale.set_size_request(SLIDER_WIDTH, -1)
            scale.connect('value-changed', on_change)
            vbox.append(Gtk.Image.new_from_icon_name(icon1_name))
            vbox.append(scale)
            vbox.append(Gtk.Image.new_from_icon_name(icon2_name))
            val = getter()
            scale.set_value(val)
            box.append(vbox)

        make_slider(get_brightness_percent, self.on_brightness_change,
                    'weather-clear-night-symbolic', 'display-brightness-symbolic')
        make_slider(get_volume_percent, self.on_volume_change,
                   'audio-volume-muted-symbolic', 'audio-volume-high-symbolic')
        self.new_brightness = self.new_volume = None
        GLib.timeout_add(SLIDER_CHANGE_INTERVAL_MS, self.apply_last_settings)

        # popup menu
        max_submenu_width = 0

        def btn_popup_menu(menu_item):
            nonlocal max_submenu_width
            btn = Gtk.Button(label=menu_item.title)
            item_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=ICON_SPACING)
            item_box.append(Gtk.Image.new_from_icon_name(menu_item.icon_name))
            item_box.append(Gtk.Label(label=menu_item.title))
            btn.set_child(item_box)
            if menu_item.items:
                item_box.append(Gtk.Image.new_from_icon_name('go-next-symbolic'))
                vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
                for submenu_item in menu_item.items:
                    sub_btn = btn_popup_menu(submenu_item)
                    width = sub_btn.measure(Gtk.Orientation.HORIZONTAL, -1)[0]
                    max_submenu_width = max(width, max_submenu_width)
                    vbox.append(sub_btn)
                popover = Gtk.Popover.new()
                popover.set_has_arrow(False)
                popover.set_child(vbox)
                popover.set_parent(btn)
                popover.set_position(Gtk.PositionType.RIGHT)
                menu_height = -(PADDING_MENU_BUTTON + 10 + 3)
                cur_item = vbox.get_first_child()
                while cur_item:
                    bounds = cur_item.compute_bounds(cur_item.get_parent())[1]
                    menu_height += bounds.size.height + PADDING_MENU_BUTTON + 3
                    cur_item = cur_item.get_next_sibling()
                popover.set_offset(0, menu_height)
                btn.connect('clicked', lambda _: popover.popup())
            else:
                btn.connect('clicked', menu_item.on_click)
            btn.add_css_class('menu-button')
            return btn

        for menu_item in MENU_ITEMS:
            box.append(btn_popup_menu(menu_item))
        old_margin = Gtk4LayerShell.get_margin(self.gtk_window,
                                               Gtk4LayerShell.Edge.RIGHT)
        Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.RIGHT,
                                  old_margin + max_submenu_width)

        # css styling
        display = Gdk.Display.get_default()
        provider = Gtk.CssProvider.new()
        provider.load_from_string(CSS)
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

    def apply_last_settings(self):
        if self.new_brightness:
            set_brightness_percent(self.new_brightness)
        if self.new_volume:
            set_volume_percent(self.new_volume)
        self.new_brightness = self.new_volume = None
        return True

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
        self.new_brightness = value

    def on_volume_change(self, scale):
        value = scale.get_value()
        self.new_volume = value

    def event_key_pressed_cb(self, keyval, keycode, state, controller):
        if keycode == Gdk.KEY_Escape:
            app.quit()
            return True
        return False


app = Tray()
app.run(None)
