#!/usr/bin/env python3
import subprocess
from ctypes import CDLL
from os import environ
from uuid import uuid4
import gi
CDLL('libgtk4-layer-shell.so')
gi.require_version('Gtk', '4.0')
gi.require_version('Gdk', '4.0')
gi.require_version('Gio', '2.0')
gi.require_version('Gtk4LayerShell', '1.0')
from gi.repository import Gdk, Gtk, Gio, GLib, Gtk4LayerShell

TRAY_TEST_MODE = int(environ.get('TRAY_TEST_MODE', '2'))

# old variant: building pop-up menu widget tree by hand
# DISADVANTAGES:
# - doesn't look very "standard"
# - bug: sometimes doesn't grab keyboard control
BUILD_DIY_MENU = TRAY_TEST_MODE == 0

# pop out menu from a button inserted /over/ the waybar, ideally where it places the systray icon
# DISADVANTAGES:
# - don't know where waybar will place systray icon, so WAYBAR_HEIGHT and SYSTRAY_X values guessing game
# - bug: two clicks necessary for first systray open
HOVER_BUTTON = TRAY_TEST_MODE == 1

# on systray click pop down menu at fixed place
# DISADVANTAGES:
# - placement questionable if systray icon not near right edge of screen
# - somewhat hacky abuse of LayerShell using OFFSCREEN_X
# - bug: keyboard navigation of menu only works once, after a first systray opening, user clicked into different window before (?)
FIXED_POPDOWN = TRAY_TEST_MODE == 2

USE_SYSTRAY = BUILD_DIY_MENU or FIXED_POPDOWN
SLIDER_CHANGE_INTERVAL_MS = 250
SLIDER_WIDTH = 120
SYSTRAY_ICON_NAME = 'view-more-symbolic'
if BUILD_DIY_MENU:
    ICON_SPACING = 10
    MARGIN_MENU_WINDOW = 20
    PADDING_MENU_WINDOW = 5
    PADDING_MENU_BUTTON = 5
    SPACE_FOR_SUBMENUS = MARGIN_MENU_WINDOW + PADDING_MENU_WINDOW + 2 * PADDING_MENU_BUTTON
    CSS = '''
.menu-window {
    margin: ''' + str(MARGIN_MENU_WINDOW) + '''px;
    padding: ''' + str(PADDING_MENU_WINDOW) + '''px;
    box-shadow: 0px 0px 10px 0px rgba(0,0,0,0.5);
}
.menu-button {
    border-radius: 0px;
    margin: 0px;
    padding: ''' + str(PADDING_MENU_BUTTON) + '''px;
}
'''
if HOVER_BUTTON:
    WAYBAR_HEIGHT = int(environ.get('WAYBAR_HEIGHT', '32'))
    SYSTRAY_X = int(environ.get('SYSTRAY_X', '150'))  # distance to right edge of it in pixels
if FIXED_POPDOWN:
    OFFSCREEN_X = -9001

SNI_PATH = '/org/mntre/sni'
SNI_WATCHER = 'org.kde.StatusNotifierWatcher'
SNI_ITEM = 'org.kde.StatusNotifierItem'
DBUS_NAME_DOTS = 'com.mntre.waycontrol'
DBUS_NAME_SLASHES = f'/{DBUS_NAME_DOTS.replace(".", "/")}'
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


class MenuItem:

    def __init__(self, title, icon_name, items, on_click=lambda *x: print(f'CLICKED: {x}')):
        self.title = title
        self.icon_name = icon_name
        self.items = items
        self.parent = None
        for item in self.items:
            item.parent = self
        self.on_click = on_click


MENU_ITEMS = [
    MenuItem('Foo', 'system-reboot-symbolic', []),
    MenuItem('Bar', 'system-shutdown-symbolic', []),
    MenuItem(
        'Help', 'help-about-symbolic', [
            MenuItem('Handbook', 'system-help-symbolic', []),
            MenuItem('Keyboard Shortcuts', 'preferences-desktop-keyboard-shortcuts-symbolic', []),
            ]),
    MenuItem(
        'Settings', 'emblem-system-symbolic', [
            MenuItem('Audio', 'multimedia-volume-control-symbolic', []),
            MenuItem('Display', 'video-display-symbolic', []),
            MenuItem('Keyboard', 'input-keyboard-symbolic', [
                MenuItem('Test0', 'system-reboot-symbolic', []),
                MenuItem('Test1', 'application-exit-symbolic', [])
                ]),
            MenuItem('Mouse', 'input-mouse-symbolic', []),
            MenuItem('Wallpaper', 'preferences-desktop-wallpaper-symbolic', []),
            MenuItem('Devices', 'media-floppy-symbolic', []),
            MenuItem('test_a', 'media-floppy-symbolic', []),
            MenuItem('test_b', 'media-floppy-symbolic', []),
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

    def do_activate(self):
        # sliders setup
        self.new_brightness = self.new_volume = None
        sliders = self.make_sliders()
        GLib.timeout_add(SLIDER_CHANGE_INTERVAL_MS, self.apply_last_settings)

        # layer shell setup
        self.gtk_window = Gtk.ApplicationWindow(application=self)
        Gtk4LayerShell.init_for_window(self.gtk_window)
        Gtk4LayerShell.set_keyboard_mode(self.gtk_window, Gtk4LayerShell.KeyboardMode.EXCLUSIVE)
        Gtk4LayerShell.set_layer(self.gtk_window, Gtk4LayerShell.Layer.TOP)
        for i in range(Gtk4LayerShell.Edge.ENTRY_NUMBER):
            Gtk4LayerShell.set_anchor(self.gtk_window, i, (False, True, True, False)[i])
        if BUILD_DIY_MENU:
            Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.RIGHT, SPACE_FOR_SUBMENUS)
        elif HOVER_BUTTON:
            Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.TOP, -WAYBAR_HEIGHT)
            Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.RIGHT, SYSTRAY_X)
            self.gtk_window.set_visible(True)
        elif FIXED_POPDOWN:
            # to not be bothered in placing the popdown ourselves, move generated layer out of sight
            Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.RIGHT, OFFSCREEN_X)
        # FIXME: doesn't seem to work for FIXED_POPDOWN
        event_controller = Gtk.EventControllerKey.new()
        event_controller.connect('key_pressed', self.event_key_pressed_cb)
        self.gtk_window.add_controller(event_controller)

        # publish StatusNotifierItem to make tray icon that toggles our visibility
        props = {'Category': ('s', 'Hardware'),
                 'Id': ('s', 'mntre'),
                 'IconName': ('s', SYSTRAY_ICON_NAME),
                 'Status': ('s', ''),
                 'Title': ('s', ''),
                 'ToolTip': ('(sa(iiay)ss)', ('', [], '', '')),
                 'Menu': ('s', ''),
                 'ItemIsMenu': ('b', False)}
        bus = Gio.bus_get_sync(Gio.BusType.SESSION)
        bus.register_object(
                SNI_PATH,
                Gio.DBusNodeInfo.new_for_xml(NODE_XML).interfaces[0],
                lambda *_: bus.emit_signal(None, DBUS_NAME_SLASHES, DBUS_NAME_DOTS, 'Show', None),
                lambda _a, _b, _c, _d, k: GLib.Variant(props[k][0], props[k][1]), None)
        bus.call_sync(
            SNI_WATCHER,
            '/StatusNotifierWatcher',
            SNI_WATCHER,
            'RegisterStatusNotifierItem',
            GLib.Variant('(s)', (SNI_PATH,)),
            None,
            Gio.DBusCallFlags.NONE,
            -1)
        if USE_SYSTRAY:
            # listen for signal to toggle visiblity
            Gio.bus_own_name(Gio.BusType.SESSION,
                             DBUS_NAME_DOTS,
                             Gio.BusNameOwnerFlags.NONE,
                             self.on_bus_acquired)

        if BUILD_DIY_MENU:
            box = Gtk.Box.new(Gtk.Orientation.VERTICAL, 0)
            self.gtk_window.props.child = box
            for slider in sliders:
                box.append(slider)
            self.max_submenu_width = 0
            for menu_item in MENU_ITEMS:
                box.append(self.make_diy_menu_button(menu_item))
            old_margin = Gtk4LayerShell.get_margin(self.gtk_window,
                                                   Gtk4LayerShell.Edge.RIGHT)
            Gtk4LayerShell.set_margin(self.gtk_window, Gtk4LayerShell.Edge.RIGHT,
                                      old_margin + self.max_submenu_width)
            # css styling
            display = Gdk.Display.get_default()
            provider = Gtk.CssProvider.new()
            provider.load_from_string(CSS)
            Gtk.StyleContext.add_provider_for_display(display, provider,
                                                      Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
            self.gtk_window.add_css_class('menu-window')
            return

        self.top_menu = Gio.Menu()
        popover = Gtk.PopoverMenu.new_from_model(self.top_menu)
        for slider in sliders:
            self.make_menu_slider(popover, slider)
        # create basic (icon-free) menu/widget tree, collect items with children in .submenu_parents
        self.submenu_parents = []
        for item in MENU_ITEMS:
            self.top_menu.append_item(self.walk_menu_basic(item))
        # iconify menu by walking menu widget tree in parallel to MENU_ITEMS and .submenu_parents
        # (NB: shown step-by-step by here to better understand hierarchy)
        widget = popover.get_first_child()  # PopoverContent
        widget = widget.get_first_child()  # ScrolledWindow
        widget = widget.get_first_child()  # Viewport
        widget = widget.get_first_child()  # Stack
        section_box = widget.get_first_child()
        widget = section_box.get_first_child()  # Box
        widget = widget.get_first_child()  # Gizmo / first slider
        widget = widget.get_next_sibling()  # Gizmo / second slider
        self.iconify_buttons(widget.get_next_sibling(), MENU_ITEMS)  # top-level non-submenu buttons
        for submenu_item in self.submenu_parents:  # submenus are flattened into sibling section_boxes
            section_box = section_box.get_next_sibling()
            submenu_header_btn = section_box.get_first_child()
            label = 'back'
            if submenu_item.parent:
                self.iconify_button(submenu_header_btn, submenu_item.parent.icon_name)
                label = submenu_item.parent.title
            submenu_header_btn.get_first_child().get_next_sibling().set_label(label)
            submenu_box = submenu_header_btn.get_next_sibling()  # after header button, next buttons contained in Box
            submenu_btn = submenu_box.get_first_child()
            self.iconify_buttons(submenu_btn, submenu_item.items)
        if HOVER_BUTTON:  # create systray-like button to pop the menu
            systray_btn = Gtk.Button()
            self.gtk_window.props.child = systray_btn
            item_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
            item_box.append(Gtk.Image.new_from_icon_name('view-more-symbolic'))
            systray_btn.set_child(item_box)
            popover.set_parent(systray_btn)
            systray_btn.connect('clicked', lambda _: popover.popup())
        elif FIXED_POPDOWN:
            self.popover = popover
            popover.set_has_arrow(False)
            self.gtk_window.props.child = popover
            self.popover.popup()
            self.popover_height = self.popover.measure(Gtk.Orientation.VERTICAL, -1)[1]

    def event_key_pressed_cb(self, keyval, keycode, state, controller):
        if keycode == Gdk.KEY_Escape:
            app.quit()
            return True
        return False

    # sliders management

    def make_sliders(self):

        def make_slider(getter, on_change, icon1_name, icon2_name):
            hbox = Gtk.Box.new(Gtk.Orientation.HORIZONTAL, 0)
            scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
            scale.set_size_request(SLIDER_WIDTH, -1)
            scale.connect('value-changed', on_change)
            hbox.append(Gtk.Image.new_from_icon_name(icon1_name))
            hbox.append(scale)
            hbox.append(Gtk.Image.new_from_icon_name(icon2_name))
            val = getter()
            scale.set_value(val)
            return hbox

        brightness = make_slider(
            get_brightness_percent, self.on_brightness_change,
            'weather-clear-night-symbolic', 'display-brightness-symbolic')
        volume = make_slider(
            get_volume_percent, self.on_volume_change,
            'audio-volume-muted-symbolic', 'audio-volume-high-symbolic')
        return brightness, volume

    def on_brightness_change(self, scale):
        value = scale.get_value()
        self.new_brightness = value

    def on_volume_change(self, scale):
        value = scale.get_value()
        self.new_volume = value

    def apply_last_settings(self):
        if self.new_brightness:
            set_brightness_percent(self.new_brightness)
        if self.new_volume:
            set_volume_percent(self.new_volume)
        self.new_brightness = self.new_volume = None
        return True

    # for USE_SYSTRAY

    def on_signal_received(self, *_):
        if BUILD_DIY_MENU:
            if self.gtk_window.props.visible:
                self.gtk_window.set_visible(False)
            else:
                self.gtk_window.present()
        elif FIXED_POPDOWN:
            if not self.gtk_window.props.visible:
                self.gtk_window.present()
            self.popover.set_offset(0, -self.popover_height)
            self.popover.popup()

    def on_bus_acquired(self, connection, _name):
        connection.signal_subscribe(
            None,
            DBUS_NAME_DOTS,
            'Show',
            DBUS_NAME_SLASHES,
            None,
            Gio.DBusSignalFlags.NONE,
            self.on_signal_received)

    # for BUILD_DIY_MENU

    def make_diy_menu_button(self, menu_item):
        btn = Gtk.Button(label=menu_item.title)
        item_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=ICON_SPACING)
        item_box.append(Gtk.Image.new_from_icon_name(menu_item.icon_name))
        item_box.append(Gtk.Label(label=menu_item.title))
        btn.set_child(item_box)
        if menu_item.items:
            item_box.append(Gtk.Image.new_from_icon_name('go-next-symbolic'))
            vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            for submenu_item in menu_item.items:
                sub_btn = self.make_diy_menu_button(submenu_item)
                width = sub_btn.measure(Gtk.Orientation.HORIZONTAL, -1)[0]
                self.max_submenu_width = max(width, self.max_submenu_width)
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

    # for HOVER_BUTTON and FIXED_POPDOWN

    def make_menu_slider(self, menu_widget, hbox):
        id_ = str(uuid4())
        slider_item = Gio.MenuItem.new()
        slider_item.set_attribute_value("custom", GLib.Variant.new_string(id_))
        self.top_menu.append_item(slider_item)
        menu_widget.add_child(hbox, id_)

    def walk_menu_basic(self, item):
        if item.items:
            submenu = Gio.Menu()
            self.submenu_parents += [item]
            for sub_item in item.items:
                submenu.append_item(self.walk_menu_basic(sub_item))
            menu_item = Gio.MenuItem.new_submenu(item.title, submenu)
        else:
            uuid = str(uuid4())
            action = Gio.SimpleAction.new(uuid, None)
            action.connect("activate", item.on_click)
            self.add_action(action)
            menu_item = Gio.MenuItem.new(item.title, f'app.{uuid}')
            # # NB: contrary to expectations, below not doing anything, explained in
            # # <https://discourse.gnome.org/t/icon-and-tooltip-for-menu-item/10665>
            # menu_item.set_icon(Gio.ThemedIcon.new(item.icon_name))
        return menu_item

    @staticmethod
    def iconify_button(menu_button, icon_name):
        icon_image = Gtk.Image.new_from_icon_name(icon_name)
        menu_button.get_first_child().append(icon_image)

    @classmethod
    def iconify_buttons(cls, next_menu_button, menu_items):
        for item in menu_items:
            cls.iconify_button(next_menu_button, item.icon_name)
            next_menu_button = next_menu_button.get_next_sibling()


app = Tray()
app.run(None)
