/*
 * dimmrza1h board support
 *
 * Copyright (C) 2014  emtrion GmbH <ferdinand.schwenk@emtrion.de>
 *
 * Based on: board-rskrza1.c
 *
 * Copyright (C) 2013  Renesas Solutions Corp.
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
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sh_mmcif.h>
#include <linux/mmc/sh_mobile_sdhi.h>
#include <linux/mfd/tmio.h>
#include <linux/platform_data/dma-rza1.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/i2c/pca953x.h>
#include <linux/fb.h>
#include <mach/rza1.h>
#include <video/vdc5fb.h>
#include <linux/usb/r8a66597.h>

#define IRQ_TOUCH_ID 510

#define gic_spi(nr)             ((nr) + 32)
#define gic_iid(nr)             (nr) /* ICCIAR / interrupt ID */

/* MMCIF */
static struct resource sh_mmcif_resources[] = {
	[0] = {
		.name	= "MMCIF",
		.start	= 0xe804c800,
		.end	= 0xe804c8ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 300,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= 301,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_mmcif_plat_data sh_mmcif_platdata = {
	.ocr		= MMC_VDD_32_33,
	.caps		= MMC_CAP_4_BIT_DATA |
			  MMC_CAP_8_BIT_DATA |
				MMC_CAP_NONREMOVABLE,
};

static struct platform_device mmc_device = {
	.name		= "sh_mmcif",
	.id		= -1,
	.dev		= {
		.dma_mask		= NULL,
		.coherent_dma_mask	= 0xffffffff,
		.platform_data		= &sh_mmcif_platdata,
	},
	.num_resources	= ARRAY_SIZE(sh_mmcif_resources),
	.resource	= sh_mmcif_resources,
};

/* SDHI0 */
static struct sh_mobile_sdhi_info sdhi0_info = {
	.dma_slave_tx	= RZA1DMA_SLAVE_SDHI0_TX,
	.dma_slave_rx	= RZA1DMA_SLAVE_SDHI0_RX,
	.tmio_caps	= MMC_CAP_SD_HIGHSPEED | MMC_CAP_SDIO_IRQ,
	.tmio_ocr_mask	= MMC_VDD_32_33,
	.tmio_flags	= TMIO_MMC_HAS_IDLE_WAIT,
};

static struct resource sdhi0_resources[] = {
	[0] = {
		.name	= "SDHI0",
		.start	= 0xe804e000,
		.end	= 0xe804e0ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name   = SH_MOBILE_SDHI_IRQ_CARD_DETECT,
		.start	= 302,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.name   = SH_MOBILE_SDHI_IRQ_SDCARD,
		.start	= 303,
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.name   = SH_MOBILE_SDHI_IRQ_SDIO,
		.start	= 304,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device sdhi0_device = {
	.name		= "sh_mobile_sdhi",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(sdhi0_resources),
	.resource	= sdhi0_resources,
	.dev	= {
		.platform_data	= &sdhi0_info,
	},
};

/* I2C */
static struct i2c_board_info dimmrza1h_i2c3_board_info[] __initdata = {
#if defined (CONFIG_TOUCHSCREEN_AR1020) || defined (CONFIG_TOUCHSCREEN_AR1020_MODULE)
	{
		I2C_BOARD_INFO("ar1020", 0x4d),
		.irq = IRQ_TOUCH_ID,
	},
#endif
	{
		I2C_BOARD_INFO("ds1337", 0x68),
//		.type = "ds1337",
	},
};


static struct pca953x_platform_data pca9555 = {
	.gpio_base = 200,
	.invert = 0,
};

static struct at24_platform_data m24c01 = {
	.byte_len	= SZ_1K / 8,
	.page_size	= 16,
};

static struct i2c_board_info dimmrza1h_i2c1_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("PCA9555", 0x26),
		.platform_data = &pca9555,
	},
	{
		I2C_BOARD_INFO("M24C01", 0x51),
		.platform_data = &m24c01,
	},
};

/* LEDs */
static struct gpio_led dimmrza1h_led_pins[] = {
	{
		.name            = "cpu:green",
		.gpio            = 124,
		.active_low      = true,
		.default_trigger = "cpu0",
		.default_state   = LEDS_GPIO_DEFSTATE_ON,
	},
	{
		.name            = "cpu:red",
		.gpio            = 125,
		//.active_low      = true,
		.default_trigger = "cpu0",
		.default_state   = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name            = "lcd:backlight",
		.gpio            = 90,
		//.active_low      = true,
		.default_trigger = "backlight",
		//.default_state  = LEDS_GPIO_DEFSTATE_KEEP,
		.default_state  = LEDS_GPIO_DEFSTATE_ON,
	},
};

static struct gpio_led_platform_data dimmrza1h_led_data = {
	.num_leds            = ARRAY_SIZE(dimmrza1h_led_pins),
	.leds                = dimmrza1h_led_pins,
};

static struct platform_device dimmrza1h_leds = {
	.name                = "leds-gpio",
	.id                  = -1,
	.dev.platform_data   = &dimmrza1h_led_data,
};

static struct platform_device *dimmrza1h_devices[] __initdata = {
	&mmc_device,
	&sdhi0_device,
	&dimmrza1h_leds,
};

/* SPI_Flash */
static struct mtd_partition flash_partitions[] = {
	{
		.name		= "flash_loader",
		.offset		= 0x00000000,
		.size		= 0x00080000,
		/* .mask_flags	= MTD_WRITEABLE, */
	},
	{
		.name		= "flash_bootenv",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x00040000,
	},
	{
		.name		= "flash_kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x00400000,
	},
	{
		.name		= "flash_dtb",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 0x00040000,
	},
	{
		.name		= "flash_rootfs",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	},
};


static struct flash_platform_data spi_flash_data0 = {
	.name	= "m25p80",
	.parts	= flash_partitions,
	.nr_parts = ARRAY_SIZE(flash_partitions),
	.type = "s25fl129p0",
};


static struct spi_board_info dimmrza1h_spi_devices[] __initdata = {
	{
		/* spidev */
		.modalias		= "spidev",
		.chip_select		= 0,
		.max_speed_hz		= 5000000,
		.bus_num		= 4,
		.mode			= SPI_MODE_3,
		.clk_delay		= 2,
		.cs_negate_delay	= 2,
		.next_access_delay	= 2,
	},
	{
		/* SPI Flash */
		.modalias = "m25p80",
		/* .max_speed_hz = 25000000, */
		.bus_num = 5,
		.chip_select = 0,
		.platform_data = &spi_flash_data0,
	},
};

/* USB Host */
static const struct r8a66597_platdata r8a66597_pdata __initconst = {
 .endian = 0,
 .on_chip = 1,
 .xtal = R8A66597_PLATDATA_XTAL_48MHZ,
};

static const struct resource r8a66597_usb_host0_resources[] __initconst = {
 DEFINE_RES_MEM(0xe8010000, 0x1a0),
 DEFINE_RES_IRQ(gic_iid(73)),
};

static const struct platform_device_info r8a66597_usb_host0_info __initconst= {
 .name = "r8a66597_hcd",
 .id = 0,
 .data = &r8a66597_pdata,
 .size_data = sizeof(r8a66597_pdata),
 .res = r8a66597_usb_host0_resources,
 .num_res = ARRAY_SIZE(r8a66597_usb_host0_resources),
};

static const struct resource r8a66597_usb_host1_resources[] __initconst = {
 DEFINE_RES_MEM(0xe8207000, 0x1a0),
 DEFINE_RES_IRQ(gic_iid(74)),
};

static const struct platform_device_info r8a66597_usb_host1_info __initconst = {
 .name = "r8a66597_hcd",
 .id = 1,
 .data = &r8a66597_pdata,
 .size_data = sizeof(r8a66597_pdata),
 .res = r8a66597_usb_host1_resources,
 .num_res = ARRAY_SIZE(r8a66597_usb_host1_resources),
};


static void __init dimmrza1h_init_spi(void)
{
	/* register SPI device information */
	spi_register_board_info(dimmrza1h_spi_devices,
		ARRAY_SIZE(dimmrza1h_spi_devices));
}

static void dimmrza1h_power_off(void)
{
	unsigned int tmp;

	/* Switch LCD off */
	rza1_pfc_pin_assign(P6_9, PMODE, DIR_IN);	/* LCD_DON */

	/* Switch USB-Power off */
	rza1_pfc_pin_assign(P6_5, PMODE, DIR_IN);	/* CPU_LED_GREEN */

	/* Go to Deep Sleep */
	iowrite8(       0x00, (void __iomem *) 0xFCFF1800); // No Data-Retention
	iowrite8(       0x00, (void __iomem *) 0xFCFF1802); // No ExternRAM Retention, restart according to Boot-Mode
	iowrite16(    0x0000, (void __iomem *) 0xFCFF1804); // No Cancel-Source
	iowrite16(    0x0000, (void __iomem *) 0xFCFF1806); // No Cancel-Source
	rza1_pfc_pin_assign(P8_11, PMODE, DIR_IN);	/* CPU_LED_GREEN */
	rza1_pfc_pin_assign(P8_12, PMODE, DIR_IN);	/* CPU_LED_RED */
	iowrite8(       0xC0, (void __iomem *) 0xFCFE0020); // Select Deep-Standby
	tmp = ioread8(        (void __iomem *) 0xFCFE0020);
	iowrite16(    0x0000, (void __iomem *) 0xFCFF1808); // Clear Reset-Flags
	tmp = ioread16(       (void __iomem *) 0xFCFF1808);
	iowrite32(0x00000000, (void __iomem *) 0xE8202000); // Disable Interrupts
	while (1)
	{
		asm volatile("WFI");
	}
}

void dimmrza1h_restart(char mode, const char *cmd)
{
	unsigned int reg16;
	rza1_pfc_pin_assign(P3_7, PMODE, DIR_OUT);	/* WDTOVF# */
	reg16 = ioread16((void __iomem *) 0xFCFE300C);
	reg16 &= ~0x0100;
	iowrite16(reg16, (void __iomem *) 0xFCFE300C);
}

#include "rskrza1-vdc5fb.c"
void __init dimmrza1h_init(void)
{
	if (disable_sdhi)
		sdhi0_device.name = "mobile_sdhi(hidden)";

	platform_add_devices(dimmrza1h_devices, ARRAY_SIZE(dimmrza1h_devices));

	pm_power_off = dimmrza1h_power_off;

	rza1_pinmux_setup();

	rza1_devices_setup();

	/* We don't need to assign pin functions. */
	/* The Pins are allready configured by the bootloader */

	i2c_register_board_info(1, dimmrza1h_i2c1_board_info,
			ARRAY_SIZE(dimmrza1h_i2c1_board_info));
	i2c_register_board_info(3, dimmrza1h_i2c3_board_info,
			ARRAY_SIZE(dimmrza1h_i2c3_board_info));

#if defined(CONFIG_SPI_MASTER)
	dimmrza1h_init_spi();
#endif

#if defined(CONFIG_FB_VDC5)
	vdc5fb_setup();
#endif

  platform_device_register_full(&r8a66597_usb_host0_info);
  platform_device_register_full(&r8a66597_usb_host1_info);

}

static const char *dimmrza1h_boards_compat_dt[] __initdata = {
	"renesas,dimmrza1h",
	NULL,
};

DT_MACHINE_START(RSKRZA1_DT, "dimmrza1h")
	.nr_irqs	= NR_IRQS_LEGACY,
	.map_io		= rza1_map_io,
	.init_early	= rza1_add_early_devices,
	.init_irq	= rza1_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= dimmrza1h_init,
	.init_late	= shmobile_init_late,
	.timer		= &shmobile_timer,
	.dt_compat	= dimmrza1h_boards_compat_dt,
	.restart	= dimmrza1h_restart,
MACHINE_END
