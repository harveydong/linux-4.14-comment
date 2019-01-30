/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright 2018 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/reset.h>
#include <linux/pinctrl/consumer.h>

#include "x2/x2_ips.h"
#define  RST_MAX (6)
struct ips_dev_s {
	struct platform_device *pdev;
	void __iomem *regaddr;
	void __iomem *clkaddr;
	int irq;
	int irqnum;
	unsigned int intstatus;
	spinlock_t spinlock;
	spinlock_t *lock;
	ips_irqhandler_t irq_handle[3];
	void *irq_data[3];
	struct reset_control *rst[RST_MAX];
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_bt;
	struct pinctrl_state *pins_dvp;
};
struct ips_dev_s *g_ipsdev;

char *reset_name[RST_MAX] = {"mipi_ipi", "mipi_cfg", "sif", "ipu", "dvp", "bt"};

unsigned int ips_debug_ctl = 0;
module_param(ips_debug_ctl, uint, S_IRUGO | S_IWUSR);
#define IPS_DEBUG_PRINT(format, args...)	\
	do {									\
		if(ips_debug_ctl)					\
			printk(format, ## args);		\
	} while(0)

static inline int irq_to_regbit(int irq)
{
	switch (irq) {
	case ISP_INT:
		return ISP_INT_BITS;
	case IPU_INT:
		return IPU_INT_BITS;
	case SIF_INT:
		return SIF_INT_BITS;
	default:
		return 0;
	}
}

int ips_irq_enable(int irq)
{
	unsigned long flags;
	u32 val, irqmask;
	if (!g_ipsdev)
		return -1;
	irqmask = ~(irq_to_regbit(irq));
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPSINTMASK);
	val &= irqmask;
	writel(val, g_ipsdev->regaddr + IPSINTMASK);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	printk(KERN_INFO "module %d's irq enabled\n", irq);
	return 0;
}
EXPORT_SYMBOL_GPL(ips_irq_enable);

int ips_irq_disable(int irq)
{
	unsigned long flags;
	u32 val, irqmask;
	if (!g_ipsdev)
		return -1;
	irqmask = irq_to_regbit(irq);
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPSINTMASK);
	val |= irqmask;
	writel(val, g_ipsdev->regaddr + IPSINTMASK);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	printk(KERN_INFO "module %d's irq disabled\n", irq);
	return 0;
}
EXPORT_SYMBOL_GPL(ips_irq_disable);

int ips_mask_int(unsigned int mask)
{
	unsigned long flags;
	u32 int_mask;
	if (!g_ipsdev)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	int_mask = readl(g_ipsdev->regaddr + IPSINTMASK);
	int_mask |= mask;
	writel(int_mask, g_ipsdev->regaddr + IPSINTMASK);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(ips_mask_int);

int ips_unmask_int(unsigned int mask)
{
	unsigned long flags;
	u32 int_mask;
	if (!g_ipsdev)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	int_mask = readl(g_ipsdev->regaddr + IPSINTMASK);
	int_mask &= ~mask;
	writel(int_mask, g_ipsdev->regaddr + IPSINTMASK);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(ips_unmask_int);

int ips_register_irqhandle(int irq, ips_irqhandler_t handle, void *data)
{
	if (irq < SIF_INT || irq > IPU_INT || !g_ipsdev) {
		printk(KERN_ERR "register irq %d failed\n", irq);
		return -1;
	}
	printk(KERN_INFO "module %d's irq registed\n", irq);
	g_ipsdev->irq_handle[irq] = handle;
	g_ipsdev->irq_data[irq] = data;
	return 0;
}
EXPORT_SYMBOL_GPL(ips_register_irqhandle);

unsigned int ips_get_intstatus(void)
{
	if (!g_ipsdev)
		return 0;
	return g_ipsdev->intstatus;
}
EXPORT_SYMBOL_GPL(ips_get_intstatus);

static irqreturn_t x2_ips_irq(int this_irq, void *data)
{
	struct ips_dev_s *ips = data;
	unsigned long flags;
	disable_irq_nosync(this_irq);

	spin_lock_irqsave(ips->lock, flags);
	ips->intstatus = readl(ips->regaddr + IPSINTSTATE);
	spin_unlock_irqrestore(ips->lock, flags);
	IPS_DEBUG_PRINT("ips intstatus:0x%x\n", ips->intstatus);
	if ((ips->intstatus & ISP_INT_BITS) && ips->irq_handle[ISP_INT]) {
		IPS_DEBUG_PRINT("ISP_INT\n");
		ips->irq_handle[ISP_INT](ips->intstatus, ips->irq_data[ISP_INT]);
	}

	if ((ips->intstatus & SIF_INT_BITS) && ips->irq_handle[SIF_INT]) {
		IPS_DEBUG_PRINT("SIF_INT\n");
		ips->irq_handle[SIF_INT](ips->intstatus, ips->irq_data[SIF_INT]);
	}

	if ((ips->intstatus & IPU_INT_BITS) && ips->irq_handle[IPU_INT]) {
		IPS_DEBUG_PRINT("IPU_INT\n");
		ips->irq_handle[IPU_INT](ips->intstatus, ips->irq_data[IPU_INT]);
	}

	enable_irq(this_irq);
	ips->intstatus = 0;
	return IRQ_HANDLED;
}

int ips_busctl_set(unsigned int type, unsigned int index, unsigned int region, unsigned int value)
{
	unsigned long flags;
	u32 val;
	void __iomem *regaddr;
	if (!g_ipsdev)
		return -1;
	switch (type) {
	case BUSCTL_WM:
		if (index < 0 || index > 15)
			return -1;
		regaddr = g_ipsdev->regaddr + IPSBUSCTL_WM0 + index * 4;
		break;
	case BUSCTL_RM:
		if (index < 0 || index > 8)
			return -1;
		regaddr = g_ipsdev->regaddr + IPSBUSCTL_RM0 + index * 4;
		break;
	default:
		return -1;
	}
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(regaddr);
	switch (region) {
	case BUSCTL_REGION_PRI:
		val &= ~(BUSCTL_PRI_X);
		val |= ((value << BUSCTLD_PRI_SHIFT_X) & BUSCTL_PRI_X);
		break;

	case BUSCTL_REGION_ENDIAN:
		val &= ~(BUSCTL_ENDIAN_X);
		val |= ((value << BUSCTL_ENDIAN_SHIFT_X) & BUSCTL_ENDIAN_X);
		break;

	case BUSCTL_REGION_MAXLEN:
		val &= ~(BUSCTL_MAXLEN_X);
		val |= ((value << BUSCTL_MAXLEN_SHIFT_X) & BUSCTL_MAXLEN_X);
		break;
	default:
		break;
	}
	writel(val, regaddr);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ips_busctl_set);

int ips_busctl_get(unsigned int type, unsigned int index, unsigned int region)
{
	unsigned long flags;
	u32 val;
	void __iomem *regaddr;
	if (!g_ipsdev)
		return -1;
	switch (type) {
	case BUSCTL_WM:
		if (index < 0 || index > 15)
			return -1;
		regaddr = g_ipsdev->regaddr + IPSBUSCTL_WM0 + index * 4;
		break;
	case BUSCTL_RM:
		if (index < 0 || index > 8)
			return -1;
		regaddr = g_ipsdev->regaddr + IPSBUSCTL_RM0 + index * 4;
		break;
	default:
		return -1;
	}
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(regaddr);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	switch (region) {
	case BUSCTL_REGION_PRI:
		val &= BUSCTL_PRI_X;
		val = val >> BUSCTLD_PRI_SHIFT_X;
		break;

	case BUSCTL_REGION_ENDIAN:
		val &= BUSCTL_ENDIAN_X;
		val = val >> BUSCTL_ENDIAN_SHIFT_X;
		break;

	case BUSCTL_REGION_MAXLEN:
		val &= BUSCTL_MAXLEN_X;
		val = val >> BUSCTL_MAXLEN_SHIFT_X;
		break;
	default:
		val = -1;
		break;
	}
	return val;
}
EXPORT_SYMBOL_GPL(ips_busctl_get);

int ips_mipi_ctl_set(unsigned int region, unsigned int value)
{
	unsigned long flags;
	u32 val;
	if (!g_ipsdev)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPS_MIPI_CTRL);
	switch (region) {
	case MIPI_BYPASS_GEN_HSYNC_DLY_CNT:
		val &= ~(MIPI_BPASS_GEN_DLY);
		val |= ((value << 4) & MIPI_BPASS_GEN_DLY);
		break;

	case MIPI_BYPASS_GEN_HSYNC_EN:
		val &= ~(MIPI_BPASS_GEN_HSYNC);
		val |= ((value << 1) & MIPI_BPASS_GEN_HSYNC);
		break;

	case MIPI_DEV_SHADOW_CLEAR:
		val &= ~(MIPI_DEV_SHADOW_CLR);
		val |= ((value << 0) & MIPI_DEV_SHADOW_CLR);
		break;
	default:
		break;
	}
	writel(val, g_ipsdev->regaddr + IPS_MIPI_CTRL);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ips_mipi_ctl_set);

int ips_mipi_ctl_get(unsigned int region)
{
	unsigned long flags;
	u32 val;
	if (!g_ipsdev)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPS_MIPI_CTRL);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	switch (region) {
	case MIPI_BYPASS_GEN_HSYNC_DLY_CNT:
		val &= MIPI_BPASS_GEN_DLY;
		val = val >> 4;
		break;

	case MIPI_BYPASS_GEN_HSYNC_EN:
		val &= MIPI_BPASS_GEN_HSYNC;
		val = val >> 1;
		break;

	case MIPI_DEV_SHADOW_CLEAR:
		val &= MIPI_DEV_SHADOW_CLR;
		val = val >> 0;
		break;
	default:
		val = -1;
		break;
	}
	return val;
}
EXPORT_SYMBOL_GPL(ips_mipi_ctl_get);

int ips_control_set(unsigned int region, unsigned int state)
{
	unsigned long flags;
	u32 val;
	if (!g_ipsdev || region < MIPI_DEV_CFG_CLK_GATE_EN || region > ISP_CLK_GATE_EN)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPS_CTL);
	if (state)
		val |= (0x1 << region);
	else
		val &= ~(0x1 << region);
	writel(val, g_ipsdev->regaddr + IPS_CTL);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ips_control_set);

int ips_control_get(unsigned int region)
{
	unsigned long flags;
	u32 val;
	if (!g_ipsdev || region < MIPI_DEV_CFG_CLK_GATE_EN || region > ISP_CLK_GATE_EN)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPS_CTL);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	if (val & (0x1 << region))
		return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(ips_control_get);

int ips_get_status(unsigned int region)
{
	unsigned long flags;
	u32 val;
	if (!g_ipsdev || region < PYM_STATUS || region > SIF_STATUS)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPS_STATUS);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	if (val & (0x1 << region))
		return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(ips_get_status);

int ips_get_mipi_freqrange(unsigned int region)
{
	unsigned long flags;
	u32 val;
	if (!g_ipsdev)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPS_MIPI_FREQRANGE);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	switch (region) {
	case MIPI_DEV_CFGCLKFREQRANGE:
		val &= MIPI_DEV_CFGCLK_FRANGE;
		val = val >> 24;
		break;

	case MIPI_DEV_HSFREQRANGE:
		val &= MIPI_DEV_HS_FRANGE;
		val = val >> 16;
		break;

	case MIPI_HOST_CFGCLKFREQRANGE:
		val &= MIPI_HOST_CFGCLK_FRANGE;
		val = val >> 8;
		break;
	case MIPI_HOST_HSFREQRANGE:
		val &= MIPI_HOST_HS_FRANGE;
		val = val >> 0;
		break;
	default:
		val = -1;
		break;
	}
	return val;
}
EXPORT_SYMBOL_GPL(ips_get_mipi_freqrange);

int ips_set_mipi_freqrange(unsigned int region, unsigned int value)
{
	unsigned long flags;
	u32 val;
	if (!g_ipsdev)
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->regaddr + IPS_MIPI_FREQRANGE);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	switch (region) {
	case MIPI_DEV_CFGCLKFREQRANGE:
		val &= (~MIPI_DEV_CFGCLK_FRANGE);
		val |= ((value << 24) & MIPI_DEV_CFGCLK_FRANGE);
		break;

	case MIPI_DEV_HSFREQRANGE:
		val &= (~MIPI_DEV_HS_FRANGE);
		val |= ((value << 24) & MIPI_DEV_HS_FRANGE);
		break;

	case MIPI_HOST_CFGCLKFREQRANGE:
		val &= (~MIPI_HOST_CFGCLK_FRANGE);
		val |= ((value << 8) & MIPI_HOST_CFGCLK_FRANGE);
		break;
	case MIPI_HOST_HSFREQRANGE:
		val &= (~MIPI_HOST_HS_FRANGE);
		val |= ((value << 0) & MIPI_HOST_HS_FRANGE);
		break;
	default:
		val = -1;
		break;
	}
	return val;
}
EXPORT_SYMBOL_GPL(ips_set_mipi_freqrange);

int ips_pinmux_bt(void)
{
	if (!g_ipsdev->pins_bt)
		return -ENODEV;
	return pinctrl_select_state(g_ipsdev->pinctrl, g_ipsdev->pins_bt);

}
EXPORT_SYMBOL_GPL(ips_pinmux_bt);

int ips_pinmux_dvp(void)
{
	if (!g_ipsdev->pins_dvp)
		return -ENODEV;
	return pinctrl_select_state(g_ipsdev->pinctrl, g_ipsdev->pins_dvp);

}
EXPORT_SYMBOL_GPL(ips_pinmux_dvp);

int ips_set_btout_clksrc(unsigned int mode)
{
	int val, ret = 0;
	unsigned long flags;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	val = readl(g_ipsdev->clkaddr + VIOSYS_CLK_CTRL);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	if (mode == BYPASS_CLK)
		val |= BIT(16);
	else if (mode == IAR_CLK)
		val &= ~BIT(16);
	else
		return -1;
	spin_lock_irqsave(g_ipsdev->lock, flags);
	writel(val, g_ipsdev->clkaddr + VIOSYS_CLK_CTRL);
	spin_unlock_irqrestore(g_ipsdev->lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(ips_set_btout_clksrc);

void ips_module_reset(unsigned int module)
{
	int i;
	for (i = 0; i < RST_MAX; i++) {
		if (BIT(i) & module) {
			reset_control_assert(g_ipsdev->rst[i]);
			udelay(2);
			reset_control_deassert(g_ipsdev->rst[i]);
			IPS_DEBUG_PRINT("reset %d end \n", i);
		}
	}
}
EXPORT_SYMBOL_GPL(ips_module_reset);

static int x2_ips_probe(struct platform_device *pdev)
{
	struct resource *res,*irq;
	int i, ret = 0;

	printk(KERN_INFO "ips driver init enter\n");
	g_ipsdev = devm_kzalloc(&pdev->dev, sizeof(struct ips_dev_s), GFP_KERNEL);
	if (!g_ipsdev) {
		dev_err(&pdev->dev, "Unable to alloc IPS DEV\n");
		return -ENOMEM;
	}
	dev_set_drvdata(&pdev->dev, g_ipsdev);

	spin_lock_init(&g_ipsdev->spinlock);
	g_ipsdev->lock = &g_ipsdev->spinlock;
	g_ipsdev->pdev = pdev;

	for (i = 0; i < RST_MAX; i++) {
		g_ipsdev->rst[i] = devm_reset_control_get(&pdev->dev, reset_name[i]);
		if (IS_ERR(g_ipsdev->rst)) {
			dev_err(&pdev->dev, "missing controller reset %s\n", reset_name[i]);
			return PTR_ERR(g_ipsdev->rst);
		}
	}
	ips_module_reset(RST_MIPI_IPI | RST_MIPI_CFG | RST_SIF | RST_IPU | RST_DVP | RST_BT);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	g_ipsdev->regaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(g_ipsdev->regaddr)) {
		dev_err(&pdev->dev, "ioremap regaddr error\n");
		return PTR_ERR(g_ipsdev->regaddr);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	g_ipsdev->clkaddr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(g_ipsdev->clkaddr)) {
		dev_err(&pdev->dev, "ioremap regaddr error\n");
		return PTR_ERR(g_ipsdev->clkaddr);
	}
	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq) {
		dev_err(&pdev->dev, "No IRQ resource\n");
		return -ENODEV;
	}
	g_ipsdev->irq = irq->start;
	ret = request_threaded_irq(g_ipsdev->irq, x2_ips_irq, NULL, IRQF_TRIGGER_HIGH,
							   dev_name(&pdev->dev), g_ipsdev);

	g_ipsdev->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(g_ipsdev->pinctrl)) {
		dev_err(&pdev->dev, "pinctrl get error\n");
		return PTR_ERR(g_ipsdev->pinctrl);
	}
	g_ipsdev->pins_bt = pinctrl_lookup_state(g_ipsdev->pinctrl, "bt_func");
	if (IS_ERR(g_ipsdev->pins_bt)) {
		dev_err(&pdev->dev, "bt in pinctrl state error\n");
		return PTR_ERR(g_ipsdev->pins_bt);
	}

	g_ipsdev->pins_dvp = pinctrl_lookup_state(g_ipsdev->pinctrl, "dvp_func");
	if (IS_ERR(g_ipsdev->pins_dvp)) {
		dev_err(&pdev->dev, "dvp in pinctrl state error\n");
		return PTR_ERR(g_ipsdev->pins_dvp);
	}

	g_ipsdev->irqnum = 3;
	g_ipsdev->intstatus = 0;

	printk(KERN_INFO "ips driver init end\n");
	return ret;
}

static int x2_ips_remove(struct platform_device *pdev)
{
	struct ips_dev_s *ips;

	ips = dev_get_drvdata(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id x2_ips_of_match[] = {
	{.compatible = "hobot,x2-ips"},
	{},
};
MODULE_DEVICE_TABLE(of, x2_ips_of_match);
#endif

static struct platform_driver x2_ips_driver = {
	.probe = x2_ips_probe,
	.remove = x2_ips_remove,
	.driver = {
		.name = "x2-ips",
		.of_match_table = of_match_ptr(x2_ips_of_match),
		//.pm = &x2_ips_pm,
	},
};
static int dbg_ips_show(struct seq_file *s, void *unused)
{

	//unsigned int num;

	seq_printf(s, "The ips module status:\n");

	return 0;
}

ssize_t ips_debug_write(struct file *file, const char __user *buf, size_t size, loff_t *p)
{
	int i;

	char info[255];
	memset(info, 0, 255);
	if (copy_from_user(info, buf, size))
		return size;
	printk("ips:%s\n", info);
	if (!memcmp(info, "bt", 2)) {
		ips_pinmux_bt();
	} else if (!memcmp(info, "dvp", 3)) {
		ips_pinmux_dvp();
	} else if (!memcmp(info, "regdump", 7)) {
		for (i = 0; i <= IPS_CTL; i += 0x4) {
			printk("regaddr:0x%p, value:0x%x \n", (g_ipsdev->regaddr + i), readl(g_ipsdev->regaddr + i));
		}
		return size;
	}

	return size;
}

static int dbg_ips_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_ips_show, &inode->i_private);
}

static const struct file_operations debug_fops = {
	.open = dbg_ips_open,
	.read = seq_read,
	.write = ips_debug_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init x2_ips_debuginit(void)
{
	(void)debugfs_create_file("x2_ips", S_IRUGO, NULL, NULL, &debug_fops);
	return 0;
}

module_platform_driver(x2_ips_driver);
late_initcall(x2_ips_debuginit);

/* Module information */
MODULE_DESCRIPTION("X2 IPS Interface");
MODULE_ALIAS("platform:x2-ips");
MODULE_LICENSE("GPL");
