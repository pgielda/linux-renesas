/*
 * Copyright (C) 2011-2013 Renesas Solutions Corp.
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
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <mach/intc.h>
#include <mach/irqs.h>
#include <mach/rza1.h>

static int rza1_set_wake(struct irq_data *data, unsigned int on)
{
	return 0; /* always allow wakeup */
}

void __init rza1_init_irq(void)
{
	void __iomem *gic_dist_base = IOMEM(0xe8201000);
	void __iomem *gic_cpu_base = IOMEM(0xe8202000);

	/* Initialize GIC. */
	gic_init(0, 29, gic_dist_base, gic_cpu_base);
	gic_arch_extn.irq_set_wake = rza1_set_wake;

}
