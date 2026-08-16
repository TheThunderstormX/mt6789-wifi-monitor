// SPDX-License-Identifier: GPL-2.0
/*
 * monx4 - raw 802.11 capture for the internal MediaTek connac2 Wi-Fi
 *         (MT6789 / Helio G99, e.g. Redmi Note 13 Pro 4G "emerald").
 *
 * Places a kprobe on the driver RX entry points nicRxProcessDataPacket and
 * nicRxProcessMgmtPacket. Argument 1 (x1) is the SW_RFB; the raw receive
 * buffer pointer sits at rfb+0x18. The buffer starts with the connac RXD
 * (RXD + RXV); the 802.11 frame follows the descriptor. Length is the low
 * 14 bits of the first RXD word (RXByteCount). The raw buffer is copied into
 * a kfifo, length-prefixed (u16), and exposed read-only at /proc/monx4.
 *
 * The chip normally drops frames not addressed to us. Open the hardware RX
 * filter first by writing RFCR 0x820f5000 (see monx.c / scripts). Then the
 * driver RX path sees every frame on the tuned channel and this probe copies
 * it out. See docs/MECHANISM.md.
 *
 * Reversible: rmmod removes the probes; no writes to flash/EEPROM/efuse.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/kfifo.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#define CAP 1600
static DEFINE_KFIFO(fifo, unsigned char, 262144);
static DEFINE_SPINLOCK(lk);
static unsigned char scratch[CAP];
static unsigned long captured, dropped;
static int kp_pre(struct kprobe *p, struct pt_regs *regs){
    void *rfb=(void*)regs->regs[1], *raw=NULL;
    u16 rxbc=0, n; unsigned long fl;
    if(!rfb) return 0;
    if(copy_from_kernel_nofault(&raw,(char*)rfb+0x18,8)||!raw) return 0;
    if(copy_from_kernel_nofault(&rxbc,raw,2)) return 0;
    rxbc &= 0x3fff;
    if(rxbc<20) return 0;
    n = (rxbc>CAP)?CAP:rxbc;
    spin_lock_irqsave(&lk,fl);
    if(!copy_from_kernel_nofault(scratch,raw,n)){
        if(kfifo_avail(&fifo)>=(unsigned)n+2){ kfifo_in(&fifo,(unsigned char*)&n,2); kfifo_in(&fifo,scratch,n); captured++; }
        else dropped++;
    }
    spin_unlock_irqrestore(&lk,fl);
    return 0;
}
static struct kprobe kps[2]={
 {.symbol_name="nicRxProcessDataPacket",.pre_handler=kp_pre},
 {.symbol_name="nicRxProcessMgmtPacket",.pre_handler=kp_pre},
};
static ssize_t rd(struct file *f,char __user *ub,size_t len,loff_t *o){
    unsigned char *kb; unsigned int got=0; ssize_t r=0; unsigned long fl;
    size_t nn=min(len,(size_t)16384);
    kb=kmalloc(nn,GFP_KERNEL); if(!kb) return -ENOMEM;
    spin_lock_irqsave(&lk,fl); got=kfifo_out(&fifo,kb,nn); spin_unlock_irqrestore(&lk,fl);
    if(got){ if(copy_to_user(ub,kb,got)) r=-EFAULT; else r=got; }
    kfree(kb); return r;
}
static const struct proc_ops pops={.proc_read=rd};
static int __init mi(void){int i,n=0; proc_create("monx4",0,NULL,&pops);
    for(i=0;i<2;i++) if(!register_kprobe(&kps[i])) n++;
    pr_info("MONX4 v2: probes=%d /proc/monx4\n",n); return 0;}
static void __exit me(void){int i; for(i=0;i<2;i++) unregister_kprobe(&kps[i]);
    remove_proc_entry("monx4",NULL); pr_info("MONX4 captured=%lu dropped=%lu\n",captured,dropped);}
module_init(mi); module_exit(me);
MODULE_LICENSE("GPL"); MODULE_DESCRIPTION("monx4 raw capture");
