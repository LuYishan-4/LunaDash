"""Plain GTK 3 X11 window titled "Welcome" for the XWayland lifecycle test."""
import os

os.environ["GDK_BACKEND"] = "x11"
os.environ.pop("WAYLAND_DISPLAY", None)

import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

window = Gtk.Window(title="Welcome")
window.set_default_size(480, 320)
window.add(Gtk.Label(label="XWayland lifecycle probe"))
window.connect("destroy", Gtk.main_quit)
window.show_all()
Gtk.main()
