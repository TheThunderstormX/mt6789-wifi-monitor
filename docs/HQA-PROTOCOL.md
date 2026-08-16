# HQA / ATE protocol

HQA (also called ATE) is MediaTek's factory RF-test path. It gives direct
read/write access to the chip's MAC/BBP registers, which is how the RX filter
register was located and probed ([REGISTERS.md](REGISTERS.md)). It is a
reverse-engineering aid, not part of the capture path.

> **WARNING — HQA drops the Wi-Fi link.** Entering HQA/ATE (the `0x8BF0` ioctl)
> switches the chip into test mode and tears down the association. Use it only
> for offline register probing. For live capture, write registers in normal mode
> with `set_mcr` (`monx.ko`) instead — the link stays up.

## Transport

```
ioctl(socket(AF_INET, SOCK_DGRAM, 0), 0x8BF0, &iwreq)   binary HQA commands
ioctl(...,                          0x8BEF, ...)          text ("iwpriv") commands

struct iwreq:
  ifr_name        = "wlan0"
  u.data.pointer  = buffer with the request packet
  u.data.length   = size of the reply window (too small => the driver truncates)
```

## Packet format

```
offset  size  field
   0      4    signature: 18 14 28 80
   4      2    type: 00 05 request / 08 80 response   (response = command ran)
   6      2    command id, BIG-endian
   8      2    data length, BIG-endian
  10      2    counter: ff ff
  12    ...    data

register read (0x1300) reply:
  12      2    status (00 00 = ok)
  14      4    register value
```

## Command table

All 148 commands with their real names are in
[../reference/hqa-command-table.txt](../reference/hqa-command-table.txt),
extracted from the driver's group tables in `.rodata` (7 group records of 16
bytes: pointer / count / base id; `id = base + index`). Static handler names
resolve through the ELF relocations (a section symbol plus an addend — resolve
that or the names stay hidden).

Confirmed live on the device:

```
0x1300  hqa_mac_bbp_reg_read       reads registers            WORKS
0x1301  hqa_mac_bbp_reg_write      writes registers (test mode)
0x1500  hqa_get_fw_info            firmware build date        WORKS
0x1510  hqa_get_chipid             0x11c0031
0x152f  hqa_icap_ctrl              I/Q capture, replies ok
0x1531  hqa_get_dump_rxv           replies, but 0 records
```

## Prebuilt tools

`prebuilt/` ships four aarch64 tools (NDK). Their sources were lost; the binaries
and this protocol description are enough to rebuild equivalents.

```
hqacmd   <ifname> <cmd> ...            command with a small reply
hqax     <ifname> <cmd> <win> <bytes>  command with a reply up to ~60 KB   (main tool)
hqascan  <ifname> <base> <count> <step> [all|filter]   register-range snapshot
icstool  <level> <seconds> <chunks>    in-driver ICS sniffer control

examples:
  hqax    wlan0 1300 512 82 0f 50 00      read register 0x820f5000
  hqascan wlan0 0x820e0000 32768 4 all    snapshot the register space (~43 s)
```

## NEVER write these

`hqa_write_bulk_eeprom` (0x1308), `hqa_write_bulk_eeprom_v2` (0x131a), and any
efuse write. EFUSE is burned physically; a bad radio calibration is permanent.
Register writes are volatile and safe.
