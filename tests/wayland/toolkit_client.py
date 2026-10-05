"""Small real GTK client used by the cross-toolkit lifecycle test."""
import sys
import gi

version = sys.argv[1]
gi.require_version('Gtk', version)
from gi.repository import Gtk

app = Gtk.Application(application_id='org.lunadash.ToolkitProbe' + version[0])

def activate(application):
    window = Gtk.ApplicationWindow(application=application)
    window.set_title('LunaDash GTK ' + version)
    window.set_default_size(420, 280)
    label = Gtk.Label(label='Wayland toolkit lifecycle probe')
    if version == '4.0':
        window.set_child(label)
        window.present()
    else:
        window.add(label)
        window.show_all()

app.connect('activate', activate)
raise SystemExit(app.run([]))
