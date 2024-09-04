#!/usr/bin/env python3
# Copyright 2024 Johannes Schauer Marin Rodrigues <josch@mister-muffin.de>
# SPDX-License-Identifier: MIT
# inspired by https://gitlab.com/carbonOS/gde/gde-background

import gi


gi.require_version("Gtk", "3.0")
gi.require_version("GtkLayerShell", "0.1")
gi.require_version("GDesktopEnums", "3.0")

from gi.repository import Gtk, Gdk, GtkLayerShell, GDesktopEnums, Gio, GdkPixbuf, GLib


class BackgroundWindow(Gtk.Window):
    def __init__(self):
        super().__init__()
        self.set_decorated(False)
        self.set_title("reform-wallpaper")
        self._pixbuf = GdkPixbuf.Pixbuf.new(
            GdkPixbuf.Colorspace.RGB, False, 8, 200, 200
        )
        self._file_monitor = None
        self.connect("draw", self._draw_cb)
        self.connect("size-allocate", self._size_allocate_cb)
        GtkLayerShell.init_for_window(self)
        GtkLayerShell.set_exclusive_zone(self, -1)
        GtkLayerShell.set_layer(self, GtkLayerShell.Layer.BACKGROUND)
        GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.LEFT, True)
        GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.RIGHT, True)
        GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.TOP, True)
        GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.BOTTOM, True)
        self._settings = Gio.Settings(schema_id="org.gnome.desktop.background")
        self._settings.connect("changed", self._settings_changed_cb)

    def _settings_changed_cb(self, settings, key):
        if key in [
            "picture-uri",
            "primary-color",
            "picture-options",
            "picture-opacity",
        ]:
            self._size_allocate_cb(self, self.get_size())

    def _draw_cb(self, widget, ctx):
        ctx.save()
        ctx.scale(1.0 / self.get_scale_factor(), 1.0 / self.get_scale_factor())
        Gdk.cairo_set_source_pixbuf(ctx, self._pixbuf, 0, 0)
        ctx.paint()
        ctx.restore()
        return True

    def _size_allocate_cb(self, widget, rect):
        self._pixbuf = GdkPixbuf.Pixbuf.new(
            GdkPixbuf.Colorspace.RGB,
            False,
            8,
            rect.width * self.get_scale_factor(),
            rect.height * self.get_scale_factor(),
        )
        fill_color = Gdk.RGBA()
        if fill_color.parse(self._settings.get_string("primary-color")):
            self._pixbuf.fill(
                (
                    (int(fill_color.red * 0xFF)) << 24
                    | (int(fill_color.green * 0xFF)) << 16
                    | (int(fill_color.blue * 0xFF)) << 8
                )
                + 0xFF
            )
        else:
            # if parsing the value failed, fill with the default color from
            # /usr/share/glib-2.0/schemas/org.gnome.desktop.background.gschema.xml
            self._pixbuf.fill(0x023C88FF)

        alpha = 255.0 * (self._settings.get_int("picture-opacity") / 100.0)
        if alpha == 0:
            self.queue_draw()
            return

        bg_mode = self._settings.get_enum("picture-options")
        match bg_mode:
            case GDesktopEnums.BackgroundStyle.NONE:
                self.queue_draw()
                return
            case GDesktopEnums.BackgroundStyle.SCALED:
                pass
            case GDesktopEnums.BackgroundStyle.ZOOM:
                pass
            case _:
                print("W: background style not supported:", bg_mode)

        path = self._settings.get_string("picture-uri")
        scheme = GLib.uri_parse_scheme(path)
        if scheme == "file":
            path = GLib.filename_from_uri(path)[0]
        else:
            print(f"W: scheme {scheme} not supported")
        if path.endswith(".xml"):
            print("W: GNOME timed wallpaper xml not supported")
        if self._file_monitor is not None:
            self._file_monitor.cancel()
            self._file_monitor = None
        try:
            self._file_monitor = Gio.File.new_for_path(path).monitor(
                Gio.FileMonitorFlags.NONE
            )
            self._file_monitor.connect(
                "changed",
                lambda a, b, c, d: self._size_allocate_cb(self, self.get_size()),
            )
        except Exception as e:
            print(f"E: attaching the file monitor failed with: {e}")
        img = GdkPixbuf.Pixbuf.new_from_file(path)
        ratio_w = self._pixbuf.get_width() / img.get_width()
        ratio_h = self._pixbuf.get_height() / img.get_height()
        if bg_mode == GDesktopEnums.BackgroundStyle.SCALED:
            ratio = ratio_w if ratio_w < ratio_h else ratio_h
        else:
            ratio = ratio_w if ratio_w > ratio_h else ratio_h
        final_width = ratio * img.get_width()
        final_height = ratio * img.get_height()
        off_x = (self._pixbuf.get_width() - final_width) / 2
        off_y = (self._pixbuf.get_height() - final_height) / 2
        if bg_mode == GDesktopEnums.BackgroundStyle.SCALED:
            img.composite(
                self._pixbuf,
                off_x,
                off_y,
                final_width,
                final_height,
                off_x,
                off_y,
                ratio,
                ratio,
                GdkPixbuf.InterpType.BILINEAR,
                alpha,
            )
        else:
            img.composite(
                self._pixbuf,
                0,
                0,
                self._pixbuf.get_width(),
                self._pixbuf.get_height(),
                off_x,
                off_y,
                ratio,
                ratio,
                GdkPixbuf.InterpType.BILINEAR,
                alpha,
            )
        self.queue_draw()


def create_window(monitor):
    win = BackgroundWindow()
    GtkLayerShell.set_monitor(win, monitor)
    setattr(monitor, "reform-wallpaper", win)
    win.show_all()


def destroy_window(monitor):
    getattr(monitor, "reform-wallpaper").destroy()


if __name__ == "__main__":
    display = Gdk.Display.get_default()
    for i in range(display.get_n_monitors()):
        create_window(display.get_monitor(i))
    display.connect("monitor_added", create_window)
    display.connect("monitor_removed", destroy_window)

    Gtk.main()
