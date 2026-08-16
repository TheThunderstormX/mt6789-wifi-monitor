#!/system/bin/sh
# Capture all traffic on the currently associated channel (internal Wi-Fi).
# Usage: sh mon_capture.sh [seconds]   (default 8)
# Output: /data/local/tmp/monx4_cap.bin  -> pull to PC and run wrap_pcap.py
# Needs monx.ko + monx4.ko in /data/local/tmp. Run as root.
cd /data/local/tmp
SEC=${1:-8}
echo 0 > /proc/sys/kernel/kptr_restrict
SM=0x$(awk '$3=="priv_driver_set_mcr"{print $1}' /proc/kallsyms)
wr(){ CMD="set_mcr $1 $2"; rmmod monx 2>/dev/null; insmod monx.ko ifname=wlan0 a1=$SM c1="\"$CMD\"" 2>/dev/null; rmmod monx 2>/dev/null; }
echo "link: $(iw dev wlan0 link 2>/dev/null | head -1)"
rmmod monx4 2>/dev/null
insmod monx4.ko || { echo "monx4 insmod fail"; exit 1; }
wr 0x820f5000 0x0000e00b            # open RX filter (whole channel)
: > monx4_cap.bin
end=$(( $(date +%s) + SEC ))
while [ $(date +%s) -lt $end ]; do cat /proc/monx4 >> monx4_cap.bin 2>/dev/null; sleep 0.2; done
wr 0x820f5000 0x000cef1b            # restore default filter
rmmod monx4 2>/dev/null
echo "done: $(wc -c < monx4_cap.bin) bytes in /data/local/tmp/monx4_cap.bin"
dmesg | grep 'MONX4 captured' | tail -1
