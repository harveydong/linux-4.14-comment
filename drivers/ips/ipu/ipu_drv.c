#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/device.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <x2/x2_ips.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/eventpoll.h>
#include "ipu_dev.h"
#include "ipu_drv.h"
#include "ipu_common.h"
#include <asm-generic/io.h>
#include <asm/string.h>
#include <linux/uaccess.h>

#define X2_IPU_NAME		"x2-ipu"

#define ENABLE 1
#define DISABLE 0

struct x2_ipu_data *g_ipu = NULL;
unsigned int ipu_debug_level = 0;
module_param(ipu_debug_level, uint, 0644);

int8_t ipu_cfg_ddrinfo_init(ipu_cfg_t *ipu)
{
	uint32_t w = 0, h = 0;
	uint32_t size = 0;
	uint32_t i = 0;
	uint64_t ddrbase = 0; //(uint64_t)g_ipu->paddr;
	uint32_t limit = ddrbase + IPU_SLOT_SIZE;

	/* step0. calculate slot head info size */
	ddrbase = ALIGN_16(ddrbase);

	/* step1. calculate crop space */
	if (ipu->ctrl.crop_ddr_en == 1) {
		w = ALIGN_16(ipu->crop.crop_ed.w - ipu->crop.crop_st.w);
		h = ALIGN_16(ipu->crop.crop_ed.h - ipu->crop.crop_st.h);
		size = w * h;
		ipu->crop_ddr.y_addr = ddrbase;
		ipu->crop_ddr.c_addr = ddrbase + size;
		ddrbase += size * 3 >> 1;
		ddrbase = ALIGN_16(ddrbase);
		if (ddrbase >= limit)
			goto err_out;
	}

	/* step2. calculate scale space */
	if (ipu->ctrl.scale_ddr_en == 1) {
		w = ALIGN_16(ipu->scale.scale_tgt.w);
		h = ALIGN_16(ipu->scale.scale_tgt.h);
		size = w * h;
		ipu->scale_ddr.y_addr = ddrbase;
		ipu->scale_ddr.c_addr = ddrbase + size;
		ddrbase += size * 3 >> 1;
		ddrbase = ALIGN_16(ddrbase);
		if (ddrbase >= limit)
			goto err_out;
	}

	/* step3. calculate pymid ds space */
	if (ipu->pymid.pymid_en == 1) {
		for (i = 0; i <= ipu->pymid.ds_layer_en; i++) {
			if (i % 4 != 0 && ipu->pymid.ds_factor[i] == 0) {
				/* if factor == 0, bypass layer */
				ipu->ds_ddr[i].y_addr = 0;
				ipu->ds_ddr[i].c_addr = 0;
				continue;
			}
			w = ALIGN_16(ipu->pymid.ds_roi[i].w);
			h = ALIGN_16(ipu->pymid.ds_roi[i].h);
			size = w * h;
			ipu->ds_ddr[i].y_addr = ddrbase;
			if (ipu->pymid.ds_uv_bypass & (1 << i)) {
				/* uv bypass layer won't write to ddr */
				ipu->ds_ddr[i].c_addr = 0;
				ddrbase += size;
			} else {
				ipu->ds_ddr[i].c_addr = ddrbase + size;
				ddrbase += size * 3 >> 1;
			}
			ddrbase = ALIGN_16(ddrbase);
			ipu_info("pym%d %d %d %d %d 0x%llx 0x%llx", i, w, h, ipu->pymid.ds_roi[i].w, ipu->pymid.ds_roi[i].h, ipu->ds_ddr[i].y_addr, ipu->ds_ddr[i].c_addr);
			if (ddrbase >= limit)
				goto err_out;
			//set_ds_layer_addr(i, ipu->ds_ddr[i].y_addr, ipu->ds_ddr[i].c_addr);
		}

		/* step3.1. calculate pymid us space */
		for (i = 0; i < 6; i++) {
			if (!(ipu->pymid.us_layer_en & 1 << i)) {
				/* layer disable */
				ipu->us_ddr[i].y_addr = 0;
				ipu->us_ddr[i].c_addr = 0;
				continue;
			}
			w = ALIGN_16(ipu->pymid.us_roi[i].w);
			h = ALIGN_16(ipu->pymid.us_roi[i].h);
			size = w * h;
			ipu->us_ddr[i].y_addr = ddrbase;
			if (ipu->pymid.us_uv_bypass & 1 << i) {
				ipu->us_ddr[i].c_addr = ddrbase + size;
				ddrbase += size * 3 >> 1;
			} else {
				/* uv bypass layer won't write to ddr */
				ipu->us_ddr[i].c_addr = 0;
				ddrbase += size;
			}
			ddrbase = ALIGN_16(ddrbase);
			if (ddrbase >= limit)
				goto err_out;
		}
	}

	return 0;
err_out:
	return -1;
}

static int8_t ipu_set_crop_ddr(ipu_cfg_t *ipu, uint64_t ddrbase)
{
	/* step0. calculate slot head info size */
	ddrbase = ALIGN_16(ddrbase);

	/* step1. calculate crop space */
	if (ipu->ctrl.crop_ddr_en == 1) {
		set_ipu_addr(0, ipu->crop_ddr.y_addr + ddrbase, ipu->crop_ddr.c_addr + ddrbase);
	}
	return 0;
}
static int8_t ipu_set_scale_ddr(ipu_cfg_t *ipu, uint64_t ddrbase)
{
	/* step2. calculate scale space */
	if (ipu->ctrl.scale_ddr_en == 1) {
		set_ipu_addr(1, ipu->scale_ddr.y_addr + ddrbase, ipu->scale_ddr.c_addr + ddrbase);
	}
	return 0;
}
static int8_t ipu_set_pym_ddr(ipu_cfg_t *ipu, uint64_t ddrbase)
{
	uint32_t i = 0;

	/* step3. calculate pymid ds space */
	if (ipu->pymid.pymid_en == 1) {
		for (i = 0; i <= ipu->pymid.ds_layer_en; i++) {
			if (i % 4 != 0 && ipu->pymid.ds_factor[i] == 0) {
				/* if factor == 0, bypass layer */
				ipu->ds_ddr[i].y_addr = 0;
				ipu->ds_ddr[i].c_addr = 0;
				continue;
			}
			set_ds_layer_addr(i, ipu->ds_ddr[i].y_addr + ddrbase, ipu->ds_ddr[i].c_addr + ddrbase);
		}

		/* step3.1. calculate pymid us space */
		for (i = 0; i < 6; i++) {
			if (!(ipu->pymid.us_layer_en & 1 << i)) {
				/* layer disable */
				ipu->us_ddr[i].y_addr = 0;
				ipu->us_ddr[i].c_addr = 0;
				continue;
			}
			set_us_layer_addr(i, ipu->us_ddr[i].y_addr + ddrbase, ipu->us_ddr[i].c_addr + ddrbase);
		}
	}

	return 0;
}

static int8_t ipu_set_pymsrc_ddr(ipu_cfg_t *ipu, uint64_t ddrbase, bool first)
{
	if (first)
		set_ds_src_addr(ipu->crop_ddr.y_addr + ddrbase, ipu->crop_ddr.c_addr + ddrbase);
	else
		set_ds_src_addr(ipu->scale_ddr.y_addr + ddrbase, ipu->scale_ddr.c_addr + ddrbase);

	return 0;
}


/********************************************************************
 * @brief ipu_set_ddr
 *
 * @param ipu
 * @param ddrbase physical address
 *
 * @return
 ********************************************************************/
static int8_t ipu_set_ddr(ipu_cfg_t *ipu, uint64_t ddrbase)
{
	int8_t ret = 0;
	uint32_t i = 0;
	/* step1. set crop addr */
	if (ipu->ctrl.crop_ddr_en == 1) {
		ret |= set_ipu_addr(0, ipu->crop_ddr.y_addr + ddrbase, ipu->crop_ddr.c_addr + ddrbase);
	}

	/* step2. set scale addr */
	if (ipu->ctrl.scale_ddr_en == 1) {
		ret |= set_ipu_addr(1, ipu->scale_ddr.y_addr + ddrbase, ipu->scale_ddr.c_addr + ddrbase);
	}

	/* step3. set pymid addr */
	if (ipu->pymid.pymid_en == 1) {
		/* step3.1 set pymid ds addr */
		for (i = 0; i <= ipu->pymid.ds_layer_en; i++) {
			if (i == 0 || ipu->pymid.ds_factor[i] != 0)
				ret |= set_ds_layer_addr(i, ipu->ds_ddr[i].y_addr + ddrbase, ipu->ds_ddr[i].c_addr + ddrbase);
		}

		/* step3.2. set pymid us addr */
		for (i = 0; i < 6; i++) {
			if (ipu->pymid.us_layer_en & 1 << i)
				ret |= set_us_layer_addr(i, ipu->us_ddr[i].y_addr + ddrbase, ipu->us_ddr[i].c_addr + ddrbase);
		}
	}
	return ret;
}

int8_t ipu_set(ipu_cmd_e cmd, ipu_cfg_t *ipu_cfg, uint64_t data)
{
	switch (cmd) {
	case IPUC_SET_DDR:
		ipu_set_ddr(ipu_cfg, data);
		break;
	case IPUC_SET_CROP_DDR:
		ipu_set_crop_ddr(ipu_cfg, data);
		break;
	case IPUC_SET_SCALE_DDR:
		ipu_set_scale_ddr(ipu_cfg, data);
		break;
	case IPUC_SET_PYM_DDR:
		ipu_set_pym_ddr(ipu_cfg, data);
		break;
	case IPUC_SET_PYM_1ST_SRC_DDR:
		ipu_set_pymsrc_ddr(ipu_cfg, data, true);
		break;
	case IPUC_SET_PYM_2ND_SRC_DDR:
		ipu_set_pymsrc_ddr(ipu_cfg, data, false);
		break;
	case IPUC_SET_BASE:
		set_ipu_ctrl(&ipu_cfg->ctrl);
		set_ipu_video_size(&ipu_cfg->video_in);
		break;
	case IPUC_SET_CROP:
		set_ipu_crop(&ipu_cfg->crop);
		break;
	case IPUC_SET_SCALE:
		set_ipu_scale(&ipu_cfg->scale);
		break;
	case IPUC_SET_FRAME_ID:
		set_ipu_frame_id(&ipu_cfg->frame_id);
		break;
	case IPUC_SET_PYMID:
		set_ipu_pymid(&ipu_cfg->pymid);
		break;
	default:
		break;
	}

	return 0;
}

int8_t ipu_drv_start(void)
{
	ipu_info("ipu start\n");
	spin_lock(&g_ipu->elock);
	if (g_ipu->cfg->ctrl.crop_ddr_en)
		ctrl_ipu_to_ddr(CROP_TO_DDR, ENABLE);
	if (g_ipu->cfg->ctrl.scale_ddr_en)
		ctrl_ipu_to_ddr(SCALAR_TO_DDR, ENABLE);
	ctrl_ipu_to_ddr(PYM_TO_DDR, ENABLE);
	ips_irq_enable(IPU_INT);
	g_ipu->stop = false;
	spin_unlock(&g_ipu->elock);
	return 0;
}

int8_t ipu_drv_stop(void)
{
	if (g_ipu->stop) {
		ipu_info("ipu already stop\n");
		return 0;
	}
	spin_lock(&g_ipu->elock);
	g_ipu->stop = true;
	ctrl_ipu_to_ddr(CROP_TO_DDR | SCALAR_TO_DDR | PYM_TO_DDR, DISABLE);
	//ctrl_ipu_to_ddr(CROP_TO_DDR | SCALAR_TO_DDR, DISABLE);
	ips_irq_disable(IPU_INT);
	g_ipu->isr_data = 0;
	spin_unlock(&g_ipu->elock);
	return 0;
}

static int ipu_thread(void *data)
{
	//unsigned long flag;
	uint32_t status = 0;
	struct x2_ipu_data *ipu = (struct x2_ipu_data *)data;
	ipu_info("ipu thread run\n");
	do {
		wait_event_interruptible(ipu->wq_head, test_and_clear_bit(IPU_TRIGGER_ISR, &ipu->runflags));

		if (kthread_should_stop())
			break;

		spin_lock(&ipu->elock);
		if (ipu->stop) {
			ipu->isr_data = 0;
			spin_unlock(&ipu->elock);
			continue;
		}
		status = ipu->isr_data;
		ipu->isr_data = 0;
		ipu->pymid_done = false;
		ipu->done_idx = -1;

		if (ipu->ipu_mode && ipu->ipu_handle[ipu->ipu_mode])
			ipu->ipu_handle[ipu->ipu_mode](status);
		spin_unlock(&ipu->elock);

	} while (!kthread_should_stop());
	ipu_info("ipu thread exit\n");
	return 0;
}

void x2_ipu_isr(unsigned int status, void *data)
{
	struct x2_ipu_data *ipu = (struct x2_ipu_data *)data;
	//printk("x2_ipu_isr\n");
	if (ipu->stop) {
		ipu->isr_data = 0;
		return;
	} else {
		ipu->isr_data |= status;
	}
	if (!test_and_set_bit(IPU_TRIGGER_ISR, &ipu->runflags))
		wake_up_interruptible(&ipu->wq_head);
}

static int8_t ipu_stop_thread(struct x2_ipu_data *ipu)
{
	if (!IS_ERR(ipu->ipu_task)) {
		kthread_stop(g_ipu->ipu_task);
		wake_up_interruptible(&ipu->wq_head);
	}
	ipu->ipu_task = NULL;
	return 0;
}

void init_test_data(ipu_cfg_t *info)
{
	info->video_in.w = 1280;
	info->video_in.h = 720;
	info->ctrl.crop_ddr_en = 1;
	info->ctrl.crop_en = 1;
	info->ctrl.scale_ddr_en = 1;
	info->ctrl.src_fmt = 0;  // from sif
	info->ctrl.to_pymid = 1;
	info->ctrl.uv_fmt = 1;

	info->crop.crop_st.w = 0;
	info->crop.crop_st.h = 0;
	info->crop.crop_ed.w = 1280;
	info->crop.crop_ed.h = 720;

	info->scale.scale_src.w = 1280;
	info->scale.scale_src.h = 720;
	info->scale.scale_tgt.w = 1280;
	info->scale.scale_tgt.h = 720;
	info->scale.step_x = 4095;
	info->scale.step_y = 4095;
	info->scale.bypass_x = 1;
	info->scale.bypass_y = 1;
	info->scale.pre_scale_x = 0;
	info->scale.pre_scale_y = 0;

	info->frame_id.bus_mode = 0;
	info->frame_id.crop_en = 0;
	info->frame_id.scale_en = 0;

	info->pymid.pymid_en = 0;
	info->pymid.src_from = 0; //isp mode
	info->pymid.ds_layer_en = 3;
	info->pymid.ds_factor[1] = 18;
	info->pymid.ds_roi[1].l = 600;
	info->pymid.ds_roi[1].t = 250;
	info->pymid.ds_roi[1].w = 326;
	info->pymid.ds_roi[1].h = 30;
	info->pymid.ds_src_width[1] = 418;

	info->pymid.ds_factor[2] = 50;
	info->pymid.ds_roi[2].l = 300;
	info->pymid.ds_roi[2].t = 194;
	info->pymid.ds_roi[2].w = 330;
	info->pymid.ds_roi[2].h = 54;
	info->pymid.ds_src_width[2] = 394;

	info->pymid.ds_factor[3] = 12;
	info->pymid.ds_roi[3].l = 116;
	info->pymid.ds_roi[3].t = 60;
	info->pymid.ds_roi[3].w = 132;
	info->pymid.ds_roi[3].h = 92;
	info->pymid.ds_src_width[3] = 250;

	info->pymid.ds_factor[5] = 57;
	info->pymid.ds_roi[5].l = 100;
	info->pymid.ds_roi[5].t = 102;
	info->pymid.ds_roi[5].w = 80;
	info->pymid.ds_roi[5].h = 100;
	info->pymid.ds_src_width[5] = 104;

	info->pymid.us_layer_en = 5;
	info->pymid.us_roi[0].l = 782;
	info->pymid.us_roi[0].t = 0;
	info->pymid.us_roi[0].w = 48;
	info->pymid.us_roi[0].h = 356;
	info->pymid.us_src_width[0] = 38;

	info->pymid.us_roi[2].l = 200;
	info->pymid.us_roi[2].t = 186;
	info->pymid.us_roi[2].w = 516;
	info->pymid.us_roi[2].h = 160;
	info->pymid.us_src_width[2] = 260;
}

static void* ipu_vmap(phys_addr_t start, size_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);
	prot = pgprot_noncached(PAGE_KERNEL);
	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vm_map_ram(pages, page_count, -1, prot);
	kfree(pages);

	return vaddr;
}

static int x2_ipu_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;
	struct resource r;
	struct x2_ipu_data *ipu = NULL;
	struct device_node *np = NULL;
	ipu_cfg_t *ipu_cfg = NULL;

	ipu = devm_kzalloc(&pdev->dev, sizeof(*ipu), GFP_KERNEL);
	if (!ipu) {
		return -ENOMEM;
	}

	ipu_cfg = (ipu_cfg_t *)kzalloc(sizeof(ipu_cfg_t), GFP_KERNEL);
	if (!ipu_cfg) {
		rc = -ENOMEM;
		goto err_out;
	}

	ipu->cfg = ipu_cfg;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		rc = -ENODEV;
		goto err_out;
	}
	if (!request_mem_region(res->start, resource_size(res), X2_IPU_NAME)) {
		rc = -ENOMEM;
		goto err_out;
	}

	ipu->regbase = ioremap(res->start, resource_size(res));
	if (!ipu->regbase) {
		dev_err(&pdev->dev, "unable to map registers\n");
		rc = -ENOMEM;
		goto err_out1;
	}

	dev_info(&pdev->dev, "ipu res regbase=0x%x, mapbase=0x%llx\n",
			 (uint32_t)res->start, (uint64_t)ipu->regbase);
	set_ipu_regbase(ipu->regbase);
	ipu->io_r = res;

	/* request memory address */
	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np) {
		dev_err(&pdev->dev, "No %s specified\n", "memory-region");
		rc = -ENODEV;
		goto err_out2;
	}

	rc = of_address_to_resource(np, 0, &r);
	if (rc) {
		dev_err(&pdev->dev, "No memory address assigned to the region\n");
		goto err_out2;
	}
	ipu->paddr = r.start;
	ipu->memsize = resource_size(&r);
	ipu->vaddr = ipu_vmap(r.start, ipu->memsize);
	//ipu->vaddr = memremap(r.start, ipu->memsize, MEMREMAP_WB);
	dev_info(&pdev->dev, "Allocate reserved memory, paddr: 0x%0llx, vaddr: 0x%0llx, len=0x%x\n",
			 ipu->paddr, (uint64_t)ipu->vaddr, ipu->memsize);

	platform_set_drvdata(pdev, ipu);
	g_ipu = ipu;
	ips_irq_disable(IPU_INT);
	ips_register_irqhandle(IPU_INT, x2_ipu_isr, (void *)g_ipu);
	spin_lock_init(&ipu->elock);
	init_waitqueue_head(&ipu->wq_head);
	ipu->stop = true;
	ips_module_reset(RST_IPU);
	if (ipu->ipu_task == NULL) {
		ipu->ipu_task = kthread_run(ipu_thread, (void *)g_ipu, "ipu_thread");
		if (IS_ERR(ipu->ipu_task)) {
			ipu->ipu_task = NULL;
			ipu_err("thread create fail\n");
			return -1;
		}
	}
	dev_info(&pdev->dev, "x2 ipu probe success\n");
	return 0;
err_out2:
	clr_ipu_regbase();
	iounmap(ipu->regbase);
	ipu->io_r = NULL;

err_out1:
	release_mem_region(res->start, resource_size(res));

err_out:
	kfree(ipu);
	return rc;
}

static int x2_ipu_remove(struct platform_device *pdev)
{
	struct x2_ipu_data *ipu = platform_get_drvdata(pdev);

	ipu_stop_thread(ipu);
	vm_unmap_ram(ipu->vaddr, ipu->memsize / PAGE_SIZE);
	release_mem_region(ipu->io_r->start, resource_size(ipu->io_r));
	clr_ipu_regbase();
	iounmap(ipu->regbase);
	ipu->regbase = NULL;
	ipu->io_r = NULL;

	if (ipu->cfg)
		kfree(ipu->cfg);

	if (ipu)
		devm_kfree(&pdev->dev, ipu);

	g_ipu = NULL;

	return 0;
}

/* Match table for of_platform binding */
static const struct of_device_id x2_ipu_of_match[] = {
	{.compatible = "hobot,x2-ipu", },
	{}
};

MODULE_DEVICE_TABLE(of, x2_ipu_of_match);

static struct platform_driver x2_ipu_platform_driver = {
	.probe	 = x2_ipu_probe,
	.remove  = x2_ipu_remove,
	.driver  = {
		.name = X2_IPU_NAME,
		.of_match_table = x2_ipu_of_match,
	},
};


struct kobject *x2_ipu_kobj;
static ssize_t x2_ipu_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	ipu_dump_regs();
	return (s - buf);
}
static ssize_t x2_ipu_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	int error = -EINVAL;
	return error ? error : n;
}

static struct kobj_attribute ipu_test_attr = {
	.attr   = {
		.name = __stringify(ipu_test_attr),
		.mode = 0644,
	},
	.show   = x2_ipu_show,
	.store  = x2_ipu_store,
};

static struct attribute *attributes[] = {
	&ipu_test_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attributes,
};

static int __init x2_ipu_init(void)
{
	int ret = 0;

	/* Register the platform driver */
	ret = platform_driver_register(&x2_ipu_platform_driver);
	x2_ipu_kobj = kobject_create_and_add("x2_ipu", NULL);
	if (!x2_ipu_kobj)
		return -ENOMEM;
	return sysfs_create_group(x2_ipu_kobj, &attr_group);
	return ret;
}

static void __exit x2_ipu_exit(void)
{
	/* Unregister the platform driver */
	platform_driver_unregister(&x2_ipu_platform_driver);
}

module_init(x2_ipu_init);
module_exit(x2_ipu_exit);

MODULE_DESCRIPTION("Driver for X2 IPU Dev");
MODULE_AUTHOR("Horizon Inc.");
MODULE_LICENSE("GPL");
