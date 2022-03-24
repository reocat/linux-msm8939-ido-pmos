/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "devbw: " fmt

#include <linux/devfreq.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interconnect.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <trace/events/power.h>

struct dev_data {
	struct icc_path *path;
	unsigned int ab_percent;
	unsigned long curr_freq;
	struct devfreq *df;
	struct devfreq_dev_profile dp;
	struct opp_table *opp_table;
	struct mutex lock;
};

static int devbw_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct dev_data *d = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	int ret = 0;

	/* Get correct frequency for d. */
	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp)) {
		dev_err(dev, "failed to get recommended opp instance\n");
		return PTR_ERR(opp);
	}

	/* Change voltage and frequency according to new OPP level */
	mutex_lock(&d->lock);
        ret = dev_pm_opp_set_opp(dev, opp);
        dev_pm_opp_put(opp);
	if (!ret)
		d->curr_freq = *freq;

	mutex_unlock(&d->lock);

	return ret;

}

static int devbw_get_dev_status(struct device *dev,
				struct devfreq_dev_status *stat)
{
	struct dev_data *d = dev_get_drvdata(dev);

	stat->current_frequency = d->curr_freq;

	return 0;
}

static int devfreq_add_devbw(struct device *dev)
{
	struct dev_data *d;
	struct devfreq_dev_profile *p;
	const char *gov_name = DEVFREQ_GOV_USERSPACE;
	int ret;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	mutex_init(&d->lock);

	p = &d->dp;
	p->polling_ms = 50;
	p->target = devbw_target;
	p->get_dev_status = devbw_get_dev_status;

	d->path = of_icc_get(dev, "mem");
	if (IS_ERR(d->path)) {
		dev_err(dev, "Unable to get interconnect path\n");
		ret = PTR_ERR(d->path);
		goto devbw_exit;
	}

	dev_set_drvdata(dev, d);

	/* Get the freq from OPP table to scale the d freq */
	ret = dev_pm_opp_of_add_table(dev);
	if (ret < 0) {
		dev_err(dev, "failed to get OPP table\n");
		goto path_exit;
	}

	d->df = devm_devfreq_add_device(dev, p, gov_name, NULL);
	if (IS_ERR(d->df)) {
		ret = PTR_ERR(d->df);
		goto path_exit;
	}

	return 0;

path_exit:
	icc_put(d->path);
devbw_exit:
	return ret;
}

static int devfreq_remove_devbw(struct device *dev)
{
	struct dev_data *d = dev_get_drvdata(dev);
	icc_put(d->path);
	devfreq_remove_device(d->df);
	return 0;
}

static int devfreq_devbw_probe(struct platform_device *pdev)
{
	return devfreq_add_devbw(&pdev->dev);
}

static int devfreq_devbw_remove(struct platform_device *pdev)
{
	return devfreq_remove_devbw(&pdev->dev);
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,devbw" },
	{}
};

static struct platform_driver devbw_driver = {
	.probe = devfreq_devbw_probe,
	.remove = devfreq_devbw_remove,
	.driver = {
		.name = "devbw",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init devbw_init(void)
{
	platform_driver_register(&devbw_driver);
	return 0;
}
device_initcall(devbw_init);

MODULE_DESCRIPTION("Device DDR bandwidth voting driver MSM SoCs");
MODULE_LICENSE("GPL v2");
