# Gera "ATÉ LOGO!" (tela de encerramento): fonte Quicksand, letras espaçadas, brilho suave.
import cairo, gi
gi.require_version('Pango','1.0'); gi.require_version('PangoCairo','1.0')
from gi.repository import Pango, PangoCairo
def lay(cr, txt):
    l = PangoCairo.create_layout(cr)
    l.set_font_description(Pango.FontDescription("Quicksand Medium 62"))
    a = Pango.AttrList(); a.insert(Pango.attr_letter_spacing_new(14*Pango.SCALE)); l.set_attributes(a)
    l.set_text(txt, -1); return l
t = cairo.Context(cairo.ImageSurface(cairo.FORMAT_ARGB32, 10, 10))
w, h = lay(t, "ATÉ LOGO!").get_pixel_size()
P = 70; s = cairo.ImageSurface(cairo.FORMAT_ARGB32, w+2*P, h+2*P); cr = cairo.Context(s)
for r, a in ((18,.025),(9,.04)):      # brilho azulado
    for dx in range(-r, r+1, 2):
        for dy in range(-r, r+1, 2):
            if dx*dx+dy*dy <= r*r:
                cr.save(); cr.translate(P+dx, P+dy); cr.set_source_rgba(.45,.66,1,a/3); PangoCairo.show_layout(cr, lay(cr,"ATÉ LOGO!")); cr.restore()
cr.translate(P, P); cr.set_source_rgba(.95,.97,1,.97); PangoCairo.show_layout(cr, lay(cr,"ATÉ LOGO!"))
s.write_to_png("tema/nova-os/ate-logo.png"); print(w, h)
