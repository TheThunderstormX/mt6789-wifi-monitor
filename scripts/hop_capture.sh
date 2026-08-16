#!/system/bin/sh
# Channel-hop capture: the Android scan drives the radio across every channel
# (2.4 and 5 GHz), monx4 grabs whatever lands on the driver RX path. Gives a
# survey of all nearby APs rather than one channel's full traffic.
# Usage: sh hop_capture.sh [seconds]   (default 20)
# Output: /data/local/tmp/monx4_cap.bin -> pull and run dev_inventory.py / wrap_pcap.py
# Needs monx.ko + monx4.ko in /data/local/tmp. Run as root.
cd /data/local/tmp
SEC=${1:-20}
echo 0 > /proc/sys/kernel/kptr_restrict
SM=0x$(awk '$3=="priv_driver_set_mcr"{print $1}' /proc/kallsyms)
wr(){ CMD="set_mcr $1 $2"; rmmod monx 2>/dev/null; insmod monx.ko ifname=wlan0 a1=$SM c1="\"$CMD\"" 2>/dev/null; rmmod monx 2>/dev/null; }
rmmod monx4 2>/dev/null
insmod monx4.ko || { echo fail; exit 1; }
: > monx4_cap.bin
end=$(( $(date +%s) + SEC ))
# background scan loop = channel hopping
( while [ $(date +%s) -lt $end ]; do cmd wifi start-scan >/dev/null 2>&1; sleep 1.5; done ) &
while [ $(date +%s) -lt $end ]; do
  wr 0x820f5000 0x0000e00b            # keep the filter open (each scan resets it)
  cat /proc/monx4 >> monx4_cap.bin 2>/dev/null
  sleep 0.3
done
wr 0x820f5000 0x000cef1b
rmmod monx4 2>/dev/null
echo "done: $(wc -c < monx4_cap.bin) bytes"
dmesg | grep 'MONX4 captured' | tail -1
