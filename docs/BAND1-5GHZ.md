# 5 GHz data-lock (band 1) — open item

## Status

| capability | band | state |
|---|---|---|
| Full data-lock (all traffic on one channel) | 0 / 2.4 GHz | works |
| Discovery (all APs via channel hop) | 0 + 1 / 2.4 + 5 GHz | works |
| Full data-lock on one 5 GHz channel | 1 / 5 GHz | **open** — band-1 RFCR not located |

Everything 2.4 GHz is done: the band-0 RX filter register (RFCR) is
`0x820f5000`, opened with `0x0000e00b` ([REGISTERS.md](REGISTERS.md)). 5 GHz
discovery works because a channel-hop scan visits 5 GHz channels and the beacons
reach the driver RX path. What is missing is the **band-1 RFCR** — the 5 GHz
equivalent of `0x820f5000` — so that all data on one 5 GHz channel can be
forwarded to the driver and captured.

## Why it is still open

To locate and prove the band-1 register, the radio has to be parked on a 5 GHz
channel **in normal driver mode** (so `monx4`'s kprobe on `nicRxProcessDataPacket`
fires). On the development device that was not reachable:

- **STA on 5 GHz** — the clean path, but it needs association to a 5 GHz AP you
  own. None was available/authorized during development.
- **SoftAP on 5 GHz** — blocked by the vendor firmware. Under the device's `RU`
  regulatory domain the driver advertises **no** 5 GHz AP channels
  (`SupportedChannelListIn5g[]` is empty); ACS falls back to 2.4 GHz and forcing
  a 5 GHz channel is rejected. Overriding the country to `US` adds 5 GHz
  frequencies to the hostapd freqlist but the driver still refuses 5 GHz AP
  operation. Confirmed by reading the chip: with SoftAP up, `0x820f5000` (band 0)
  was active on both `wlan0` and `ap0`, and every band-1 candidate read `0`.
- **ATE / factory mode** (`wifitest -O; wifitest -r -c 44`) does tune the chip to
  a 5 GHz channel, but test-mode RX bypasses the normal driver RX path, so the
  kprobe capture sees nothing. ATE can *find* the register but cannot *prove* a
  live normal-mode capture.

So closing this needs one thing: a 5 GHz link in normal mode (associate to a
5 GHz AP you own).

## How to close it

On any MT6789 device once it is associated to a 5 GHz AP (radio parked on a
5 GHz channel, band 1 active):

1. **Read registers in normal mode** with `priv_driver_get_mcr` (no link drop) —
   `scripts/read_reg.sh <addr>`, or directly:

   ```sh
   GM=0x$(awk '$3=="priv_driver_get_mcr"{print $1}' /proc/kallsyms)
   insmod monx.ko ifname=wlan0 a1=$GM c1="get_mcr <addr>"
   dmesg | grep 'command result is'      # -> command result is 0x........
   ```

2. **Find the band-1 RFCR by differencing.** It is the register that reads the
   filter default (`0x000cef1b`, or similar with `DROP_*` bits set) while on
   5 GHz and `0x00000000` while the 5 GHz radio is idle. Band-0 RFCR is
   `0x820f5000`; band 1 sits at a parallel address. Candidates already confirmed
   `0` while idle (re-read them while 5 GHz is active): `0x82105000`, `0x82115000`,
   `0x82125000`, `0x82135000`, `0x820f9000`. Widen with `hqascan` if none hit.

3. **Open it and capture**, exactly like band 0: `set_mcr <band1_rfcr> 0x0000e00b`,
   then `mon_capture.sh` on the 5 GHz channel, and confirm foreign data frames in
   the pcap.

The tooling (`monx.ko`, `monx4.ko`, the scripts) is unchanged — only the register
address differs. Everything else is proven on band 0.
