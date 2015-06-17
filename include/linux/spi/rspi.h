/*
 * Renesas SPI driver
 *
 * Copyright (C) 2012-2013  Renesas Solutions Corp.
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

#ifndef __LINUX_SPI_RENESAS_SPI_H__
#define __LINUX_SPI_RENESAS_SPI_H__

struct rspi_plat_data {
	u8 data_width; /* data register access width */
	bool txmode;	/* tx only mode  */
	bool spcr;	/* set parity register */

	unsigned int dma_tx_id;
	unsigned int dma_rx_id;

	unsigned dma_width_16bit:1;	/* DMAC read/write width = 16-bit */
};

#endif
