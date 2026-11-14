#!/usr/bin/python3
# Camadas do papel de parede vivo: céu fixo + 4 ondas em PNG transparente (um pouco maiores que a tela, para poderem se mover).
W,H,MX,MY=1366,768,60,24
P=["M0,470 C260,360 520,330 760,420 S1180,560 1366,430 V900 H0 Z","M0,560 C230,470 470,500 700,560 S1130,640 1366,520 V900 H0 Z",
   "M0,640 C300,560 560,620 820,650 S1220,640 1366,600 V900 H0 Z","M0,710 C320,650 640,700 900,715 S1250,700 1366,680 V900 H0 Z"]
def gen(name, sky, glows, waves):
    s=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}"><defs><linearGradient id="sky" x1="0" y1="0" x2="0.25" y2="1">'
       +''.join(f'<stop offset="{o}" stop-color="{c}"/>' for o,c in sky)+'</linearGradient>']
    for i,(cx,cy,r,c,o) in enumerate(glows):
        s.append(f'<radialGradient id="g{i}" cx="{cx}" cy="{cy}" r="{r}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="{c}" stop-opacity="{o}"/><stop offset="1" stop-color="{c}" stop-opacity="0"/></radialGradient>')
    s.append(f'</defs><rect width="{W}" height="{H}" fill="url(#sky)"/>'+''.join(f'<rect width="{W}" height="{H}" fill="url(#g{i})"/>' for i in range(len(glows)))+'</svg>')
    open(f'{name}-sky.svg','w').write(''.join(s))
    for i,(a,b) in enumerate(waves):
        LW,LH=W+2*MX,H+2*MY
        open(f'{name}-w{i}.svg','w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{LW}" height="{LH}"><defs>'
          f'<linearGradient id="w" x1="0" y1="0" x2="1" y2="0.9"><stop offset="0" stop-color="{a}"/><stop offset="1" stop-color="{b}"/></linearGradient>'
          f'<linearGradient id="hl" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".30"/><stop offset=".25" stop-color="#fff" stop-opacity="0"/></linearGradient>'
          f'<filter id="soft" x="-5%" y="-30%" width="110%" height="160%"><feGaussianBlur stdDeviation="26"/></filter></defs>'
          f'<g transform="translate(0,{MY}) scale({LW/W:.5f},1)"><path d="{P[i]}" fill="#000" opacity=".16" filter="url(#soft)" transform="translate(0,-14)"/>'
          f'<path d="{P[i]}" fill="url(#w)"/><path d="{P[i]}" fill="url(#hl)"/></g></svg>')
gen('vivo-claro',[(0,'#3f8cff'),(.45,'#8fc0ff'),(.8,'#ffd9c7'),(1,'#ffc7a8')],[(1050,120,620,'#ffffff',.55),(200,300,520,'#7a6cff',.35)],
    [('#5b6cf9','#9a5cf0'),('#c259e6','#ff6fa3'),('#ff7d6b','#ffb561'),('#2f6bff','#5aa8ff')])
gen('vivo-escuro',[(0,'#070b24'),(.5,'#141a4a'),(1,'#2b1740')],[(1050,120,620,'#3d5bff',.30),(200,300,520,'#5b2a9b',.30)],
    [('#1d2a86','#3a2390'),('#5a2488','#8f2f6e'),('#8f3a4a','#a8603c'),('#0f2a7a','#1c4aa8')])
