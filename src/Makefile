# Build monx.ko + monx4.ko against a matching android16-6.12 GKI source tree.
#
#   KDIR=$HOME/gki-src make          # build
#   KDIR=$HOME/gki-src make clean
#
# The modules load on the stock kernel unsigned and with a mismatched vermagic:
# CONFIG_MODULE_SIG_FORCE is not set, and same_magic() skips the version string
# when modversions/CRCs are present. To satisfy symbol CRCs without a full
# vmlinux build, generate Module.symvers from the device's live CRCs:
#
#   awk '{printf "0x%s\t%s\tvmlinux\tEXPORT_SYMBOL_GPL\t\n",$1,$2}' \
#       reference/kernel-crcs.txt > Module.symvers
#
# See docs/BUILD.md.

obj-m := monx.o monx4.o

# monx suppresses kCFI at the single vendor call site with __nocfi in the source
# (see monx.c). Do NOT add -fno-sanitize=kcfi here: disabling kCFI file-wide
# strips the module's kCFI type-ids and the kernel's CFI-checked initcall traps.

KDIR ?= $(HOME)/gki-src
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm64 LLVM=1 modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm64 LLVM=1 clean
