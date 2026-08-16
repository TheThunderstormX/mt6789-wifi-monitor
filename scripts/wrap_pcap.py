#!/usr/bin/env python3
# Wrap raw monx4 captures into a radiotap 802.11 pcap for Wireshark / tshark.
# Usage: python3 wrap_pcap.py monx4_cap.bin [out.pcap]
#
# The raw stream is a sequence of <u16 length><raw chip RX buffer> records.
# Each buffer holds the connac RXD/RXV followed by the 802.11 frame, so the
# frame offset varies; we locate the 802.11 header generically rather than
# assuming a fixed descriptor size.
import struct, sys
data = open(sys.argv[1], 'rb').read()
out = sys.argv[2] if len(sys.argv) > 2 else 'capture.pcap'

def mac(b): return ':'.join('%02x' % x for x in b)

# Generic 802.11 header search, not tied to any BSSID:
# beacon/probe-resp (addr2==addr3=BSSID), probe-req, data with LLC/SNAP aa-aa-03.
def find_hdr(rec):
    L = len(rec)
    for o in range(0x14, L - 24):
        fc0 = rec[o]; sub = (fc0 >> 4) & 0xf; typ = (fc0 >> 2) & 3
        if fc0 & 3: continue                        # protocol version 0 only
        if typ == 0:                                # management
            a1 = rec[o+4:o+10]; a2 = rec[o+10:o+16]; a3 = rec[o+16:o+22]
            if a2 == a3 and a2[:3] not in (b'\x00\x00\x00', b'\xff\xff\xff'): return o
            if sub == 4 and a1 == b'\xff'*6 and a2[:3] not in (b'\x00\x00\x00', b'\xff\xff\xff'): return o
        elif typ == 2:                              # data
            hlen = 26 if (sub & 8) else 24          # QoS adds 2 bytes
            if o+hlen+3 <= L and rec[o+hlen:o+hlen+3] == b'\xaa\xaa\x03': return o
    return -1

pc = struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 262144, 127)  # pcap hdr, linktype 127 = radiotap
recs = []; nn = 0; skip = 0; i = 0
while i + 2 <= len(data):
    l = struct.unpack_from('<H', data, i)[0]; i += 2
    if l < 24 or i + l > len(data): break
    rec = data[i:i+l]; i += l
    o = find_hdr(rec)
    if o < 0: skip += 1; continue
    frame = rec[o:]
    pkt = struct.pack('<BBHI', 0, 0, 8, 0) + frame  # minimal radiotap header + 802.11
    recs.append(struct.pack('<IIII', nn, 0, len(pkt), len(pkt)) + pkt); nn += 1
open(out, 'wb').write(pc + b''.join(recs))
print(f"frames in pcap: {nn} (skipped {skip}) -> {out}")
