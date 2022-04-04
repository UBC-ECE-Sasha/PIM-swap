/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/pci.h>

#include <dpu_fpga_kc705_device.h>
#include <dpu_fpga_kc705_register.h>

#include <dpu_region.h>

static ssize_t reset_ila_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "0\n");
}

static ssize_t reset_ila_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t len)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);
	struct dpu_region *region = rank->region;
	struct pci_device_fpga *pdev = region->addr_translate.private;
	uint32_t off = CONTROL_INTERFACE_BAR_OFFSET + DPU_ANALYZER_OFFSET +
		       ILA_RESET_OFFSET;
	uint64_t tmp = 0xFFFFFFFFFFFFFFFFULL;

	dpu_device_write_register(&tmp, off, pdev, region);

	return len;
}

static ssize_t activate_ila_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", rank->region->activate_ila);
}

static ssize_t activate_ila_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);
	struct dpu_region *region = rank->region;
	struct pci_device_fpga *pdev = region->addr_translate.private;
	uint64_t tmp;
	int ret;

	ret = kstrtou64(buf, 10, &tmp);
	if (ret)
		return ret;

	if (tmp != 0 && tmp != 1)
		return -EINVAL;

	if (tmp != region->activate_ila) {
		uint32_t off = CONTROL_INTERFACE_BAR_OFFSET +
			       DPU_ANALYZER_OFFSET + ILA_TRIGGER_OFFSET;

		tmp = ((tmp << TRIGGER_ENABLE_SHIFT) |
		       (ISTATE_TRIGGER_SELECTION << TRIGGER_SELECTION_SHIFT) |
		       (ISTATE_BOOT_STATE_VALUE << TRIGGER_VALUE_SHIFT));

		dpu_device_write_register(&tmp, off, pdev, region);

		region->activate_ila = (uint8_t)tmp;
	}

	return len;
}

static ssize_t activate_filtering_ila_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", rank->region->activate_filtering_ila);
}

static ssize_t activate_filtering_ila_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t len)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);
	struct dpu_region *region = rank->region;
	struct pci_device_fpga *pdev = region->addr_translate.private;
	uint64_t tmp;
	int ret;

	ret = kstrtou64(buf, 10, &tmp);
	if (ret)
		return ret;

	if (tmp != 0 && tmp != 1)
		return -EINVAL;

	if (tmp != region->activate_filtering_ila) {
		uint32_t off = CONTROL_INTERFACE_BAR_OFFSET +
			       DPU_ANALYZER_OFFSET + ILA_FILTER_OFFSET;

		dpu_device_write_register(&tmp, off, pdev, region);

		region->activate_filtering_ila = (uint8_t)tmp;
	}

	return len;
}

static ssize_t activate_mram_bypass_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", rank->region->activate_mram_bypass);
}

static ssize_t activate_mram_bypass_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t len)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);
	struct dpu_region *region = rank->region;
	struct pci_device_fpga *pdev = region->addr_translate.private;
	uint64_t tmp;
	int ret;

	ret = kstrtou64(buf, 10, &tmp);
	if (ret)
		return ret;

	if (tmp != 0 && tmp != 1)
		return -EINVAL;

	if (tmp != region->activate_mram_bypass) {
		uint32_t off = CONTROL_INTERFACE_BAR_OFFSET +
			       DPU_ANALYZER_OFFSET + MRAM_BYPASS_OFFSET;

		dpu_device_write_register(&tmp, off, pdev, region);

		region->activate_mram_bypass = (uint8_t)tmp;
	}

	return len;
}

static ssize_t mram_refresh_emulation_period_show(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       rank->region->mram_refresh_emulation_period);
}

static ssize_t
mram_refresh_emulation_period_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);
	struct dpu_region *region = rank->region;
	struct pci_device_fpga *pdev = region->addr_translate.private;
	uint64_t tmp, enabled;
	int ret;

	ret = kstrtou64(buf, 10, &tmp);
	if (ret)
		return ret;

	if (tmp != region->mram_refresh_emulation_period) {
		uint32_t off = CONTROL_INTERFACE_BAR_OFFSET +
			       DPU_ANALYZER_OFFSET + MRAM_EMUL_REFRESH_OFFSET;

		enabled = tmp != 0 ? 1ULL : 0ULL;
		tmp = (enabled << 63) | (tmp & 0x7FFFFFFFULL);

		dpu_device_write_register(&tmp, off, pdev, region);

		region->mram_refresh_emulation_period = (uint32_t)tmp;
	}

	return len;
}

static ssize_t inject_faults_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}

static ssize_t inject_faults_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);
	struct dpu_region *region = rank->region;
	struct pci_device_fpga *pdev = region->addr_translate.private;
	uint32_t off = CONTROL_INTERFACE_BAR_OFFSET + DPU_ANALYZER_OFFSET;
	// Iram
	uint64_t addr0 = 0;
	uint64_t mask0 = 0x0000FFFFFFFFFFFFL;
	uint64_t data0 = 0x0000000000000000L;
	uint64_t addr1 = 0;
	uint64_t mask1 = 0x0000FFFFFFFFFFFFL;
	uint64_t data1 = 0x0000000000000000L;
	uint64_t addr2 = 0;
	uint64_t mask2 = 0x0000FFFFFFFFFFFFL;
	uint64_t data2 = 0x0000000000000000L;
	uint64_t addr3 = 0;
	uint64_t mask3 = 0x0000FFFFFFFFFFFFL;
	uint64_t data3 = 0x0000000000000000L;

	dpu_device_write_register(&addr0, off + 0x280, pdev, region);
	dpu_device_write_register(&mask0, off + 0x288, pdev, region);
	dpu_device_write_register(&data0, off + 0x290, pdev, region);
	dpu_device_write_register(&addr1, off + 0x298, pdev, region);
	dpu_device_write_register(&mask1, off + 0x2A0, pdev, region);
	dpu_device_write_register(&data1, off + 0x2A8, pdev, region);
	dpu_device_write_register(&addr2, off + 0x2B0, pdev, region);
	dpu_device_write_register(&mask2, off + 0x2B8, pdev, region);
	dpu_device_write_register(&data2, off + 0x2C0, pdev, region);
	dpu_device_write_register(&addr3, off + 0x2C8, pdev, region);
	dpu_device_write_register(&mask3, off + 0x2D0, pdev, region);
	dpu_device_write_register(&data3, off + 0x2D8, pdev, region);

	// Wram Bank 0
	addr0 = 0x0;
	mask0 = 0x00000000FFFFFFFFL;
	data0 = 0x0000000000000000L;
	addr1 = 0x0;
	mask1 = 0x00000000FFFFFFFFL;
	data1 = 0x0000000000000000L;
	addr2 = 0x0;
	mask2 = 0x00000000FFFFFFFFL;
	data2 = 0x0000000000000000L;
	addr3 = 0x0;
	mask3 = 0x00000000FFFFFFFFL;
	data3 = 0x0000000000000000L;

	dpu_device_write_register(&addr0, off + 0x080, pdev, region);
	dpu_device_write_register(&mask0, off + 0x088, pdev, region);
	dpu_device_write_register(&data0, off + 0x090, pdev, region);
	dpu_device_write_register(&addr1, off + 0x098, pdev, region);
	dpu_device_write_register(&mask1, off + 0x0A0, pdev, region);
	dpu_device_write_register(&data1, off + 0x0A8, pdev, region);
	dpu_device_write_register(&addr2, off + 0x0B0, pdev, region);
	dpu_device_write_register(&mask2, off + 0x0B8, pdev, region);
	dpu_device_write_register(&data2, off + 0x0C0, pdev, region);
	dpu_device_write_register(&addr3, off + 0x0C8, pdev, region);
	dpu_device_write_register(&mask3, off + 0x0D0, pdev, region);
	dpu_device_write_register(&data3, off + 0x0D8, pdev, region);

	// Wram Bank 1
	addr0 = 0x0;
	mask0 = 0x00000000FFFFFFFFL;
	data0 = 0x0000000000000000L;
	addr1 = 0x0;
	mask1 = 0x00000000FFFFFFFFL;
	data1 = 0x0000000000000000L;
	addr2 = 0x0;
	mask2 = 0x00000000FFFFFFFFL;
	data2 = 0x0000000000000000L;
	addr3 = 0x0;
	mask3 = 0x00000000FFFFFFFFL;
	data3 = 0x0000000000000000L;

	dpu_device_write_register(&addr0, off + 0x100, pdev, region);
	dpu_device_write_register(&mask0, off + 0x108, pdev, region);
	dpu_device_write_register(&data0, off + 0x110, pdev, region);
	dpu_device_write_register(&addr1, off + 0x118, pdev, region);
	dpu_device_write_register(&mask1, off + 0x120, pdev, region);
	dpu_device_write_register(&data1, off + 0x128, pdev, region);
	dpu_device_write_register(&addr2, off + 0x130, pdev, region);
	dpu_device_write_register(&mask2, off + 0x138, pdev, region);
	dpu_device_write_register(&data2, off + 0x140, pdev, region);
	dpu_device_write_register(&addr3, off + 0x148, pdev, region);
	dpu_device_write_register(&mask3, off + 0x150, pdev, region);
	dpu_device_write_register(&data3, off + 0x158, pdev, region);

	// Wram Bank 2
	addr0 = 0x0;
	mask0 = 0x00000000FFFFFFFFL;
	data0 = 0x0000000000000000L;
	addr1 = 0x0;
	mask1 = 0x00000000FFFFFFFFL;
	data1 = 0x0000000000000000L;
	addr2 = 0x0;
	mask2 = 0x00000000FFFFFFFFL;
	data2 = 0x0000000000000000L;
	addr3 = 0x0;
	mask3 = 0x00000000FFFFFFFFL;
	data3 = 0x0000000000000000L;

	dpu_device_write_register(&addr0, off + 0x180, pdev, region);
	dpu_device_write_register(&mask0, off + 0x188, pdev, region);
	dpu_device_write_register(&data0, off + 0x190, pdev, region);
	dpu_device_write_register(&addr1, off + 0x198, pdev, region);
	dpu_device_write_register(&mask1, off + 0x1A0, pdev, region);
	dpu_device_write_register(&data1, off + 0x1A8, pdev, region);
	dpu_device_write_register(&addr2, off + 0x1B0, pdev, region);
	dpu_device_write_register(&mask2, off + 0x1B8, pdev, region);
	dpu_device_write_register(&data2, off + 0x1C0, pdev, region);
	dpu_device_write_register(&addr3, off + 0x1C8, pdev, region);
	dpu_device_write_register(&mask3, off + 0x1D0, pdev, region);
	dpu_device_write_register(&data3, off + 0x1D8, pdev, region);

	// Wram Bank 3
	addr0 = 0x0;
	mask0 = 0x00000000FFFFFFFFL;
	data0 = 0x0000000000000000L;
	addr1 = 0x0;
	mask1 = 0x00000000FFFFFFFFL;
	data1 = 0x0000000000000000L;
	addr2 = 0x0;
	mask2 = 0x00000000FFFFFFFFL;
	data2 = 0x0000000000000000L;
	addr3 = 0x0;
	mask3 = 0x00000000FFFFFFFFL;
	data3 = 0x0000000000000000L;

	dpu_device_write_register(&addr0, off + 0x200, pdev, region);
	dpu_device_write_register(&mask0, off + 0x208, pdev, region);
	dpu_device_write_register(&data0, off + 0x210, pdev, region);
	dpu_device_write_register(&addr1, off + 0x218, pdev, region);
	dpu_device_write_register(&mask1, off + 0x220, pdev, region);
	dpu_device_write_register(&data1, off + 0x228, pdev, region);
	dpu_device_write_register(&addr2, off + 0x230, pdev, region);
	dpu_device_write_register(&mask2, off + 0x238, pdev, region);
	dpu_device_write_register(&data2, off + 0x240, pdev, region);
	dpu_device_write_register(&addr3, off + 0x248, pdev, region);
	dpu_device_write_register(&mask3, off + 0x250, pdev, region);
	dpu_device_write_register(&data3, off + 0x258, pdev, region);

	return len;
}

static ssize_t spi_mode_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", rank->region->spi_mode_enabled);
}

static ssize_t spi_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t len)
{
	struct dpu_rank_t *rank = dev_get_drvdata(dev);
	struct dpu_region *region = rank->region;
	struct pci_device_fpga *pdev = region->addr_translate.private;
	int ret;
	uint8_t tmp;

	ret = kstrtou8(buf, 10, &tmp);
	if (ret)
		return ret;

	if (tmp && !region->spi_mode_enabled) {
		dpu_spi_reset(pdev->banks[DPU_SPI_BANK_NUMBER].addr);
	} else if (!tmp && region->spi_mode_enabled) {
		uint8_t i;

		for (i = 0; i < 9; ++i)
			dpu_spi_write_read_byte(
				pdev->banks[DPU_SPI_BANK_NUMBER].addr, 0);
	}

	region->spi_mode_enabled = tmp;

	return len;
}

#define ILADUMP_SIZE (ILA_FIFO_DEPTH * ILA_OUTPUT_ENTRY_LENGTH)

static ssize_t iladump_show(struct file *filp, char __user *buf, size_t sz,
			    loff_t *loff)
{
	struct pci_device_fpga *pdev = filp->f_inode->i_private;
	uint32_t off = CONTROL_INTERFACE_BAR_OFFSET + DPU_ANALYZER_OFFSET;
	uint32_t timeout = 1000000;
	u64 temp = 0;
	bool fifo_empty = false;
	bool fifo_full = false;
	u8 *kbuffer;
	u8 *current_kbuffer;
	uint8_t thread_id;
	uint16_t thread_pc;
	uint8_t thread_cf;
	uint8_t thread_zf;
	uint8_t wram_bank_again;
	uint8_t thread_istate;
	uint16_t thread_rstate;
	uint64_t thread_resx;
	uint64_t thread_instr;
	uint32_t nb_of_entries = 0;

	kbuffer = vmalloc((ILADUMP_SIZE + 1) * sizeof(uint8_t));
	if (kbuffer == NULL)
		return -ENOMEM;

	current_kbuffer = kbuffer;

	while (!fifo_full && ((timeout--) != 0)) {
		read_register64(&temp, off + 0x10, pdev);
		fifo_full = (temp & 1) != 0;
	}

	if (timeout == 0) {
		vfree(kbuffer);
		return -EFAULT;
	}

	temp = 0L;

	while (!fifo_empty &&
	       ((current_kbuffer + 79) <= (kbuffer + ILADUMP_SIZE))) {
		read_register64(&temp, off + 0x40, pdev);
		thread_id = (uint8_t)(temp & 0x1F);
		thread_pc = (uint16_t)((temp >> 8) & 0xFFF);
		thread_cf = (uint8_t)((temp >> 24) & 0x1);
		thread_zf = (uint8_t)((temp >> 25) & 0x1);
		wram_bank_again = (uint8_t)((temp >> 26) & 0x1);
		thread_istate = (uint8_t)((temp >> 32) & 0x3L);
		thread_rstate = (uint16_t)((temp >> 48) & 0xFF);

		read_register64(&thread_resx, off + 0x50, pdev);
		read_register64(&thread_instr, off + 0x60, pdev);

		snprintf(current_kbuffer, 80,
			 "0x%02x, 0x%04x, 0x%01x, 0x%01x, "
			 "0x%01x, 0x%01x, 0x%02x, 0x%16llx, 0x%16llx\n",
			 thread_id, thread_pc, thread_cf, thread_zf,
			 wram_bank_again, thread_istate, thread_rstate,
			 thread_resx, thread_instr);
		current_kbuffer = current_kbuffer + 79;

		read_register64(&temp, off + 0x8, pdev);
		fifo_empty = (temp & 1) != 0;
		nb_of_entries++;
	}

	if (copy_to_user(buf, kbuffer, ILADUMP_SIZE)) {
		vfree(kbuffer);
		return -EFAULT;
	}

	vfree(kbuffer);

	return nb_of_entries;
}

static DEVICE_ATTR_RW(spi_mode);

static struct attribute *fpga_kc705_spi_attrs[] = {
	&dev_attr_spi_mode.attr,
	NULL,
};

static const struct attribute_group fpga_kc705_spi_group = {
	.attrs = fpga_kc705_spi_attrs,
};

static const struct file_operations iladump_fops = {
	.owner = THIS_MODULE,
	.read = iladump_show,
};

static DEVICE_ATTR_RW(reset_ila);
static DEVICE_ATTR_RW(activate_ila);
static DEVICE_ATTR_RW(activate_filtering_ila);
static DEVICE_ATTR_RW(activate_mram_bypass);
static DEVICE_ATTR_RW(mram_refresh_emulation_period);
static DEVICE_ATTR_RW(inject_faults);

static struct attribute *fpga_kc705_ila_attrs[] = {
	&dev_attr_reset_ila.attr,
	&dev_attr_activate_ila.attr,
	&dev_attr_activate_filtering_ila.attr,
	&dev_attr_activate_mram_bypass.attr,
	&dev_attr_mram_refresh_emulation_period.attr,
	&dev_attr_inject_faults.attr,
	NULL,
};

static const struct attribute_group fpga_kc705_ila_group = {
	.attrs = fpga_kc705_ila_attrs,
};

int dpu_debugfs_sysfs_fpga_kc705_create(struct pci_device_fpga *pdev,
					struct dpu_region *region)
{
	struct pci_dev *pci_dev = pdev->dev;
	/* sysfs/debugfs attributes are associated with the rank */
	struct dpu_rank_t *rank = &region->rank;
	struct device *dev = &rank->dev;
	char debugfs_rank[16];
	int ret;

	if (pci_dev->subsystem_device & SPI_ADDON) {
		ret = sysfs_create_group(&dev->kobj, &fpga_kc705_spi_group);
		if (ret)
			return ret;
	}

	if (pci_dev->subsystem_device & ILA_ADDON) {
		ret = sysfs_create_group(&dev->kobj, &fpga_kc705_ila_group);
		if (ret)
			goto free_spi_group;

		sprintf(debugfs_rank, "dpu_rank%d", region->rank.id);
		region->dpu_debugfs = debugfs_create_dir(debugfs_rank, NULL);
		if (IS_ERR_OR_NULL(region->dpu_debugfs)) {
			ret = -EINVAL;
			goto error_debugfs_dir;
		}

		region->iladump = debugfs_create_file("iladump", S_IRUGO,
						      region->dpu_debugfs, pdev,
						      &iladump_fops);
		if (IS_ERR_OR_NULL(region->iladump)) {
			ret = -EINVAL;
			goto error_debugfs_file;
		}
	}

	return 0;

error_debugfs_file:
	if (pci_dev->subsystem_device & ILA_ADDON)
		debugfs_remove(region->dpu_debugfs);
error_debugfs_dir:
	if (pci_dev->subsystem_device & ILA_ADDON)
		sysfs_remove_group(&dev->kobj, &fpga_kc705_ila_group);
free_spi_group:
	if (pci_dev->subsystem_device & SPI_ADDON)
		sysfs_remove_group(&dev->kobj, &fpga_kc705_spi_group);

	return ret;
}

int dpu_debugfs_sysfs_fpga_kc705_destroy(struct pci_device_fpga *pdev,
					 struct dpu_region *region)
{
	struct pci_dev *pci_dev = pdev->dev;
	struct dpu_rank_t *rank = &region->rank;
	struct device *dev = &rank->dev;

	if (pci_dev->subsystem_device & SPI_ADDON) {
		sysfs_remove_group(&dev->kobj, &fpga_kc705_spi_group);
	}

	if (pci_dev->subsystem_device & ILA_ADDON) {
		sysfs_remove_group(&dev->kobj, &fpga_kc705_ila_group);
		debugfs_remove_recursive(region->dpu_debugfs);
	}

	return 0;
}
