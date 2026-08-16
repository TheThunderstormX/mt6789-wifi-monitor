# Chip registers

## RX filter register (RFCR)

```
0x820f5000   band 0 / 2.4 GHz RX filter control
  default : 0x000cef1b   drops everything not addressed to us
  open    : 0x0000e00b   forwards all frames on the channel to the driver
```

Write the open value to capture; write the default back when done. The write is
volatile (a reboot restores it) and does not drop the association when done in
normal mode via `set_mcr` (see [MECHANISM.md](MECHANISM.md)).

### How it was located

The mt76 open-source headers give an `MT_WF_RFCR` base, but those addresses read
back as zero on MT6789 — the memory map differs. So the register was found
empirically by **snapshot differencing** over the live register space
`0x820e0000 … 0x82100000` (32768 words, read via HQA `0x1300`):

```
A, B   two snapshots at rest        (identical pairs drop the ~3900 free-running counters)
C, D   two snapshots during RX      (state with the filter open)
E      a snapshot at rest again     (confirms the value returns)

wanted register:  A == B == E,  C == D,  C != A
```

That narrowed 32768 registers to ~12 candidates; matching the changed bits
against the mt76 connac `DROP_*` map identified the filter.

### Filter bit map (mt76 connac family)

```
BIT(1)  DROP_FCSFAIL        BIT(11) DROP_OTHER_BEACON
BIT(3)  DROP_VERSION        BIT(12) DROP_FRAME_REPORT
BIT(4)  DROP_PROBEREQ       BIT(13) DROP_CTL_RSV
BIT(5)  DROP_MCAST          BIT(14) DROP_CTS
BIT(6)  DROP_BCAST          BIT(15) DROP_RTS
BIT(8)  DROP_A3_MAC         BIT(16) DROP_DUPLICATE
BIT(9)  DROP_A3_BSSID       BIT(17) DROP_OTHER_BSS
BIT(10) DROP_A2_BSSID       BIT(18) DROP_OTHER_UC
                            BIT(21) DROP_UNWANTED_CTL
```

To receive the whole channel, clear the address/BSS drop bits (8, 9, 10, 17, 18)
and the control drops you don't want. `0x0000e00b` is the value used in practice.

A band-0 sub-filter at `0x820f3080` (bits 8/9 = `DROP_A3_MAC` / `DROP_A3_BSSID`)
was an earlier candidate; the register that actually gates capture is the RFCR
at `0x820f5000`. The band-1 (5 GHz) RFCR is not mapped yet — 5 GHz data-lock is
the open item, see [BAND1-5GHZ.md](BAND1-5GHZ.md) for the exact method to find it.

## Two ways to write a register

- **`set_mcr` (normal mode)** — the driver private handler, called from `monx.ko`.
  The link stays up. This is what capture uses.
- **HQA `0x1301` `hqa_mac_bbp_reg_write` (test mode)** — reaches the chip through
  the ATE path, but resets it into test mode and drops the Wi-Fi link. Fine for
  offline register probing, not for live capture.

Both are volatile; a reboot restores defaults.

## Reading a register (normal mode)

The driver also exports `priv_driver_get_mcr`, which reads a register without
leaving normal mode — the association stays up. `monx.ko` calls it the same way
it calls `set_mcr`; the value is printed by the driver to the kernel log:

```sh
GM=0x$(awk '$3=="priv_driver_get_mcr"{print $1}' /proc/kallsyms)
insmod monx.ko ifname=wlan0 a1=$GM c1="get_mcr 0x820f5000"
dmesg | grep 'command result is'      # -> command result is 0x000cef1b
```

`scripts/read_reg.sh <addr>` wraps this. Reading via HQA (`hqax`, command
`0x1300`) also works but drops the link (test mode); prefer `get_mcr` for live
inspection. This is what locates the band-1 RFCR — see [BAND1-5GHZ.md](BAND1-5GHZ.md).

## NEVER write these — permanent damage

```
HQA_WriteEEPROM / HQA_WriteBulkEEPROM / HQA_WriteEFuseFromBuffer
HQA_eFusePhysicalWrite / HQA_eFuseLogicalWrite     (0x1308, 0x131a, ...)
wifitest -e / -K / -u
```

EFUSE is burned physically. A corrupted radio calibration bricks Wi-Fi for good.
Register writes (`set_mcr` / `0x1301`) are volatile and safe — a reboot heals them.
