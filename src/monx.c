// SPDX-License-Identifier: GPL-2.0
/*
 * monx - call the driver's private "set_mcr" handler to write a chip MAC/BBP
 *        register on the internal MediaTek connac2 Wi-Fi, in NORMAL mode
 *        (association stays up; unlike the HQA/ATE path which resets the chip).
 *
 * Used to open the hardware RX filter (RFCR) so monx4 can capture the whole
 * channel:  set_mcr 0x820f5000 0x0000e00b   (see docs/REGISTERS.md).
 * Register writes are volatile - a reboot restores defaults.
 *
 * The stock driver exports priv_driver_set_mcr(struct net_device*, char*, int)
 * but it is not reachable from userspace (not in the private-command table).
 * We call it directly by address. Pass the address from kallsyms:
 *
 *   SM=0x$(awk '$3=="priv_driver_set_mcr"{print $1}' /proc/kallsyms)
 *   insmod monx.ko ifname=wlan0 a1=$SM c1="set_mcr 0x820f5000 0x0000e00b"
 *
 * NOTE: reconstructed from the documented module interface used by the
 * capture scripts (the original source was lost). Verify on your device
 * before relying on it. The chip is generation-specific; the RFCR address
 * and the handler symbol are for MT6789 / gen4m and may differ elsewhere.
 *
 * kCFI: the indirect call to the vendor handler would trap (brk #0x8228)
 * under kCFI. Suppress the check with __nocfi on monx_init ONLY. Do not
 * disable kCFI for the whole file: that strips the module's kCFI type-ids,
 * and the kernel's CFI-checked initcall then traps when it calls monx_init.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/string.h>

static char  *ifname = "wlan0";
static unsigned long a1;          /* kallsyms address of priv_driver_set_mcr */
static char  *c1 = "";            /* command string, e.g. "set_mcr 0x820f5000 0x0000e00b" */
module_param(ifname, charp, 0);
module_param(a1, ulong, 0);
module_param(c1, charp, 0);
MODULE_PARM_DESC(ifname, "wireless interface name (default wlan0)");
MODULE_PARM_DESC(a1, "kallsyms address of priv_driver_set_mcr");
MODULE_PARM_DESC(c1, "priv command string to run");

typedef int (*priv_fn)(struct net_device *dev, char *cmd, int len);

static int __nocfi __init monx_init(void)
{
    struct net_device *dev;
    priv_fn fn = (priv_fn)a1;
    char buf[128];
    int ret;

    if (!a1) { pr_err("monx: a1 (handler address) not set\n"); return -EINVAL; }

    dev = dev_get_by_name(&init_net, ifname);
    if (!dev) { pr_err("monx: interface %s not found\n", ifname); return -ENODEV; }

    /* the handler tokenises the command in place, so hand it a writable copy */
    strscpy(buf, c1, sizeof(buf));
    ret = fn(dev, buf, strlen(buf));
    pr_info("monx: %s(\"%s\") = %d\n", ifname, c1, ret);

    dev_put(dev);
    /* the work is done in init; nothing to keep resident */
    return -EAGAIN;
}

static void __exit monx_exit(void) { }

module_init(monx_init);
module_exit(monx_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("monx: call priv_driver_set_mcr to write a chip register in normal mode");
