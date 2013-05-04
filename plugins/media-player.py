# Make code still work under Python 2.6/2.7
from __future__ import print_function

from gi.repository import GObject
from gi.repository import Peas
from gi.repository import PeasGtk
from gi.repository import GLib
from gi.repository import Gtk
from gi.repository import Liferea
from gi.repository import Gst

# FIXME: Upgrade to 0.11
#import gi
#gi.require_version('Gst', '0.11')
#from gi.repository import Gst

class MediaPlayerPlugin(GObject.Object, Liferea.MediaPlayerActivatable):
    __gtype_name__ = 'MediaPlayerPlugin'

    object = GObject.property(type=GObject.Object)

    def __init__(self):
        Gst.init_check(None)
        self.IS_GST010 = Gst.version()[0] == 0

        playbin = "playbin2" if self.IS_GST010 else "playbin"
        self.player = Gst.ElementFactory.make(playbin, "player")
        fakesink = Gst.ElementFactory.make("fakesink", "fakesink")
        self.player.set_property("video-sink", fakesink)
        bus = self.player.get_bus()
        bus.add_signal_watch()
        bus.connect("message::eos", self.on_eos)
        bus.connect("message::error", self.on_error)
        self.player.connect("about-to-finish",  self.on_finished)

        self.moving_slider = False

    def on_error(self, bus, message):
        self.player.set_state(Gst.State.NULL)
        err, debug = message.parse_error()
        print("Error: %s" % err, debug)
        self.playing = False
        self.updateButtons()

    def on_eos(self, bus, message):
        self.player.set_state(Gst.State.NULL)
        self.playing = False
        self.updateButtons()

    def on_finished(self, player):
        self.playing = False
        self.slider.set_value(0)
        self.set_label(0)
        self.updateButtons()

    def play(self):
        uri = Liferea.enclosure_get_url(self.enclosures[self.pos])
        self.player.set_property("uri", uri)
        self.player.set_state(Gst.State.PLAYING)
        Liferea.ItemView.select_enclosure(self.pos)
        GObject.timeout_add(1000, self.updateSlider)

    def stop(self):
        self.player.set_state(Gst.State.NULL)
        
    def playToggled(self, w):
        self.slider.set_value(0)
        self.set_label(0)

        if(self.playing == False):
                self.play()
        else:
                self.stop()

        self.playing=not(self.playing)
        self.updateButtons()

    def next(self, w):
        self.stop()
        self.pos+=1
        self.play()
        self.updateButtons()

    def prev(self, w):
        self.stop()
        self.pos-=1
        self.play()
        self.updateButtons()       

    def set_label(self, position):
        self.label.set_text ("%d:%02d" % (position / 60, position % 60))

    def updateSlider(self):
        if not self.playing or self.moving_slider:
           return False # cancel timeout

        try:
           if self.IS_GST010:
              nanosecs = self.player.query_position(Gst.Format.TIME)[2]
              duration_nanosecs = self.player.query_duration(Gst.Format.TIME)[2]
           else:
              nanosecs = self.player.query_position(Gst.Format.TIME)[1]
              duration_nanosecs = self.player.query_duration(Gst.Format.TIME)[1]

           duration = float(duration_nanosecs) / Gst.SECOND
           position = float(nanosecs) / Gst.SECOND
           self.slider.set_range(0, duration)
           self.slider.set_value(position)
           self.set_label(position)

        except Exception as e:
                # pipeline must not be ready and does not know position
                print(e)
                pass

        return True

    def updateButtons(self):
        Gtk.Widget.set_sensitive(self.prevButton, (self.pos != 0))
        Gtk.Widget.set_sensitive(self.nextButton, (len(self.enclosures) - 1 > self.pos))

        if(self.playing == False):
           self.playButtonImage.set_from_stock("gtk-media-play", Gtk.IconSize.BUTTON)
        else:
           self.playButtonImage.set_from_stock("gtk-media-stop", Gtk.IconSize.BUTTON)

    def on_slider_change_value(self, widget, scroll, value):
        if not self.moving_slider:
            nanosecs = value * Gst.SECOND
            self.player.seek_simple(Gst.Format.TIME,
                                    Gst.SeekFlags.FLUSH | 
                                    Gst.SeekFlags.KEY_UNIT,
                                    nanosecs)
            # Do this directly to avoid delay
            self.set_label(value) 

        return False

    def on_slider_button_press(self, widget, event):
        self.moving_slider = True

    def on_slider_button_release(self, widget, event):
        self.moving_slider = False

    def do_load(self, parentWidget, enclosures):
        if parentWidget == None:
           print("ERROR: Could not find media player insertion widget!")

        # Test whether Media Player widget already exists
        childList = Gtk.Container.get_children(parentWidget)

        if len(childList) == 1:
           # We need to add a media player...
           vbox = Gtk.Box(Gtk.Orientation.HORIZONTAL, 0)
           vbox.set_margin_top(3)
           vbox.set_margin_bottom(3)
           Gtk.Box.pack_start(parentWidget, vbox, True, True, 0);

           image = Gtk.Image()
           image.set_from_stock("gtk-media-previous", Gtk.IconSize.BUTTON)
           self.prevButton = Gtk.Button.new()
           self.prevButton.add(image)
           self.prevButton.connect("clicked", self.prev)
           Gtk.Box.pack_start(vbox, self.prevButton, False, False, 0)

           self.playButtonImage = Gtk.Image()
           self.playButtonImage.set_from_stock("gtk-media-play", Gtk.IconSize.BUTTON)
           self.playButton = Gtk.Button.new()
           self.playButton.add(self.playButtonImage)
           self.playButton.connect("clicked", self.playToggled)
           Gtk.Box.pack_start(vbox, self.playButton, False, False, 0)

           image = Gtk.Image()
           image.set_from_stock("gtk-media-next", Gtk.IconSize.BUTTON)
           self.nextButton = Gtk.Button.new()
           self.nextButton.add(image)
           self.nextButton.connect("clicked", self.next)
           Gtk.Box.pack_start(vbox, self.nextButton, False, False, 0)

           self.slider = Gtk.Scale(orientation = Gtk.Orientation.HORIZONTAL)
           self.slider.set_margin_left(6)
           self.slider.set_margin_right(6)
           self.slider.set_draw_value(False)
           self.slider.set_range(0, 100)
           self.slider.set_increments(1, 10)
           self.slider.connect("change-value", self.on_slider_change_value)
           self.slider.connect("button-press-event",
                               self.on_slider_button_press)
           self.slider.connect("button-release-event", 
                               self.on_slider_button_release)

           Gtk.Box.pack_start(vbox, self.slider, True, True, 0)

           self.label = Gtk.Label()
           self.set_label(0)
           self.label.set_margin_left(6)
           self.label.set_margin_right(6)
           Gtk.Box.pack_start(vbox, self.label, False, False, 0)

           Gtk.Widget.show_all(vbox)

        self.enclosures = enclosures
        self.pos = 0
        self.player.set_state(Gst.State.NULL)   # FIXME: Make this configurable?
        self.on_finished(self.player)

    #def do_activate(self):
        #print("=== MediaPlayer activate")

    def do_deactivate(self):
        window = self.object

