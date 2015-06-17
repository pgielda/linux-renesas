/*
 * raa730300.h
 *
 * Copyright (C) 2013 Renesas Solutions Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _UAPI_RAA730300_H
#define _UAPI_RAA730300_H

struct sa_reg {
	unsigned char	addr;
	unsigned char	value;
};

#define RAA730300_NUM_OF_REGS 0x17
struct sa_regs {
	int		num;
	struct sa_reg	regs[RAA730300_NUM_OF_REGS];
};

#define	RAA730300_IOCTL_REG_READ	0
#define	RAA730300_IOCTL_REG_WRITE	1

#endif		/* _UAPI_RAA730300_H */
