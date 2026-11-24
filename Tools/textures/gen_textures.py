"""
No Sleep Pogo - generador procedural de texturas estilo PS1.

Genera PNG pequenos (16-256 px), con ruido, paleta reducida a 15 bits y
dithering ordenado, listos para importarse en Unreal con filtrado Nearest.

Uso:  python -I Tools/textures/gen_textures.py <carpeta_salida>
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "SourceArt", "Textures")
os.makedirs(OUT, exist_ok=True)

RNG = np.random.default_rng(1234)
random.seed(1234)

# Paleta de produccion del GDD
POGO = (168, 58, 54)
META = (35, 70, 200)
TECHO = (168, 203, 227)
CIUDAD = (184, 176, 164)

BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16.0 - 0.5


def hexc(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def canvas(w, h, color):
    a = np.zeros((h, w, 3), dtype=np.float32)
    a[:, :] = color
    return a


def noise(a, amount=10.0, scale=1):
    h, w = a.shape[:2]
    n = RNG.normal(0, amount, (max(1, h // scale), max(1, w // scale), 1)).astype(np.float32)
    if scale > 1:
        n = np.repeat(np.repeat(n, scale, 0), scale, 1)[:h, :w]
    return a + n


def smooth_noise(w, h, cells=4, amount=20.0):
    """Ruido de valor tileable (bloques interpolados)."""
    g = RNG.normal(0, 1, (cells, cells)).astype(np.float32)
    ys = np.linspace(0, cells, h, endpoint=False)
    xs = np.linspace(0, cells, w, endpoint=False)
    y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
    fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
    fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
    a = g[y0 % cells][:, x0 % cells]
    b = g[y0 % cells][:, (x0 + 1) % cells]
    c = g[(y0 + 1) % cells][:, x0 % cells]
    d = g[(y0 + 1) % cells][:, (x0 + 1) % cells]
    v = (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy
    return (v * amount)[:, :, None]


def rect(a, x0, y0, x1, y1, color, jitter=0.0):
    h, w = a.shape[:2]
    x0, x1 = max(0, int(x0)), min(w, int(x1))
    y0, y1 = max(0, int(y0)), min(h, int(y1))
    if x1 <= x0 or y1 <= y0:
        return
    a[y0:y1, x0:x1] = color
    if jitter:
        a[y0:y1, x0:x1] += RNG.normal(0, jitter, (y1 - y0, x1 - x0, 1))


def shade(color, f):
    return tuple(max(0, min(255, c * f)) for c in color)


def finalize(a, name, dither=True, levels=32, alpha=None):
    a = np.clip(a, 0, 255)
    if dither:
        h, w = a.shape[:2]
        t = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w, None]
        step = 256.0 / levels
        a = np.floor(a / step + 0.5 + t * 0.9) * step
    a = np.clip(a, 0, 255).astype(np.uint8)
    if alpha is not None:
        img = Image.fromarray(np.dstack([a, alpha.astype(np.uint8)]), "RGBA")
    else:
        img = Image.fromarray(a, "RGB")
    img.save(os.path.join(OUT, name + ".png"))
    return img


# ---------------------------------------------------------------- suelo
def t_paving():
    """Adoquines rojizos y grises de vereda (2 m por repeticion)."""
    w = h = 64
    a = canvas(w, h, (70, 62, 58))
    bw, bh = 16, 8
    for row in range(h // bh):
        off = (bw // 2) if row % 2 else 0
        for col in range(-1, w // bw + 1):
            x = col * bw + off
            r = RNG.random()
            base = (150, 82, 70) if r < 0.55 else ((128, 118, 112) if r < 0.85 else (170, 120, 96))
            base = shade(base, 0.85 + RNG.random() * 0.25)
            rect(a, x + 1, row * bh + 1, x + bw, row * bh + bh, base, jitter=7)
            rect(a, x + 1, row * bh + 1, x + bw, row * bh + 2, shade(base, 1.12))
    finalize(noise(a, 4), "T_Paving")


def t_tiles():
    """Baldosas amplias grises (vereda de salida)."""
    w = h = 64
    a = canvas(w, h, (92, 92, 90))
    for gy in range(2):
        for gx in range(2):
            base = shade((158, 156, 150), 0.9 + RNG.random() * 0.15)
            rect(a, gx * 32 + 1, gy * 32 + 1, gx * 32 + 32, gy * 32 + 32, base, jitter=5)
            for k in range(3):  # tactiles / puntos
                pass
    a += smooth_noise(w, h, 4, 8)
    # baldosa guia amarilla en una franja
    finalize(noise(a, 4), "T_Tiles")


def t_guide():
    w = h = 32
    a = canvas(w, h, (200, 168, 40))
    for y in range(2, h, 6):
        rect(a, 0, y, w, y + 2, (222, 190, 60))
    finalize(noise(a, 6), "T_GuideTile")


def t_asphalt():
    w = h = 64
    a = canvas(w, h, (66, 66, 70))
    a += smooth_noise(w, h, 8, 10)
    a = noise(a, 9)
    for _ in range(30):
        x, y = RNG.integers(0, w), RNG.integers(0, h)
        a[y, x] += 25
    finalize(a, "T_Asphalt")


def t_concrete():
    w = h = 64
    a = canvas(w, h, (150, 148, 142))
    a += smooth_noise(w, h, 4, 14)
    a = noise(a, 7)
    for _ in range(6):  # manchas
        x, y, r = RNG.integers(0, w), RNG.integers(0, h), RNG.integers(3, 9)
        yy, xx = np.ogrid[:h, :w]
        m = ((xx - x) ** 2 + (yy - y) ** 2) < r * r
        a[m] *= 0.88
    finalize(a, "T_Concrete")


def t_curb():
    w = h = 32
    a = canvas(w, h, (176, 174, 168))
    a = noise(a, 6)
    rect(a, 0, 0, w, 3, (200, 198, 192))
    rect(a, 15, 0, 16, h, (120, 118, 114))
    finalize(a, "T_Curb")


def t_roof_mosaic():
    """Piso de azotea de mosaiquitos celestes."""
    w = h = 64
    a = canvas(w, h, (120, 140, 150))
    c = 4
    for y in range(0, h, c):
        for x in range(0, w, c):
            base = TECHO if RNG.random() < 0.8 else (190, 212, 230)
            if RNG.random() < 0.06:
                base = (140, 175, 205)
            rect(a, x, y, x + c - 1, y + c - 1, shade(base, 0.92 + RNG.random() * 0.12))
    a += smooth_noise(w, h, 4, 8)
    finalize(a, "T_RoofMosaic")


# ---------------------------------------------------------------- fachadas
def window(a, x0, y0, x1, y1, frame=(70, 70, 74), lit=False):
    rect(a, x0, y0, x1, y1, frame)
    glass = (60, 78, 104) if not lit else (180, 170, 120)
    rect(a, x0 + 1, y0 + 1, x1 - 1, y1 - 1, glass, jitter=6)
    # reflejo
    for i in range(min(x1 - x0, y1 - y0) - 2):
        xx, yy = x0 + 2 + i, y0 + 1 + i
        if xx < x1 - 1 and yy < y1 - 1 and i % 3 != 2:
            a[yy, xx] = (110, 132, 160) if not lit else (220, 210, 160)
    midx = (x0 + x1) // 2
    rect(a, midx, y0 + 1, midx + 1, y1 - 1, frame)


def t_facade(name, wall, tile=(56, 52, 48), variant=0):
    """Un piso de 3 m x 3 m con una ventana. 64x64."""
    w = h = 64
    a = canvas(w, h, wall)
    a += smooth_noise(w, h, 4, 10)
    a = noise(a, 5)
    if variant == 1:  # azulejos
        for y in range(0, h, 4):
            rect(a, 0, y, w, y + 1, shade(wall, 0.85))
        for x in range(0, w, 6):
            rect(a, x, 0, x + 1, h, shade(wall, 0.9))
    # losa de piso
    rect(a, 0, 58, w, 64, shade(wall, 0.78), jitter=4)
    rect(a, 0, 58, w, 59, shade(wall, 1.15))
    window(a, 14, 16, 50, 46)
    # alfeizar
    rect(a, 12, 46, 52, 49, shade(wall, 1.2))
    # manchas de lluvia
    for x in (14, 49):
        rect(a, x, 49, x + 2, 58, shade(wall, 0.85))
    finalize(a, name)


def t_shop(name, wall, shutter=False, sign_color=(40, 90, 160)):
    """Planta baja comercial (3 m x 3.2 m). 64x64."""
    w = h = 64
    a = canvas(w, h, wall)
    a = noise(a, 5)
    rect(a, 0, 0, w, 12, sign_color, jitter=4)  # cartel
    for i in range(4):  # pseudo glifos
        gx = 8 + i * 13
        rect(a, gx, 3, gx + 8, 4, (235, 235, 225))
        rect(a, gx + 3, 3, gx + 5, 10, (235, 235, 225))
        rect(a, gx, 7, gx + 8, 8, (235, 235, 225))
        if i % 2:
            rect(a, gx, 9, gx + 3, 10, (235, 235, 225))
    if shutter:
        rect(a, 4, 14, 60, 62, (150, 152, 150))
        for y in range(15, 62, 3):
            rect(a, 4, y, 60, y + 1, (112, 114, 116))
        rect(a, 4, 58, 60, 62, (96, 96, 98))
    else:
        rect(a, 4, 14, 60, 62, (60, 60, 64))
        rect(a, 6, 16, 38, 56, (78, 96, 112), jitter=8)  # vidriera
        for i in range(5):
            rect(a, 8 + i * 6, 40 - (i % 3) * 6, 12 + i * 6, 56, shade((200, 120, 60), 0.6 + 0.1 * i))
        rect(a, 42, 16, 58, 62, (52, 58, 66))  # puerta
        rect(a, 44, 18, 56, 44, (90, 110, 128), jitter=6)
        rect(a, 54, 46, 56, 50, (200, 190, 120))
    rect(a, 0, 62, w, 64, (70, 70, 70))
    finalize(a, name)


def t_shutter():
    w = h = 32
    a = canvas(w, h, (150, 152, 150))
    for y in range(0, h, 3):
        rect(a, 0, y, w, y + 1, (110, 112, 114))
    finalize(noise(a, 5), "T_Shutter")


def t_plaster(name, color):
    w = h = 64
    a = canvas(w, h, color)
    a += smooth_noise(w, h, 4, 12)
    a = noise(a, 5)
    for x in range(0, w, 32):
        rect(a, x, 0, x + 1, h, shade(color, 0.86))
    finalize(a, name)


def t_awning(name, c1, c2, stripe=8):
    w = h = 64
    a = canvas(w, h, c1)
    for x in range(0, w, stripe * 2):
        rect(a, x + stripe, 0, x + 2 * stripe, h, c2)
    a += smooth_noise(w, h, 4, 8)
    a = noise(a, 4)
    # pliegues horizontales
    for y in range(0, h, 16):
        rect(a, 0, y, w, y + 1, (0, 0, 0)) if False else None
        a[y:y + 1] *= 0.85
    finalize(a, name)


def t_stripe(name, c):
    a = canvas(16, 16, c)
    finalize(noise(a, 4), name)


def t_hazard():
    w = h = 32
    a = canvas(w, h, (230, 226, 214))
    for i in range(-h, w, 8):
        for y in range(h):
            x0 = i + y
            rect(a, x0, y, x0 + 4, y + 1, POGO)
    finalize(noise(a, 4), "T_HighLine")


# ---------------------------------------------------------------- props
def t_metal(name, c, rust=0.0, scratches=0):
    w = h = 32
    a = canvas(w, h, c)
    a += smooth_noise(w, h, 4, 10)
    a = noise(a, 6)
    for _ in range(scratches):
        x, y = RNG.integers(0, w), RNG.integers(0, h)
        ln = RNG.integers(3, 9)
        for k in range(ln):
            xx, yy = (x + k) % w, (y + k // 2) % h
            a[yy, xx] = shade(c, 1.45)
    if rust:
        m = smooth_noise(w, h, 4, 1.0)[:, :, 0] > (1.0 - rust)
        a[m] = a[m] * 0.6 + np.array((120, 70, 40)) * 0.4
    finalize(a, name)


def t_grip():
    w = h = 16
    a = canvas(w, h, (214, 128, 150))
    for y in range(0, h, 3):
        rect(a, 0, y, w, y + 1, (170, 92, 112))
    finalize(noise(a, 5), "T_Grip")


def t_skin():
    w = h = 32
    a = canvas(w, h, (214, 138, 92))
    a += smooth_noise(w, h, 4, 14)
    finalize(noise(a, 4), "T_Skin")


def t_denim():
    w = h = 32
    a = canvas(w, h, (52, 74, 128))
    for y in range(h):
        for x in range(w):
            if (x + y) % 3 == 0:
                a[y, x] = (66, 90, 146)
    a += smooth_noise(w, h, 4, 10)
    finalize(noise(a, 4), "T_Denim")


def t_sneaker():
    w = h = 32
    a = canvas(w, h, (226, 226, 222))
    rect(a, 0, 24, w, 32, (190, 190, 186))
    rect(a, 0, 22, w, 24, (150, 150, 150))
    for x in range(6, 26, 5):
        rect(a, x, 6, x + 3, 8, (90, 90, 96))
    finalize(noise(a, 5), "T_Sneaker")


def t_solid(name, c, n=5, size=16):
    a = canvas(size, size, c)
    finalize(noise(a, n), name)


def t_car_body():
    w = h = 32
    a = canvas(w, h, (232, 232, 228))
    a += smooth_noise(w, h, 2, 8)
    finalize(noise(a, 3), "T_CarBody")


def t_car_glass():
    w = h = 32
    a = canvas(w, h, (40, 52, 70))
    for i in range(w):
        a[:, i] += i * 1.2
    for y in range(h):
        x = y + 4
        if x < w:
            rect(a, x, y, x + 3, y + 1, (110, 128, 150))
    finalize(noise(a, 4), "T_CarGlass")


def t_tire():
    w = h = 32
    a = canvas(w, h, (30, 30, 32))
    yy, xx = np.ogrid[:h, :w]
    r = np.sqrt((xx - 15.5) ** 2 + (yy - 15.5) ** 2)
    a[r < 9] = (150, 152, 156)
    a[r < 4] = (90, 90, 94)
    finalize(noise(a, 4), "T_Tire")


def t_lights():
    """Atlas 32x32: 4 cuadrantes (faro, trasero, giro, patente)."""
    a = canvas(32, 32, (240, 238, 220))
    rect(a, 16, 0, 32, 16, (200, 40, 36))
    rect(a, 0, 16, 16, 32, (240, 150, 40))
    rect(a, 16, 16, 32, 32, (236, 236, 226))
    rect(a, 18, 22, 30, 26, (40, 50, 90))
    finalize(noise(a, 3), "T_CarLights")


def t_vending():
    w, h = 32, 64
    a = canvas(w, h, (210, 60, 50))
    rect(a, 3, 4, 29, 36, (225, 232, 236))
    cols = [(220, 60, 60), (60, 120, 220), (240, 200, 60), (80, 180, 90), (240, 240, 240), (230, 120, 40)]
    for row in range(3):
        for c in range(5):
            x = 5 + c * 5
            y = 7 + row * 10
            col = cols[(row * 5 + c) % len(cols)]
            rect(a, x, y, x + 3, y + 7, col)
            rect(a, x, y + 7, x + 3, y + 8, (90, 90, 90))
    rect(a, 4, 40, 20, 46, (40, 40, 44))
    rect(a, 22, 40, 28, 52, (60, 60, 64))
    rect(a, 4, 52, 28, 60, (30, 30, 34))
    finalize(noise(a, 4), "T_Vending")


def t_crate():
    w = h = 32
    a = canvas(w, h, (220, 180, 40))
    for x in range(4, 32, 8):
        rect(a, x, 6, x + 4, 26, (150, 118, 20))
    rect(a, 0, 0, w, 3, (240, 200, 60))
    rect(a, 0, 29, w, 32, (190, 150, 30))
    finalize(noise(a, 5), "T_Crate")


def t_wood():
    w = h = 32
    a = canvas(w, h, (130, 92, 58))
    for y in range(0, h, 8):
        rect(a, 0, y, w, y + 1, (80, 54, 34))
    for _ in range(40):
        x, y = RNG.integers(0, w), RNG.integers(0, h)
        rect(a, x, y, x + RNG.integers(2, 8), y + 1, (110, 76, 46))
    finalize(noise(a, 4), "T_Wood")


def t_planter():
    w = h = 32
    a = canvas(w, h, (60, 110, 54))
    a += smooth_noise(w, h, 8, 24)
    for _ in range(60):
        x, y = RNG.integers(0, w), RNG.integers(0, h)
        a[y, x] = (90, 150, 70)
    finalize(noise(a, 6), "T_Leaves")


def t_lantern():
    w = h = 32
    a = canvas(w, h, (200, 40, 36))
    for y in range(0, h, 4):
        rect(a, 0, y, w, y + 1, (150, 26, 24))
    rect(a, 0, 0, w, 3, (30, 30, 30))
    rect(a, 0, 29, w, 32, (30, 30, 30))
    rect(a, 12, 10, 20, 12, (240, 220, 200))
    rect(a, 15, 8, 17, 22, (240, 220, 200))
    finalize(noise(a, 4), "T_Lantern")


def t_sign_vertical(name, bg, fg):
    w, h = 16, 64
    a = canvas(w, h, bg)
    rect(a, 0, 0, w, 1, shade(bg, 0.6)); rect(a, 0, h - 1, w, h, shade(bg, 0.6))
    for i in range(5):
        y = 4 + i * 12
        rect(a, 3, y, 13, y + 1, fg)
        rect(a, 7, y, 9, y + 9, fg)
        if i % 2:
            rect(a, 3, y + 4, 13, y + 5, fg)
        else:
            rect(a, 3, y + 8, 7, y + 9, fg)
            rect(a, 10, y + 3, 13, y + 4, fg)
    finalize(noise(a, 4), name)


def t_ac():
    w = h = 32
    a = canvas(w, h, (214, 210, 196))
    yy, xx = np.ogrid[:h, :w]
    r = np.sqrt((xx - 11.5) ** 2 + (yy - 15.5) ** 2)
    a[r < 9] = (90, 90, 92)
    for k in range(-8, 9, 3):
        rect(a, 3, 15 + k, 20, 16 + k, (70, 70, 72))
    rect(a, 23, 6, 29, 26, (190, 186, 172))
    finalize(noise(a, 4), "T_ACUnit")


def t_pole():
    w, h = 16, 64
    a = canvas(w, h, (150, 146, 138))
    a = noise(a, 5)
    for y in (20, 22):
        rect(a, 0, y, w, y + 1, (200, 196, 60))
    rect(a, 0, 40, w, 48, (40, 40, 40))
    finalize(a, "T_Pole")


def t_water_tank():
    w = h = 32
    a = canvas(w, h, (170, 196, 186))
    for y in range(0, h, 8):
        rect(a, 0, y, w, y + 1, (130, 150, 144))
    finalize(noise(a, 5), "T_WaterTank")


# ---------------------------------------------------------------- meta / UI en mundo
FONT5x7 = {
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    " ": ["00000"] * 7,
}


def t_eye_mosaic():
    """Ojo azul + NO SLEEP en mosaico (64 x 40 teselas de 4 px)."""
    cw, ch, cs = 64, 40, 4
    grid = np.zeros((ch, cw), dtype=np.int32)  # 0 fondo, 1 azul, 2 azul oscuro, 3 blanco, 4 celeste, 5 negro
    cx, cy = 32, 11
    for y in range(ch):
        for x in range(cw):
            dx, dy = (x - cx) / 17.0, (y - cy) / 8.0
            # almendra del ojo
            if abs(dy) < (1 - dx * dx) * 1.05 and abs(dx) < 1:
                grid[y, x] = 3
                r = math.hypot((x - cx) / 1.0, (y - cy) / 1.0)
                if r < 6.5:
                    grid[y, x] = 1
                if r < 4.0:
                    grid[y, x] = 4 if r > 2.6 else 5
            # contorno
            inside = abs(dy) < (1 - dx * dx) * 1.05 and abs(dx) < 1
            edge = abs(dy) < (1 - dx * dx) * 1.05 + 0.28 and abs(dx) < 1.08
            if edge and not inside:
                grid[y, x] = 2
    # pestanas
    for k in range(-3, 4):
        x = cx + k * 5
        top = cy - int(8 * (1 - (k * 5 / 17.0) ** 2) * 1.05) - 2
        for t in range(2):
            if 0 <= top - t < ch:
                grid[top - t, x + (k > 0) - (k < 0)] = 2
    grid[cy - 2, cx + 1] = 3  # brillo
    text = "NO SLEEP"
    tx = (cw - len(text) * 6 + 1) // 2
    ty = 25
    for i, chh in enumerate(text):
        g = FONT5x7[chh]
        for yy in range(7):
            for xx in range(5):
                if g[yy][xx] == "1":
                    grid[ty + yy, tx + i * 6 + xx] = 2
    pal = {0: TECHO, 1: META, 2: (22, 40, 120), 3: (236, 240, 244), 4: (90, 150, 230), 5: (14, 18, 40)}
    W, H = cw * cs, ch * cs
    a = canvas(W, H, (110, 130, 140))  # pastina
    for y in range(ch):
        for x in range(cw):
            base = pal[int(grid[y, x])]
            v = 0.9 + RNG.random() * 0.16
            rect(a, x * cs, y * cs, x * cs + cs - 1, y * cs + cs - 1, shade(base, v))
    finalize(a, "T_EyeMosaic", dither=False)
    # textura emisiva (solo el azul) para el brillo de meta
    em = np.zeros((H, W, 3), dtype=np.float32)
    for y in range(ch):
        for x in range(cw):
            if grid[y, x] in (1, 4):
                rect(em, x * cs, y * cs, x * cs + cs - 1, y * cs + cs - 1, (60, 110, 255))
    finalize(em, "T_EyeGlow", dither=False)


def t_ring(name, broken):
    s = 64
    yy, xx = np.ogrid[:s, :s]
    r = np.sqrt((xx - 31.5) ** 2 + (yy - 31.5) ** 2)
    alpha = ((r > 22) & (r < 29)).astype(np.float32) * 255
    alpha[(r > 9) & (r < 12)] = 255  # punto central
    if broken:
        ang = np.degrees(np.arctan2(yy - 31.5, xx - 31.5)) + np.zeros((s, s))
        cut = (np.abs(ang - 90) < 14) | (np.abs(ang + 90) < 14)
        alpha[cut] = 0
        # desplazar mitades
        alpha2 = np.zeros_like(alpha)
        alpha2[:, 3:] = alpha[:, :-3] * (xx[:, 3:] > 34)
        alpha2[:, :-3] += alpha[:, 3:] * (xx[:, :-3] < 29)
        alpha = np.clip(alpha2, 0, 255)
        col = canvas(s, s, (240, 90, 70))
    else:
        col = canvas(s, s, (240, 244, 250))
    finalize(col, name, dither=False, alpha=alpha)


def t_sky():
    w, h = 256, 64
    top, hor = np.array((70, 110, 200), np.float32), np.array((190, 210, 236), np.float32)
    a = np.zeros((h, w, 3), np.float32)
    for y in range(h):
        t = (y / (h - 1)) ** 1.4
        a[y, :] = top * (1 - t) + hor * t
    # nubes pixeladas
    n = smooth_noise(w, h, 16, 1.0)[:, :, 0] + smooth_noise(w, h, 32, 0.5)[:, :, 0]
    mask = (n > 0.6) & (np.arange(h)[:, None] > 12) & (np.arange(h)[:, None] < 50)
    a[mask] = a[mask] * 0.3 + np.array((240, 244, 250)) * 0.7
    finalize(a, "T_Sky")


def t_wire():
    t_solid("T_Wire", (26, 26, 28), 2, 8)


def t_bayer():
    """Matriz de Bayer 4x4 en escala de grises para el dithering de post-proceso."""
    a = ((BAYER4 + 0.5) * 255.0 + 255.0 / 32).clip(0, 255).astype(np.uint8)
    Image.fromarray(a, "L").save(os.path.join(OUT, "T_Bayer.png"))


def main():
    t_bayer()
    t_paving(); t_tiles(); t_guide(); t_asphalt(); t_concrete(); t_curb(); t_roof_mosaic()
    t_facade("T_FacadeBeige", (196, 180, 150))
    t_facade("T_FacadeGray", (160, 160, 156))
    t_facade("T_FacadeBrown", (150, 104, 82), variant=1)
    t_facade("T_FacadeTile", (200, 196, 186), variant=1)
    t_shop("T_ShopA", (120, 110, 100), False, (40, 90, 160))
    t_shop("T_ShopB", (140, 126, 110), True, (170, 40, 40))
    t_shop("T_ShopC", (110, 100, 96), False, (40, 120, 70))
    t_shutter()
    t_plaster("T_PlasterGray", (164, 162, 156))
    t_plaster("T_PlasterBeige", (190, 176, 150))
    t_awning("T_AwningRed", (214, 70, 40), (236, 226, 210))
    t_awning("T_AwningBlue", (40, 74, 170), (52, 92, 196), 4)
    t_awning("T_AwningGreen", (40, 120, 76), (230, 226, 210))
    t_stripe("T_EdgeStripe", (232, 226, 200))
    t_hazard()
    t_metal("T_PogoRed", POGO, 0, 14)
    t_metal("T_MetalRust", (130, 62, 46), 0.35, 4)
    t_metal("T_MetalGray", (120, 124, 128), 0.0, 6)
    t_metal("T_SpringMetal", (176, 178, 184), 0.0, 4)
    t_grip(); t_skin(); t_denim(); t_sneaker()
    t_solid("T_Rubber", (34, 34, 36))
    t_solid("T_White", (236, 236, 232), 3)
    t_solid("T_Black", (24, 24, 26), 3)
    t_car_body(); t_car_glass(); t_tire(); t_lights()
    t_vending(); t_crate(); t_wood(); t_planter(); t_lantern(); t_ac(); t_pole(); t_water_tank()
    t_sign_vertical("T_SignRed", (190, 36, 36), (250, 240, 220))
    t_sign_vertical("T_SignBlue", (36, 70, 170), (250, 250, 250))
    t_sign_vertical("T_SignYellow", (230, 196, 40), (40, 30, 20))
    t_eye_mosaic()
    t_ring("T_Ring", False); t_ring("T_RingBroken", True)
    t_sky(); t_wire()
    print("TEXTURES", len(os.listdir(OUT)), OUT)


if __name__ == "__main__":
    main()
