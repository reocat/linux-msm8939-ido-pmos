// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm A53 PLL driver
 *
 * Copyright (c) 2017, Linaro Limited
 * Author: Georgi Djakov <georgi.djakov@linaro.org>
 */

#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/module.h>

#include "clk-pll.h"
#include "clk-regmap.h"

struct pll_data {
	const struct pll_freq_tbl *table;
	const char *clk_name;
	int (*init)(struct regmap *regmap, struct clk_pll *pll);
};

static const struct pll_freq_tbl msm8916_freq[] = {
	{  998400000, 52, 0x0, 0x1, 0 },
	{ 1094400000, 57, 0x0, 0x1, 0 },
	{ 1152000000, 62, 0x0, 0x1, 0 },
	{ 1209600000, 63, 0x0, 0x1, 0 },
	{ 1248000000, 65, 0x0, 0x1, 0 },
	{ 1363200000, 71, 0x0, 0x1, 0 },
	{ 1401600000, 73, 0x0, 0x1, 0 },
	{ }
};

static const struct pll_data msm8916_data = {
	.table = msm8916_freq,
	.clk_name = "a53pll",
};

static const struct pll_freq_tbl msm8939_c0_freq[] = {
	{  998400000,  52, 0x0, 0x1, 0 },
	{ 1113600000,  58, 0x0, 0x1, 0 },
	{ 1209600000,  63, 0x0, 0x1, 0 },
};

static int msm8939_c0_init(struct regmap *regmap, struct clk_pll *pll)
{
	const struct pll_freq_tbl *tbl = &pll->freq_tbl[0];

	/* Disable PLL to be safe for programming */
	regmap_write(regmap, pll->mode_reg, 0);

	/* Configure L/M/N values with the first freq_tbl entry */
	regmap_write(regmap, pll->l_reg, tbl->l);
	regmap_write(regmap, pll->m_reg, tbl->m);
	regmap_write(regmap, pll->n_reg, tbl->n);

	/* Configure USER_CTL and CONFIG_CTL value */
	regmap_write(regmap, pll->user_reg, 0x0100000f);
	regmap_write(regmap, pll->config_reg, 0x4c015765);

	return 0;
}

static const struct pll_data msm8939_c0_data = {
	.table = msm8939_c0_freq,
	.clk_name = "a53pll_c0",
	.init = msm8939_c0_init,
};

static const struct pll_freq_tbl msm8939_c1_freq[] = {
	{  652800000, 34, 0x0, 0x1, 0 },
	{  691200000, 36, 0x0, 0x1, 0 },
	{  729600000, 38, 0x0, 0x1, 0 },
	{  806400000, 42, 0x0, 0x1, 0 },
	{  844800000, 44, 0x0, 0x1, 0 },
	{  883200000, 46, 0x0, 0x1, 0 },
	{  960000000, 50, 0x0, 0x1, 0 },
	{  998400000, 52, 0x0, 0x1, 0 },
	{ 1036800000, 54, 0x0, 0x1, 0 },
	{ 1113600000, 58, 0x0, 0x1, 0 },
	{ 1209600000, 63, 0x0, 0x1, 0 },
	{ 1190400000, 62, 0x0, 0x1, 0 },
	{ 1267200000, 66, 0x0, 0x1, 0 },
	{ 1344000000, 70, 0x0, 0x1, 0 },
	{ 1363200000, 71, 0x0, 0x1, 0 },
	{ 1420800000, 74, 0x0, 0x1, 0 },
	{ 1459200000, 76, 0x0, 0x1, 0 },
	{ 1497600000, 78, 0x0, 0x1, 0 },
	{ 1536000000, 80, 0x0, 0x1, 0 },
	{ 1574400000, 82, 0x0, 0x1, 0 },
	{ 1612800000, 84, 0x0, 0x1, 0 },
	{ 1632000000, 85, 0x0, 0x1, 0 },
	{ 1651200000, 86, 0x0, 0x1, 0 },
	{ 1689600000, 88, 0x0, 0x1, 0 },
	{ 1708800000, 89, 0x0, 0x1, 0 },
};

static const struct pll_data msm8939_c1_data = {
	.table = msm8939_c1_freq,
	.clk_name = "a53pll_c1",
};

static const struct pll_freq_tbl msm8939_cci_freq[] = {
	{ 403200000, 21, 0x0, 0x1, 0 },
	{ 595200000, 31, 0x0, 0x1, 0 },
};

static const struct pll_data msm8939_cci_data = {
	.table = msm8939_cci_freq,
	.clk_name = "a53pll_cci",
};

static const struct regmap_config a53pll_regmap_config = {
	.reg_bits		= 32,
	.reg_stride		= 4,
	.val_bits		= 32,
	.max_register		= 0x40,
	.fast_io		= true,
};

static int qcom_a53pll_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	struct resource *res;
	struct clk_pll *pll;
	void __iomem *base;
	struct clk_init_data init = { };
	const struct pll_data *data;
	int ret;

	data = of_device_get_match_data(&pdev->dev);
	if (!data)
                return -ENODEV;

	pll = devm_kzalloc(dev, sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap = devm_regmap_init_mmio(dev, base, &a53pll_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	pll->l_reg = 0x04;
	pll->m_reg = 0x08;
	pll->n_reg = 0x0c;
	pll->user_reg = 0x10;
	pll->config_reg = 0x14;
	pll->mode_reg = 0x00;
	pll->status_reg = 0x1c;
	pll->status_bit = 16;
	pll->freq_tbl = data->table;

	init.name = data->clk_name;
	init.parent_names = (const char *[]){ "xo" };
	init.num_parents = 1;
	init.ops = &clk_pll_sr2_ops;
	pll->clkr.hw.init = &init;

	if (data->init) {
		ret = data->init(regmap, pll);
		if (ret) {
			dev_err(dev, "failed to init pll: %d\n", ret);
			return ret;
		}
	}

	ret = devm_clk_register_regmap(dev, &pll->clkr);
	if (ret) {
		dev_err(dev, "failed to register regmap clock: %d\n", ret);
		return ret;
	}

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_simple_get,
					  &pll->clkr.hw);
	if (ret) {
		dev_err(dev, "failed to add clock provider: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id qcom_a53pll_match_table[] = {
	{ .compatible = "qcom,msm8916-a53pll", .data = &msm8916_data, },
	{ .compatible = "qcom,msm8939-a53pll-c0", .data = &msm8939_c0_data, },
	{ .compatible = "qcom,msm8939-a53pll-c1", .data = &msm8939_c1_data, },
	{ .compatible = "qcom,msm8939-a53pll-cci", .data = &msm8939_cci_data, },
	{ }
};
MODULE_DEVICE_TABLE(of, qcom_a53pll_match_table);

static struct platform_driver qcom_a53pll_driver = {
	.probe = qcom_a53pll_probe,
	.driver = {
		.name = "qcom-a53pll",
		.of_match_table = qcom_a53pll_match_table,
	},
};
module_platform_driver(qcom_a53pll_driver);

MODULE_DESCRIPTION("Qualcomm A53 PLL Driver");
MODULE_LICENSE("GPL v2");
