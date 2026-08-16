#!/system/bin/sh
# Read a chip register in NORMAL mode (no link drop) via priv_driver_get_mcr.
# Usage: sh read_reg.sh 0x820f5000
# Needs monx.ko in /data/local/tmp. Run as root.
cd /data/local/tmp
echo 0 > /proc/sys/kernel/kptr_restrict
GM=0x$(awk '$3=="priv_driver_get_mcr"{print $1}' /proc/kallsyms)
CMD="get_mcr $1"
dmesg -c >/dev/null 2>&1
rmmod monx 2>/dev/null
insmod monx.ko ifname=wlan0 a1=$GM c1="\"$CMD\"" 2>/dev/null
rmmod monx 2>/dev/null
dmesg | grep -o 'command result is 0x[0-9a-fA-F]*' | tail -1 | awk '{print $NF}'
