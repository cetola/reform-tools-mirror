# Defaults for MNT Reform

# enable NIR shader path in mesa. without this,
# some Xwayland applications will be blank
export ETNA_MESA_DEBUG=nir

# set GTK2 theme
export GTK2_RC_FILES=/usr/share/themes/Adwaita-Dark/gtk-2.0/gtkrc

# set QT Platform and Theme
export QT_QPA_PLATFORM=wayland
export QT_QPA_PLATFORMTHEME=qt5ct

# enable harware acceleration in clapper
# https://github.com/Rafostar/clapper/wiki/Hardware-acceleration#mobile-devices-v4l2-codecs
export GST_CLAPPER_USE_PLAYBIN3=1

# enables wayland for firefox
export MOZ_ENABLE_WAYLAND=1

# fix misbehavior where Java application starts with a blank screen
export _JAVA_AWT_WM_NONREPARENTING=1
