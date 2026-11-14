# Gera as imagens do tema de boot NOVA OS (mesmo visual do login).
import cairo, math, random, os
O = "tema/nova-os/"; W, H = 1366, 768
def glow(cr, x, y, r, R, G, B, a):
    g = cairo.RadialGradient(x, y, 0, x, y, r)
    g.add_color_stop_rgba(0, R, G, B, a); g.add_color_stop_rgba(.45, R, G, B, a*.35); g.add_color_stop_rgba(1, R, G, B, 0)
    cr.set_source(g); cr.paint()
def new(w, h):
    s = cairo.ImageSurface(cairo.FORMAT_ARGB32, w, h); return s, cairo.Context(s)

# fundo (céu escuro + horizonte com fio de luz), igual ao do login
s, cr = new(W, H)
g = cairo.LinearGradient(0, 0, 0, H); g.add_color_stop_rgb(0, .016, .018, .030); g.add_color_stop_rgb(1, .030, .036, .062)
cr.set_source(g); cr.paint()
R = W*1.25; hy = H*.83; cx = W/2; cy = hy+R
glow(cr, cx, hy+30, H*.62, .16, .36, 1.0, .30)
rim = cairo.RadialGradient(cx, cy, R-2, cx, cy, R+70)
for st, c in ((0,(.85,.92,1,.95)),(.05,(.45,.66,1,.50)),(.35,(.20,.42,1,.14)),(1,(.20,.42,1,0))): rim.add_color_stop_rgba(st, *c)
m = cairo.RadialGradient(cx, hy, 0, cx, hy, W*.5)
m.add_color_stop_rgba(0,0,0,0,1); m.add_color_stop_rgba(.55,0,0,0,.45); m.add_color_stop_rgba(1,0,0,0,0)
cr.set_source(rim); cr.mask(m)
cr.arc(cx, cy, R-1, 0, 2*math.pi); cr.set_source_rgb(.012,.014,.024); cr.fill()
s.flush(); buf = s.get_data(); random.seed(7)
for i in range(0, len(buf), 4):
    n = random.randint(-2, 2)
    for k in range(3): buf[i+k] = max(0, min(255, buf[i+k]+n))
s.write_to_png(O+"fundo.png")

# brilho do horizonte (animado: respira)
s, cr = new(W, 700); glow(cr, W/2, 600, 560, .55, .72, 1.0, .35)
fade = cairo.LinearGradient(0, 640, 0, 700); fade.add_color_stop_rgba(0, 0, 0, 0, 1); fade.add_color_stop_rgba(1, 0, 0, 0, 0)
cr.set_operator(cairo.OPERATOR_DEST_IN); cr.set_source(fade); cr.paint(); cr.rectangle(0, 0, W, 640); cr.set_source_rgba(0,0,0,1); cr.fill()
s.write_to_png(O+"brilho.png")

# logo N com halo
S = 200; s, cr = new(S*2, S*2); glow(cr, S, S, S*.95, .4, .62, 1.0, .30)
u = S/64*0.9; ox = oy = S-32*u
cr.set_line_width(8*u); cr.set_line_cap(cairo.LINE_CAP_ROUND); cr.set_line_join(cairo.LINE_JOIN_ROUND)
cr.move_to(ox+14*u, oy+52*u); cr.line_to(ox+14*u, oy+14*u); cr.line_to(ox+50*u, oy+52*u); cr.line_to(ox+50*u, oy+14*u)
cr.set_source_rgba(1,1,1,.94); cr.stroke(); s.write_to_png(O+"logo.png")

def texto(nome, txt, size, alpha, space, peso=cairo.FONT_WEIGHT_NORMAL):
    s0, c0 = new(10, 10); c0.select_font_face("Inter", cairo.FONT_SLANT_NORMAL, peso); c0.set_font_size(size)
    wd = sum(c0.text_extents(ch).x_advance + space for ch in txt) - space; w = int(wd+40); h = int(size*1.8)
    s, cr = new(w, h); cr.select_font_face("Inter", cairo.FONT_SLANT_NORMAL, peso); cr.set_font_size(size)
    x = 20
    for ch in txt:
        cr.move_to(x, size*1.25); cr.set_source_rgba(.92,.95,1,alpha); cr.show_text(ch); x += cr.text_extents(ch).x_advance + space
    s.write_to_png(O+nome)
texto("nome.png", "NOVA OS", 30, .88, 14, cairo.FONT_WEIGHT_NORMAL)
texto("ate-logo.png", "Até logo", 54, .96, 3, cairo.FONT_WEIGHT_NORMAL)
texto("sub-desliga.png", "Desligando com segurança", 18, .55, 1.5)
texto("sub-reinicia.png", "Reiniciando", 18, .55, 1.5)
texto("sub-inicia.png", "Preparando tudo para você", 18, .55, 1.5)

# barra de progresso (trilho e preenchimento)
BW, BH = 260, 4
s, cr = new(BW, BH); cr.arc(2,2,2,math.pi/2,3*math.pi/2); cr.arc(BW-2,2,2,-math.pi/2,math.pi/2); cr.close_path(); cr.set_source_rgba(1,1,1,.14); cr.fill(); s.write_to_png(O+"trilho.png")
s, cr = new(BW, BH); cr.arc(2,2,2,math.pi/2,3*math.pi/2); cr.arc(BW-2,2,2,-math.pi/2,math.pi/2); cr.close_path()
g = cairo.LinearGradient(0,0,BW,0); g.add_color_stop_rgba(0,.45,.66,1,.9); g.add_color_stop_rgba(1,.85,.93,1,1); cr.set_source(g); cr.fill(); s.write_to_png(O+"barra.png")
