#!/usr/bin/env python3
# Inventory access points and probing clients from a raw monx4 capture.
# Usage: python3 dev_inventory.py monx4_cap.bin
# Best fed a channel-hop capture (hop_capture.sh) for a full-area survey.
import struct, sys
d = open(sys.argv[1], 'rb').read()

def mac(b): return ':'.join('%02x' % x for x in b)

def find_beacon(rec):
    L = len(rec)
    for o in range(0x14, L - 40):
        if rec[o] == 0x80 and rec[o+1] == 0x00:         # beacon, no DS bits
            if rec[o+4:o+10] != b'\xff'*6: continue       # DA must be broadcast
            a2 = rec[o+10:o+16]; a3 = rec[o+16:o+22]
            if a2 != a3 or a2[:3] == b'\x00\x00\x00': continue
            return o
    return -1

def parse_beacon(rec, o):
    bssid = mac(rec[o+10:o+16]); ssid = ''; chan = 0
    p = o + 36; L = len(rec)                              # skip fixed beacon params
    while p + 2 <= L:
        t = rec[p]; ln = rec[p+1]; p += 2
        if p + ln > L: break
        if t == 0: ssid = rec[p:p+ln].decode('latin1', 'replace')   # SSID element
        elif t == 3 and ln >= 1: chan = rec[p]            # DS parameter set
        p += ln
    return bssid, ssid, chan

aps = {}; clients = {}; i = 0; frames = 0
while i + 2 <= len(d):
    l = struct.unpack_from('<H', d, i)[0]; i += 2
    if l < 24 or i + l > len(d): break
    rec = d[i:i+l]; i += l; frames += 1
    o = find_beacon(rec)
    if o >= 0:
        b, s, c = parse_beacon(rec, o)
        if b not in aps or (c and not aps[b][1]): aps[b] = (s, c)
        continue
    # probe-request (client discovery): FC=0x40, addr2 = client
    for o in range(0x14, l - 24):
        if rec[o] == 0x40 and (rec[o+1] & 3) == 0:
            a2 = rec[o+10:o+16]
            if a2[:3] not in (b'\x00\x00\x00', b'\xff\xff\xff'):
                clients[mac(a2)] = clients.get(mac(a2), 0) + 1
            break

chans = {}
for b, (s, c) in aps.items(): chans[c] = chans.get(c, 0) + 1
print(f"frames: {frames}")
print(f"\n=== ACCESS POINTS nearby: {len(aps)} (channels: {sorted(k for k in chans if k)}) ===")
for b in sorted(aps, key=lambda x: (aps[x][1] or 99, x)):
    s, c = aps[b]; print(f"  ch{c:>3}  {b}  \"{s}\"")
print(f"\n=== probe-requests from clients: {len(clients)} ===")
for m, n in sorted(clients.items(), key=lambda x: -x[1])[:25]: print(f"  {m}  x{n}")
