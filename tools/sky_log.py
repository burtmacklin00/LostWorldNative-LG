"""Compares the sky's scroll (2D back layers) with the world camera (3D) in a
LW_SKYLOG log: how many 2D pixels the sky moves per radian the camera turns,
whether the 2D frames lag the 3D ones, and where they disagree.
Usage: python tools/sky_log.py out/sky.log [row [from to]]: row is an index into the
logged rows (every 8th screen row); from-to prints those frames step by step"""
import math
import sys


def wrap(d, m=512):
    return (d + m // 2) % m - m // 2


def main():
    path = sys.argv[1]
    row = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    table = tuple(map(int, sys.argv[3:5])) if len(sys.argv) > 4 else None   # index into the logged rows (every 8th screen row)
    d2, d3, pres = {}, {}, []
    for line in open(path):
        f = line.split()
        if not f:
            continue
        if f[0] == '2d':
            d2[int(f[1])] = [tuple(map(int, r.split(','))) for r in f[2:]]
        elif f[0] == '3d' and len(f) > 3 and f[2] == 'axes':
            a = list(map(float, f[3:12]))
            proj = list(map(float, f[17:21]))
            vp = list(map(float, f[22:26]))
            fwd = (-a[2], -a[5], -a[8])
            yaw = math.atan2(fwd[0], fwd[2])
            pitch = math.asin(max(-1, min(1, fwd[1])))
            roll = math.atan2(a[3], a[4])
            d3[int(f[1])] = (yaw, pitch, roll, proj, vp, tuple(map(float, f[13:16])))
        elif f[0] == 'p':
            pres.append(tuple(f[1:]))
    frames = sorted(set(d2) & set(d3))
    print(f'{len(d2)} 2D frames, {len(d3)} 3D world frames, {len(frames)} both, {len(pres)} presents')
    if pres:
        unpaired = sum(1 for p in pres if p[5] == '0')
        print(f'presents with the sky not interpolated (unpaired): {unpaired} ({100 * unpaired / len(pres):.1f}%)')
        wp = [p for p in pres if int(p[0]) in d3]
        if wp:
            un = [p for p in wp if p[5] == '0']
            print(f'  during world frames: {len(un)} of {len(wp)} ({100 * len(un) / len(wp):.1f}%)')
            for p in un[:15]:
                print('   ', ' '.join(p))
    # Per frame steps
    steps = []
    for a, b in zip(frames, frames[1:]):
        if b != a + 1:
            continue
        ra, rb = d2[a][row], d2[b][row]
        if ra[0] < 0 or ra[0] != rb[0]:
            continue
        dx, dy = wrap(rb[1] - ra[1]), wrap(rb[2] - ra[2])
        ya, pa, rla = d3[a][:3]
        yb, pb, rlb = d3[b][:3]
        dyaw = math.remainder(yb - ya, 2 * math.pi)
        steps.append((b, dx, dy, dyaw, pb - pa, rlb - rla, d3[b]))
    if table:
        print('frame dx dy yaw_step pitch_step(deg) roll yaw pitch(deg) layer x y cam')
        for s in steps:
            if table[0] <= s[0] <= table[1]:
                r = d2[s[0]][row]
                print(s[0], s[1], s[2], f'{math.degrees(s[3]):+.3f} {math.degrees(s[4]):+.3f} {math.degrees(s[6][2]):+.2f} '
                      f'{math.degrees(s[6][0]):+.2f} {math.degrees(s[6][1]):+.2f}', r, s[6][5])
    if not steps:
        print('no consecutive frames with sky')
        return
    # Lag: correlate the 2D steps with the 3D steps k frames apart
    by = {s[0]: s for s in steps}
    for k in range(-3, 4):
        num = den1 = den2 = 0
        for s in steps:
            t = by.get(s[0] + k)
            if t is None:
                continue
            num += s[1] * t[3]
            den1 += s[1] ** 2
            den2 += t[3] ** 2
        c = num / math.sqrt(den1 * den2) if den1 and den2 else 0
        print(f'lag {k:+d} (2D frame n vs 3D frame n{k:+d}): correlation x/yaw {c:+.3f}')
    moving = [s for s in steps if abs(s[3]) > 0.002]
    if moving:
        ks = sorted(s[1] / s[3] for s in moving)
        k = ks[len(ks) // 2]
        p = moving[len(moving) // 2][6]
        print(f'sky x pixels per radian of yaw: median {k:.1f} (10%: {ks[len(ks) // 10]:.1f}, 90%: {ks[9 * len(ks) // 10]:.1f}); '
              f'3D centre px/rad = proj[0]*vp_w/2 = {p[3][0] * p[4][2] / 2:.1f}')
        res = [(s[0], s[1] - k * s[3]) for s in steps]
        big = [r for r in res if abs(r[1]) > 1.5]
        print(f'frames where the sky step differs from k*yaw by >1.5 px: {len(big)} of {len(res)}')
        for fr, r in big[:40]:
            s = by[fr]
            print(f'  {fr}: dx {s[1]:+d} dy {s[2]:+d} yaw {math.degrees(s[3]):+.3f} pitch {math.degrees(s[4]):+.3f} '
                  f'roll {math.degrees(s[5]):+.3f} cam {s[6][5]}')
    mp = [s for s in steps if abs(s[4]) > 0.002]
    if mp:
        ks = sorted(s[2] / s[4] for s in mp)
        print(f'sky y pixels per radian of pitch: median {ks[len(ks) // 2]:.1f} (10%: {ks[len(ks) // 10]:.1f}, 90%: {ks[9 * len(ks) // 10]:.1f})')
    rolls = [abs(math.degrees(d3[f][2])) for f in frames]
    print(f'roll: max {max(rolls):.2f} deg, frames over 1 deg: {sum(r > 1 for r in rolls)}')
    smoothness(d2, d3, pres, row)
    # Translation without rotation: the sky should not move
    still = [s for s in steps if abs(s[3]) < 0.0005 and abs(s[4]) < 0.0005]
    moved = [s for s in still if s[1] or s[2]]
    print(f'frames with the camera not turning: {len(still)}, of which the sky moved: {len(moved)}')
    for s in moved[:20]:
        print(f'  {s[0]}: dx {s[1]:+d} dy {s[2]:+d} cam {s[6][5]}')


def smoothness(d2, d3, pres, row):
    """How the sky of screen row 40 (logged row 5) moves from present to present
    against the world camera, from the sub-pixel position in the 'p' lines
    (sky_motion.h) and, for comparison, as the whole-pixel scroll alone would
    be drawn: the error against the camera (a running median removes the slow
    drift of a sky that does not follow it exactly) and how often it changes
    by more than half a pixel between presents, which is what shows as a tremor"""
    K = 4096 / (2 * math.pi)
    if not any(len(p) >= 12 for p in pres) or row != 5:
        print('(no sub-pixel sky positions in the log, or not row 5: smoothness not measured)')
        return
    # camera yaw in sky pixels, unwrapped, single-frame blips smoothed
    yaw = {}
    acc = 0.0
    prev = None
    for n in sorted(d3):
        acc = K * d3[n][0] if prev is None or n - prev > 3 else acc + K * math.remainder(d3[n][0] - d3[prev][0], 2 * math.pi)
        yaw[n] = acc
        prev = n
    for n in sorted(yaw):
        if n - 1 in yaw and n + 1 in yaw:
            m = (yaw[n - 1] + yaw[n + 1]) / 2
            if abs(yaw[n] - m) > 1.5 and abs(yaw[n + 1] - yaw[n - 1]) < 3:
                yaw[n] = m
    def positions(sub):
        out = []
        last = None
        for p in pres:
            n, bfrom, sfrom, sto, t, paired = int(p[0]), int(p[1]), int(p[2]), int(p[3]), float(p[4]), p[5] == '1'
            if n not in d2 or d2[n][row][0] < 0 or n - 1 not in yaw or n not in yaw or bfrom != n - 1 or (sub and len(p) < 12):
                last = None
                continue
            x = d2[n][row][1]
            if sub:
                pos = x + float(p[7]) + (float(p[8]) * t * (sto - sfrom) if paired else 0) if p[6] == '1' else x
            else:
                dx = wrap(x - d2[sfrom][row][1]) if paired and sfrom in d2 and d2[sfrom][row][0] == d2[n][row][0] else 0
                pos = x + (dx * t if abs(dx) < 100 else 0)
            tru = yaw[n - 1] + t * (yaw[n] - yaw[n - 1])
            if last is not None and last[0] >= n - 1:
                upos = last[2] + wrap(round((pos - last[1]) * 1000), 512000) / 1000
            else:
                upos = pos
            out.append((n, upos - tru))
            last = (n, pos, upos)
        # the slow drift removed
        res = []
        W = 30
        for i, (n, e) in enumerate(out):
            win = sorted(e2 for n2, e2 in out[max(0, i - W):i + W + 1] if abs(n2 - n) <= W)
            c = win[len(win) // 2]
            if abs(e - c) < 6:
                res.append((n, e - c))
        return res
    for name, sub in (('whole-pixel scroll, extrapolated by its last step', False), ('sub-pixel (as drawn)', True)):
        res = positions(sub)
        if len(res) < 2:
            continue
        ch = [res[i + 1][1] - res[i][1] for i in range(len(res) - 1) if res[i + 1][0] - res[i][0] <= 1]
        big = sum(1 for c in ch if abs(c) > 0.5)
        mean = sum(abs(e) for _, e in res) / len(res)
        rms = math.sqrt(sum(c * c for c in ch) / max(1, len(ch)))
        print(f'sky vs camera, {name}: mean error {mean:.3f} px, change between presents rms {rms:.3f} px, '
              f'over 0.5 px in {big} of {len(ch)} ({100 * big / max(1, len(ch)):.1f}%)')


main()
