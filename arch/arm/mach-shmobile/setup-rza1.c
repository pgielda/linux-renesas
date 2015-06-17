/*
 * rskrza1 board support
 *
 * Copyright (C) 2012-2013 Renesas Solutions Corp.
 * Copyright (C) 2012  Renesas Electronics Europe Ltd
 * Copyright (C) 2012  Phil Edworthy
 * Copyright (C) 2012 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 * Copyright (C) 2010  Magnus Damm
 * Copyright (C) 2008  Yoshihiro Shimoda
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
 */

#include <asm/mach/map.h>
#include <asm/mach-types.h>
#include <asm/mach/time.h>
#include <linux/init.h>
#include <linux/i2c/riic.h>
#include <linux/io.h>
#include <linux/platform_data/sh_adc.h>
#include <linux/platform_device.h>
#include <linux/serial.h>
#include <linux/serial_sci.h>
#include <linux/spi/rspi.h>
#include <linux/sh_eth.h>
#include <linux/sh_timer.h>
#include <linux/usb/r8a66597.h>
#include <linux/spi/sh_spibsc.h>
#include <linux/platform_data/dma-rza1.h>
#include <linux/can/platform/rza1_can.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/rza1.h>
#include <sound/sh_scux.h>

static struct map_desc rza1_io_desc[] __initdata = {
	/* create a 1:1 entity map for 0xe8xxxxxx
	 * used by INTC.
	 */
	{
		.virtual	= 0xe8000000,
		.pfn		= __phys_to_pfn(0xe8000000),
		.length		= 256 << 20,
		.type		= MT_DEVICE_NONSHARED
	},
	/* create a 1:1 entity map for 0xfcffxxxx
	 * used by MTU2, RTC.
	 */
	{
		.virtual	= 0xfcff0000,
		.pfn		= __phys_to_pfn(0xfcff0000),
		.length		= 256 << 8,
		.type		= MT_DEVICE_NONSHARED
	},
	/* create a 1:1 entity map for 0xfcfexxxx
	 * used by MSTP, CPG.
	 */
	{
		.virtual	= 0xfcfe0000,
		.pfn		= __phys_to_pfn(0xfcfe0000),
		.length		= (256 << 8) - 1,
		.type		= MT_DEVICE_NONSHARED
	},
};

void __init rza1_map_io(void)
{
	iotable_init(rza1_io_desc, ARRAY_SIZE(rza1_io_desc));
}

#if !defined(CONFIG_MACH_DIMMRZA1H)
static struct plat_sci_port scif0_platform_data = {
	.mapbase	= 0xe8007000,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RIE | SCSCR_TIE | SCSCR_RE | SCSCR_TE |
			  SCSCR_REIE,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		=  { 222, 223, 224, 221 },
	.regtype	= SCIx_SH2_SCIF_FIFODATA_REGTYPE,
};

static struct platform_device scif0_device = {
	.name		= "sh-sci",
	.id		= 0,
	.dev		= {
		.platform_data	= &scif0_platform_data,
	},
};
#endif

static struct plat_sci_port scif1_platform_data = {
	.mapbase	= 0xe8007800,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RIE | SCSCR_TIE | SCSCR_RE | SCSCR_TE |
			  SCSCR_REIE,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		=  { 226, 227, 228, 225 },
	.regtype	= SCIx_SH2_SCIF_FIFODATA_REGTYPE,
};

static struct platform_device scif1_device = {
	.name		= "sh-sci",
	.id		= 1,
	.dev		= {
		.platform_data	= &scif1_platform_data,
	},
};

static struct plat_sci_port scif2_platform_data = {
	.mapbase	= 0xe8008000,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RIE | SCSCR_TIE | SCSCR_RE | SCSCR_TE |
			  SCSCR_REIE,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		=  { 230, 231, 232, 229 },
	.regtype	= SCIx_SH2_SCIF_FIFODATA_REGTYPE,
};

static struct platform_device scif2_device = {
	.name		= "sh-sci",
	.id		= 2,
	.dev		= {
		.platform_data	= &scif2_platform_data,
	},
};

static struct plat_sci_port scif3_platform_data = {
	.mapbase	= 0xe8008800,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RIE | SCSCR_TIE | SCSCR_RE | SCSCR_TE |
			  SCSCR_REIE,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		=  { 234, 235, 236, 233 },
	.regtype	= SCIx_SH2_SCIF_FIFODATA_REGTYPE,
};

static struct platform_device scif3_device = {
	.name		= "sh-sci",
	.id		= 3,
	.dev		= {
		.platform_data	= &scif3_platform_data,
	},
};

#if !defined(CONFIG_MACH_DIMMRZA1H)
static struct plat_sci_port scif4_platform_data = {
	.mapbase	= 0xe8009000,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RIE | SCSCR_TIE | SCSCR_RE | SCSCR_TE |
			  SCSCR_REIE,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		=  { 238, 239, 240, 237 },
	.regtype	= SCIx_SH2_SCIF_FIFODATA_REGTYPE,
};

static struct platform_device scif4_device = {
	.name		= "sh-sci",
	.id		= 4,
	.dev		= {
		.platform_data	= &scif4_platform_data,
	},
};
#endif

static struct plat_sci_port scif5_platform_data = {
	.mapbase	= 0xe8009800,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RIE | SCSCR_TIE | SCSCR_RE | SCSCR_TE |
			  SCSCR_REIE,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		=  { 242, 243, 244, 241 },
	.regtype	= SCIx_SH2_SCIF_FIFODATA_REGTYPE,
};

static struct platform_device scif5_device = {
	.name		= "sh-sci",
	.id		= 5,
	.dev		= {
		.platform_data	= &scif5_platform_data,
	},
};

static struct plat_sci_port scif6_platform_data = {
	.mapbase	= 0xe800a000,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RIE | SCSCR_TIE | SCSCR_RE | SCSCR_TE |
			  SCSCR_REIE,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		=  { 246, 247, 248, 245 },
	.regtype	= SCIx_SH2_SCIF_FIFODATA_REGTYPE,
};

static struct platform_device scif6_device = {
	.name		= "sh-sci",
	.id		= 6,
	.dev		= {
		.platform_data	= &scif6_platform_data,
	},
};

#if !defined(CONFIG_MACH_DIMMRZA1H)
static struct plat_sci_port scif7_platform_data = {
	.mapbase	= 0xe800a800,
	.flags		= UPF_BOOT_AUTOCONF,
	.scscr		= SCSCR_RIE | SCSCR_TIE | SCSCR_RE | SCSCR_TE |
			  SCSCR_REIE,
	.scbrr_algo_id	= SCBRR_ALGO_2,
	.type		= PORT_SCIF,
	.irqs		=  { 250, 251, 252, 249 },
	.regtype	= SCIx_SH2_SCIF_FIFODATA_REGTYPE,
};

static struct platform_device scif7_device = {
	.name		= "sh-sci",
	.id		= 7,
	.dev		= {
		.platform_data	= &scif7_platform_data,
	},
};
#endif

static struct sh_timer_config mtu2_0_platform_data = {
	.channel_offset = -0x80,
	.timer_bit = 0,
	.clockevent_rating = 200,
};

static struct resource mtu2_0_resources[] = {
	[0] = {
		.start	= 0xfcff0300,
		.end	= 0xfcff0326,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 139,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device mtu2_0_device = {
	.name		= "sh_mtu2",
	.id		= 0,
	.dev = {
		.platform_data = &mtu2_0_platform_data,
	},
	.resource	= mtu2_0_resources,
	.num_resources	= ARRAY_SIZE(mtu2_0_resources),
};

/* RTC */
#if !defined(CONFIG_MACH_DIMMRZA1H)
static struct resource rtc_resources[] = {
	[0] = {
		.start	= 0xfcff1000,
		.end	= 0xfcff102d,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		/* Period IRQ */
		.start	= 309,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		/* Carry IRQ */
		.start	= 310,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		/* Alarm IRQ */
		.start	= 308,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device rtc_device = {
	.name		= "sh-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rtc_resources),
	.resource	= rtc_resources,
};
#endif

/* USB Host */
static struct r8a66597_platdata r8a66597_data = {
	.endian = 0,
	.on_chip = 1,
	.xtal = R8A66597_PLATDATA_XTAL_48MHZ,
};

static struct resource r8a66597_usb_host0_resources[] = {
	[0] = {
		.start	= 0xe8010000,
		.end	= 0xe80101a0 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 73,
		.end	= 73,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device r8a66597_usb_host0_device = {
	.name		= "r8a66597_hcd",
	.id		= 0,
	.dev = {
		.dma_mask		= NULL,		/*  not use dma */
		.coherent_dma_mask	= 0xffffffff,
		.platform_data		= &r8a66597_data,
	},
	.num_resources	= ARRAY_SIZE(r8a66597_usb_host0_resources),
	.resource	= r8a66597_usb_host0_resources,
};

static struct resource r8a66597_usb_host1_resources[] = {
	[0] = {
		.start	= 0xe8207000,
		.end	= 0xe82071a0 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 74,
		.end	= 74,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device r8a66597_usb_host1_device = {
	.name		= "r8a66597_hcd",
	.id		= 1,
	.dev = {
		.dma_mask		= NULL,		/*  not use dma */
		.coherent_dma_mask	= 0xffffffff,
		.platform_data		= &r8a66597_data,
	},
	.num_resources	= ARRAY_SIZE(r8a66597_usb_host1_resources),
	.resource	= r8a66597_usb_host1_resources,
};

/* Ether */
static struct sh_eth_plat_data sh_eth_platdata = {
	.phy			= 0x00,
	.edmac_endian		= EDMAC_LITTLE_ENDIAN,
	.register_type		= SH_ETH_REG_GIGABIT,
	.phy_interface		= PHY_INTERFACE_MODE_MII,
};

static struct resource sh_eth_resources[] = {
	[0] = {
		.start	= 0xe8203000,
		.end	= 0xe8204a00 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 359,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device sh_eth_device = {
	.name = "sh-eth",
	.id = 0,
	.dev = {
		.platform_data = &sh_eth_platdata,
	},
	.resource = sh_eth_resources,
	.num_resources = ARRAY_SIZE(sh_eth_resources),
};

/* riic(i2c) */
#if !defined(CONFIG_MACH_DIMMRZA1H)
static struct resource i2c_resources0[] = {
	[0] = {
		.start	= 0xfcfee000,
		.end	= 0xfcfee400 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 189,
		.end	= 189,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= 190,
		.end	= 191,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
	[3] = {
		.start	= 192,
		.end	= 196,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct riic_platform_data pdata0 = {
	/* Transfer Rate 100 or 400 [kbps] */
	.clock			= 100,
};

static struct platform_device i2c_device0 = {
	.name		= "i2c-riic",
	.id		= 0,
	.dev.platform_data = &pdata0,
	.num_resources	= ARRAY_SIZE(i2c_resources0),
	.resource	= i2c_resources0,
};
#endif

static struct resource i2c_resources1[] = {
	[0] = {
		.start	= 0xfcfee400,
		.end	= 0xfcfee800 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 197,
		.end	= 197,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= 198,
		.end	= 199,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
	[3] = {
		.start	= 200,
		.end	= 204,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct riic_platform_data pdata1 = {
	/* Transfer Rate 100 or 400 [kbps] */
	.clock			= 100,
};

static struct platform_device i2c_device1 = {
	.name		= "i2c-riic",
	.id		= 1,
	.dev.platform_data = &pdata1,
	.num_resources	= ARRAY_SIZE(i2c_resources1),
	.resource	= i2c_resources1,
};

#if !defined(CONFIG_MACH_DIMMRZA1H)
static struct resource i2c_resources2[] = {
	[0] = {
		.start	= 0xfcfee800,
		.end	= 0xfcfeec00 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 205,
		.end	= 205,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= 206,
		.end	= 207,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
	[3] = {
		.start	= 208,
		.end	= 212,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct riic_platform_data pdata2 = {
	/* Transfer Rate 100 or 400 [kbps] */
	.clock			= 100,
};

static struct platform_device i2c_device2 = {
	.name		= "i2c-riic",
	.id		= 2,
	.dev.platform_data = &pdata2,
	.num_resources	= ARRAY_SIZE(i2c_resources2),
	.resource	= i2c_resources2,
};
#endif

static struct resource i2c_resources3[] = {
	[0] = {
		.start	= 0xfcfeec00,
		.end	= 0xfcfeefff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 213,
		.end	= 213,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= 214,
		.end	= 215,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
	[3] = {
		.start	= 216,
		.end	= 220,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct riic_platform_data pdata3 = {
	/* Transfer Rate 100 or 400 [kbps] */
	.clock			= 100,
};

static struct platform_device i2c_device3 = {
	.name		= "i2c-riic",
	.id		= 3,
	.dev.platform_data = &pdata3,
	.num_resources	= ARRAY_SIZE(i2c_resources3),
	.resource	= i2c_resources3,
};

/* SPI */
static struct rspi_plat_data rspi_pdata = {
	.data_width = 8,
	.spcr = false,
	.txmode = false,
};

#if !defined(CONFIG_MACH_DIMMRZA1H)
static struct resource spi0_resources[] = {
	[0] = {
		.start	= 0xe800c800,
		.end	= 0xe800c8ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 270,
		.end	= 272,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device spi0_device = {
	.name	= "rspi",
	.id	= 0,
	.dev	= {
		.platform_data		= &rspi_pdata,
	},
	.num_resources	= ARRAY_SIZE(spi0_resources),
	.resource	= spi0_resources,
};

static struct resource spi1_resources[] = {
	[0] = {
		.start	= 0xe800d000,
		.end	= 0xe800d0ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 273,
		.end	= 275,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device spi1_device = {
	.name	= "rspi",
	.id	= 1,
	.dev	= {
		.platform_data		= &rspi_pdata,
	},
	.num_resources	= ARRAY_SIZE(spi1_resources),
	.resource	= spi1_resources,
};

static struct resource spi2_resources[] = {
	[0] = {
		.start	= 0xe800d800,
		.end	= 0xe800d8ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 276,
		.end	= 278,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device spi2_device = {
	.name	= "rspi",
	.id	= 2,
	.dev	= {
		.platform_data		= &rspi_pdata,
	},
	.num_resources	= ARRAY_SIZE(spi2_resources),
	.resource	= spi2_resources,
};

static struct resource spi3_resources[] = {
	[0] = {
		.start	= 0xe800e000,
		.end	= 0xe800e0ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 279,
		.end	= 281,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device spi3_device = {
	.name	= "rspi",
	.id	= 3,
	.dev	= {
		.platform_data		= &rspi_pdata,
	},
	.num_resources	= ARRAY_SIZE(spi3_resources),
	.resource	= spi3_resources,
};
#endif

static struct resource spi4_resources[] = {
	[0] = {
		.start	= 0xe800e800,
		.end	= 0xe800e8ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 282,
		.end	= 284,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device spi4_device = {
	.name	= "rspi",
	.id	= 4,
	.dev	= {
		.platform_data		= &rspi_pdata,
	},
	.num_resources	= ARRAY_SIZE(spi4_resources),
	.resource	= spi4_resources,
};

static struct resource rz_can_resources[] = {
	[0] = {
		.start	= 0xe803a000,
		.end	= 0xe803b813,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 258,
		.end	= 258,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= 260,
		.end	= 260,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= 259,
		.end	= 259,
		.flags	= IORESOURCE_IRQ,
	},
	[4] = {
		.start	= 253,
		.end	= 253,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct rz_can_platform_data rz_can_data = {
#ifdef CONFIG_MACH_DIMMRZA1H
	.channel	= 4,
#else
	.channel	= 1,
#endif
	.clock_select	= CLKR_CLKC,
};

static struct platform_device rz_can_device = {
	.name		= "rz_can",
	.num_resources	= ARRAY_SIZE(rz_can_resources),
	.resource	= rz_can_resources,
	.dev	= {
		.platform_data	= &rz_can_data,

	},
};

/* ADC */
static struct resource adc0_resources[] = {
	[0] = {
		.start	= 0xe8005800,
		.end	= 0xe80058ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = { /* mtu2 share regs */
		.start	= 0xfcff0280,
		.end	= 0xfcff0285,
		.flags	= IORESOURCE_MEM,
	},
	[2] = { /* mtu2 ch.1 regs */
		.start	= 0xfcff0380,
		.end	= 0xfcff03a0,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= 170,
		.end	= 171,
		.flags	= IORESOURCE_IRQ,
	},
	[4] = { /* mtu2 */
		.start	= 146,
		.end	= 146,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_adc_data adc_data = {
	.num_channels = 8,
	.mtu2_ch = 1,
};

static struct platform_device adc0_device = {
	.name	= "sh_adc",
	.id	= 0,
	.dev	= {
		.platform_data		= &adc_data,
		.dma_mask		= NULL,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(adc0_resources),
	.resource	= adc0_resources,
};

/* spibsc */
static struct sh_spibsc_info spibsc0_info = {
	.bus_num	= 5,
};

static struct resource spibsc0_resources[] = {
	[0] = {
		.start	= 0x3fefa000,
		.end	= 0x3fefa0ff,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device spibsc0_device = {
	.name		= "spibsc",
	.id		= 0,
	.dev.platform_data = &spibsc0_info,
	.num_resources	= ARRAY_SIZE(spibsc0_resources),
	.resource	= spibsc0_resources,
};

#if !defined(CONFIG_MACH_DIMMRZA1H)
static struct sh_spibsc_info spibsc1_info = {
	.bus_num	= 6,
};

static struct resource spibsc1_resources[] = {
	[0] = {
		.start	= 0x3fefb000,
		.end	= 0x3fefb0ff,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device spibsc1_device = {
	.name		= "spibsc",
	.id		= 1,
	.dev.platform_data = &spibsc1_info,
	.num_resources	= ARRAY_SIZE(spibsc1_resources),
	.resource	= spibsc1_resources,
};
#endif

/* DMA */
#define CHCFG(reqd_v, loen_v, hien_v, lvl_v, am_v, sds_v, dds_v, tm_v)\
	{								\
		.reqd	=	reqd_v,					\
		.loen	=	loen_v,					\
		.hien	=	hien_v,					\
		.lvl	=	lvl_v,					\
		.am	=	am_v,					\
		.sds	=	sds_v,					\
		.dds	=	dds_v,					\
		.tm	=	tm_v,					\
	}
#define DMARS(rid_v, mid_v)	\
	{								\
		.rid	= rid_v,					\
		.mid	= mid_v,					\
	}

static const struct rza1_dma_slave_config rza1_dma_slaves[] = {
	{
		.slave_id	= RZA1DMA_SLAVE_SDHI0_TX,
		.addr		= 0xe804e030,
		.chcfg		= CHCFG(0x1, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x0),
		.dmars		= DMARS(0x1, 0x30),
	},
	{
		.slave_id	= RZA1DMA_SLAVE_SDHI0_RX,
		.addr		= 0xe804e030,
		.chcfg		= CHCFG(0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x0),
		.dmars		= DMARS(0x2, 0x30),
	},
	{
		.slave_id	= RZA1DMA_SLAVE_SDHI1_TX,
		.addr		= 0xe804e830,
		.chcfg		= CHCFG(0x1, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x0),
		.dmars		= DMARS(0x1, 0x31),
	},
	{
		.slave_id	= RZA1DMA_SLAVE_SDHI1_RX,
		.addr		= 0xe804e830,
		.chcfg		= CHCFG(0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x0),
		.dmars		= DMARS(0x2, 0x31),
	},
	{
		.slave_id	= RZA1DMA_SLAVE_MMCIF_TX,
		.addr		= 0xe804c834,
		.chcfg		= CHCFG(0x1, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x0),
		.dmars		= DMARS(0x1, 0x32),
	},
	{
		.slave_id	= RZA1DMA_SLAVE_MMCIF_RX,
		.addr		= 0xe804c834,
		.chcfg		= CHCFG(0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x0),
		.dmars		= DMARS(0x2, 0x32),
	},
	{
		.slave_id	= RZA1DMA_SLAVE_PCM_MEM_SSI0,
		.addr		= 0xe820b018,		/* SSIFTDR_0 */
		.chcfg		= CHCFG(0x1, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x0),
		.dmars		= DMARS(0x1, 0x38),
	}, {
		.slave_id	= RZA1DMA_SLAVE_PCM_MEM_SRC1,
		.addr		= 0xe820970c,		/* DMATD1_CIM */
		.chcfg		= CHCFG(0x1, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0),
		.dmars		= DMARS(0x1, 0x41),
	}, {
		.slave_id	= RZA1DMA_SLAVE_PCM_SSI0_MEM,
		.addr		= 0xe820b01c,		/* SSIFRDR_0 */
		.chcfg		= CHCFG(0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x0),
		.dmars		= DMARS(0x2, 0x38),
	}, {
		.slave_id	= RZA1DMA_SLAVE_PCM_SRC0_MEM,
		.addr		= 0xe8209718,		/* DMATU0_CIM */
		.chcfg		= CHCFG(0x0, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0),
		.dmars		= DMARS(0x2, 0x40),
	},
};

static struct rza1_dma_pdata dma_platform_data = {
	.slave		= rza1_dma_slaves,
	.slave_num	= ARRAY_SIZE(rza1_dma_slaves),
	.channel_num	= 16,
};

static struct resource rza1_dma_resources[] = {
	{
		.start	= 0xe8200000,
		.end	= 0xe8200fff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= 0xfcfe1000,
		.end	= 0xfcfe1fff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= 41,
		.end	= 56,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= 57,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device dma_device = {
	.name		= "rza1-dma",
	.id		= -1,
	.resource	= rza1_dma_resources,
	.num_resources	= ARRAY_SIZE(rza1_dma_resources),
	.dev		= {
		.platform_data	= &dma_platform_data,
	},
};

/* Audio */
static struct platform_device alsa_soc_platform_device = {
	.name		= "genmai_alsa_soc_platform",

	.id		= 0,
};

static struct resource scux_resources[] = {
	[0] = {
		.name   = "scux",
		.start  = 0xe8208000,	/* SCUX Register Address */
		.end    = 0xe8209777,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.name   = "ssif0",
		.start  = 0xe820b000,	/* SSIF0 Register Address  */
		.end    = 0xe820d82f,
		.flags  = IORESOURCE_MEM,
	},
};

/* Audio */
static struct scu_config ssi_ch_value[] = {
	{RP_MEM_SSI0,		SSI0},
	{RP_MEM_SRC1_SSI0,	SSI0},
	{RP_MEM_SRC1_DVC1_SSI0,	SSI0},
	{RC_SSI0_MEM,		SSI0},
	{RC_SSI0_SRC0_MEM,	SSI0},
};

static struct scu_config src_ch_value[] = {
	{RP_MEM_SSI0,		-1},
	{RP_MEM_SRC1_SSI0,	SRC1},
	{RP_MEM_SRC1_DVC1_SSI0,	SRC1},
	{RC_SSI0_MEM,		-1},
	{RC_SSI0_SRC0_MEM,	SRC0},
};

static struct scu_config dvc_ch_value[] = {
	{RP_MEM_SSI0,		-1},
	{RP_MEM_SRC1_SSI0,	-1},
	{RP_MEM_SRC1_DVC1_SSI0,	DVC1},
	{RC_SSI0_MEM,		-1},
	{RC_SSI0_SRC0_MEM,	-1},
};

static struct scu_config audma_slave_value[] = {
	{RP_MEM_SSI0,		RZA1DMA_SLAVE_PCM_MEM_SSI0},
	{RP_MEM_SRC1_SSI0,	RZA1DMA_SLAVE_PCM_MEM_SRC1},
	{RP_MEM_SRC1_DVC1_SSI0,	RZA1DMA_SLAVE_PCM_MEM_SRC1},
	{RC_SSI0_MEM,		RZA1DMA_SLAVE_PCM_SSI0_MEM},
	{RC_SSI0_SRC0_MEM,	RZA1DMA_SLAVE_PCM_SRC0_MEM},
};

static struct scu_config ssi_depend_value[] = {
	{RP_MEM_SSI0,		SSI_INDEPENDANT},
	{RP_MEM_SRC1_SSI0,	SSI_DEPENDANT},
	{RP_MEM_SRC1_DVC1_SSI0,	SSI_DEPENDANT},
	{RC_SSI0_MEM,		SSI_INDEPENDANT},
	{RC_SSI0_SRC0_MEM,	SSI_DEPENDANT},
};

static struct scu_config ssi_mode_value[] = {
	{RP_MEM_SSI0,		SSI_MASTER},
	{RP_MEM_SRC1_SSI0,	SSI_MASTER},
	{RP_MEM_SRC1_DVC1_SSI0,	SSI_MASTER},
	{RC_SSI0_MEM,		SSI_SLAVE},
	{RC_SSI0_SRC0_MEM,	SSI_SLAVE},
};

static struct scu_config src_mode_value[] = {
	{RP_MEM_SSI0,		SRC_CR_ASYNC},
	{RP_MEM_SRC1_SSI0,	SRC_CR_ASYNC},
	{RP_MEM_SRC1_DVC1_SSI0,	SRC_CR_ASYNC},
	{RC_SSI0_MEM,		SRC_CR_ASYNC},
	{RC_SSI0_SRC0_MEM,	SRC_CR_ASYNC},
};


static struct scu_platform_data scu_pdata = {
	.ssi_master		= SSI0,
	.ssi_slave		= SSI0,
	.ssi_ch			= ssi_ch_value,
	.ssi_ch_num		= ARRAY_SIZE(ssi_ch_value),
	.src_ch			= src_ch_value,
	.src_ch_num		= ARRAY_SIZE(src_ch_value),
	.dvc_ch			= dvc_ch_value,
	.dvc_ch_num		= ARRAY_SIZE(dvc_ch_value),
	.dma_slave_maxnum	= RZA1DMA_SLAVE_PCM_MAX,
	.audma_slave		= audma_slave_value,
	.audma_slave_num	= ARRAY_SIZE(audma_slave_value),
	.ssi_depend		= ssi_depend_value,
	.ssi_depend_num		= ARRAY_SIZE(ssi_depend_value),
	.ssi_mode		= ssi_mode_value,
	.ssi_mode_num		= ARRAY_SIZE(ssi_mode_value),
	.src_mode		= src_mode_value,
	.src_mode_num		= ARRAY_SIZE(src_mode_value),
};

static struct platform_device scux_device = {
	.name		= "scux-pcm-audio",
	.id		= 0,
	.dev.platform_data = &scu_pdata,
	.num_resources	= ARRAY_SIZE(scux_resources),
	.resource	= scux_resources,
};

static struct platform_device *rza1_devices[] __initdata = {
#if !defined(CONFIG_MACH_DIMMRZA1H)
	&i2c_device0,
#endif
	&i2c_device1,
#if !defined(CONFIG_MACH_DIMMRZA1H)
	&i2c_device2,
#endif
	&i2c_device3,
	&r8a66597_usb_host0_device,
	&r8a66597_usb_host1_device,
	&sh_eth_device,
#if !defined(CONFIG_MACH_DIMMRZA1H)
	&spi0_device,
	&spi1_device,
	&spi2_device,
	&spi3_device,
#endif
	&spi4_device,
	&adc0_device,
	&spibsc0_device,
#if !defined(CONFIG_MACH_DIMMRZA1H)
	&spibsc1_device,
#endif
	&rz_can_device,
};

static struct platform_device *rza1_early_devices[] __initdata = {
	&dma_device,
#if !defined(CONFIG_MACH_DIMMRZA1H)
	&scif0_device,
#endif
	&scif1_device,
	&scif2_device,
	&scif3_device,
#if !defined(CONFIG_MACH_DIMMRZA1H)
	&scif4_device,
#endif
	&scif5_device,
	&scif6_device,
#if !defined(CONFIG_MACH_DIMMRZA1H)
	&scif7_device,
#endif
	&mtu2_0_device,
#if !defined(CONFIG_MACH_DIMMRZA1H)
	&rtc_device,
#endif
	&alsa_soc_platform_device,
	&scux_device,
};

void __init rza1_devices_setup(void)
{
	if (disable_ether)
		sh_eth_device.name = "sh-eth(hidden)";

	platform_add_devices(rza1_early_devices,
			    ARRAY_SIZE(rza1_early_devices));
	platform_add_devices(rza1_devices,
			    ARRAY_SIZE(rza1_devices));
}

static void __init rza1_earlytimer_init(void)
{
	rza1_clock_init();
	shmobile_earlytimer_init();
}

void __init rza1_add_early_devices(void)
{
	early_platform_add_devices(rza1_early_devices,
				   ARRAY_SIZE(rza1_early_devices));

	/* setup early console here as well */
	shmobile_setup_console();

	/* override timer setup with soc-specific code */
	shmobile_timer.init = rza1_earlytimer_init;
}
