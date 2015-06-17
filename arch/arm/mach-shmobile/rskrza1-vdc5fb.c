/*
 * Frame Buffer Device Driver for VDC5
 *
 * Copyright (C) 2013 Renesas Solutions Corp.
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

#if 0
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <mach/common.h>
#include <mach/rza1.h>
#include <video/vdc5fb.h>
#endif

/*************************************************************************/
/* RESOURCES */

/* CHANNEL 0 */
static struct resource vdc5fb_resources_ch0[VDC5FB_NUM_RES] = {
	[0] = {
		.name	= "vdc5fb.0: reg",
		.start	= VDC5FB_REG_BASE(0),
		.end	= (VDC5FB_REG_BASE(0) + VDC5FB_REG_SIZE - 1),
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name	= "vdc5fb.0: fb",
		.start	= 0x60200000,
		.end	= 0x605fffff,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.name	= "vdc5fb.0: irq",
		.start	= VDC5FB_IRQ_BASE(0),
		.end	= (VDC5FB_IRQ_BASE(0) + VDC5FB_IRQ_SIZE - 1),
		.flags	= IORESOURCE_IRQ,
	},
};
#if (VDC5FB_NUM_CH > 1)
/* CHANNEL 1 */
static struct resource vdc5fb_resources_ch1[VDC5FB_NUM_RES] = {
	[0] = {
		.name	= "vdc5fb.1: reg",
		.start	= VDC5FB_REG_BASE(1),
		.end	= (VDC5FB_REG_BASE(1) + VDC5FB_REG_SIZE - 1),
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name	= "vdc5fb.1: fb",
		.start	= 0x60600000,
		.end	= 0x609fffff,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.name	= "vdc5fb.1: irq",
		.start	= VDC5FB_IRQ_BASE(1),
		.end	= (VDC5FB_IRQ_BASE(1) + VDC5FB_IRQ_SIZE - 1),
		.flags	= IORESOURCE_IRQ,
	},
};
#endif

/*************************************************************************/
/* LCD MONITOR */

#define	P1CLK			((13330000 * 30) / 6)
#define	PIXCLOCK(hz, div)	\
	(u32)(1000000000000 / ((double)(hz) / (double)(div)))

/* VIDEOMODE */

#if 0
static struct fb_videomode videomode_wxga = {
	.name		= "WXGA",
	.refresh	= -1,
	.xres		= 1280,
	.yres		= 768,
	.pixclock	= PIXCLOCK(P1CLK, -1),
	.left_margin	= -1,
	.right_margin	= -1,
	.upper_margin	= -1,
	.lower_margin	= -1,
	.hsync_len	= -1,
	.vsync_len	= -1,
	.sync		= -1,
	.vmode		= -1,
	.flag		= -1,
};
#endif
/* Based on VESA TMD v1r11 1024x768@60Hz */
static struct fb_videomode videomode_xga = {
	.name		= "XGA",
	.refresh	= 61,	/* calculated */
	.xres		= 1024,
	.yres		= 768,
	.pixclock	= PIXCLOCK(P1CLK, 1),
	.left_margin	= 32,	/* 160, */
	.right_margin	= 152,	/* 24, */
	.upper_margin	= 29,
	.lower_margin	= 3,
	.hsync_len	= 136,
	.vsync_len	= 6,
	.sync		= 0,
	.vmode		= FB_VMODE_NONINTERLACED,
	.flag		= 0,
};

#if 0
static struct fb_videomode videomode_svga = {
	.name		= "SVGA",
	.refresh	= -1,	/* unsued */
	.xres		= 800,
	.yres		= 600,
	.pixclock	= PIXCLOCK(P1CLK, -1),
	.left_margin	= -1,
	.right_margin	= -1,
	.upper_margin	= -1,
	.lower_margin	= -1,
	.hsync_len	= -1,
	.vsync_len	= -1,
	.sync		= -1,
	.vmode		= -1,
	.flag		= -1,
};
#endif

static int vdc5fb_pinmux_vga(struct platform_device *pdev);

static struct vdc5fb_pdata vdc5fb_pdata_ch0_vga = {
	.name			= "VESA VGA",
	.videomode		= &videomode_xga,
	.panel_icksel		= ICKSEL_P1CLK,
	.bpp			= 16,
	.panel_width		= 0,	/* unused */
	.panel_height		= 0,	/* unused */
	.flm_max		= 1,
	.out_format		= OUT_FORMAT_RGB888,
	.use_lvds		= 0,
	.tcon_sel		= {
		[LCD_TCON0]	= TCON_SEL_STH,		/* HSYNC */
		[LCD_TCON1]	= TCON_SEL_STVA,	/* VSYNC */
		[LCD_TCON2]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON3]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON4]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON5]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON6]	= TCON_SEL_UNUSED,	/* NC */
	},
	.pinmux			= vdc5fb_pinmux_vga,
	.reset			= NULL,
};
static struct vdc5fb_pdata vdc5fb_pdata_ch1_vga = {
	.name			= "VESA VGA",
	.videomode		= &videomode_xga,
	.panel_icksel		= ICKSEL_P1CLK,
	.bpp			= 16,
	.panel_width		= 0,	/* unused */
	.panel_height		= 0,	/* unused */
	.flm_max		= 1,
	.out_format		= OUT_FORMAT_RGB888,
	.use_lvds		= 0,
	.tcon_sel		= {
		[LCD_TCON0]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON1]	= TCON_SEL_STVA,	/* VSYNC */
		[LCD_TCON2]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON3]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON4]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON5]	= TCON_SEL_STH,		/* HSYNC */
		[LCD_TCON6]	= TCON_SEL_UNUSED,	/* NC */
	},
	.pinmux			= vdc5fb_pinmux_vga,
	.reset			= NULL,
};

static int vdc5fb_pinmux_lcd(struct platform_device *pdev, int rgb)
{
	struct vdc5fb_pdata *pdata
	    = (struct vdc5fb_pdata *)pdev->dev.platform_data;

	if (pdev->id == 0) {	/* VDC5 CHANNEL 0 */
		/* LCD0_EXTCLK */
		if (pdata->panel_icksel == ICKSEL_EXTCLK0)
			rza1_pfc_pin_assign(P5_8, ALT1, DIR_PIPC);
		/* LCD0_DATA0 - LCD0_DATA17 */
		rza1_pfc_pin_assign(P11_7, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P11_6, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P11_5, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P11_4, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P11_3, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P11_2, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P11_1, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P11_0, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_15, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_14, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_13, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_12, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_11, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_10, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_9, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_8, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_7, ALT5, DIR_PIPC);
		rza1_pfc_pin_assign(P10_6, ALT5, DIR_PIPC);
		if (rgb == 24) {
			/* LCD0_DATA18 - LCD0_DATA23 */
			rza1_pfc_pin_assign(P10_5, ALT5, DIR_PIPC);
			rza1_pfc_pin_assign(P10_4, ALT5, DIR_PIPC);
			rza1_pfc_pin_assign(P10_3, ALT5, DIR_PIPC);
			rza1_pfc_pin_assign(P10_2, ALT5, DIR_PIPC);
			rza1_pfc_pin_assign(P10_1, ALT5, DIR_PIPC);
			rza1_pfc_pin_assign(P10_0, ALT5, DIR_PIPC);
		}
		/* LCD0_CLK (CLOCK) */
		rza1_pfc_pin_assign(P11_15, ALT5, DIR_PIPC);
		/* LCD0_TCON0 - LCD0_TCON6 */
		if (pdata->tcon_sel[LCD_TCON0] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P11_14, ALT5, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON1] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P11_13, ALT5, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON2] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P11_12, ALT5, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON3] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P11_11, ALT5, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON4] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P11_10, ALT5, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON5] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P11_9, ALT5, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON6] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P11_8, ALT5, DIR_PIPC);
		return 0;
	}
#if (VDC5FB_NUM_CH > 1)
	if (pdev->id == 1) {	/* VDC5 CHANNEL 1 */
		/* LCD1_EXTCLK */
		if (pdata->panel_icksel == ICKSEL_EXTCLK1)
			rza1_pfc_pin_assign(P3_7, ALT4, DIR_PIPC);
		/*LCD1_DATA0 - LCD1_DATA17 */
		rza1_pfc_pin_assign(P5_0, ALT2, DIR_PIPC);
		rza1_pfc_pin_assign(P5_1, ALT2, DIR_PIPC);
		rza1_pfc_pin_assign(P5_2, ALT2, DIR_PIPC);
		rza1_pfc_pin_assign(P5_3, ALT2, DIR_PIPC);
		rza1_pfc_pin_assign(P5_4, ALT2, DIR_PIPC);
		rza1_pfc_pin_assign(P5_5, ALT2, DIR_PIPC);
		rza1_pfc_pin_assign(P5_6, ALT2, DIR_PIPC);
		rza1_pfc_pin_assign(P5_7, ALT2, DIR_PIPC);
		rza1_pfc_pin_assign(P2_8, ALT6, DIR_PIPC);
		rza1_pfc_pin_assign(P2_9, ALT6, DIR_PIPC);
		rza1_pfc_pin_assign(P2_10, ALT6, DIR_PIPC);
		rza1_pfc_pin_assign(P2_11, ALT6, DIR_PIPC);
		rza1_pfc_pin_assign(P2_12, ALT7, DIR_PIPC);
		rza1_pfc_pin_assign(P2_13, ALT7, DIR_PIPC);
		rza1_pfc_pin_assign(P2_14, ALT7, DIR_PIPC);
		rza1_pfc_pin_assign(P2_15, ALT7, DIR_PIPC);
		rza1_pfc_pin_assign(P5_9, ALT7, DIR_PIPC);
		rza1_pfc_pin_assign(P5_10, ALT7, DIR_PIPC);
		if (rgb == 24) {
			/* LCD1_DATA18 - LCD1_DATA23 */
			rza1_pfc_pin_assign(P9_2, ALT1, DIR_PIPC);
			rza1_pfc_pin_assign(P9_3, ALT1, DIR_PIPC);
			rza1_pfc_pin_assign(P9_4, ALT1, DIR_PIPC);
			rza1_pfc_pin_assign(P9_5, ALT1, DIR_PIPC);
			rza1_pfc_pin_assign(P9_6, ALT1, DIR_PIPC);
			rza1_pfc_pin_assign(P9_7, ALT1, DIR_PIPC);
		}
		/* LCD1_CLK (CLOCK) */
		rza1_pfc_pin_assign(P4_12,  ALT2, DIR_PIPC);
		/* LCD0_TCON0 - LCD0_TCON6 */
		if (pdata->tcon_sel[LCD_TCON0] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P4_13,  ALT2, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON1] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P4_14,  ALT2, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON2] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P4_15,  ALT2, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON3] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P4_8,  ALT2, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON4] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P4_9,  ALT2, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON5] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P4_10,  ALT2, DIR_PIPC);
		if (pdata->tcon_sel[LCD_TCON6] != TCON_SEL_UNUSED)
			rza1_pfc_pin_assign(P4_11,  ALT2, DIR_PIPC);
		return 0;
	}
#endif /* VDC5FB_NUM_CH > 1 */
	return -ENODEV;
}

static int vdc5fb_pinmux_vga(struct platform_device *pdev)
{
	return vdc5fb_pinmux_lcd(pdev, 24);
}

/*************************************************************************/
/* LCD-KIT-B01 */

static struct fb_videomode videomode_wvga_lcd_kit_b01 = {
	.name		= "WVGA",
	.refresh	= 60,	/* unsued */
	.xres		= 800,
	.yres		= 480,
	.pixclock	= PIXCLOCK(P1CLK, 2),
	.left_margin	= 0,
	.right_margin	= 64,
	.upper_margin	= 18,
	.lower_margin	= 18,
	.hsync_len	= 128,
	.vsync_len	= 4,
	.sync		= 0,	/* to be fixed */
	.vmode		= 0,	/* to be fixed */
	.flag		= 0,	/* to be fixed */
};

static int vdc5fb_pinmux_lcd_kit_b01(struct platform_device *pdev);
static int vdc5fb_reset_lcd_kit_b01(struct platform_device *pdev);

static struct vdc5fb_pdata vdc5fb_pdata_ch0_lcd_kit_b01 = {
	.name			= "LCD-KIT-B01",
	.videomode		= &videomode_wvga_lcd_kit_b01,
	.panel_icksel		= ICKSEL_P1CLK,
	.bpp			= 32,
	.panel_width		= 184,	/* mm, unused */
	.panel_height		= 132,	/* mm, unused */
	.flm_max		= 1,
	.out_format		= OUT_FORMAT_RGB666,
	.use_lvds		= 0,
	.tcon_sel		= {
		[LCD_TCON0]	= TCON_SEL_UNUSED,	/* RESET */
		[LCD_TCON1]	= TCON_SEL_UNUSED,	/* INT */
		[LCD_TCON2]	= TCON_SEL_DE,		/* DE */
		[LCD_TCON3]	= TCON_SEL_STH,		/* HSYNC(NC) */
		[LCD_TCON4]	= TCON_SEL_STVA,	/* VSYNC(NC) */
		[LCD_TCON5]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON6]	= TCON_SEL_UNUSED,	/* NC */
	},
	.pinmux			= vdc5fb_pinmux_lcd_kit_b01,
	.reset			= vdc5fb_reset_lcd_kit_b01,
};
static struct vdc5fb_pdata vdc5fb_pdata_ch1_lcd_kit_b01 = {
	.name			= "LCD-KIT-B01",
	.videomode		= &videomode_wvga_lcd_kit_b01,
	.panel_icksel		= ICKSEL_P1CLK,
	.bpp			= 32,
	.panel_width		= 184,	/* mm, unused */
	.panel_height		= 132,	/* mm, unused */
	.flm_max		= 1,
	.out_format		= OUT_FORMAT_RGB666,
	.use_lvds		= 0,
	.tcon_sel		= {
		[LCD_TCON0]	= TCON_SEL_STH,		/* HSYNC(NC) */
		[LCD_TCON1]	= TCON_SEL_DE,		/* DE */
		[LCD_TCON2]	= TCON_SEL_STVA,	/* VSYNC(NC) */
		[LCD_TCON3]	= TCON_SEL_UNUSED,	/* INT */
		[LCD_TCON4]	= TCON_SEL_UNUSED,	/* RESET */
		[LCD_TCON5]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON6]	= TCON_SEL_UNUSED,	/* NC */
	},
	.pinmux			= vdc5fb_pinmux_lcd_kit_b01,
	.reset			= vdc5fb_reset_lcd_kit_b01,
};

static int vdc5fb_pinmux_lcd_kit_b01(struct platform_device *pdev)
{
	vdc5fb_pinmux_lcd(pdev, 18);

	if (pdev->id == 0) {
		/* RIIC0SCL and RIIC0SDA should be initialized by I2C */

		/* LCD0_TCON0 (RESET) */
		/* This should be initailized by pdata->reset func, */

		/* LCD0_TCON1 (INT) */
		rza1_pfc_pin_assign(P11_13, PMODE, DIR_IN);
		return 0;
	}
#if (VDC5FB_NUM_CH > 1)
	if (pdev->id == 1) {
		/* RIIC3SCL and RIIC3SDA should be initialized by I2C */

		/* LCD1_TCON4 (RESET) */
		/* This should be initailized by pdata->reset func, */

		/* LCD1_TCON3 (INT) */
		rza1_pfc_pin_assign(P4_8, PMODE, DIR_IN);
		return 0;
	}
#endif /* VDC5FB_NUM_CH > 1 */
	return -ENODEV;
}

static int vdc5fb_reset_lcd_kit_b01(struct platform_device *pdev)
{
	void *PSR;
	u32 tmp;

	if (pdev->id == 0) {
		/* VDC5 CHANNEL 0 (RESET is P11_14) */
		PSR = (void *)(0xFCFE3000 + 0x012C);

		tmp = (1u << (16 + 14)) | (1u << 14);
		iowrite32(tmp, PSR);
		/* LCD0_TCON0 (RESET) */
		rza1_pfc_pin_assign(P11_14, PMODE, DIR_OUT);
		udelay(500);
		tmp = (1u << (16 + 14)) | (0u << 14);
		iowrite32(tmp, PSR);
		udelay(500);
		tmp = (1u << (16 + 14)) | (1u << 14);
		iowrite32(tmp, PSR);
		return 0;
	}
#if (VDC5FB_NUM_CH > 1)
	if (pdev->id == 1) {
		/* VDC5 CHANNEL 1 (RESET is P4_9) */
		PSR = (void *)(0xFCFE3000 + 0x0110);

		tmp = (1u << (16 + 9)) | (1u << 9);
		iowrite32(tmp, PSR);
		/* LCD1_TCON4 (RESET) */
		rza1_pfc_pin_assign(P4_9, PMODE, DIR_OUT);
		udelay(500);
		tmp = (1u << (16 + 9)) | (0u << 9);
		iowrite32(tmp, PSR);
		udelay(500);
		tmp = (1u << (16 + 9)) | (1u << 9);
		iowrite32(tmp, PSR);
		return 0;
	}
#endif /* VDC5FB_NUM_CH > 1 */
	return -ENODEV;
}

/*************************************************************************/
/* PLATFORM DATA */

/* Based on VESA TMD v1r11 800x600@56Hz */
static struct fb_videomode videomode_wvga_r0p7724le0011rl = {
	.name		= "WVGA",
	.refresh	= -1,	/* not fixed */
	.xres		= 800,
	.yres		= 480,
	.pixclock	= PIXCLOCK(P1CLK, 2),
	.left_margin	= 128,
	.right_margin	= 24,
	.upper_margin	= 22,
	.lower_margin	= 1,
	.hsync_len	= 72,
	.vsync_len	= 2,
	.sync		= (FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT),
	.vmode		= FB_VMODE_NONINTERLACED,
	.flag		= 0,
};

static int vdc5fb_pinmux_r0p7724le0011rl(struct platform_device *pdev);

#if (VDC5FB_NUM_CH > 1)
/* CHANNEL 1 */
static struct vdc5fb_pdata vdc5fb_pdata_r0p7724le0011rl = {
	.name			= "R0P7724LE0011RL",
	.videomode		= &videomode_wvga_r0p7724le0011rl,
	.panel_icksel		= ICKSEL_P1CLK,
	.bpp			= 32,
	.panel_width		= 165,	/* mm, unused */
	.panel_height		= 106,	/* mm, unused */
	.flm_max		= 1,
	.out_format		= OUT_FORMAT_RGB666,
	.use_lvds		= 0,
	.tcon_sel		= {
		[LCD_TCON0]	= TCON_SEL_UNUSED,	/* NC */
		[LCD_TCON1]	= TCON_SEL_UNUSED,	/* NC*/
		[LCD_TCON2]	= TCON_SEL_UNUSED,	/* NC*/
		[LCD_TCON3]	= TCON_SEL_STVA,	/* LCDVSYN */
		[LCD_TCON4]	= TCON_SEL_STH,		/* LCDHSYN */
		[LCD_TCON5]	= TCON_SEL_DE,		/* LCDDISP(DE) */
		[LCD_TCON6]	= TCON_SEL_UNUSED,	/* TP_IRQ# */
	},
	.pinmux			= vdc5fb_pinmux_r0p7724le0011rl,
	.reset			= NULL,
};
#endif /* (VDC5FB_NUM_CH > 1) */

static int vdc5fb_pinmux_r0p7724le0011rl(struct platform_device *pdev)
{
	vdc5fb_pinmux_lcd(pdev, 18);

#if (VDC5FB_NUM_CH > 1)
	if (pdev->id == 1) {
		/* RIIC3SCL and RIIC3SDA should be initialized by I2C */

		/* LCD1_TCON6 (TP_IRQ#) */
		rza1_pfc_pin_assign(P4_11, PMODE, DIR_IN);
		return 0;
	}
#endif /* VDC5FB_NUM_CH > 1 */
	return -ENODEV;
}

/*************************************************************************/
/* PLATFORM DEVICES */

static struct platform_device vdc5fb_devices[VDC5FB_NUM_CH] = {
	[0] = {
		.name		= "vdc5fb",
		.id		= 0,
		.num_resources	= ARRAY_SIZE(vdc5fb_resources_ch0),
		.resource	= vdc5fb_resources_ch0,
		.dev = {
			.dma_mask		= NULL,
			.coherent_dma_mask	= 0xffffffff,
			.platform_data		= &vdc5fb_pdata_ch0_lcd_kit_b01,
		},
	},
#if (VDC5FB_NUM_CH > 1)
	[1] = {
		.name		= "vdc5fb",
		.id		= 1,
		.num_resources	= ARRAY_SIZE(vdc5fb_resources_ch1),
		.resource	= vdc5fb_resources_ch1,
		.dev = {
			.dma_mask		= NULL,
			.coherent_dma_mask	= 0xffffffff,
			.platform_data		= &vdc5fb_pdata_ch1_vga,
		},
	},
#endif /* VDC5FB_NUM_CH > 1 */
};

/*************************************************************************/
/* BOOT OPTIONS */

int disable_ether /* = 0 */;
static int disable_sdhi /* = 0 */;
static unsigned int vdc5fb0_opts = 1;
static unsigned int vdc5fb1_opts /* = 0 */;

int __init early_vdc5fb0(char *str)
{
	get_option(&str, &vdc5fb0_opts);
	return 0;
}
early_param("vdc5fb0", early_vdc5fb0);

int __init early_vdc5fb1(char *str)
{
	get_option(&str, &vdc5fb1_opts);
	if (vdc5fb1_opts != 0) {
		disable_ether = 1;
		disable_sdhi = 1;
	}
	return 0;
}
early_param("vdc5fb1", early_vdc5fb1);

/*************************************************************************/
/* SETUP */

static struct platform_device *display_devices[] __initdata = {
#if defined(CONFIG_FB_VDC5)
	&vdc5fb_devices[0],
#if (VDC5FB_NUM_CH > 1)
	&vdc5fb_devices[1],
#endif
#endif
};

static int vdc5fb_setup(void)
{
	struct platform_device *pdev;
	int n;

	for (n = 0; n < VDC5FB_NUM_CH; n++) {
		pdev = &vdc5fb_devices[n];

		if (pdev->id == 0) {		/* VDC5 CHANNEL 0 */
			switch (vdc5fb0_opts) {
			case 0:	/* Turn off */
				dev_info(&pdev->dev,
					"vdc5fb.%d: channel 0 is turned off\n",
					pdev->id);
				pdev->name = "vdc5fb.0(hidden)";
				break;
			case 1:	/* LCD_KIT_B01 (default) */
				pdev->dev.platform_data =
					&vdc5fb_pdata_ch0_lcd_kit_b01;
				break;
			case 2:	/* LCD Monitor (VGA) */
				pdev->dev.platform_data =
					&vdc5fb_pdata_ch0_vga;
				break;
			case 3:	/* Add channel 1 first */
				break;
			default:
				break;
			}
		}
#if (VDC5FB_NUM_CH > 1)
		else if (pdev->id == 1) {	/* VDC5 CHANNEL 1 */
			switch (vdc5fb1_opts) {
			case 0:	/* Turn off (default) */
				dev_info(&pdev->dev,
					"vdc5fb.%d: channel 1 is turned off\n",
					pdev->id);
				pdev->name = "vdc5fb.1(hidden)";
				break;
			case 1:	/* LCD-KIT-B01 */
				pdev->dev.platform_data =
					&vdc5fb_pdata_ch1_lcd_kit_b01;
				break;
			case 2:	/* R0P7724LE0011RL */
				pdev->dev.platform_data =
					&vdc5fb_pdata_r0p7724le0011rl;
				break;
			case 3:	/* LCD monitor */
				pdev->dev.platform_data =
					&vdc5fb_pdata_ch1_vga;
				break;
			default:
				break;
			}
		}
#endif
	}
	platform_add_devices(display_devices, ARRAY_SIZE(display_devices));
	return 0;
}

