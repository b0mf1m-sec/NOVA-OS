#!/usr/bin/python3
# Gera os papéis de parede do NOVA OS (claro e escuro) em SVG. Uso: python3 gerar.py && convertê-los com rsvg-convert.
def wall(name, sky, glows, waves):
    W,H=1366,768
    s=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}"><defs>']
    s.append('<linearGradient id="sky" x1="0" y1="0" x2="0.25" y2="1">'+''.join(f'<stop offset="{o}" stop-color="{c}"/>' for o,c in sky)+'</linearGradient>')
    for i,(a,b,_) in enumerate(waves):
        s.append(f'<linearGradient id="w{i}" x1="0" y1="0" x2="1" y2="0.9"><stop offset="0" stop-color="{a}"/><stop offset="1" stop-color="{b}"/></linearGradient>')
    for i,(cx,cy,r,c,o) in enumerate(glows):
        s.append(f'<radialGradient id="g{i}" cx="{cx}" cy="{cy}" r="{r}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="{c}" stop-opacity="{o}"/><stop offset="1" stop-color="{c}" stop-opacity="0"/></radialGradient>')
    s.append('<linearGradient id="hl" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".30"/><stop offset=".25" stop-color="#fff" stop-opacity="0"/></linearGradient>')
    s.append('<filter id="soft" x="-5%" y="-30%" width="110%" height="160%"><feGaussianBlur stdDeviation="26"/></filter></defs>')
    s.append(f'<rect width="{W}" height="{H}" fill="url(#sky)"/>')
    for i in range(len(glows)): s.append(f'<rect width="{W}" height="{H}" fill="url(#g{i})"/>')
    for i,(a,b,p) in enumerate(waves):
        s.append(f'<path d="{p}" fill="#000" opacity=".16" filter="url(#soft)" transform="translate(0,-14)"/>')
        s.append(f'<path d="{p}" fill="url(#w{i})"/><path d="{p}" fill="url(#hl)"/>')
    s.append('</svg>'); open(name+'.svg','w').write(''.join(s))
P=["M0,470 C260,360 520,330 760,420 S1180,560 1366,430 V768 H0 Z","M0,560 C230,470 470,500 700,560 S1130,640 1366,520 V768 H0 Z",
   "M0,640 C300,560 560,620 820,650 S1220,640 1366,600 V768 H0 Z","M0,710 C320,650 640,700 900,715 S1250,700 1366,680 V768 H0 Z"]
Q=["M0,380 C300,470 620,300 900,360 S1250,460 1366,340 V768 H0 Z","M0,500 C280,560 560,440 860,500 S1240,560 1366,470 V768 H0 Z",
   "M0,610 C340,660 640,560 940,610 S1260,650 1366,590 V768 H0 Z","M0,700 C300,730 700,670 1000,700 S1280,720 1366,690 V768 H0 Z"]
wall('ondas-claro',[(0,'#3f8cff'),(.45,'#8fc0ff'),(.8,'#ffd9c7'),(1,'#ffc7a8')],[(1050,120,620,'#ffffff',.55),(200,300,520,'#7a6cff',.35)],
     [('#5b6cf9','#9a5cf0',P[0]),('#c259e6','#ff6fa3',P[1]),('#ff7d6b','#ffb561',P[2]),('#2f6bff','#5aa8ff',P[3])])
wall('ondas-escuro',[(0,'#070b24'),(.5,'#141a4a'),(1,'#2b1740')],[(1050,120,620,'#3d5bff',.30),(200,300,520,'#5b2a9b',.30)],
     [('#1d2a86','#3a2390',P[0]),('#5a2488','#8f2f6e',P[1]),('#8f3a4a','#a8603c',P[2]),('#0f2a7a','#1c4aa8',P[3])])
wall('lago-claro',[(0,'#2bb3c0'),(.5,'#8fe0d6'),(1,'#e9f7d8')],[(300,140,560,'#ffffff',.55),(1100,260,520,'#3ad1a0',.30)],
     [('#17a6a0','#3fd0a8',Q[0]),('#0f8f9c','#2fb8c8',Q[1]),('#1b6fb0','#3b9be0',Q[2]),('#124e8c','#2b74c4',Q[3])])
wall('lago-escuro',[(0,'#041b22'),(.5,'#0a3038'),(1,'#10343a')],[(300,140,560,'#2bb3c0',.25),(1100,260,520,'#1c8a6a',.25)],
     [('#0c5a58','#17786a',Q[0]),('#0a4a56','#126474',Q[1]),('#0d3b66','#17568a',Q[2]),('#082a50','#103f74',Q[3])])
wall('aurora-claro',[(0,'#ff9a6b'),(.4,'#ffb7a0'),(.75,'#e6b8ff'),(1,'#b9a2ff')],[(1000,160,600,'#fff3d6',.65),(260,260,520,'#ff6fa3',.30)],
     [('#ff7a8a','#ff9f6b',P[0]),('#e85fb0','#ff7fa0',Q[1]),('#a35cf0','#d06ae8',P[2]),('#5b4fe0','#8a6cf5',Q[3])])
wall('aurora-escuro',[(0,'#2a0f1e'),(.5,'#2b1238'),(1,'#16123c')],[(1000,160,600,'#ff7a5a',.22),(260,260,520,'#b03a80',.25)],
     [('#8a2f4a','#a04a3a',P[0]),('#7a2a6a','#983a62',Q[1]),('#4f2a90','#6e3496',P[2]),('#2a2680','#40349a',Q[3])])
wall('grafite-claro',[(0,'#d9dce3'),(.5,'#eceef2'),(1,'#f7f7f9')],[(1050,140,620,'#ffffff',.8),(200,300,520,'#b9c0d0',.35)],
     [('#c3c8d4','#d6dae3',P[0]),('#aeb4c2','#c6cbd6',Q[1]),('#989fb0','#b4bac8',P[2]),('#808899','#a0a7b8',Q[3])])
wall('grafite-escuro',[(0,'#0e0f12'),(.5,'#17181c'),(1,'#1e1f24')],[(1050,140,620,'#3a3d48',.5),(200,300,520,'#2a2c36',.5)],
     [('#24262e','#2e3039',P[0]),('#2b2d36','#363944',Q[1]),('#32343e','#40434f',P[2]),('#3a3d48','#4a4e5c',Q[3])])
