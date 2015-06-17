/*
 * RZA1 RAA730300 Driver
 *
 * Copyright (C) 2013 Renesas Solutions Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

#include <uapi/mfd/raa730300.h>

#define DRIVER_NAME "raa730300"
#define DEV_NAME "sma"

static DEFINE_MUTEX(raa730300_mutex);

struct raa730300_data {
	struct spi_device	*spi;
	struct cdev		cdev;
	dev_t			dev;
	int			offset;
	struct class		*sm_class;
	struct device		*sm_device;
};


static int raa730300_reg_write(struct spi_device *spi, char reg, char dat)
{
	char cmd[2];

	if (reg == 0xa || reg < 0 || reg > 0x17)
		return -1;

	cmd[1] = dat;
	cmd[0] = ((1 << 7) | reg);

	return spi_write_then_read(spi, cmd, 2, NULL, 0);
}

static int raa730300_reg_read(struct spi_device *spi, char reg, char *dat)
{
	if (reg == 0xa || reg < 0 || reg > 0x17)
		return -1;
	return spi_write_then_read(spi, &reg, 1, dat, 1);
}

/* fops */
int raa730300_open(struct inode *inode, struct file *file)
{
	struct raa730300_data *chip =
		container_of(inode->i_cdev, struct raa730300_data, cdev);

	file->private_data = chip;
	return 0;
}

static long raa730300_unlocked_ioctl(struct file *file, unsigned int cmd,
				     unsigned long arg)
{
	int i, ret, size, err;
	struct sa_regs *sa;
	struct raa730300_data *chip = file->private_data;

	ret = -EINVAL;
	err = copy_from_user((void *)&size, (void __user *)arg, sizeof(int));
	if (err)
		return -EFAULT;
	size = sizeof(struct sa_regs);

	sa = kmalloc(size, GFP_KERNEL);
	if (!sa)
		return -ENOMEM;

	err = copy_from_user(sa, (void __user *)arg, size);
	if (err) {
		err = -EFAULT;
		goto err;
	}

	mutex_lock(&raa730300_mutex);
	switch (cmd) {
	case RAA730300_IOCTL_REG_READ:
		for (i = 0; i < sa->num; i++) {
			ret = raa730300_reg_read(chip->spi,
						  sa->regs[i].addr,
						  &sa->regs[i].value);
			if (ret < 0)
				break;
		}
		err = copy_to_user((void __user *)arg, sa, sa->num
				* sizeof(struct sa_reg)
				+ sizeof(struct sa_regs));
		if (err)
			return -EFAULT;
		break;

	case RAA730300_IOCTL_REG_WRITE:
		for (i = 0; i < sa->num; i++) {
			ret = raa730300_reg_write(chip->spi,
						  sa->regs[i].addr,
						  sa->regs[i].value);
			if (ret < 0)
				break;
		}
		break;
	default:
		break;
	}
	mutex_unlock(&raa730300_mutex);

err:
	kfree(sa);
	return ret;
};

static ssize_t raa730300_read(struct file *file, char __user *ubuf,
			      size_t cnt, loff_t *ppos)
{
	int ret, bufsize;
	char *pbuf, buf[RAA730300_NUM_OF_REGS * 2];
	loff_t len, idx, nread;
	struct raa730300_data *chip = file->private_data;
	bufsize = RAA730300_NUM_OF_REGS * 2;

	if (cnt < bufsize)
		return -EIO;

	idx = *ppos / 2;
	if (idx < 0xa)
		chip->offset = 0;

	pbuf = buf;
	len = idx + cnt / 2;

	mutex_lock(&raa730300_mutex);
	while (idx < len && idx < RAA730300_NUM_OF_REGS) {
		/* The address 0xa register doesn't exist. */
		if (idx >= 0xa && !chip->offset)
			chip->offset = 1;

		*pbuf = idx + chip->offset;
		ret = raa730300_reg_read(chip->spi, idx + chip->offset,
					pbuf + 1);
		if (ret < 0)
			break;
		udelay(5);
		idx++;
		pbuf += 2;
	}
	idx *= 2;
	nread = idx - *ppos;
	ret = copy_to_user(ubuf, buf, nread);
	if (ret)
		return -EFAULT;
	*ppos = idx;
	mutex_unlock(&raa730300_mutex);

	return nread;
}

static ssize_t raa730300_write(struct file *file, const char __user *ubuf,
			       size_t cnt, loff_t *ppos)
{
	int ret, bufsize;
	char *pbuf, buf[RAA730300_NUM_OF_REGS * 2];
	loff_t len, idx, nwrite;
	struct raa730300_data *chip = file->private_data;

	idx = *ppos;
	if (cnt % 2) /* must write [addr] + [reg data] */
		return -EINVAL;

	pbuf = buf;
	bufsize = RAA730300_NUM_OF_REGS * 2;
	if (cnt > bufsize)
		cnt = bufsize;
	ret = copy_from_user(buf, ubuf, cnt);
	if (ret)
		return -EFAULT;
	len = idx + cnt;

	mutex_lock(&raa730300_mutex);
	while (idx < len && idx < bufsize) {
		ret = raa730300_reg_write(chip->spi, *pbuf, *(pbuf + 1));
		if (ret < 0)
			break;
		idx += 2;
		pbuf += 2;
	}
	nwrite = idx - *ppos;
	*ppos = idx;
	mutex_unlock(&raa730300_mutex);

	return nwrite;
}

static const struct file_operations raa730300_fops = {
	.owner = THIS_MODULE,
	.open = raa730300_open,
	.unlocked_ioctl = raa730300_unlocked_ioctl,
	.read = raa730300_read,
	.write = raa730300_write,
};

static int raa730300_probe(struct spi_device *spi)
{
	struct sman_platform_data *pd;
	struct raa730300_data *chip;
	dev_t dev;
	int ret = -EINVAL;

	pd = spi->dev.platform_data;
	if (!pd) {
		dev_err(&spi->dev, "no platform data\n");
		return ret;
	}

	chip = devm_kzalloc(&spi->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->spi = spi;
	chip->offset = 0;

	spi_set_drvdata(spi, chip);

	ret = alloc_chrdev_region(&dev, 0, 1, DRIVER_NAME);
	if (ret < 0) {
		dev_err(&spi->dev, "can't alloc chrdev_region.\n");
		goto error1;
	}
	chip->dev = dev;
	cdev_init(&chip->cdev, &raa730300_fops);
	ret = cdev_add(&chip->cdev, MKDEV(MAJOR(dev), 0), 1);
	if (ret < 0) {
		dev_err(&spi->dev, "failed to cdev_add.\n");
		goto error2;
	}

	chip->sm_class = class_create(THIS_MODULE, DEV_NAME);
	chip->sm_device = device_create(chip->sm_class, NULL, dev, NULL,
					DEV_NAME);
	if (IS_ERR(chip->sm_device))
		goto error3;

	return 0;

error3:
	class_destroy(chip->sm_class);
error2:
	unregister_chrdev_region(chip->dev, 1);
error1:
	devm_kfree(&spi->dev, chip);
	return ret;
}

static int raa730300_remove(struct spi_device *spi)
{
	struct raa730300_data *chip;
	chip = spi_get_drvdata(spi);

	device_destroy(chip->sm_class, chip->dev);
	class_destroy(chip->sm_class);
	cdev_del(&chip->cdev);
	unregister_chrdev_region(chip->dev, 1);
	devm_kfree(&spi->dev, chip);
	return 0;
}

static struct spi_driver raa730300_driver = {
	.driver = {
		.name	= "raa730300",
		.owner	= THIS_MODULE,
	},
	.probe		= raa730300_probe,
	.remove		= raa730300_remove,
};

module_spi_driver(raa730300_driver);
