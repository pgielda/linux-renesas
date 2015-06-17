/*
 * SH RSPI driver
 *
 * Copyright (C) 2011-2013  Renesas Solutions Corp.
 *
 * Based on spi-sh.c:
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/sh_dma.h>
#include <linux/spi/spi.h>
#include <linux/spi/rspi.h>

#define RSPI_SPCR		0x00
#define RSPI_SSLP		0x01
#define RSPI_SPPCR		0x02
#define RSPI_SPSR		0x03
#define RSPI_SPDR		0x04
#define RSPI_SPSCR		0x08
#define RSPI_SPSSR		0x09
#define RSPI_SPBR		0x0a
#define RSPI_SPDCR		0x0b
#define RSPI_SPCKD		0x0c
#define RSPI_SSLND		0x0d
#define RSPI_SPND		0x0e
#define RSPI_SPCR2		0x0f
#define RSPI_SPCMD0		0x10
#define RSPI_SPCMD1		0x12
#define RSPI_SPCMD2		0x14
#define RSPI_SPCMD3		0x16
#define RSPI_SPCMD4		0x18
#define RSPI_SPCMD5		0x1a
#define RSPI_SPCMD6		0x1c
#define RSPI_SPCMD7		0x1e
#define RSPI_SPBFCR		0x20
#define RSPI_SPBFDR		0x22

/* SPCR */
#define SPCR_SPRIE		0x80
#define SPCR_SPE		0x40
#define SPCR_SPTIE		0x20
#define SPCR_SPEIE		0x10
#define SPCR_MSTR		0x08
#define SPCR_MODFEN		0x04
#define SPCR_TXMD		0x02
#define SPCR_SPMS		0x01

/* SSLP */
#define SSLP_SSL1P		0x02
#define SSLP_SSL0P		0x01

/* SPPCR */
#define SPPCR_MOIFE		0x20
#define SPPCR_MOIFV		0x10
#define SPPCR_SPOM		0x04
#define SPPCR_SPLP2		0x02
#define SPPCR_SPLP		0x01

/* SPSR */
#define SPSR_SPRF		0x80
#define SPSR_SPTE		0x40
#define SPSR_SPTEF		0x20
#define SPSR_PERF		0x08
#define SPSR_MODF		0x04
#define SPSR_IDLNF		0x02
#define SPSR_OVRF		0x01

/* SPSCR */
#define SPSCR_SPSLN_MASK	0x07

/* SPSSR */
#define SPSSR_SPECM_MASK	0x70
#define SPSSR_SPCP_MASK		0x07

/* SPDCR */
#define SPDCR_TXDMY		0x80
#define SPDCR_SPLLWORD		(SPDCR_SPLW1 | SPDCR_SPLW0)
#define SPDCR_SPLWORD		SPDCR_SPLW1
#define SPDCR_SPLBYTE		SPDCR_SPLW0
#define SPDCR_SPLW1		0x40
#define SPDCR_SPLW0		0x20
#define SPDCR_SPRDTD		0x10
#define SPDCR_SLSEL1		0x08
#define SPDCR_SLSEL0		0x04
#define SPDCR_SLSEL_MASK	0x0c
#define SPDCR_SPFC1		0x02
#define SPDCR_SPFC0		0x01

/* SPCKD */
#define SPCKD_SCKDL_MASK	0x07

/* SSLND */
#define SSLND_SLNDL_MASK	0x07

/* SPND */
#define SPND_SPNDL_MASK		0x07

/* SPCR2 */
#define SPCR2_PTE		0x08
#define SPCR2_SPIE		0x04
#define SPCR2_SPOE		0x02
#define SPCR2_SPPE		0x01

/* SPCMDn */
#define SPCMD_SCKDEN		0x8000
#define SPCMD_SLNDEN		0x4000
#define SPCMD_SPNDEN		0x2000
#define SPCMD_LSBF		0x1000
#define SPCMD_SPB_MASK		0x0f00
#define SPCMD_SPB_8_TO_16(bit)	(((bit - 1) << 8) & SPCMD_SPB_MASK)
#define SPCMD_SPB_20BIT		0x0000
#define SPCMD_SPB_24BIT		0x0100
#define SPCMD_SPB_32BIT		0x0200
#define SPCMD_SSLKP		0x0080
#define SPCMD_SSLA_MASK		0x0030
#define SPCMD_BRDV_MASK		0x000c
#define SPCMD_CPOL		0x0002
#define SPCMD_CPHA		0x0001

/* SPBFCR */
#define SPBFCR_TXRST		0x0080
#define SPBFCR_RXRST		0x0040

/* SPBFDR */
#define SPBFDR_RMASK		0x003f
#define SPBFDR_TMASK		0x0f00

#define DUMMY_DATA		0x00
#define NUM_IRQ			3
#define DRIVER_NAME		"sh_rspi"
#define DRIVER_NAME_SIZE	16
static char drv_name[16][NUM_IRQ][DRIVER_NAME_SIZE];

struct rspi_data {
	void __iomem *addr;
	u32 max_speed_hz;
	struct spi_master *master;
	struct list_head queue;
	struct work_struct ws;
	wait_queue_head_t wait;
	spinlock_t lock;
	struct clk *clk;
	unsigned char spsr;
	unsigned char sppcr;
	unsigned char spdcr;
	u16 spcmd;
	u8 clk_delay;
	u8 cs_negate_delay;
	u8 next_access_delay;
	u8 data_width;
	u8 data_width_regval;
	bool txmode;
	bool spcr;
	int irq[NUM_IRQ];
	int irqn;

	/* for dmaengine */
	struct dma_chan *chan_tx;
	struct dma_chan *chan_rx;

	unsigned dma_width_16bit:1;
	unsigned dma_callbacked:1;
};

static void rspi_write8(struct rspi_data *rspi, u8 data, u16 offset)
{
	iowrite8(data, rspi->addr + offset);
}

static void rspi_write16(struct rspi_data *rspi, u16 data, u16 offset)
{
	iowrite16(data, rspi->addr + offset);
}

static u8 rspi_read8(struct rspi_data *rspi, u16 offset)
{
	return ioread8(rspi->addr + offset);
}

static u16 rspi_read16(struct rspi_data *rspi, u16 offset)
{
	return ioread16(rspi->addr + offset);
}

static void rspi_write_data(struct rspi_data *rspi, u16 data)
{
	if (rspi->data_width == 8)
		rspi_write8(rspi, (u8)data, RSPI_SPDR);
	else if (rspi->data_width == 16)
		rspi_write16(rspi, data, RSPI_SPDR);
}

static u16 rspi_read_data(struct rspi_data *rspi)
{
	if (rspi->data_width == 8)
		return rspi_read8(rspi, RSPI_SPDR);
	else if (rspi->data_width == 16)
		return rspi_read16(rspi, RSPI_SPDR);
	else
		return 0;
}

static unsigned char rspi_calc_spbr(struct rspi_data *rspi)
{
	int tmp;
	unsigned char spbr;

	tmp = clk_get_rate(rspi->clk) / (2 * rspi->max_speed_hz) - 1;
	spbr = clamp(tmp, 0, 255);

	return spbr;
}
static void rspi_clear_rxbuf( struct rspi_data *rspi )
{
	rspi_write8(rspi, rspi_read8(rspi, RSPI_SPBFCR) |   SPBFCR_RXRST,  RSPI_SPBFCR);
	rspi_write8(rspi, rspi_read8(rspi, RSPI_SPBFCR) & (~SPBFCR_RXRST), RSPI_SPBFCR);
}
static void rspi_clear_txbuf( struct rspi_data *rspi )
{
	rspi_write8(rspi, rspi_read8(rspi, RSPI_SPBFCR) |   SPBFCR_TXRST,  RSPI_SPBFCR);
	rspi_write8(rspi, rspi_read8(rspi, RSPI_SPBFCR) & (~SPBFCR_TXRST), RSPI_SPBFCR);
}

static void rspi_enable_irq(struct rspi_data *rspi, u8 enable)
{
	rspi_write8(rspi, rspi_read8(rspi, RSPI_SPCR) | enable, RSPI_SPCR);
}

static void rspi_disable_irq(struct rspi_data *rspi, u8 disable)
{
	rspi_write8(rspi, rspi_read8(rspi, RSPI_SPCR) & ~disable, RSPI_SPCR);
}

static int rspi_wait_for_interrupt(struct rspi_data *rspi, u8 wait_mask,
				   u8 enable_bit)
{
	int ret;

	rspi->spsr = rspi_read8(rspi, RSPI_SPSR);
	rspi_enable_irq(rspi, enable_bit);
	ret = wait_event_timeout(rspi->wait, rspi->spsr & wait_mask, HZ);
	if (ret == 0 && !(rspi->spsr & wait_mask))
		return -ETIMEDOUT;

	return 0;
}

static void rspi_assert_ssl(struct rspi_data *rspi)
{
	rspi_write8(rspi, rspi_read8(rspi, RSPI_SPCR) | SPCR_SPE, RSPI_SPCR);
}

static void rspi_negate_ssl(struct rspi_data *rspi)
{
	rspi_write8(rspi, rspi_read8(rspi, RSPI_SPCR) & ~SPCR_SPE, RSPI_SPCR);
}

static void rspi_set_txmode(struct rspi_data *rspi)
{
	if (rspi->txmode)
		rspi_write8(rspi, rspi_read8(rspi, RSPI_SPCR) | SPCR_TXMD,
				RSPI_SPCR);
}

static void rspi_clear_txmode(struct rspi_data *rspi)
{
	if (rspi->txmode)
		rspi_write8(rspi, rspi_read8(rspi, RSPI_SPCR) & ~SPCR_TXMD,
				RSPI_SPCR);
}

static int rspi_setup_data_register_width(struct rspi_data *rspi,
				      struct platform_device *pdev)
{
	struct rspi_plat_data *rspi_pd = pdev->dev.platform_data;

	if (rspi_pd && rspi_pd->data_width) {
		rspi->data_width = rspi_pd->data_width;
		/* This version driver supports only 8bits width */
		if (rspi_pd->data_width == 8) {
			rspi->data_width_regval = SPDCR_SPLBYTE;
			return 0;
		} else {
			return -1;
		}
		rspi->data_width_regval = 0;
	} else {
		/* Use 16bits width data access if a data_width value isn't */
		/* defined in a platform data. */
		rspi->data_width = 16;
		rspi->data_width_regval = 0;
	}

	return 0;
}

static int rspi_set_config_register(struct rspi_data *rspi, int access_size)
{
	/* Sets output mode(CMOS) and MOSI signal(from previous transfer) */
	rspi_write8(rspi, rspi->sppcr, RSPI_SPPCR);

	/* Sets transfer bit rate */
	rspi_write8(rspi, rspi_calc_spbr(rspi), RSPI_SPBR);

	/* Sets number of frames to be used: 1 frame */
	rspi_write8(rspi, rspi->spdcr, RSPI_SPDCR);

	/* Sets delays for RSPCK, SSL negate and next-access */
	rspi_write8(rspi, rspi->clk_delay, RSPI_SPCKD);
	rspi_write8(rspi, rspi->cs_negate_delay, RSPI_SSLND);
	rspi_write8(rspi, rspi->next_access_delay, RSPI_SPND);

	/* Sets parity, interrupt mask */
	if (rspi->spcr)
		rspi_write8(rspi, 0x00, RSPI_SPCR2);

	/* Sets SPCMD */
	rspi_write16(rspi, SPCMD_SPB_8_TO_16(access_size) | rspi->spcmd,
		     RSPI_SPCMD0);

	/* Sets RSPI mode */
	rspi_write8(rspi, SPCR_MSTR, RSPI_SPCR);

	return 0;
}

static int rspi_send_pio(struct rspi_data *rspi, struct spi_message *mesg,
			 struct spi_transfer *t)
{
	int remain = t->len;
	u8 *data;

	if( rspi->sppcr & SPPCR_SPLP ) {
		/* loop back mode */
		rspi_clear_txbuf(rspi);
		rspi_clear_rxbuf(rspi);
	}

	data = (u8 *)t->tx_buf;
	while (remain > 0) {
		rspi_set_txmode(rspi);

		if (rspi_wait_for_interrupt(rspi, SPSR_SPTEF, SPCR_SPTIE) < 0) {
			dev_err(&rspi->master->dev,
				"%s: tx empty timeout\n", __func__);
			return -ETIMEDOUT;
		}
		if (!rspi->txmode && remain != t->len) {
			if( !(rspi->sppcr & SPPCR_SPLP) ) {
				rspi_wait_for_interrupt(rspi, SPSR_SPRF, SPCR_SPRIE);
				rspi_read_data(rspi); /* dummy read */
			}
		}
		rspi_write_data(rspi, *data);
		data++;
		remain--;
	}

	/* Waiting for the last transmition */
	rspi_wait_for_interrupt(rspi, SPSR_SPTEF, SPCR_SPTIE);

	if (rspi_wait_for_interrupt(rspi, SPSR_SPRF, SPCR_SPRIE) < 0) {
		dev_err(&rspi->master->dev,
			"%s: receive timeout\n", __func__);
		return -ETIMEDOUT;
	}
	if (!rspi->txmode) {
		if( !(rspi->sppcr & SPPCR_SPLP) )
			rspi_read_data(rspi); /* dummy read */
	}

	return 0;
}

static void rspi_dma_complete(void *arg)
{
	struct rspi_data *rspi = arg;

	rspi->dma_callbacked = 1;
	wake_up_interruptible(&rspi->wait);
}

static int rspi_dma_map_sg(struct scatterlist *sg, void *buf, unsigned len,
			   struct dma_chan *chan,
			   enum dma_transfer_direction dir)
{
	sg_init_table(sg, 1);
	sg_set_buf(sg, buf, len);
	sg_dma_len(sg) = len;
	return dma_map_sg(chan->device->dev, sg, 1, dir);
}

static void rspi_dma_unmap_sg(struct scatterlist *sg, struct dma_chan *chan,
			      enum dma_transfer_direction dir)
{
	dma_unmap_sg(chan->device->dev, sg, 1, dir);
}

static void rspi_memory_to_8bit(void *buf, const void *data, unsigned len)
{
	u16 *dst = buf;
	const u8 *src = data;

	while (len) {
		*dst++ = (u16)(*src++);
		len--;
	}
}

static void rspi_memory_from_8bit(void *buf, const void *data, unsigned len)
{
	u8 *dst = buf;
	const u16 *src = data;

	while (len) {
		*dst++ = (u8)*src++;
		len--;
	}
}

static int rspi_send_dma(struct rspi_data *rspi, struct spi_transfer *t)
{
	struct scatterlist sg;
	void *buf = NULL;
	struct dma_async_tx_descriptor *desc;
	unsigned len;
	int i, ret = 0;

	if (rspi->dma_width_16bit) {
		/*
		 * If DMAC bus width is 16-bit, the driver allocates a dummy
		 * buffer. And, the driver converts original data into the
		 * DMAC data as the following format:
		 *  original data: 1st byte, 2nd byte ...
		 *  DMAC data:     1st byte, dummy, 2nd byte, dummy ...
		 */
		len = t->len * 2;
		buf = kmalloc(len, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
		rspi_memory_to_8bit(buf, t->tx_buf, t->len);
	} else {
		len = t->len;
		buf = (void *)t->tx_buf;
	}

	if (!rspi_dma_map_sg(&sg, buf, len, rspi->chan_tx, DMA_TO_DEVICE)) {
		ret = -EFAULT;
		goto end_nomap;
	}
	desc = dmaengine_prep_slave_sg(rspi->chan_tx, &sg, 1, DMA_TO_DEVICE,
				       DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		ret = -EIO;
		goto end;
	}

	/*
	 * DMAC needs SPTIE, but if SPTIE is set, this IRQ routine will be
	 * called. So, this driver disables the IRQ while DMA transfer.
	 */
	for (i = 0; i < rspi->irqn; i++)
		disable_irq(rspi->irq[i]);

	rspi_set_txmode(rspi);
	rspi_enable_irq(rspi, SPCR_SPTIE);
	rspi->dma_callbacked = 0;

	desc->callback = rspi_dma_complete;
	desc->callback_param = rspi;
	dmaengine_submit(desc);
	dma_async_issue_pending(rspi->chan_tx);

	ret = wait_event_interruptible_timeout(rspi->wait,
					       rspi->dma_callbacked, HZ);
	if (ret > 0 && rspi->dma_callbacked)
		ret = 0;
	else if (!ret)
		ret = -ETIMEDOUT;
	rspi_disable_irq(rspi, SPCR_SPTIE);

	for (i = 0; i < rspi->irqn; i++)
		enable_irq(rspi->irq[i]);

end:
	rspi_dma_unmap_sg(&sg, rspi->chan_tx, DMA_TO_DEVICE);
end_nomap:
	if (rspi->dma_width_16bit)
		kfree(buf);

	return ret;
}

static void rspi_receive_init(struct rspi_data *rspi)
{
	unsigned char spsr;

	spsr = rspi_read8(rspi, RSPI_SPSR);
	if( (spsr & SPSR_SPRF) && !(rspi->sppcr & SPPCR_SPLP) )
		rspi_read_data(rspi); /* dummy read */

	if (spsr & SPSR_OVRF)
		rspi_write8(rspi, rspi_read8(rspi, RSPI_SPSR) & ~SPSR_OVRF,
			    RSPI_SPSR);
}

static int rspi_receive_pio(struct rspi_data *rspi, struct spi_message *mesg,
			    struct spi_transfer *t)
{
	int remain = t->len;
	u8 *data;

	rspi_receive_init(rspi);

	data = (u8 *)t->rx_buf;
	while (remain > 0) {
		rspi_clear_txmode(rspi);

		if (rspi_wait_for_interrupt(rspi, SPSR_SPTEF, SPCR_SPTIE) < 0) {
			dev_err(&rspi->master->dev,
				"%s: tx empty timeout\n", __func__);
			return -ETIMEDOUT;
		}
		/* dummy write for generate clock */
		rspi_write_data(rspi, DUMMY_DATA);

		if (rspi_wait_for_interrupt(rspi, SPSR_SPRF, SPCR_SPRIE) < 0) {
			dev_err(&rspi->master->dev,
				"%s: receive timeout\n", __func__);
			return -ETIMEDOUT;
		}
		*data = (u8)rspi_read_data(rspi);
		data++;
		remain--;
	}
	if( rspi->sppcr & SPPCR_SPLP ) {
		/* loop back mode */
		rspi_clear_txbuf(rspi);
		rspi_clear_rxbuf(rspi);
	}

	return 0;
}

static int rspi_receive_dma(struct rspi_data *rspi, struct spi_transfer *t)
{
	struct scatterlist sg, sg_dummy;
	void *dummy = NULL, *rx_buf = NULL;
	struct dma_async_tx_descriptor *desc, *desc_dummy;
	unsigned len;
	int i, ret = 0;

	if (rspi->dma_width_16bit) {
		/*
		 * If DMAC bus width is 16-bit, the driver allocates a dummy
		 * buffer. And, finally the driver converts the DMAC data into
		 * actual data as the following format:
		 *  DMAC data:   1st byte, dummy, 2nd byte, dummy ...
		 *  actual data: 1st byte, 2nd byte ...
		 */
		len = t->len * 2;
		rx_buf = kmalloc(len, GFP_KERNEL);
		if (!rx_buf)
			return -ENOMEM;
	} else {
		len = t->len;
		rx_buf = t->rx_buf;
	}

	/* prepare dummy transfer to generate SPI clocks */
	dummy = kzalloc(len, GFP_KERNEL);
	if (!dummy) {
		ret = -ENOMEM;
		goto end_nomap;
	}
	if (!rspi_dma_map_sg(&sg_dummy, dummy, len, rspi->chan_tx,
			     DMA_TO_DEVICE)) {
		ret = -EFAULT;
		goto end_nomap;
	}
	desc_dummy = dmaengine_prep_slave_sg(rspi->chan_tx, &sg_dummy, 1,
			DMA_TO_DEVICE, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc_dummy) {
		ret = -EIO;
		goto end_dummy_mapped;
	}

	/* prepare receive transfer */
	if (!rspi_dma_map_sg(&sg, rx_buf, len, rspi->chan_rx,
			     DMA_FROM_DEVICE)) {
		ret = -EFAULT;
		goto end_dummy_mapped;

	}
	desc = dmaengine_prep_slave_sg(rspi->chan_rx, &sg, 1, DMA_FROM_DEVICE,
				       DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		ret = -EIO;
		goto end;
	}

	rspi_receive_init(rspi);

	/*
	 * DMAC needs SPTIE, but if SPTIE is set, this IRQ routine will be
	 * called. So, this driver disables the IRQ while DMA transfer.
	 */
	for (i = 0; i < rspi->irqn; i++)
		disable_irq(rspi->irq[i]);

	rspi_clear_txmode(rspi);
	rspi_enable_irq(rspi, SPCR_SPTIE | SPCR_SPRIE);
	rspi->dma_callbacked = 0;

	desc->callback = rspi_dma_complete;
	desc->callback_param = rspi;
	dmaengine_submit(desc);
	dma_async_issue_pending(rspi->chan_rx);

	desc_dummy->callback = NULL;	/* No callback */
	dmaengine_submit(desc_dummy);
	dma_async_issue_pending(rspi->chan_tx);

	ret = wait_event_interruptible_timeout(rspi->wait,
					       rspi->dma_callbacked, HZ);
	if (ret > 0 && rspi->dma_callbacked)
		ret = 0;
	else if (!ret)
		ret = -ETIMEDOUT;
	rspi_disable_irq(rspi, SPCR_SPTIE | SPCR_SPRIE);

	for (i = 0; i < rspi->irqn; i++)
		enable_irq(rspi->irq[i]);

end:
	rspi_dma_unmap_sg(&sg, rspi->chan_rx, DMA_FROM_DEVICE);
end_dummy_mapped:
	rspi_dma_unmap_sg(&sg_dummy, rspi->chan_tx, DMA_TO_DEVICE);
end_nomap:
	if (rspi->dma_width_16bit) {
		if (!ret)
			rspi_memory_from_8bit(t->rx_buf, rx_buf, t->len);
		kfree(rx_buf);
	}
	kfree(dummy);

	return ret;
}

static int rspi_is_dma(struct rspi_data *rspi, struct spi_transfer *t)
{
	if (t->tx_buf && rspi->chan_tx)
		return 1;
	/* If the module receives data by DMAC, it also needs TX DMAC */
	if (t->rx_buf && rspi->chan_tx && rspi->chan_rx)
		return 1;

	return 0;
}

static void rspi_work(struct work_struct *work)
{
	struct rspi_data *rspi = container_of(work, struct rspi_data, ws);
	struct spi_message *mesg;
	struct spi_transfer *t;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&rspi->lock, flags);
	while (!list_empty(&rspi->queue)) {
		mesg = list_entry(rspi->queue.next, struct spi_message, queue);
		list_del_init(&mesg->queue);
		spin_unlock_irqrestore(&rspi->lock, flags);

		rspi_assert_ssl(rspi);

		list_for_each_entry(t, &mesg->transfers, transfer_list) {
			if (t->tx_buf) {
				if (rspi_is_dma(rspi, t))
					ret = rspi_send_dma(rspi, t);
				else
					ret = rspi_send_pio(rspi, mesg, t);
				if (ret < 0)
					goto error;
			}
			if (t->rx_buf) {
				if (rspi_is_dma(rspi, t))
					ret = rspi_receive_dma(rspi, t);
				else
					ret = rspi_receive_pio(rspi, mesg, t);
				if (ret < 0)
					goto error;
			}
			mesg->actual_length += t->len;
		}
		rspi_negate_ssl(rspi);

		mesg->status = 0;
		mesg->complete(mesg->context);

		spin_lock_irqsave(&rspi->lock, flags);
	}
	spin_unlock_irqrestore(&rspi->lock, flags);

	return;

error:
	mesg->status = ret;
	mesg->complete(mesg->context);
}

static int rspi_setup(struct spi_device *spi)
{
	struct rspi_data *rspi = spi_master_get_devdata(spi->master);

	if (!spi->bits_per_word)
		spi->bits_per_word = 8;
	rspi->max_speed_hz = spi->max_speed_hz;

	rspi->spcmd = SPCMD_SSLKP;

	if (spi->mode & SPI_CPOL)
		rspi->spcmd |= SPCMD_CPOL;

	if (spi->mode & SPI_CPHA)
		rspi->spcmd |= SPCMD_CPHA;

	rspi->sppcr = 0;
	if (spi->mode & SPI_LOOP)
		rspi->sppcr |= SPPCR_SPLP;

	rspi->spdcr = rspi->data_width_regval;

	if (spi->clk_delay) {
		rspi->clk_delay = spi->clk_delay;
		rspi->spcmd |= SPCMD_SCKDEN;
	}
	if (spi->cs_negate_delay) {
		rspi->cs_negate_delay = spi->cs_negate_delay;
		rspi->spcmd |= SPCMD_SLNDEN;
	}
	if (spi->next_access_delay) {
		rspi->next_access_delay = spi->next_access_delay;
		rspi->spcmd |= SPCMD_SPNDEN;
	}

	rspi_set_config_register(rspi, 8);

	return 0;
}

static int rspi_transfer(struct spi_device *spi, struct spi_message *mesg)
{
	struct rspi_data *rspi = spi_master_get_devdata(spi->master);
	unsigned long flags;

	mesg->actual_length = 0;
	mesg->status = -EINPROGRESS;

	spin_lock_irqsave(&rspi->lock, flags);
	list_add_tail(&mesg->queue, &rspi->queue);
	schedule_work(&rspi->ws);
	spin_unlock_irqrestore(&rspi->lock, flags);

	return 0;
}

static void rspi_cleanup(struct spi_device *spi)
{
}

static irqreturn_t rspi_irq(int irq, void *_sr)
{
	struct rspi_data *rspi = (struct rspi_data *)_sr;
	unsigned long spsr;
	irqreturn_t ret = IRQ_NONE;
	unsigned char disable_irq = 0;

	rspi->spsr = spsr = rspi_read8(rspi, RSPI_SPSR);
	if (spsr & SPSR_SPRF)
		disable_irq |= SPCR_SPRIE;
	if (spsr & SPSR_SPTEF)
		disable_irq |= SPCR_SPTIE;

	if (disable_irq) {
		ret = IRQ_HANDLED;
		rspi_disable_irq(rspi, disable_irq);
		wake_up(&rspi->wait);
	}

	return ret;
}

static int rspi_request_dma(struct rspi_data *rspi,
				      struct platform_device *pdev)
{
	struct rspi_plat_data *rspi_pd = pdev->dev.platform_data;
	dma_cap_mask_t mask;
	struct dma_slave_config cfg;
	int ret;

	if (!rspi_pd)
		return 0;	/* The driver assumes no error. */

	rspi->dma_width_16bit = rspi_pd->dma_width_16bit;

	/* If the module receives data by DMAC, it also needs TX DMAC */
	if (rspi_pd->dma_rx_id && rspi_pd->dma_tx_id) {
		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		rspi->chan_rx = dma_request_channel(mask, shdma_chan_filter,
						    (void *)rspi_pd->dma_rx_id);
		if (rspi->chan_rx) {
			cfg.slave_id = rspi_pd->dma_rx_id;
			cfg.direction = DMA_DEV_TO_MEM;
			ret = dmaengine_slave_config(rspi->chan_rx, &cfg);
			if (!ret)
				dev_info(&pdev->dev, "Use DMA when rx.\n");
			else
				return ret;
		}
	}
	if (rspi_pd->dma_tx_id) {
		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		rspi->chan_tx = dma_request_channel(mask, shdma_chan_filter,
						    (void *)rspi_pd->dma_tx_id);
		if (rspi->chan_tx) {
			cfg.slave_id = rspi_pd->dma_tx_id;
			cfg.direction = DMA_MEM_TO_DEV;
			ret = dmaengine_slave_config(rspi->chan_tx, &cfg);
			if (!ret)
				dev_info(&pdev->dev, "Use DMA when tx\n");
			else
				return ret;
		}
	}

	return 0;
}

static void rspi_release_dma(struct rspi_data *rspi)
{
	if (rspi->chan_tx)
		dma_release_channel(rspi->chan_tx);
	if (rspi->chan_rx)
		dma_release_channel(rspi->chan_rx);
}

static int rspi_remove(struct platform_device *pdev)
{
	struct rspi_data *rspi = dev_get_drvdata(&pdev->dev);
	int i;

	spi_unregister_master(rspi->master);
	rspi_release_dma(rspi);
	for (i = 0; i < rspi->irqn; i++)
		free_irq(rspi->irq[i], rspi);
	clk_put(rspi->clk);
	iounmap(rspi->addr);
	spi_master_put(rspi->master);

	return 0;
}

static int rspi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct spi_master *master;
	struct rspi_data *rspi;
	int ret, irq, i, j;
	struct rspi_plat_data *rspi_pd;
	char clk_name[16];

	/* get base addr */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(res == NULL)) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	master = spi_alloc_master(&pdev->dev, sizeof(struct rspi_data));
	if (master == NULL) {
		dev_err(&pdev->dev, "spi_alloc_master error.\n");
		return -ENOMEM;
	}

	rspi = spi_master_get_devdata(master);
	dev_set_drvdata(&pdev->dev, rspi);

	rspi->master = master;
	rspi->addr = ioremap(res->start, resource_size(res));
	if (rspi->addr == NULL) {
		dev_err(&pdev->dev, "ioremap error.\n");
		ret = -ENOMEM;
		goto error1;
	}

	snprintf(clk_name, sizeof(clk_name), "rspi%d", pdev->id);
	rspi->clk = clk_get(&pdev->dev, clk_name);
	if (IS_ERR(rspi->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		ret = PTR_ERR(rspi->clk);
		goto error2;
	}
	clk_enable(rspi->clk);

	INIT_LIST_HEAD(&rspi->queue);
	spin_lock_init(&rspi->lock);
	INIT_WORK(&rspi->ws, rspi_work);
	init_waitqueue_head(&rspi->wait);

	master->num_chipselect = 2;
	master->bus_num = pdev->id;
	master->setup = rspi_setup;
	master->transfer = rspi_transfer;
	master->cleanup = rspi_cleanup;
	master->mode_bits = (SPI_CPHA | SPI_CPOL | SPI_LOOP);

	i = j = 0;
	while ((res = platform_get_resource(pdev, IORESOURCE_IRQ, i++))) {
		for (irq = res->start; irq <= res->end; irq++) {
			if (j >= NUM_IRQ) {
				dev_err(&pdev->dev, "irq resource is over\n");
				ret = -ENODEV;
				goto error3;
			}
			snprintf(drv_name[pdev->id][j], DRIVER_NAME_SIZE,
				 "%s.%d-%d", DRIVER_NAME, pdev->id, j);
			ret = request_irq(irq, rspi_irq, IRQF_DISABLED,
					  drv_name[pdev->id][j], rspi);
			if (ret < 0) {
				dev_err(&pdev->dev, "request_irq error\n");
				goto error3;
			}
			rspi->irq[j++] = irq;
		}
	}
	rspi->irqn = j;

	ret = rspi_setup_data_register_width(rspi, pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "rspi setup invalid data width.\n");
		goto error4;
	}

	ret = rspi_request_dma(rspi, pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "rspi_request_dma failed.\n");
		goto error4;
	}

	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "spi_register_master error.\n");
		goto error4;
	}

	rspi_pd = pdev->dev.platform_data;
	rspi->spcr = rspi_pd->spcr;
	rspi->txmode = rspi_pd->txmode;

	dev_info(&pdev->dev, "probed\n");

	return 0;

error4:
	rspi_release_dma(rspi);
error3:
	for (i = 0; i < j; i++)
		free_irq(rspi->irq[i], rspi);
	clk_put(rspi->clk);
error2:
	iounmap(rspi->addr);
error1:
	spi_master_put(master);

	return ret;
}

static struct platform_driver rspi_driver = {
	.probe =	rspi_probe,
	.remove =	rspi_remove,
	.driver		= {
		.name = "rspi",
		.owner	= THIS_MODULE,
	},
};
module_platform_driver(rspi_driver);

MODULE_DESCRIPTION("Renesas RSPI bus driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Yoshihiro Shimoda");
MODULE_ALIAS("platform:rspi");
