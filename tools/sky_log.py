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
    # Translation without rotation: the sky should not move
    still = [s for s in steps if abs(s[3]) < 0.0005 and abs(s[4]) < 0.0005]
    moved = [s for s in still if s[1] or s[2]]
    print(f'frames with the camera not turning: {len(still)}, of which the sky moved: {len(moved)}')
    for s in moved[:20]:
        print(f'  {s[0]}: dx {s[1]:+d} dy {s[2]:+d} cam {s[6][5]}')


main()
