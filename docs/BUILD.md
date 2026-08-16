# Building the modules

`monx.ko` and `monx4.ko` load on the **stock** kernel — no custom kernel, no
re-flash. Three stock-kernel facts make that work:

- `# CONFIG_MODULE_SIG_FORCE is not set` — unsigned modules load.
- `same_magic()` skips the version string when modversions/CRCs are present, so a
  module built from generic GKI source loads despite a vermagic mismatch.
- The symbol CRCs the loader checks can be taken from the running device instead
  of a full `vmlinux` build.

## Toolchain

A generic `android16-6.12` GKI tree and clang (`LLVM=1`). The device kernel here
is `6.12.30-android16-5` (stock Google GKI, not a Xiaomi build), so the common
tree matches without vendor source.

```sh
git clone --depth 1 -b android16-6.12 \
    https://android.googlesource.com/kernel/common ~/gki-src
cd ~/gki-src
cp /path/to/reference/kernel-config-live.txt .config     # /proc/config.gz from the device
make ARCH=arm64 LLVM=1 olddefconfig
touch protected_module_names_list                        # GKI expects this file
make ARCH=arm64 LLVM=1 modules_prepare -j"$(nproc)"      # needs libdw-dev
```

## Symbol CRCs from the device

There is no full `vmlinux`, so build `Module.symvers` from the device's live CRCs
(`reference/kernel-crcs.txt`, taken from `/proc/kallsyms` + module CRCs):

```sh
awk '{printf "0x%s\t%s\tvmlinux\tEXPORT_SYMBOL_GPL\t\n",$1,$2}' \
    reference/kernel-crcs.txt > /path/to/src/Module.symvers
```

## Build

```sh
cd src
KDIR=~/gki-src make
# -> monx.ko, monx4.ko
```

### kCFI note (important)

The stock kernel is built with kCFI. `monx` makes a raw indirect call to the
driver's `set_mcr` handler by address. Do **not** disable kCFI for the whole file
(`-fno-sanitize=kcfi`): that strips the module's kCFI type-ids, and the kernel's
CFI-checked initcall then traps (`brk #0x8228`) the moment it calls the module's
init — the phone panics and reboots. Instead suppress the check at the one call
site only, with `__nocfi` on the calling function (already done in `monx.c`).
`monx_init` keeps its type-id (verify: a `.word <id>` sits right before the
function, same as any other module init), while the vendor call compiles to a
plain `blr` with no check.

## Deploy

```sh
adb push monx.ko monx4.ko /data/local/tmp/
adb shell su -c 'sh /data/local/tmp/mon_capture.sh 8'    # root required
```

The modules are reversible: `rmmod` unloads them, and the only register write is
volatile (a reboot restores the default filter). Nothing is written to flash.

## Full pentest kernel (optional, separate)

`reference/pentest.fragment` is a Kleaf defconfig fragment that rebuilds GKI with
a wireless stack, USB-gadget, and tracing options for external-adapter and
BadUSB use. That is a full kernel rebuild + flash, out of scope for the capture
tool here; the fragment is included for reference.
