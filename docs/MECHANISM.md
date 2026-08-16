# How it works

Target: the internal Wi-Fi of MediaTek MT6789 (Helio G99), a connac2-class
chip driven by the closed `wlan_drv_gen4m` module. There is no Linux `cfg80211`
monitor mode: the driver never advertises the `monitor` interface type, the
promiscuous entry points (`nicRxEnablePromiscuousMode`, `wlanSetPromiscuousMode`,
`mt_op_set_rx_filter`) are stubs (`mov w0,0; ret`), and the built-in sniffer code
is compiled in but has no call path from userspace. `iw phy0 info` lists
`managed`, `AP`, `P2P` — no `monitor`.

But the radio hears everything. In factory RX test the chip reports tens of
thousands of detected frames while `driver_rx_count` stays 0 — every foreign
frame is dropped **inside the chip** by the RX filter, before the driver.

The capture is two steps.

## 1. Open the hardware RX filter

The chip's receive filter register (RFCR) is at **`0x820f5000`** (band 0 / 2.4 GHz).
Default value `0x000cef1b` drops everything not addressed to us. Writing
**`0x0000e00b`** clears the `DROP_*` bits, so the chip forwards every frame on
the tuned channel to the driver RX path. See [REGISTERS.md](REGISTERS.md).

The write is done in **normal mode** through the driver's private `set_mcr`
handler (`monx.ko`), so the association stays up. Do **not** use the HQA/ATE
path for this — it puts the chip into test mode and drops the link
(see [HQA-PROTOCOL.md](HQA-PROTOCOL.md)). The write is volatile; a reboot
restores the default. A scan rewrites the filter, so during channel-hopping the
value is re-applied in a loop.

## 2. Capture at the driver RX entry

`monx4.ko` puts a kprobe on `nicRxProcessDataPacket` and
`nicRxProcessMgmtPacket`. On entry, `x1` is the `SW_RFB`; the raw chip receive
buffer pointer is at `*(SW_RFB + 0x18)`. That buffer is:

```
+0x00  connac RXD (descriptor) + RXV   (~0x44 bytes; RXByteCount = word0 & 0x3fff)
+0x44  802.11 frame
```

The probe copies the raw buffer (length-prefixed) into a kfifo exposed at
`/proc/monx4`. The RXD size is not assumed downstream — the 802.11 header is
located generically when wrapping to pcap.

## 3. Post-process on the PC

- `wrap_pcap.py` — finds the 802.11 header in each raw record and wraps it in a
  minimal radiotap header, producing a standard pcap for Wireshark / tshark /
  airodump-ng.
- `dev_inventory.py` — lists access points (BSSID / SSID / channel) and the
  clients seen sending probe-requests.

## Two capture modes

- **Single channel, full data** (`mon_capture.sh`) — stay associated to an AP,
  which locks the radio to that AP's channel, and capture all of its traffic
  (every station, uplink and downlink).
- **Channel hop, whole area** (`hop_capture.sh`) — the Android scan drives the
  radio across every channel in 2.4 and 5 GHz; you see every nearby AP in one
  pass, but only the brief dwell of each channel.

## Reversibility and safety

- Register writes are volatile RAM — a reboot restores defaults.
- `rmmod monx4` removes the probe; nothing persists.
- No writes to flash, EEPROM, or efuse at any point.

## Limits (hardware, not configuration)

- One radio = one channel at a time. "Whole area" means hopping, not parallel.
- 2.4 and 5 GHz only (802.11ac chip). No 6 GHz.
- WPA2 unicast payloads stay encrypted under each station's key; capture sees
  the frames, not the cleartext. Same as any sniffer.
- Continuous **data** capture on 5 GHz needs the band-1 RFCR (the analogue of
  `0x820f5000` for band 1), which is not mapped yet. 5 GHz **discovery**
  (beacons via hopping) already works.
