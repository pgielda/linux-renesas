/*
 * RZA1 clock framework support
 *
 * Copyright (C) 2013  Renesas Solutions Corp.
 * Copyright (C) 2012  Phil Edworthy
 * Copyright (C) 2011  Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/sh_clk.h>
#include <linux/clkdev.h>
#include <mach/common.h>

/* registers */
#define FRQCR		0xfcfe0010
#define FRQCR2		0xfcfe0014
#define STBCR3		0xfcfe0420
#define STBCR4		0xfcfe0424
#define STBCR5		0xfcfe0428
#define STBCR6		0xfcfe042c
#define STBCR7		0xfcfe0430
#define STBCR8		0xfcfe0434
#define STBCR9		0xfcfe0438
#define STBCR10		0xfcfe043c
#define STBCR11		0xfcfe0440
#define STBCR12		0xfcfe0444

#define PLL_RATE 30

/* Fixed 32 KHz root clock for RTC */
static struct clk r_clk = {
	.rate		= 32768,
};

/*
 * Default rate for the root input clock, reset this with clk_set_rate()
 * from the platform code.
 */
static struct clk extal_clk = {
	.rate		= 13330000,
};

static unsigned long pll_recalc(struct clk *clk)
{
	return clk->parent->rate * PLL_RATE;
}

static struct sh_clk_ops pll_clk_ops = {
	.recalc		= pll_recalc,
};

static struct clk pll_clk = {
	.ops		= &pll_clk_ops,
	.parent		= &extal_clk,
	.flags		= CLK_ENABLE_ON_INIT,
};

static unsigned long bus_recalc(struct clk *clk)
{
	return clk->parent->rate * 2 / 3;
}

static struct sh_clk_ops bus_clk_ops = {
	.recalc		= bus_recalc,
};

static struct clk bus_clk = {
	.ops		= &bus_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
};

static unsigned long peripheral0_recalc(struct clk *clk)
{
	return clk->parent->rate / 12;
}

static struct sh_clk_ops peripheral0_clk_ops = {
	.recalc		= peripheral0_recalc,
};

static struct clk peripheral0_clk = {
	.ops		= &peripheral0_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
};

static unsigned long peripheral1_recalc(struct clk *clk)
{
	return clk->parent->rate / 6;
}

static struct sh_clk_ops peripheral1_clk_ops = {
	.recalc		= peripheral1_recalc,
};

static struct clk peripheral1_clk = {
	.ops		= &peripheral1_clk_ops,
	.parent		= &pll_clk,
	.flags		= CLK_ENABLE_ON_INIT,
};

struct clk *main_clks[] = {
	&r_clk,
	&extal_clk,
	&pll_clk,
	&bus_clk,
	&peripheral0_clk,
	&peripheral1_clk,
};

static int div2[] = { 1, 3, 0, 3 }; /* 1, 2/3, reserve, 1/3 */
static int multipliers[] = { 1, 2, 1, 1 };

static struct clk_div_mult_table div4_div_mult_table = {
	.divisors = div2,
	.nr_divisors = ARRAY_SIZE(div2),
	.multipliers = multipliers,
	.nr_multipliers = ARRAY_SIZE(multipliers),
};

static struct clk_div4_table div4_table = {
	.div_mult_table = &div4_div_mult_table,
};

enum { DIV4_I,
	DIV4_NR };

#define DIV4(_reg, _bit, _mask, _flags) \
	SH_CLK_DIV4(&pll_clk, _reg, _bit, _mask, _flags)

/* The mask field specifies the div2 entries that are valid */
struct clk div4_clks[DIV4_NR] = {
	[DIV4_I]  = DIV4(FRQCR, 8, 0xB, CLK_ENABLE_REG_16BIT
					| CLK_ENABLE_ON_INIT),
};

enum { MSTP51, MSTP50,
	MSTP71, MSTP70,
	MSTP47, MSTP46, MSTP45, MSTP44, MSTP43, MSTP42, MSTP41, MSTP40,
	MSTP33, MSTP67, MSTP60,
	MSTP84,
	MSTP92, MSTP93,
#if defined(CONFIG_FB_VDC5)
	MSTP90, MSTP91,
#endif
	MSTP94, MSTP95, MSTP96, MSTP97,
	MSTP107, MSTP106, MSTP105, MSTP104, MSTP103,
	MSTP123, MSTP122, MSTP121, MSTP120,
	MSTP81,		/* SCUX */
	MSTP115,	/* SSIF0 */
	MSTP_NR };

static struct clk mstp_clks[MSTP_NR] = {
	[MSTP107] = SH_CLK_MSTP8(&peripheral1_clk, STBCR10, 7, 0), /* RSPI0 */
	[MSTP106] = SH_CLK_MSTP8(&peripheral1_clk, STBCR10, 6, 0), /* RSPI1 */
	[MSTP105] = SH_CLK_MSTP8(&peripheral1_clk, STBCR10, 5, 0), /* RSPI2 */
	[MSTP104] = SH_CLK_MSTP8(&peripheral1_clk, STBCR10, 4, 0), /* RSPI3 */
	[MSTP103] = SH_CLK_MSTP8(&peripheral1_clk, STBCR10, 3, 0), /* RSPI4 */
	[MSTP51] = SH_CLK_MSTP8(&peripheral0_clk, STBCR5, 0, 0),   /* OST0 */
	[MSTP50] = SH_CLK_MSTP8(&peripheral0_clk, STBCR5, 1, 0),   /* OST1 */
	[MSTP71] = SH_CLK_MSTP8(&peripheral1_clk, STBCR7, 1, 0),   /* USB0 */
	[MSTP70] = SH_CLK_MSTP8(&peripheral1_clk, STBCR7, 0, 0),   /* USB1 */
	[MSTP47] = SH_CLK_MSTP8(&peripheral1_clk, STBCR4, 7, 0),   /* SCIF0 */
	[MSTP46] = SH_CLK_MSTP8(&peripheral1_clk, STBCR4, 6, 0),   /* SCIF1 */
	[MSTP45] = SH_CLK_MSTP8(&peripheral1_clk, STBCR4, 5, 0),   /* SCIF2 */
	[MSTP44] = SH_CLK_MSTP8(&peripheral1_clk, STBCR4, 4, 0),   /* SCIF3 */
	[MSTP43] = SH_CLK_MSTP8(&peripheral1_clk, STBCR4, 3, 0),   /* SCIF4 */
	[MSTP42] = SH_CLK_MSTP8(&peripheral1_clk, STBCR4, 2, 0),   /* SCIF5 */
	[MSTP41] = SH_CLK_MSTP8(&peripheral1_clk, STBCR4, 1, 0),   /* SCIF6 */
	[MSTP40] = SH_CLK_MSTP8(&peripheral1_clk, STBCR4, 0, 0),   /* SCIF7 */
	[MSTP33] = SH_CLK_MSTP8(&peripheral0_clk, STBCR3, 3, 0),   /* MTU2 */
	[MSTP67] = SH_CLK_MSTP8(&peripheral1_clk, STBCR6, 7, 0),   /* ADC */
	[MSTP60] = SH_CLK_MSTP8(&r_clk, STBCR6, 0, 0),		   /* RTC */
	[MSTP123] = SH_CLK_MSTP8(&peripheral1_clk, STBCR12, 3, 0), /* SDHI00 */
	[MSTP122] = SH_CLK_MSTP8(&peripheral1_clk, STBCR12, 2,
			CLK_ENABLE_ON_INIT),			   /* SDHI01 */
	[MSTP121] = SH_CLK_MSTP8(&peripheral1_clk, STBCR12, 1, 0), /* SDHI10 */
	[MSTP120] = SH_CLK_MSTP8(&peripheral1_clk, STBCR12, 0,
			CLK_ENABLE_ON_INIT),			   /* SDHI11 */
	[MSTP84] = SH_CLK_MSTP8(&peripheral1_clk, STBCR8, 4, 0),   /* MMC */
#if defined(CONFIG_FB_VDC5)
	[MSTP90] = SH_CLK_MSTP8(&peripheral1_clk, STBCR9, 0, 0),   /* SPIBSC1 */
	[MSTP91] = SH_CLK_MSTP8(&peripheral1_clk, STBCR9, 1, 0),   /* SPIBSC1 */
#endif
	[MSTP92] = SH_CLK_MSTP8(&bus_clk, STBCR9, 2, 0),	   /* SPIBSC1 */
	[MSTP93] = SH_CLK_MSTP8(&bus_clk, STBCR9, 3, 0),	   /* SPIBSC0 */
	[MSTP94] = SH_CLK_MSTP8(&peripheral0_clk, STBCR9, 4, 0),   /* RIIC3 */
	[MSTP95] = SH_CLK_MSTP8(&peripheral0_clk, STBCR9, 5, 0),   /* RIIC2 */
	[MSTP96] = SH_CLK_MSTP8(&peripheral0_clk, STBCR9, 6, 0),   /* RIIC1 */
	[MSTP97] = SH_CLK_MSTP8(&peripheral0_clk, STBCR9, 7, 0),   /* RIIC0 */
	[MSTP81] = SH_CLK_MSTP8(&peripheral0_clk, STBCR8, 1,
			CLK_ENABLE_ON_INIT),			   /* SCUX */
	[MSTP115] = SH_CLK_MSTP8(&peripheral0_clk, STBCR11, 5,
			CLK_ENABLE_ON_INIT),			   /* SSIF0 */
};

static struct clk_lookup lookups[] = {
	/* main clocks */
	CLKDEV_CON_ID("rclk", &r_clk),
	CLKDEV_CON_ID("extal", &extal_clk),
	CLKDEV_CON_ID("pll_clk", &pll_clk),
	CLKDEV_CON_ID("peripheral_clk", &peripheral1_clk),

	/* DIV4 clocks */
	CLKDEV_CON_ID("cpu_clk", &div4_clks[DIV4_I]),

	/* MSTP clocks */
	CLKDEV_ICK_ID("sci_fck", "sh-sci.0", &mstp_clks[MSTP47]),
	CLKDEV_ICK_ID("sci_fck", "sh-sci.1", &mstp_clks[MSTP46]),
	CLKDEV_ICK_ID("sci_fck", "sh-sci.2", &mstp_clks[MSTP45]),
	CLKDEV_ICK_ID("sci_fck", "sh-sci.3", &mstp_clks[MSTP44]),
	CLKDEV_ICK_ID("sci_fck", "sh-sci.4", &mstp_clks[MSTP43]),
	CLKDEV_ICK_ID("sci_fck", "sh-sci.5", &mstp_clks[MSTP42]),
	CLKDEV_ICK_ID("sci_fck", "sh-sci.6", &mstp_clks[MSTP41]),
	CLKDEV_ICK_ID("sci_fck", "sh-sci.7", &mstp_clks[MSTP40]),
	CLKDEV_CON_ID("usb0", &mstp_clks[MSTP71]),
	CLKDEV_CON_ID("usb1", &mstp_clks[MSTP70]),
	CLKDEV_CON_ID("mtu2_fck", &mstp_clks[MSTP33]),
	CLKDEV_CON_ID("adc0", &mstp_clks[MSTP67]),
	CLKDEV_CON_ID("rtc0", &mstp_clks[MSTP60]),
	CLKDEV_DEV_ID("sh_mmcif", &mstp_clks[MSTP84]),
	CLKDEV_DEV_ID("sh_mobile_sdhi.0", &mstp_clks[MSTP123]),
	CLKDEV_DEV_ID("sh_mobile_sdhi.1", &mstp_clks[MSTP121]),
	CLKDEV_CON_ID("riic0", &mstp_clks[MSTP97]),
	CLKDEV_CON_ID("riic1", &mstp_clks[MSTP96]),
	CLKDEV_CON_ID("riic2", &mstp_clks[MSTP95]),
	CLKDEV_CON_ID("riic3", &mstp_clks[MSTP94]),
	CLKDEV_CON_ID("spibsc0", &mstp_clks[MSTP93]),
	CLKDEV_CON_ID("spibsc1", &mstp_clks[MSTP92]),
	CLKDEV_CON_ID("rspi0", &mstp_clks[MSTP107]),
	CLKDEV_CON_ID("rspi1", &mstp_clks[MSTP106]),
	CLKDEV_CON_ID("rspi2", &mstp_clks[MSTP105]),
	CLKDEV_CON_ID("rspi3", &mstp_clks[MSTP104]),
	CLKDEV_CON_ID("rspi4", &mstp_clks[MSTP103]),
	CLKDEV_CON_ID("scux", &mstp_clks[MSTP81]),	/* SCUX */
	CLKDEV_CON_ID("ssif0", &mstp_clks[MSTP115]),	/* SSIF0 */
#if defined(CONFIG_FB_VDC5)
	CLKDEV_CON_ID("lvds", &mstp_clks[MSTP91]),
	CLKDEV_CON_ID("vdc5fb.0", &mstp_clks[MSTP91]),
	CLKDEV_CON_ID("vdc5fb.1", &mstp_clks[MSTP90]),
#endif
};

int __init rza1_clock_init(void)
{
	int k, ret = 0;

	for (k = 0; !ret && (k < ARRAY_SIZE(main_clks)); k++)
		ret = clk_register(main_clks[k]);

	clkdev_add_table(lookups, ARRAY_SIZE(lookups));

	if (!ret)
		ret = sh_clk_div4_register(div4_clks, DIV4_NR, &div4_table);

	if (!ret)
		ret = sh_clk_mstp_register(mstp_clks, MSTP_NR);

	if (!ret)
		shmobile_clk_init();
	else
		panic("failed to setup rza1 clocks\n");
	return ret;
}
