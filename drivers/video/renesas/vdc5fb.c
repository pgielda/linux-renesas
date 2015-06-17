/*
 * Copyright (C) 2013 Renesas Solutions Corp.
 *
 * Based on drivers/video/ren_vdc4.c
 * Copyright (c) 2012 Renesas Electronics Europe Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/clk.h>
#include <linux/sh_clk.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <asm/div64.h>
#include <video/vdc5fb.h>

/************************************************************************/

#define PALETTE_NR 16

struct vdc5fb_priv {
	struct platform_device *pdev;
	struct vdc5fb_pdata *pdata;
	const char *dev_name;
	struct fb_videomode *videomode;	/* current */
	/* clock */
	struct clk *clk;
	struct clk *dot_clk;
	struct clk *lvds_clk;
	/* framebuffers */
	void __iomem *base;
	dma_addr_t dma_handle;
	unsigned long flm_off;
	unsigned long flm_num;
	int fb_nofree;
	/* irq */
	struct {
		int start;		/* start irq number */
		int end;		/* end irq number, inclusive */
		u32 mask[3];		/* curremnt irq mask */
		char longname[VDC5FB_IRQ_SIZE][32];	/* ire name */
	} irq;
	/* display */
	struct fb_info *info;
	unsigned long dc;		/* dot clock in Hz */
	unsigned int dcdr;		/* dot clock divisor */
	unsigned int rr;		/* refresh rate in Hz */
	unsigned int res_fv;		/* vsync period (in fh) */
	unsigned int res_fh;		/* hsync period (in dc) */
	u32 pseudo_palette[PALETTE_NR];
};

/************************************************************************/
/* Workplace for vdc5-regs.h */
#include "vdc5fb-regs.h"

/************************************************************************/

static int vdc5fb_init_syscnt(struct vdc5fb_priv *priv);
static int vdc5fb_init_sync(struct vdc5fb_priv *priv);
static int vdc5fb_init_scalers(struct vdc5fb_priv *priv);
static int vdc5fb_init_graphics(struct vdc5fb_priv *priv);
static int vdc5fb_init_outcnt(struct vdc5fb_priv *priv);
static int vdc5fb_init_tcon(struct vdc5fb_priv *priv);
static int vdc5fb_remove(struct platform_device *pdev);

/************************************************************************/

static inline struct vdc5fb_pdata *priv_to_pdata(struct vdc5fb_priv *priv)
{
	return (struct vdc5fb_pdata *)(priv->pdev->dev.platform_data);
}

/************************************************************************/
/* INTERUPT HANDLING */

static irqreturn_t vdc5fb_irq(int irq, void *data)
{
	struct vdc5fb_priv *priv = (struct vdc5fb_priv *)data;

	irq = irq - priv->irq.start;
	switch (irq) {
	case S0_VI_VSYNC:	/* INT0 */
	case S0_LO_VSYNC:	/* INT1 */
	case S0_VSYNCERR:	/* INT2 */
	case GR3_VLINE:		/* INT3 */
	case S0_VFIELD:		/* INT4 */
	case IV1_VBUFERR:	/* INT5 */
	case IV3_VBUFERR:	/* INT6 */
	case IV5_VBUFERR:	/* INT7 */
		break;
	case IV6_VBUFERR:	/* INT8 */
	case S0_WLINE:		/* INT9 */
	case S1_VI_VSYNC:	/* INT10 */
	case S1_LO_VSYNC:	/* INT11 */
	case S1_VSYNCERR:	/* INT12 */
	case S1_VFIELD:		/* INT13 */
	case IV2_VBUFERR:	/* INT14 */
	case IV4_VBUFERR:	/* INT15 */
		break;
	case S1_WLINE:		/* INT16 */
	case OIR_VI_VSYNC:	/* INT17 */
	case OIR_LO_VSYNC:	/* INT18 */
	case OIR_VLINE:		/* INT19 */
	case OIR_VFIELD:	/* INT20 */
	case IV7_VBUFERR:	/* INT21 */
	case IV8_VBUFERR:	/* INT22 */
		break;
	default:
		dev_err(&priv->pdev->dev, "unexpected irq (%d+%d)\n",
			priv->irq.start, irq);
		break;
	}

	return IRQ_HANDLED;
}

static int vdc5fb_init_irqs(struct vdc5fb_priv *priv)
{
	int error = -EINVAL;
	struct platform_device *pdev;
	struct resource *res;
	int irq;

	pdev = priv->pdev;
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return error;

	priv->irq.start = res->start;
	priv->irq.end = res->end;
	BUG_ON((priv->irq.end - priv->irq.start + 1) != VDC5FB_MAX_IRQS);

	for (irq = 0; irq < VDC5FB_MAX_IRQS; irq++) {
		snprintf(priv->irq.longname[irq],
			sizeof(priv->irq.longname[0]), "%s: %s",
			priv->dev_name, irq_names[irq]);
		error = request_irq((priv->irq.start + irq),
			vdc5fb_irq, 0, priv->irq.longname[irq], priv);
		if (error < 0) {
			while (--irq >= 0)
				free_irq(priv->irq.start + irq, priv);
			return error;
		}
	}

	return 0;
}

static void vdc5fb_deinit_irqs(struct vdc5fb_priv *priv)
{
	int irq;

	for (irq = priv->irq.start; irq <= priv->irq.end; irq++)
		free_irq(irq, priv);
}

/************************************************************************/
/* CLOCK HANDLING */

static int vdc5fb_init_clocks(struct vdc5fb_priv *priv)
{
	static const char clkname_p1clk[] = "peripheral_clk";
					/* TODO: should be global */
	static const char clkname_lvds[] = "lvds";
					/* TODO: should be global */
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	struct platform_device *pdev = priv->pdev;

	priv->clk = clk_get(&pdev->dev, priv->dev_name);
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "cannot get clock \"%s\"\n",
			priv->dev_name);
		return PTR_ERR(priv->clk);
	}

	priv->dot_clk = clk_get(&pdev->dev, clkname_p1clk);
	if (IS_ERR(priv->dot_clk)) {
		dev_err(&pdev->dev, "cannot get clock \"%s\"\n", clkname_p1clk);
		clk_put(priv->clk);
		return PTR_ERR(priv->dot_clk);
	}

	if (pdata->use_lvds) {
		priv->lvds_clk = clk_get(&pdev->dev, clkname_lvds);
		if (IS_ERR(priv->lvds_clk)) {
			dev_err(&pdev->dev, "cannot get clock \"%s\"\n",
				clkname_lvds);
			clk_put(priv->clk);
			clk_put(priv->dot_clk);
			return PTR_ERR(priv->lvds_clk);
		}
	}

	return 0;
}

static void vdc5fb_deinit_clocks(struct vdc5fb_priv *priv)
{
	if (priv->lvds_clk)
		clk_put(priv->lvds_clk);
	if (priv->dot_clk)
		clk_put(priv->dot_clk);
	if (priv->clk)
		clk_put(priv->clk);
}

/************************************************************************/

static void vdc5fb_clear_fb(struct vdc5fb_priv *priv)
{
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	char *start;
	size_t size;

	start = (char *)priv->info->screen_base;
	size = pdata->videomode->xres * pdata->videomode->yres
		* (pdata->bpp / 8);

/* TEMPORARY CODE: WRITE ENABLE SRAM */
	iowrite8(0xff, (void *)0xFCFE0400);
	iowrite8(0xff, (void *)0xFCFE0404);
	iowrite8(0x0f, (void *)0xFCFE0408);

	memset(start, 0x0, size);
}

static int vdc5fb_update_regs(struct vdc5fb_priv *priv,
	int reg, uint32_t bits, int wait)
{
	uint32_t tmp;
	long timeout;

	tmp = vdc5fb_read(priv, reg);
	tmp |= bits;
	vdc5fb_write(priv, reg, tmp);

	if (wait) {
		timeout = 50;
		do {
			tmp = vdc5fb_read(priv, reg);
			if ((tmp & bits) == 0)
				return 0;
			udelay(1000);
		} while (--timeout > 0);
	/* wait for max. 50 ms... */
	}
	dev_err(&priv->pdev->dev, "update_regs timeout at %d in %s\n",
		__LINE__, __func__);
	return -1;
}

/************************************************************************/

static int vdc5fb_set_panel_clock(struct vdc5fb_priv *priv,
	struct fb_videomode *mode)
{
	static const unsigned char dcdr_list[13] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 16, 24, 32,
	};
	uint64_t desired64 = 1000000000000;
	unsigned long desired;
	unsigned long source;
	unsigned long used;
	int n;

	source = clk_get_rate(priv->dot_clk);
	BUG_ON(source == 0);

	(void)do_div(desired64, mode->pixclock);
	desired = (unsigned long)desired64;
	for (n = 0; n < ARRAY_SIZE(dcdr_list); n++) {
		used = source / dcdr_list[n];
		if (used <= desired) {
			priv->dcdr = dcdr_list[n];
			priv->dc = used;
			return 0;
		}
	}
	return -1;
}

/************************************************************************/

static int vdc5fb_init_syscnt(struct vdc5fb_priv *priv)
{
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	u32 tmp;

	/* Ignore all irqs here */
	priv->irq.mask[0] = 0;
	priv->irq.mask[1] = 0;
	priv->irq.mask[2] = 0;
	vdc5fb_write(priv, SYSCNT_INT4, priv->irq.mask[0]);
	vdc5fb_write(priv, SYSCNT_INT5, priv->irq.mask[1]);
	vdc5fb_write(priv, SYSCNT_INT6, priv->irq.mask[2]);

	/* Clear all pending irqs */
	vdc5fb_write(priv, SYSCNT_INT1, 0);
	vdc5fb_write(priv, SYSCNT_INT2, 0);
	vdc5fb_write(priv, SYSCNT_INT3, 0);

	/* Setup panel clock */
	tmp = PANEL_DCDR(priv->dcdr);
	tmp |= PANEL_ICKEN;
	tmp |= PANEL_OCKSEL(0);
	tmp |= PANEL_ICKSEL(pdata->panel_icksel);
	vdc5fb_write(priv, SYSCNT_PANEL_CLK, tmp);

	return 0;
}

static int vdc5fb_init_sync(struct vdc5fb_priv *priv)
{
	struct fb_videomode *mode = priv->videomode;
	u32 tmp;

	/* (TODO) Freq. vsync masking and missing vsync
	 * compensation are not supported.
	 */
	vdc5fb_write(priv, SC0_SCL0_FRC1, 0);
	vdc5fb_write(priv, SC0_SCL0_FRC2, 0);
	vdc5fb_write(priv, SC1_SCL0_FRC1, 0);
	vdc5fb_write(priv, SC1_SCL0_FRC2, 0);
	vdc5fb_write(priv, OIR_SCL0_FRC1, 0);
	vdc5fb_write(priv, OIR_SCL0_FRC2, 0);

	/* Set the same free-running hsync/vsync period to
	 * all scalers (sc0, sc1 and oir). The hsync/vsync
	 * from scaler 0 is used by all scalers.
	 * (TODO) External input vsync is not supported.
	 */
	tmp = SC_RES_FH(priv->res_fh);
	tmp |= SC_RES_FV(priv->res_fv);
	vdc5fb_write(priv, SC0_SCL0_FRC4, tmp);
	vdc5fb_write(priv, SC1_SCL0_FRC4, tmp);
	vdc5fb_write(priv, OIR_SCL0_FRC4, tmp);

	tmp = (SC_RES_FLD_DLY_SEL | SC_RES_VSDLY(1));
	vdc5fb_write(priv, SC0_SCL0_FRC5, tmp);
	vdc5fb_write(priv, SC1_SCL0_FRC5, tmp);
	tmp = SC_RES_VSDLY(1);
	vdc5fb_write(priv, OIR_SCL0_FRC5, tmp);

	vdc5fb_write(priv, SC0_SCL0_FRC3, SC_RES_VS_SEL);
	vdc5fb_write(priv, SC1_SCL0_FRC3, (SC_RES_VS_SEL | SC_RES_VS_IN_SEL));
	vdc5fb_write(priv, OIR_SCL0_FRC3, 0);
	/* Note that OIR is not enabled here */

	/* Set full-screen size */
	tmp = SC_RES_F_VW(mode->yres);
	tmp |= SC_RES_F_VS(mode->vsync_len + mode->upper_margin);
	vdc5fb_write(priv, SC0_SCL0_FRC6, tmp);
	vdc5fb_write(priv, SC1_SCL0_FRC6, tmp);
	vdc5fb_write(priv, OIR_SCL0_FRC6, tmp);
	tmp = SC_RES_F_HW(mode->xres);
	tmp |= SC_RES_F_HS(mode->hsync_len + mode->left_margin);
	vdc5fb_write(priv, SC0_SCL0_FRC7, tmp);
	vdc5fb_write(priv, SC1_SCL0_FRC7, tmp);
	vdc5fb_write(priv, OIR_SCL0_FRC7, tmp);

	/* Cascade on */
	vdc5fb_setbits(priv, GR1_AB1, GR1_CUS_CON_ON);
	/* Set GR0 as current, GR1 as underlaying */
	vdc5fb_setbits(priv, GR_VIN_AB1, GR_VIN_SCL_UND_SEL);

	/* Do update here. */
	tmp = (SC_SCL_UPDATE | SC_SCL_VEN_B);
	vdc5fb_update_regs(priv, SC0_SCL0_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, SC1_SCL0_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, OIR_SCL0_UPDATE, tmp, 1);
	tmp = (GR_UPDATE | GR_P_VEN);
	vdc5fb_update_regs(priv, GR1_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, GR_VIN_UPDATE, tmp, 1);

	return 0;
}

static int vdc5fb_init_scalers(struct vdc5fb_priv *priv)
{
	struct fb_videomode *mode = priv->videomode;
	u32 tmp;

	/* Disable scaler 0 */
	vdc5fb_write(priv, SC0_SCL0_DS1, 0);
	vdc5fb_write(priv, SC0_SCL0_US1, 0);
	vdc5fb_write(priv, SC0_SCL0_OVR1, D_SC_RES_BK_COL);

	/* Disable scaler 1 */
	vdc5fb_write(priv, SC1_SCL0_DS1, 0);
	vdc5fb_write(priv, SC1_SCL0_US1, 0);
	vdc5fb_write(priv, SC1_SCL0_OVR1, D_SC_RES_BK_COL);

	/* Enable and setup OIR scaler */
	vdc5fb_write(priv, OIR_SCL0_FRC3, OIR_RES_EN);
	vdc5fb_update_regs(priv, OIR_SCL0_UPDATE, SC_SCL_UPDATE, 1);

	vdc5fb_write(priv, OIR_SCL0_DS1, 0);
	vdc5fb_write(priv, OIR_SCL0_US1, 0);
	vdc5fb_write(priv, OIR_SCL0_OVR1, D_SC_RES_BK_COL);

	tmp = (mode->vsync_len + mode->upper_margin - 1) << 16;
	tmp |= mode->yres;
	vdc5fb_write(priv, OIR_SCL0_DS2, tmp);
	vdc5fb_write(priv, OIR_SCL0_US2, tmp);

	tmp = (mode->hsync_len + mode->left_margin) << 16;
	tmp |= mode->xres;
	vdc5fb_write(priv, OIR_SCL0_DS3, tmp);
	vdc5fb_write(priv, OIR_SCL0_US3, tmp);

	tmp = mode->yres << 16;
	tmp |= mode->xres;
	vdc5fb_write(priv, OIR_SCL0_DS7, tmp);

	tmp = SC_RES_IBUS_SYNC_SEL;
	vdc5fb_write(priv, OIR_SCL0_US8, tmp);

	return 0;
}

static int vdc5fb_init_graphics(struct vdc5fb_priv *priv)
{
	struct fb_videomode *mode = priv->videomode;
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	u32 tmp;

	/* Graphics 0 (Scaler 0) */
	vdc5fb_write(priv, GR0_FLM_RD, 0);
	tmp = vdc5fb_read(priv, GR0_AB1);
	tmp &= GR_AB1_MASK;
	tmp |= GR_DISP_SEL(0);		/* background */
	vdc5fb_write(priv, GR0_AB1, tmp);
	vdc5fb_write(priv, GR0_BASE, D_GR_BASE);

	/* Graphics 1 (Scaler 1) */
	vdc5fb_write(priv, GR1_FLM_RD, 0);
	tmp = vdc5fb_read(priv, GR1_AB1);
	tmp &= GR_AB1_MASK;
	tmp |= GR_DISP_SEL(0);		/* background */
	vdc5fb_write(priv, GR1_AB1, tmp);
	vdc5fb_write(priv, GR1_BASE, D_GR_BASE);

	/* Graphics 2 (Image Synthsizer) */
	vdc5fb_write(priv, GR2_FLM_RD, 0);
	vdc5fb_write(priv, GR2_AB1, GR_DISP_SEL(0));
	vdc5fb_write(priv, GR3_BASE, D_GR_BASE);

	/* Graphics 3 (Image Synthsizer) */
	vdc5fb_write(priv, GR3_FLM_RD, 0);
	vdc5fb_write(priv, GR3_AB1, GR_DISP_SEL(0));
	vdc5fb_write(priv, GR2_BASE, D_GR_BASE);

	/* Graphics VIN (Image Synthsizer) */
	tmp = vdc5fb_read(priv, GR_VIN_AB1);
	tmp &= GR_AB1_MASK;
	tmp |= GR_DISP_SEL(0);		/* background */
	vdc5fb_write(priv, GR_VIN_AB1, tmp);
	vdc5fb_write(priv, GR_VIN_BASE, D_GR_BASE);

	/* Graphics OIR */
	vdc5fb_write(priv, GR_OIR_FLM_RD, GR_R_ENB);
	vdc5fb_write(priv, GR_OIR_FLM1, GR_FLM_SEL(1));
	vdc5fb_write(priv, GR_OIR_FLM2, priv->dma_handle);
	tmp = GR_FLM_NUM(priv->flm_num);
	tmp |= GR_LN_OFF(mode->xres * (pdata->bpp / 8));
	vdc5fb_write(priv, GR_OIR_FLM3, tmp);
	tmp = GR_FLM_OFF(priv->flm_off);
	vdc5fb_write(priv, GR_OIR_FLM4, tmp);
	tmp = GR_FLM_LOOP(mode->yres - 1);
	tmp |= GR_FLM_LNUM(mode->yres - 1);
	vdc5fb_write(priv, GR_OIR_FLM5, tmp);
	if (pdata->bpp == 16)
		tmp = D_GR_FLM6_RGB565;		/* RGB565 LE, 78563412 */
	else
		tmp = D_GR_FLM6_ARGB8888;	/* ARGB8888 LE, 56781234 */
	tmp |= GR_HW(mode->xres - 1);
	vdc5fb_write(priv, GR_OIR_FLM6, tmp);

	tmp = vdc5fb_read(priv, GR_OIR_AB1);
	tmp &= GR_AB1_MASK;
	tmp |= GR_DISP_SEL(2);		/* current graphics */
	vdc5fb_write(priv, GR_OIR_AB1, tmp);

	tmp = GR_GRC_VW(mode->yres);
	tmp |= GR_GRC_VS(mode->vsync_len + mode->upper_margin);
	vdc5fb_write(priv, GR_OIR_AB2, tmp);

	tmp = GR_GRC_HW(mode->xres);
	tmp |= GR_GRC_HS(mode->hsync_len + mode->left_margin);
	vdc5fb_write(priv, GR_OIR_AB3, tmp);

	vdc5fb_write(priv, GR_OIR_AB7, 0);
	vdc5fb_write(priv, GR_OIR_AB8, D_GR_AB8);
	vdc5fb_write(priv, GR_OIR_AB9, D_GR_AB9);
	vdc5fb_write(priv, GR_OIR_AB10, D_GR_AB10);
	vdc5fb_write(priv, GR_OIR_AB11, D_GR_AB11);

	vdc5fb_write(priv, GR_OIR_BASE, D_GR_BASE);

	return 0;
}

static int vdc5fb_init_outcnt(struct vdc5fb_priv *priv)
{
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	u32 tmp;

	vdc5fb_write(priv, OUT_CLK_PHASE, D_OUT_CLK_PHASE);
	vdc5fb_write(priv, OUT_BRIGHT1, PBRT_G(512));
	vdc5fb_write(priv, OUT_BRIGHT2, (PBRT_B(512) | PBRT_R(512)));
	tmp = (CONT_G(128) | CONT_B(128) | CONT_R(128));
	vdc5fb_write(priv, OUT_CONTRAST, tmp);

	vdc5fb_write(priv, GAM_SW, 0);

	tmp = D_OUT_PDTHA;
	tmp |= PDTHA_FORMAT(pdata->out_format);
					/* 0=RGB888,1=RGB666,2=RGB565 */
	vdc5fb_write(priv, OUT_PDTHA, tmp);

	tmp = D_OUT_SET;
	tmp |= OUT_FORMAT(pdata->out_format);
					/* 0=RGB888,1=RGB666,2=RGB565 */
	vdc5fb_write(priv, OUT_SET, tmp);

	return 0;
}

static int vdc5fb_init_tcon(struct vdc5fb_priv *priv)
{
	static const unsigned char tcon_sel[LCD_MAX_TCON]
		= { 0, 1, 2, 7, 4, 5, 6, };
	struct fb_videomode *mode = priv->videomode;
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	u32 vs_s, vs_w, ve_s, ve_w;
	u32 hs_s, hs_w, he_s, he_w;
	u32 tmp1, tmp2;

	tmp1 = TCON_OFFSET(0);
	tmp1 |= TCON_HALF(priv->res_fh / 2);
	vdc5fb_write(priv, TCON_TIM, tmp1);
	tmp2 = 0;
#if 0
	tmp2 = TCON_DE_INV;
#endif
	vdc5fb_write(priv, TCON_TIM_DE, tmp2);

	vs_s = (2 * 0);
	vs_w = (2 * mode->vsync_len);
	ve_s = (2 * (mode->vsync_len + mode->upper_margin));
	ve_w = (2 * mode->yres);

	tmp1 = TCON_VW(vs_w);
	tmp1 |= TCON_VS(vs_s);
	vdc5fb_write(priv, TCON_TIM_STVA1, tmp1);
	if (pdata->tcon_sel[LCD_TCON0] == TCON_SEL_UNUSED)
		tmp2 = TCON_SEL(tcon_sel[LCD_TCON0]);
	else
		tmp2 = TCON_SEL(pdata->tcon_sel[LCD_TCON0]);
	if (!(mode->sync & FB_SYNC_VERT_HIGH_ACT))
		tmp2 |= TCON_INV;
	vdc5fb_write(priv, TCON_TIM_STVA2, tmp2);

	tmp1 = TCON_VW(ve_w);
	tmp1 |= TCON_VS(ve_s);
	vdc5fb_write(priv, TCON_TIM_STVB1, tmp1);
	if (pdata->tcon_sel[LCD_TCON1] == TCON_SEL_UNUSED)
		tmp2 = TCON_SEL(tcon_sel[LCD_TCON1]);
	else
		tmp2 = TCON_SEL(pdata->tcon_sel[LCD_TCON1]);
#if 0
	tmp2 |= TCON_INV;
#endif
	vdc5fb_write(priv, TCON_TIM_STVB2, tmp2);

	hs_s = 0;
	hs_w = mode->hsync_len;
	he_s = (mode->hsync_len + mode->left_margin);
	he_w = mode->xres;

	tmp1 = TCON_HW(hs_w);
	tmp1 |= TCON_HS(hs_s);
	vdc5fb_write(priv, TCON_TIM_STH1, tmp1);
	if (pdata->tcon_sel[LCD_TCON2] == TCON_SEL_UNUSED)
		tmp2 = TCON_SEL(tcon_sel[LCD_TCON2]);
	else
		tmp2 = TCON_SEL(pdata->tcon_sel[LCD_TCON2]);
	if (!(mode->sync & FB_SYNC_HOR_HIGH_ACT))
		tmp2 |= TCON_INV;
#if 0
	tmp2 |= TCON_HS_SEL;
#endif
	vdc5fb_write(priv, TCON_TIM_STH2, tmp2);

	tmp1 = TCON_HW(he_w);
	tmp1 |= TCON_HS(he_s);
	vdc5fb_write(priv, TCON_TIM_STB1, tmp1);
	if (pdata->tcon_sel[LCD_TCON3] == TCON_SEL_UNUSED)
		tmp2 = TCON_SEL(tcon_sel[LCD_TCON3]);
	else
		tmp2 = TCON_SEL(pdata->tcon_sel[LCD_TCON3]);
#if 0
	tmp2 |= TCON_INV;
	tmp2 |= TCON_HS_SEL;
#endif
	vdc5fb_write(priv, TCON_TIM_STB2, tmp2);

	tmp1 = TCON_HW(hs_w);
	tmp1 |= TCON_HS(hs_s);
	vdc5fb_write(priv, TCON_TIM_CPV1, tmp1);
	if (pdata->tcon_sel[LCD_TCON4] == TCON_SEL_UNUSED)
		tmp2 = TCON_SEL(tcon_sel[LCD_TCON4]);
	else
		tmp2 = TCON_SEL(pdata->tcon_sel[LCD_TCON4]);
#if 0
	tmp2 |= TCON_INV;
	tmp2 |= TCON_HS_SEL;
#endif
	vdc5fb_write(priv, TCON_TIM_CPV2, tmp2);

	tmp1 = TCON_HW(he_w);
	tmp1 |= TCON_HS(he_s);
	vdc5fb_write(priv, TCON_TIM_POLA1, tmp1);
	if (pdata->tcon_sel[LCD_TCON5] == TCON_SEL_UNUSED)
		tmp2 = TCON_SEL(tcon_sel[LCD_TCON5]);
	else
		tmp2 = TCON_SEL(pdata->tcon_sel[LCD_TCON5]);
#if 0
	tmp2 |= TCON_HS_SEL;
	tmp2 |= TCON_INV;
	tmp2 |= TCON_MD;
#endif
	vdc5fb_write(priv, TCON_TIM_POLA2, tmp2);

	tmp1 = TCON_HW(he_w);
	tmp1 |= TCON_HS(he_s);
	vdc5fb_write(priv, TCON_TIM_POLB1, tmp1);
	if (pdata->tcon_sel[LCD_TCON6] == TCON_SEL_UNUSED)
		tmp2 = TCON_SEL(tcon_sel[LCD_TCON6]);
	else
		tmp2 = TCON_SEL(pdata->tcon_sel[LCD_TCON6]);
#if 0
	tmp2 |= TCON_INV;
	tmp2 |= TCON_HS_SEL;
	tmp2 |= TCON_MD;
#endif
	vdc5fb_write(priv, TCON_TIM_POLB2, tmp2);

	return 0;
}

static int vdc5fb_update_all(struct vdc5fb_priv *priv)
{
	u32 tmp;

	tmp = IMGCNT_VEN;
	vdc5fb_update_regs(priv, IMGCNT_UPDATE, tmp, 1);

	tmp = (SC_SCL_VEN_A | SC_SCL_VEN_B | SC_SCL_UPDATE
		| SC_SCL_VEN_C | SC_SCL_VEN_D);
	vdc5fb_update_regs(priv, SC0_SCL0_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, SC0_SCL1_UPDATE, tmp, 1);

	tmp = (GR_IBUS_VEN | GR_P_VEN | GR_UPDATE);
	vdc5fb_update_regs(priv, GR0_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, GR1_UPDATE, tmp, 1);

	tmp = ADJ_VEN;
	vdc5fb_write(priv, ADJ0_UPDATE, tmp);
	vdc5fb_write(priv, ADJ1_UPDATE, tmp);

	tmp = (GR_IBUS_VEN | GR_P_VEN | GR_UPDATE);
	vdc5fb_update_regs(priv, GR2_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, GR3_UPDATE, tmp, 1);

	tmp = (GR_P_VEN | GR_UPDATE);
	vdc5fb_update_regs(priv, GR_VIN_UPDATE, tmp, 1);

	tmp = (SC_SCL_VEN_A | SC_SCL_VEN_B | SC_SCL_UPDATE
		| SC_SCL_VEN_C | SC_SCL_VEN_D);
	vdc5fb_update_regs(priv, OIR_SCL0_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, OIR_SCL1_UPDATE, tmp, 1);

	tmp = (GR_IBUS_VEN | GR_P_VEN | GR_UPDATE);
	vdc5fb_update_regs(priv, GR_OIR_UPDATE, tmp, 1);

	tmp = OUTCNT_VEN;
	vdc5fb_update_regs(priv, OUT_UPDATE, tmp, 1);
	tmp = GAM_VEN;
	vdc5fb_update_regs(priv, GAM_G_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, GAM_B_UPDATE, tmp, 1);
	vdc5fb_update_regs(priv, GAM_R_UPDATE, tmp, 1);
	tmp = TCON_VEN;
	vdc5fb_update_regs(priv, TCON_UPDATE, tmp, 1);

	return 0;
}

static void vdc5fb_set_videomode(struct vdc5fb_priv *priv,
	struct fb_videomode *new)
{
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	struct fb_videomode *mode = pdata->videomode;
	u32 tmp;

	if (new)
		mode = new;
	priv->videomode = mode;

	if (priv->info->screen_base)	/* sanity check */
		vdc5fb_clear_fb(priv);

	if (vdc5fb_set_panel_clock(priv, mode) < 0)
		dev_err(&priv->pdev->dev, "cannot get dcdr\n");

	dev_info(&priv->pdev->dev,
		"%s: [%s] dotclock %lu.%03u MHz, dcdr %u\n",
		priv->dev_name, pdata->name,
		(priv->dc / 1000000),
		(unsigned int)((priv->dc % 1000000) / 1000),
		priv->dcdr);

	priv->res_fh = mode->hsync_len + mode->left_margin + mode->xres
		+ mode->right_margin;
	priv->res_fv = mode->vsync_len + mode->upper_margin + mode->yres
		+ mode->lower_margin;
	priv->rr = (priv->dc / (priv->res_fh * priv->res_fv));

	tmp =  mode->xres * mode->yres * (pdata->bpp / 8);
	priv->flm_off = tmp & ~0xfff;	/* page align */
	if (tmp & 0xfff)
		priv->flm_off += 0x1000;
	priv->flm_num = 0;

	vdc5fb_init_syscnt(priv);
	vdc5fb_init_sync(priv);
	vdc5fb_init_scalers(priv);
	vdc5fb_init_graphics(priv);
	vdc5fb_init_outcnt(priv);
	vdc5fb_init_tcon(priv);

	vdc5fb_update_all(priv);

	vdc5fb_clear_fb(priv);
}

/************************************************************************/

static int vdc5fb_put_bright(struct vdc5fb_priv *priv,
	struct fbio_bright *param)
{
	uint32_t tmp;

	tmp = PBRT_G(param->pbrt_g);
	vdc5fb_write(priv, OUT_BRIGHT1, tmp);
	tmp = PBRT_B(param->pbrt_b);
	tmp |= PBRT_R(param->pbrt_r);
	vdc5fb_write(priv, OUT_BRIGHT2, tmp);
	vdc5fb_update_regs(priv, OUT_UPDATE, OUTCNT_VEN, 1);

	return 0;
}

static int vdc5fb_get_bright(struct vdc5fb_priv *priv,
	struct fbio_bright *param)
{
	uint32_t tmp;

	tmp = vdc5fb_read(priv, OUT_BRIGHT1);
	param->pbrt_g = (tmp & 0x3ffu);
	tmp = vdc5fb_read(priv, OUT_BRIGHT2);
	param->pbrt_b = ((tmp >> 16) & 0x3ffu);
	param->pbrt_r = (tmp & 0x3ffu);

	return 0;
}

static int vdc5fb_put_contrast(struct vdc5fb_priv *priv,
	struct fbio_contrast *param)
{
	uint32_t tmp;

	tmp = CONT_G(param->cont_g);
	tmp |= CONT_B(param->cont_b);
	tmp |= CONT_R(param->cont_r);
	vdc5fb_write(priv, OUT_CONTRAST, tmp);
	vdc5fb_update_regs(priv, OUT_UPDATE, OUTCNT_VEN, 1);

	return 0;
}

static int vdc5fb_get_contrast(struct vdc5fb_priv *priv,
	struct fbio_contrast *param)
{
	uint32_t tmp;

	tmp = vdc5fb_read(priv, OUT_CONTRAST);
	param->cont_g = ((tmp >> 16) & 0xffu);
	param->cont_b = ((tmp >> 8) & 0xffu);
	param->cont_r = (tmp & 0xffu);

	return 0;
}

static int vdc5fb_put_frame(struct vdc5fb_priv *priv,
	struct fbio_frame *param)
{
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	uint32_t tmp;

	if (param->fr_num >= pdata->flm_max)
		return -EINVAL;

	tmp = vdc5fb_read(priv, GR_OIR_FLM3);
	tmp &= ~0x3ffu;
	tmp |= GR_FLM_NUM(param->fr_num);
	vdc5fb_write(priv, GR_OIR_FLM3, tmp);
	vdc5fb_update_regs(priv, GR_OIR_UPDATE, GR_IBUS_VEN, 1);

	return 0;
}

static int vdc5fb_get_frame(struct vdc5fb_priv *priv,
	struct fbio_frame *param)
{
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	uint32_t tmp;

	tmp = vdc5fb_read(priv, GR_OIR_FLM3);
	param->fr_max = pdata->flm_max;
	param->fr_num = (tmp & 0x3ffu);

	return 0;
}

/************************************************************************/

static int vdc5fb_setcolreg(u_int regno,
	u_int red, u_int green, u_int blue,
	u_int transp, struct fb_info *info)
{
	u32 *palette = info->pseudo_palette;

	if (regno >= PALETTE_NR)
		return -EINVAL;

	/* only FB_VISUAL_TRUECOLOR supported */
	red    >>= 16 - info->var.red.length;
	green  >>= 16 - info->var.green.length;
	blue   >>= 16 - info->var.blue.length;
	transp >>= 16 - info->var.transp.length;

	palette[regno] = (red << info->var.red.offset) |
		(green << info->var.green.offset) |
		(blue << info->var.blue.offset) |
		(transp << info->var.transp.offset);

	return 0;
}

static int vdc5fb_ioctl(struct fb_info *info, unsigned int cmd,
	unsigned long arg)
{
	struct vdc5fb_priv *priv = (struct vdc5fb_priv *)info->par;

	switch (cmd) {

	case FBIOGET_VSCREENINFO:	/* 0x00 */
	case FBIOPUT_VSCREENINFO:	/* 0x01 */
	case FBIOGET_FSCREENINFO:	/* 0x02 */
	/* Done by higher, OK */
		break;

	case FBIOGETCMAP:		/* 0x04 */
	case FBIOPUTCMAP:		/* 0x05 */
	case FBIOPAN_DISPLAY:		/* 0x06 */
	case FBIO_CURSOR:		/* 0x08 */
	case FBIOGET_CON2FBMAP:		/* 0x0F */
	case FBIOPUT_CON2FBMAP:		/* 0x10 */
	case FBIOBLANK:			/* 0x11 */
	/* Done by higher, NG */
		break;

	case FBIOGET_VBLANK:		/* 0x12 */
	case FBIO_ALLOC:		/* 0x13 */
	case FBIO_FREE:			/* 0x14 */
	case FBIOGET_GLYPH:		/* 0x15 */
	case FBIOGET_HWCINFO:		/* 0x16 */
	case FBIOPUT_MODEINFO:		/* 0x17 */
	case FBIOGET_DISPINFO:		/* 0x18 */
	case FBIO_WAITFORVSYNC:		/* 0x20 */
	/* Done by higher, NG (not supported) */
	/* vdc5fb_ioctl is also called */
		return -EINVAL;
		break;

	default:
	/* 0x03, 0x07, 0x09-0x0E, 0x19-0x1F, 0x21- (unknown) */
	/* vdc5fb_ioctl is called */
		return -EINVAL;
		break;

	case FBIOPUT_BRIGHT:
		{
			struct fbio_bright bright;

			if (copy_from_user(&bright, (void __user *)arg,
				sizeof(bright)))
				return -EFAULT;
			if (bright.pbrt_r > 1023)
				bright.pbrt_r = 1023;
			if (bright.pbrt_g > 1023)
				bright.pbrt_g = 1023;
			if (bright.pbrt_b > 1023)
				bright.pbrt_b = 1023;
			return vdc5fb_put_bright(priv, &bright);
		}
	case FBIOGET_BRIGHT:
		{
			struct fbio_bright bright;
			int ret;

			ret = vdc5fb_get_bright(priv, &bright);
			if (ret < 0)
				return ret;
			if (copy_to_user((void __user *)arg, &bright,
				sizeof(bright)))
				return -EFAULT;
			return 0;
		}

	case FBIOPUT_CONTRAST:
		{
			struct fbio_contrast contrast;

			if (copy_from_user(&contrast, (void __user *)arg,
				sizeof(contrast)))
				return -EFAULT;
			if (contrast.cont_r > 255)
				contrast.cont_r = 255;
			if (contrast.cont_g > 255)
				contrast.cont_g = 255;
			if (contrast.cont_b > 255)
				contrast.cont_b = 255;
			return vdc5fb_put_contrast(priv, &contrast);
		}
	case FBIOGET_CONTRAST:
		{
			struct fbio_contrast contrast;
			int ret;

			ret = vdc5fb_get_contrast(priv, &contrast);
			if (ret < 0)
				return ret;
			if (copy_to_user((void __user *)arg, &contrast,
				sizeof(contrast)))
				return -EFAULT;
			return 0;
		}
	case FBIOPUT_FRAME:
		{
			struct fbio_frame frame;

			if (copy_from_user(&frame, (void __user *)arg,
				sizeof(frame)))
				return -EFAULT;
			return vdc5fb_put_frame(priv, &frame);
		}
	case FBIOGET_FRAME:
		{
			struct fbio_frame frame;
			int ret;

			ret = vdc5fb_get_frame(priv, &frame);
			if (ret < 0)
				return ret;
			if (copy_to_user((void __user *)arg, &frame,
				sizeof(frame)))
				return -EFAULT;
			return 0;
		}
	}

	return 0;
}

static struct fb_fix_screeninfo vdc5fb_fix = {
	.id		= "vdc5fb",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
	.accel		= FB_ACCEL_NONE,
};

static int vdc5fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	/* NOT SUPPORTED IN THIS VERSION */

	return -EINVAL;
}

static int vdc5fb_set_par(struct fb_info *info)
{
	/* NOT SUPPORTED IN THIS VERSION */

	return 0;
}

static struct fb_ops vdc5fb_ops = {
	.owner          = THIS_MODULE,
	.fb_read        = fb_sys_read,
	.fb_write       = fb_sys_write,
	.fb_check_var	= vdc5fb_check_var,
	.fb_set_par	= vdc5fb_set_par,
	.fb_setcolreg	= vdc5fb_setcolreg,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
	.fb_ioctl	= vdc5fb_ioctl,
};

static int vdc5fb_set_bpp(struct fb_var_screeninfo *var, int bpp)
{
	switch (bpp) {
	case 16: /* RGB 565 */
		var->blue.offset = 0;
		var->blue.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->red.offset = 11;
		var->red.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32: /* ARGB 8888 */
		var->blue.offset = 0;
		var->blue.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->red.offset = 16;
		var->red.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	default:
		return -EINVAL;
	}
	var->bits_per_pixel = bpp;
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;
	return 0;
}

static int vdc5fb_start(struct vdc5fb_priv *priv)
{
	struct vdc5fb_pdata *pdata = priv_to_pdata(priv);
	int error;

	if (pdata->pinmux) {
		if (pdata->pinmux(priv->pdev) < 0) {
			dev_err(&priv->pdev->dev, "cannot setup pinmux\n");
			return -EIO;
		}
	}
	if (pdata->reset) {
		if (pdata->reset(priv->pdev) < 0) {
			dev_err(&priv->pdev->dev, "cannot setup LCD\n");
			return -EIO;
		}
	}

	error = clk_enable(priv->clk);
	if (error < 0)
		return error;

	if (priv->dot_clk) {
		error = clk_enable(priv->dot_clk);
		if (error < 0)
			return error;
	}

	vdc5fb_set_videomode(priv, NULL);

	return error;
}

static void vdc5fb_stop(struct vdc5fb_priv *priv)
{
	if (priv->dot_clk)
		clk_disable(priv->dot_clk);
	clk_disable(priv->clk);
}

static int vdc5fb_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	vdc5fb_stop(platform_get_drvdata(pdev));
	return 0;
}

static int vdc5fb_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return vdc5fb_start(platform_get_drvdata(pdev));
}

static const struct dev_pm_ops vdc5fb_dev_pm_ops = {
	.suspend = vdc5fb_suspend,
	.resume = vdc5fb_resume,
};

static int vdc5fb_probe(struct platform_device *pdev)
{
	int error = -EINVAL;
	struct vdc5fb_priv *priv = NULL;
	struct vdc5fb_pdata *pdata;
	struct fb_info *info;
	struct resource *res;
	void *buf;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "cannot allocate private data\n");
		error = -ENOMEM;
		goto err0;
	}
	platform_set_drvdata(pdev, priv);
	priv->pdev = pdev;
	priv->dev_name = dev_name(&pdev->dev);

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "cannot get platform data\n");
		goto err1;
	}
	priv->pdata = pdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "cannot get resources (reg)\n");
		goto err1;
	}
	priv->base = ioremap_nocache(res->start, resource_size(res));
	if (!priv->base) {
		dev_err(&pdev->dev, "cannot ioremap (reg)\n");
		goto err1;
	}

	error = vdc5fb_init_clocks(priv);
	if (error) {
		dev_err(&pdev->dev, "cannot init clocks\n");
		goto err1;
	}

	error = vdc5fb_init_irqs(priv);
	if (error < 0) {
		dev_err(&pdev->dev, "cannot init irqs\n");
		goto err1;
	}

	info = framebuffer_alloc(0, &pdev->dev);
	if (!info) {
		dev_err(&pdev->dev, "cannot allocate fb_info\n");
		goto err1;
	}
	priv->info = info;

	info->fbops = &vdc5fb_ops;

	info->var.xres = info->var.xres_virtual = pdata->videomode->xres;
	info->var.yres = info->var.yres_virtual = pdata->videomode->yres;
	info->var.width = pdata->panel_width;
	info->var.height = pdata->panel_height;
	info->var.activate = FB_ACTIVATE_NOW;
	info->pseudo_palette = priv->pseudo_palette;
	error = vdc5fb_set_bpp(&info->var, pdata->bpp);
	if (error) {
		dev_err(&pdev->dev, "cannot set bpp\n");
		goto err2;
	}
	info->fix = vdc5fb_fix;
	info->fix.line_length = pdata->videomode->xres * (pdata->bpp / 8);
	info->fix.smem_len = info->fix.line_length * pdata->videomode->yres;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "cannot get resources (fb)\n");
		goto err2;
	}
	if (res->start) {
		if ((res->end - res->start + 1) < info->fix.smem_len)
			goto err2;
		priv->dma_handle = res->start;
		buf = ioremap_nocache(res->start,
			(res->end - res->start + 1));
		priv->fb_nofree = 1;
	} else {
		buf = dma_alloc_writecombine(&pdev->dev, info->fix.smem_len,
			&priv->dma_handle, GFP_KERNEL);
		priv->fb_nofree = 0;
		if (!buf) {
			dev_err(&pdev->dev, "cannot allocate buffer\n");
			goto err2;
		}
	}
	info->flags = FBINFO_FLAG_DEFAULT;

	error = fb_alloc_cmap(&info->cmap, PALETTE_NR, 0);
	if (error < 0) {
		dev_err(&pdev->dev, "cannot allocate cmap\n");
		goto err3;
	}

	info->fix.smem_start = priv->dma_handle;
	info->screen_base = buf;
	info->device = &pdev->dev;
	info->par = priv;

	error = vdc5fb_start(priv);
	if (error) {
		dev_err(&pdev->dev, "cannot start hardware\n");
		goto err4;
	}

	error = register_framebuffer(info);
	if (error < 0)
		goto err5;

	dev_info(info->dev,
		"registered %s as %ux%u @ %u Hz, %d bpp.\n",
		priv->dev_name,
		pdata->videomode->xres,
		pdata->videomode->yres,
		priv->rr,
		pdata->bpp);

	return 0;

err5:
	unregister_framebuffer(priv->info);
err4:
	vdc5fb_stop(priv);
err3:
	fb_dealloc_cmap(&info->cmap);
	if (priv->fb_nofree)
		iounmap(priv->base);
	else
		dma_free_writecombine(&pdev->dev, info->fix.smem_len,
			info->screen_base, info->fix.smem_start);
err2:
	framebuffer_release(info);
	vdc5fb_deinit_irqs(priv);
	vdc5fb_deinit_clocks(priv);
err1:
	kfree(priv);
err0:
	return error;
}

static int vdc5fb_remove(struct platform_device *pdev)
{
	struct vdc5fb_priv *priv = platform_get_drvdata(pdev);
	struct fb_info *info;

	if (priv->info->dev)
		unregister_framebuffer(priv->info);

	vdc5fb_stop(priv);

	info = priv->info;

	fb_dealloc_cmap(&info->cmap);
	if (priv->fb_nofree)
		iounmap(priv->base);
	else
		dma_free_writecombine(&pdev->dev, info->fix.smem_len,
			info->screen_base, info->fix.smem_start);

	framebuffer_release(info);
	vdc5fb_deinit_irqs(priv);
	vdc5fb_deinit_clocks(priv);

	kfree(priv);

	return 0;
}

static struct platform_driver vdc5fb_driver = {
	.driver		= {
		.name		= "vdc5fb",
		.owner		= THIS_MODULE,
		.pm		= &vdc5fb_dev_pm_ops,
	},
	.probe		= vdc5fb_probe,
	.remove		= vdc5fb_remove,
};

static int __init vdc5fb_init(void)
{
	return platform_driver_register(&vdc5fb_driver);
}

static void __exit vdc5fb_exit(void)
{
	platform_driver_unregister(&vdc5fb_driver);
}

module_init(vdc5fb_init);
module_exit(vdc5fb_exit);

MODULE_DESCRIPTION("Renesas VDC5 Framebuffer driver");
MODULE_AUTHOR("Phil Edworthy <phil.edworthy@renesas.com>");
MODULE_LICENSE("GPL v2");

