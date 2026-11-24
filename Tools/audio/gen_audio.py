"""
No Sleep Pogo - sintetizador procedural de efectos de sonido lo-fi (22 kHz, 16 bits).

Uso:  python -I Tools/audio/gen_audio.py <carpeta_salida>
"""
import os
import sys
import wave

import numpy as np

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "SourceArt", "Audio")
os.makedirs(OUT, exist_ok=True)
SR = 22050
RNG = np.random.default_rng(77)


def t(dur):
    return np.arange(int(SR * dur)) / SR


def env(n, a=0.005, d=0.2, curve=4.0):
    x = np.arange(n) / SR
    att = np.clip(x / max(a, 1e-4), 0, 1)
    dec = np.exp(-np.maximum(x - a, 0) * curve / max(d, 1e-4))
    return att * dec


def lowpass(x, k=0.2):
    y = np.zeros_like(x)
    kk = np.broadcast_to(np.asarray(k, dtype=np.float64), x.shape)
    acc = 0.0
    for i, v in enumerate(x):
        acc += kk[i] * (v - acc)
        y[i] = acc
    return y


def bitcrush(x, bits=10):
    q = 2 ** (bits - 1)
    return np.round(x * q) / q


def save(name, x, gain=0.8, crush=11):
    x = np.asarray(x, dtype=np.float64)
    m = np.max(np.abs(x)) or 1.0
    x = bitcrush(x / m * gain, crush)
    data = (np.clip(x, -1, 1) * 32767).astype(np.int16)
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())


def boing(dur, f0, f1, vib=18.0, depth=0.08, decay=3.0):
    tt = t(dur)
    f = f0 + (f1 - f0) * (1 - np.exp(-tt * 9))
    f = f * (1 + depth * np.sin(2 * np.pi * vib * tt) * np.exp(-tt * 2))
    ph = 2 * np.pi * np.cumsum(f) / SR
    s = np.sin(ph) + 0.35 * np.sign(np.sin(ph * 2)) * 0.5
    return s * env(len(tt), 0.004, dur, decay)


def main():
    # Salto: resorte que se libera (se reproduce con pitch segun carga)
    save("SFX_Boing", boing(0.55, 180, 520, 22, 0.12, 4.0) + 0.3 * boing(0.55, 360, 1040, 22, 0.1, 6.0))
    save("SFX_BounceSmall", boing(0.22, 300, 520, 30, 0.08, 6.0), 0.6)
    # Compresion del resorte: chirrido ascendente
    tt = t(0.45)
    f = 90 + 260 * (tt / 0.45) ** 1.5
    ph = 2 * np.pi * np.cumsum(f) / SR
    creak = np.sign(np.sin(ph)) * 0.4 + np.sin(ph * 3.01) * 0.3
    creak *= (0.6 + 0.4 * np.sin(2 * np.pi * 31 * tt)) * np.clip(tt / 0.05, 0, 1)
    save("SFX_Compress", lowpass(creak, 0.25), 0.45)
    # Clic de carga completa
    n = int(SR * 0.06)
    click = RNG.normal(0, 1, n) * env(n, 0.0005, 0.01, 6) + np.sin(2 * np.pi * 2400 * t(0.06)) * env(n, 0.0005, 0.03, 5)
    save("SFX_ChargeFull", click, 0.7)
    # Aterrizaje: golpe grave
    tt = t(0.35)
    thud = np.sin(2 * np.pi * (70 + 60 * np.exp(-tt * 30)) * tt) * env(len(tt), 0.002, 0.3, 5)
    thud += lowpass(RNG.normal(0, 1, len(tt)), 0.15) * env(len(tt), 0.001, 0.08, 6) * 0.8
    save("SFX_Land", thud, 0.9)
    # Aterrizaje fuerte (techo / meta)
    tt = t(0.5)
    thud2 = np.sin(2 * np.pi * (50 + 50 * np.exp(-tt * 20)) * tt) * env(len(tt), 0.002, 0.45, 4)
    thud2 += lowpass(RNG.normal(0, 1, len(tt)), 0.2) * env(len(tt), 0.001, 0.12, 5)
    save("SFX_LandHeavy", thud2, 0.95)
    # Golpe metalico (choque con auto)
    tt = t(0.6)
    clang = sum(np.sin(2 * np.pi * f * tt) / (i + 1) for i, f in enumerate((310, 523, 787, 1130)))
    clang = clang * env(len(tt), 0.001, 0.5, 5) + RNG.normal(0, 1, len(tt)) * env(len(tt), 0.001, 0.05, 6)
    save("SFX_Crash", clang, 0.9)
    # Reaparicion: barrido
    tt = t(0.6)
    sweep = lowpass(RNG.normal(0, 1, len(tt)), 0.08 + 0.3 * tt / 0.6)
    sweep += 0.3 * np.sin(2 * np.pi * np.cumsum(200 + 900 * tt / 0.6) / SR)
    save("SFX_Respawn", sweep * np.sin(np.pi * tt / 0.6), 0.6)
    # Meta: arpegio
    notes = [523.25, 659.25, 783.99, 1046.5, 1318.5]
    seq = np.zeros(int(SR * 1.6))
    for i, f in enumerate(notes):
        st = int(i * SR * 0.11)
        tt = t(0.9)
        tone = (np.sign(np.sin(2 * np.pi * f * tt)) * 0.3 + np.sin(2 * np.pi * f * tt)) * env(len(tt), 0.003, 0.8, 4)
        seq[st:st + len(tt)] += tone[: len(seq) - st]
    save("SFX_Goal", seq, 0.75)
    # Checkpoint
    seq = np.zeros(int(SR * 0.5))
    for i, f in enumerate((880, 1318.5)):
        st = int(i * SR * 0.08)
        tt = t(0.35)
        tone = np.sin(2 * np.pi * f * tt) * env(len(tt), 0.002, 0.3, 5)
        seq[st:st + len(tt)] += tone[: len(seq) - st]
    save("SFX_Checkpoint", seq, 0.55)
    # Toldo de linea alta
    tt = t(0.3)
    save("SFX_Awning", np.sin(2 * np.pi * 1567 * tt) * env(len(tt), 0.002, 0.25, 5) + 0.5 * np.sin(2 * np.pi * 2093 * tt) * env(len(tt), 0.05, 0.2, 5), 0.5)
    # Motor de auto (loop de 2 s, sin cortes: frecuencias enteras)
    tt = t(2.0)
    eng = np.sign(np.sin(2 * np.pi * 45 * tt)) * 0.5 + np.sin(2 * np.pi * 90 * tt) * 0.4 + np.sin(2 * np.pi * 22 * tt) * 0.3
    eng = lowpass(eng + RNG.normal(0, 0.15, len(tt)), 0.18)
    save("SFX_CarLoop", eng, 0.7)
    # Bocina
    tt = t(0.5)
    horn = (np.sign(np.sin(2 * np.pi * 392 * tt)) + np.sign(np.sin(2 * np.pi * 494 * tt))) * env(len(tt), 0.01, 0.6, 1.5)
    save("SFX_CarHorn", lowpass(horn, 0.3), 0.6)
    # UI
    save("SFX_UIClick", np.sin(2 * np.pi * 1200 * t(0.05)) * env(int(SR * 0.05), 0.001, 0.04, 6), 0.5)
    save("SFX_UIMove", np.sin(2 * np.pi * 700 * t(0.04)) * env(int(SR * 0.04), 0.001, 0.03, 6), 0.35)
    # Pogo que cae al soltarlo
    tt = t(0.8)
    drop = np.zeros(len(tt))
    for k, st in enumerate((0.0, 0.32, 0.5, 0.6)):
        i0 = int(st * SR)
        tt2 = t(0.2)
        hit = sum(np.sin(2 * np.pi * f * tt2) for f in (420, 690, 1010)) * env(len(tt2), 0.001, 0.15, 6) / (k + 1)
        drop[i0:i0 + len(tt2)] += hit[: len(drop) - i0]
    save("SFX_PogoDrop", drop, 0.7)
    # Ambiente de ciudad (loop 8 s)
    tt = t(8.0)
    amb = lowpass(RNG.normal(0, 1, len(tt)), 0.03) * 2.0
    amb += 0.15 * np.sin(2 * np.pi * 60 * tt)
    for st in (1.3, 1.45, 4.7, 4.82, 4.95, 6.6):  # pajaritos
        i0 = int(st * SR)
        tb = t(0.09)
        ch = np.sin(2 * np.pi * (3200 + 900 * np.sin(2 * np.pi * 30 * tb)) * tb) * env(len(tb), 0.005, 0.08, 4) * 0.25
        amb[i0:i0 + len(tb)] += ch
    fade = int(SR * 0.3)  # suavizar el loop
    amb[:fade] = amb[:fade] * np.linspace(0, 1, fade) + amb[-fade:] * np.linspace(1, 0, fade)
    save("AMB_City", amb[: len(amb) - fade], 0.4)
    # Musica: loop lo-fi 4 compases a 96 bpm
    bpm = 96
    beat = 60 / bpm
    bars = 8
    n = int(SR * beat * 4 * bars)
    mus = np.zeros(n)
    prog = [(220.0, [0, 3, 7]), (174.61, [0, 4, 7]), (261.63, [0, 4, 7]), (196.0, [0, 4, 7])]
    pent = [0, 3, 5, 7, 10, 12, 15]
    mel_rng = np.random.default_rng(5)
    for bar in range(bars):
        root, chord = prog[bar % 4]
        for b in range(8):  # corcheas
            st = int((bar * 4 + b * 0.5) * beat * SR)
            tb = t(beat * 0.5)
            bass_f = root / 2 * (2 ** (chord[(b // 2) % 3] / 12)) if b % 2 == 0 else root / 2
            bass = np.sign(np.sin(2 * np.pi * bass_f * tb)) * 0.18 * env(len(tb), 0.003, beat * 0.5, 2)
            mus[st:st + len(tb)] += bass[: n - st]
            if b in (0, 3, 5, 6) or mel_rng.random() < 0.3:
                note = root * 2 * (2 ** (pent[mel_rng.integers(0, len(pent))] / 12))
                tri = (2 / np.pi) * np.arcsin(np.sin(2 * np.pi * note * tb))
                mus[st:st + len(tb)] += (tri * 0.22 * env(len(tb), 0.004, beat * 0.4, 3))[: n - st]
            # hi-hat / bombo
            hh = RNG.normal(0, 1, int(SR * 0.03)) * env(int(SR * 0.03), 0.0005, 0.02, 6) * (0.10 if b % 2 else 0.05)
            mus[st:st + len(hh)] += hh[: n - st]
            if b in (0, 4):
                tk = t(0.15)
                kick = np.sin(2 * np.pi * (50 + 90 * np.exp(-tk * 40)) * tk) * env(len(tk), 0.001, 0.12, 4) * 0.5
                mus[st:st + len(tk)] += kick[: n - st]
            if b in (2, 6):
                sn = lowpass(RNG.normal(0, 1, int(SR * 0.1)), 0.5) * env(int(SR * 0.1), 0.001, 0.08, 5) * 0.2
                mus[st:st + len(sn)] += sn[: n - st]
    save("MUS_Loop", mus, 0.55, 12)
    print("AUDIO", sorted(os.listdir(OUT)))


if __name__ == "__main__":
    main()
