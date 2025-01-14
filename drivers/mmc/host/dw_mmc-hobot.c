/*
 * Copyright (c) 2014, Fuzhou Rockchip Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/delay.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

#define X2_CLKGEN_DIV       2

struct dw_mci_hobot_priv_data {
	struct clk		*drv_clk;
	struct clk		*sample_clk;
	int			default_sample_phase;
};
static void dw_mci_x2_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
    return;
}

#define NUM_PHASES			360
#define TUNING_ITERATION_TO_PHASE(i)	(DIV_ROUND_UP((i) * 360, NUM_PHASES))

static int dw_mci_x2_execute_tuning(struct dw_mci_slot *slot, u32 opcode)
{
	struct dw_mci *host = slot->host;
	struct dw_mci_hobot_priv_data *priv = host->priv;
	struct mmc_host *mmc = slot->mmc;
	int ret = 0;
	int i;
	bool v, prev_v = 0, first_v;
	struct range_t {
		int start;
		int end; /* inclusive */
	};
	struct range_t *ranges;
	unsigned int range_count = 0;
	int longest_range_len = -1;
	int longest_range = -1;
	int middle_phase;

	if (IS_ERR(priv->sample_clk)) {
		dev_err(host->dev, "Tuning clock (sample_clk) not defined.\n");
		return -EIO;
	}

	ranges = kmalloc_array(NUM_PHASES / 2 + 1, sizeof(*ranges), GFP_KERNEL);
	if (!ranges)
		return -ENOMEM;

	/* Try each phase and extract good ranges */
	for (i = 0; i < NUM_PHASES; ) {
		clk_set_phase(priv->sample_clk, TUNING_ITERATION_TO_PHASE(i));

		v = !mmc_send_tuning(mmc, opcode, NULL);

		if (i == 0)
			first_v = v;

		if ((!prev_v) && v) {
			range_count++;
			ranges[range_count-1].start = i;
		}
		if (v) {
			ranges[range_count-1].end = i;
			i++;
		} else if (i == NUM_PHASES - 1) {
			/* No extra skipping rules if we're at the end */
			i++;
		} else {
			/*
			 * No need to check too close to an invalid
			 * one since testing bad phases is slow.  Skip
			 * 20 degrees.
			 */
			i += DIV_ROUND_UP(20 * NUM_PHASES, 360);

			/* Always test the last one */
			if (i >= NUM_PHASES)
				i = NUM_PHASES - 1;
		}

		prev_v = v;
	}

	if (range_count == 0) {
		dev_warn(host->dev, "All phases bad!");
		ret = -EIO;
		goto free;
	}

	/* wrap around case, merge the end points */
	if ((range_count > 1) && first_v && v) {
		ranges[0].start = ranges[range_count-1].start;
		range_count--;
	}

	if (ranges[0].start == 0 && ranges[0].end == NUM_PHASES - 1) {
		clk_set_phase(priv->sample_clk, priv->default_sample_phase);
		dev_info(host->dev, "All phases work, using default phase %d.",
			 priv->default_sample_phase);
		goto free;
	}

	/* Find the longest range */
	for (i = 0; i < range_count; i++) {
		int len = (ranges[i].end - ranges[i].start + 1);

		if (len < 0)
			len += NUM_PHASES;

		if (longest_range_len < len) {
			longest_range_len = len;
			longest_range = i;
		}

		dev_dbg(host->dev, "Good phase range %d-%d (%d len)\n",
			TUNING_ITERATION_TO_PHASE(ranges[i].start),
			TUNING_ITERATION_TO_PHASE(ranges[i].end),
			len
		);
	}

	dev_dbg(host->dev, "Best phase range %d-%d (%d len)\n",
		TUNING_ITERATION_TO_PHASE(ranges[longest_range].start),
		TUNING_ITERATION_TO_PHASE(ranges[longest_range].end),
		longest_range_len
	);

	middle_phase = ranges[longest_range].start + longest_range_len / 2;
	middle_phase %= NUM_PHASES;
	dev_info(host->dev, "Successfully tuned phase to %d\n",
		 TUNING_ITERATION_TO_PHASE(middle_phase));

	clk_set_phase(priv->sample_clk,
		      TUNING_ITERATION_TO_PHASE(middle_phase));

free:
	kfree(ranges);
	return ret;
}

static int dw_mci_x2_parse_dt(struct dw_mci *host)
{
	struct device_node *np = host->dev->of_node;
	struct dw_mci_hobot_priv_data *priv;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (of_property_read_u32(np, "hobot,default-sample-phase",
					&priv->default_sample_phase))
		priv->default_sample_phase = 0;

	priv->drv_clk = devm_clk_get(host->dev, "ciu-drive");
	if (IS_ERR(priv->drv_clk))
		dev_dbg(host->dev, "ciu_drv not available\n");

	priv->sample_clk = devm_clk_get(host->dev, "ciu-sample");
	if (IS_ERR(priv->sample_clk))
		dev_dbg(host->dev, "ciu_sample not available\n");

	host->priv = priv;

	return 0;
}

static int dw_mci_hobot_init(struct dw_mci *host)
{
	/* It is slot 8 on Hobot SoCs */
	host->sdio_id0 = 0;

	if (of_device_is_compatible(host->dev->of_node,
				    "hobot,x2-dw-mshc"))
		host->bus_hz /= X2_CLKGEN_DIV;

	return 0;
}

/* Common capabilities of X2 SoC */
static unsigned long dw_mci_x2_dwmmc_caps[4] = {
    MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
	MMC_CAP_CMD23,
	MMC_CAP_CMD23,
	MMC_CAP_CMD23,
};

static const struct dw_mci_drv_data x2_drv_data = {
	.caps			= dw_mci_x2_dwmmc_caps,
	.num_caps		= ARRAY_SIZE(dw_mci_x2_dwmmc_caps),
	.set_ios		= dw_mci_x2_set_ios,
	.execute_tuning		= dw_mci_x2_execute_tuning,
	.parse_dt		= dw_mci_x2_parse_dt,
	.init			= dw_mci_hobot_init,
};

static const struct of_device_id dw_mci_hobot_match[] = {
	{ .compatible = "hobot,x2-dw-mshc",
		.data = &x2_drv_data },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_hobot_match);

static int dw_mci_hobot_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data;
	const struct of_device_id *match;

   if (!pdev->dev.of_node)
       return -ENODEV;

	match = of_match_node(dw_mci_hobot_match, pdev->dev.of_node);
	drv_data = match->data;
	return dw_mci_pltfm_register(pdev, drv_data);
}

static struct platform_driver dw_mci_hobot_pltfm_driver = {
	.probe		= dw_mci_hobot_probe,
	.remove		= dw_mci_pltfm_remove,
	.driver		= {
		.name		= "dwmmc_hobot",
		.of_match_table	= dw_mci_hobot_match,
		.pm		= &dw_mci_pltfm_pmops,
	},
};

module_platform_driver(dw_mci_hobot_pltfm_driver);

MODULE_AUTHOR("shaochuanzhang <shaochuan.zhang@hobot.ai>");
MODULE_DESCRIPTION("HobotRobotics Specific DW-MSHC Driver Extension");
MODULE_ALIAS("platform:dwmmc_hobot");
MODULE_LICENSE("GPL v2");
