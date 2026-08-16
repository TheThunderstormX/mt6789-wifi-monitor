# Prebuilt artifacts

Built for the `emerald` kernel (`6.12.30-android16-5`, HyperOS/Android 16). They
load on devices running the same firmware even with a vermagic mismatch
(`same_magic()` skips the version, `MODULE_SIG_FORCE` is off). On other MT6789
devices, rebuild from `../src/` instead — see [../docs/BUILD.md](../docs/BUILD.md).

| file | source | notes |
|------|--------|-------|
| `monx.ko`   | `../src/monx.c`  | RFCR writer (`set_mcr`). Built from published source, verified on-device. Unstripped debug build. |
| `monx4.ko`  | `../src/monx4.c` | kprobe capture → `/proc/monx4`. Built from published source, verified on-device. |
| `monx2.ko`  | lost | earlier mgmt-only kprobe dumper, superseded by `monx4`. Binary kept for history. |
| `crcdump.ko`| lost | dumps live module CRCs (used to build `Module.symvers` during bring-up). |
| `hqax`, `hqacmd`, `hqascan`, `icstool` | lost | aarch64 (NDK) HQA tools. Binary-only; rebuild from [../docs/HQA-PROTOCOL.md](../docs/HQA-PROTOCOL.md). |

Load requires root. All modules are reversible with `rmmod`; the only register
write is volatile. Nothing here writes flash, EEPROM, or efuse.
