// SPDX-License-Identifier: GPL-2.0+
/*
 * PCIe host controller driver for NWL PCIe Bridge
 * Based on pcie-xilinx.c, pci-tegra.c
 *
 * (C) Copyright 2014 - 2015, Xilinx, Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip/irq-msi-lib.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/pci.h>
#include <linux/pci-ecam.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/irqchip/chained_irq.h>

#include "../pci.h"

/* Bridge core config registers */
#define BRCFG_PCIE_RX0			0x00000000
#define BRCFG_PCIE_RX1			0x00000004
#define BRCFG_INTERRUPT			0x00000010
#define BRCFG_PCIE_RX_MSG_FILTER	0x00000020

/* Egress - Bridge translation registers */
#define E_BREG_CAPABILITIES		0x00000200
#define E_BREG_CONTROL			0x00000208
#define E_BREG_BASE_LO			0x00000210
#define E_BREG_BASE_HI			0x00000214
#define E_ECAM_CAPABILITIES		0x00000220
#define E_ECAM_CONTROL			0x00000228
#define E_ECAM_BASE_LO			0x00000230
#define E_ECAM_BASE_HI			0x00000234

/* Ingress - address translations */
#define I_MSII_CAPABILITIES		0x00000300
#define I_MSII_CONTROL			0x00000308
#define I_MSII_BASE_LO			0x00000310
#define I_MSII_BASE_HI			0x00000314

#define I_ISUB_CONTROL			0x000003E8
#define SET_ISUB_CONTROL		BIT(0)
/* Rxed msg fifo  - Interrupt status registers */
#define MSGF_MISC_STATUS		0x00000400
#define MSGF_MISC_MASK			0x00000404
#define MSGF_LEG_STATUS			0x00000420
#define MSGF_LEG_MASK			0x00000424
#define MSGF_MSI_STATUS_LO		0x00000440
#define MSGF_MSI_STATUS_HI		0x00000444
#define MSGF_MSI_MASK_LO		0x00000448
#define MSGF_MSI_MASK_HI		0x0000044C

/* Msg filter mask bits */
#define CFG_ENABLE_PM_MSG_FWD		BIT(1)
#define CFG_ENABLE_INT_MSG_FWD		BIT(2)
#define CFG_ENABLE_ERR_MSG_FWD		BIT(3)
#define CFG_ENABLE_MSG_FILTER_MASK	(CFG_ENABLE_PM_MSG_FWD | \
					CFG_ENABLE_INT_MSG_FWD | \
					CFG_ENABLE_ERR_MSG_FWD)

/* Misc interrupt status mask bits */
#define MSGF_MISC_SR_RXMSG_AVAIL	BIT(0)
#define MSGF_MISC_SR_RXMSG_OVER		BIT(1)
#define MSGF_MISC_SR_SLAVE_ERR		BIT(4)
#define MSGF_MISC_SR_MASTER_ERR		BIT(5)
#define MSGF_MISC_SR_I_ADDR_ERR		BIT(6)
#define MSGF_MISC_SR_E_ADDR_ERR		BIT(7)
#define MSGF_MISC_SR_FATAL_AER		BIT(16)
#define MSGF_MISC_SR_NON_FATAL_AER	BIT(17)
#define MSGF_MISC_SR_CORR_AER		BIT(18)
#define MSGF_MISC_SR_UR_DETECT		BIT(20)
#define MSGF_MISC_SR_NON_FATAL_DEV	BIT(22)
#define MSGF_MISC_SR_FATAL_DEV		BIT(23)
#define MSGF_MISC_SR_LINK_DOWN		BIT(24)
#define MSGF_MISC_SR_LINK_AUTO_BWIDTH	BIT(25)
#define MSGF_MISC_SR_LINK_BWIDTH	BIT(26)

#define MSGF_MISC_SR_MASKALL		(MSGF_MISC_SR_RXMSG_AVAIL | \
					MSGF_MISC_SR_RXMSG_OVER | \
					MSGF_MISC_SR_SLAVE_ERR | \
					MSGF_MISC_SR_MASTER_ERR | \
					MSGF_MISC_SR_I_ADDR_ERR | \
					MSGF_MISC_SR_E_ADDR_ERR | \
					MSGF_MISC_SR_FATAL_AER | \
					MSGF_MISC_SR_NON_FATAL_AER | \
					MSGF_MISC_SR_CORR_AER | \
					MSGF_MISC_SR_UR_DETECT | \
					MSGF_MISC_SR_NON_FATAL_DEV | \
					MSGF_MISC_SR_FATAL_DEV | \
					MSGF_MISC_SR_LINK_DOWN | \
					MSGF_MISC_SR_LINK_AUTO_BWIDTH | \
					MSGF_MISC_SR_LINK_BWIDTH)

/* Legacy interrupt status mask bits */
#define MSGF_LEG_SR_INTA		BIT(0)
#define MSGF_LEG_SR_INTB		BIT(1)
#define MSGF_LEG_SR_INTC		BIT(2)
#define MSGF_LEG_SR_INTD		BIT(3)
#define MSGF_LEG_SR_MASKALL		(MSGF_LEG_SR_INTA | MSGF_LEG_SR_INTB | \
					MSGF_LEG_SR_INTC | MSGF_LEG_SR_INTD)

/* MSI interrupt status mask bits */
#define MSGF_MSI_SR_LO_MASK		GENMASK(31, 0)
#define MSGF_MSI_SR_HI_MASK		GENMASK(31, 0)

#define MSII_PRESENT			BIT(0)
#define MSII_ENABLE			BIT(0)
#define MSII_STATUS_ENABLE		BIT(15)

/* Bridge config interrupt mask */
#define BRCFG_INTERRUPT_MASK		BIT(0)
#define BREG_PRESENT			BIT(0)
#define BREG_ENABLE			BIT(0)
#define BREG_ENABLE_FORCE		BIT(1)

/* E_ECAM status mask bits */
#define E_ECAM_PRESENT			BIT(0)
#define E_ECAM_CR_ENABLE		BIT(0)
#define E_ECAM_SIZE_LOC			GENMASK(20, 16)
#define E_ECAM_SIZE_SHIFT		16
#define NWL_ECAM_MAX_SIZE		16

#define CFG_DMA_REG_BAR			GENMASK(2, 0)
#define CFG_PCIE_CACHE			GENMASK(7, 0)

#define INT_PCI_MSI_NR			(2 * 32)

/* Readin the PS_LINKUP */
#define PS_LINKUP_OFFSET		0x00000238
#define PCIE_PHY_LINKUP_BIT		BIT(0)
#define PHY_RDY_LINKUP_BIT		BIT(1)

/* Parameters for the waiting for link up routine */
#define LINK_WAIT_MAX_RETRIES          10
#define LINK_WAIT_USLEEP_MIN           90000
#define LINK_WAIT_USLEEP_MAX           100000

struct nwl_msi {			/* MSI information */
	DECLARE_BITMAP(bitmap, INT_PCI_MSI_NR);
	struct irq_domain *dev_domain;
	struct mutex lock;		/* protect bitmap variable */
	int irq_msi0;
	int irq_msi1;
};

struct nwl_pcie {
	struct device *dev;
	void __iomem *breg_base;
	void __iomem *pcireg_base;
	void __iomem *ecam_base;
	struct phy *phy[4];
	phys_addr_t phys_breg_base;	/* Physical Bridge Register Base */
	phys_addr_t phys_pcie_reg_base;	/* Physical PCIe Controller Base */
	phys_addr_t phys_ecam_base;	/* Physical Configuration Base */
	u32 breg_size;
	u32 pcie_reg_size;
	u32 ecam_size;
	int irq_intx;
	int irq_misc;
	struct nwl_msi msi;
	struct irq_domain *intx_irq_domain;
	struct clk *clk;
	raw_spinlock_t leg_mask_lock;
};

static inline u32 nwl_bridge_readl(struct nwl_pcie *pcie, u32 off)
{
	return readl(pcie->breg_base + off);
}

static inline void nwl_bridge_writel(struct nwl_pcie *pcie, u32 val, u32 off)
{
	writel(val, pcie->breg_base + off);
}

static bool nwl_pcie_link_up(struct nwl_pcie *pcie)
{
	if (readl(pcie->pcireg_base + PS_LINKUP_OFFSET) & PCIE_PHY_LINKUP_BIT)
		return true;
	return false;
}

static bool nwl_phy_link_up(struct nwl_pcie *pcie)
{
	if (readl(pcie->pcireg_base + PS_LINKUP_OFFSET) & PHY_RDY_LINKUP_BIT)
		return true;
	return false;
}

static int nwl_wait_for_link(struct nwl_pcie *pcie)
{
	struct device *dev = pcie->dev;
	int retries;

	/* check if the link is up or not */
	for (retries = 0; retries < LINK_WAIT_MAX_RETRIES; retries++) {
		if (nwl_phy_link_up(pcie))
			return 0;
		usleep_range(LINK_WAIT_USLEEP_MIN, LINK_WAIT_USLEEP_MAX);
	}

	dev_err(dev, "PHY link never came up\n");
	return -ETIMEDOUT;
}

static bool nwl_pcie_valid_device(struct pci_bus *bus, unsigned int devfn)
{
	struct nwl_pcie *pcie = bus->sysdata;

	/* Check link before accessing downstream ports */
	if (!pci_is_root_bus(bus)) {
		if (!nwl_pcie_link_up(pcie))
			return false;
	} else if (devfn > 0)
		/* Only one device down on each root port */
		return false;

	return true;
}

/**
 * nwl_pcie_map_bus - Get configuration base
 *
 * @bus: Bus structure of current bus
 * @devfn: Device/function
 * @where: Offset from base
 *
 * Return: Base address of the configuration space needed to be
 *	   accessed.
 */
static void __iomem *nwl_pcie_map_bus(struct pci_bus *bus, unsigned int devfn,
				      int where)
{
	struct nwl_pcie *pcie = bus->sysdata;

	if (!nwl_pcie_valid_device(bus, devfn))
		return NULL;

	return pcie->ecam_base + PCIE_ECAM_OFFSET(bus->number, devfn, where);
}

/* PCIe operations */
static struct pci_ops nwl_pcie_ops = {
	.map_bus = nwl_pcie_map_bus,
	.read  = pci_generic_config_read,
	.write = pci_generic_config_write,
};

static irqreturn_t nwl_pcie_misc_handler(int irq, void *data)
{
	struct nwl_pcie *pcie = data;
	struct device *dev = pcie->dev;
	u32 misc_stat;

	/* Checking for misc interrupts */
	misc_stat = nwl_bridge_readl(pcie, MSGF_MISC_STATUS) &
				     MSGF_MISC_SR_MASKALL;
	if (!misc_stat)
		return IRQ_NONE;

	if (misc_stat & MSGF_MISC_SR_RXMSG_OVER)
		dev_err_ratelimited(dev, "Received Message FIFO Overflow\n");

	if (misc_stat & MSGF_MISC_SR_SLAVE_ERR)
		dev_err_ratelimited(dev, "Slave error\n");

	if (misc_stat & MSGF_MISC_SR_MASTER_ERR)
		dev_err_ratelimited(dev, "Master error\n");

	if (misc_stat & MSGF_MISC_SR_I_ADDR_ERR)
		dev_err_ratelimited(dev, "In Misc Ingress address translation error\n");

	if (misc_stat & MSGF_MISC_SR_E_ADDR_ERR)
		dev_err_ratelimited(dev, "In Misc Egress address translation error\n");

	if (misc_stat & MSGF_MISC_SR_FATAL_AER)
		dev_err_ratelimited(dev, "Fatal Error in AER Capability\n");

	if (misc_stat & MSGF_MISC_SR_NON_FATAL_AER)
		dev_err_ratelimited(dev, "Non-Fatal Error in AER Capability\n");

	if (misc_stat & MSGF_MISC_SR_CORR_AER)
		dev_err_ratelimited(dev, "Correctable Error in AER Capability\n");

	if (misc_stat & MSGF_MISC_SR_UR_DETECT)
		dev_err_ratelimited(dev, "Unsupported request Detected\n");

	if (misc_stat & MSGF_MISC_SR_NON_FATAL_DEV)
		dev_err_ratelimited(dev, "Non-Fatal Error Detected\n");

	if (misc_stat & MSGF_MISC_SR_FATAL_DEV)
		dev_err_ratelimited(dev, "Fatal Error Detected\n");

	if (misc_stat & MSGF_MISC_SR_LINK_AUTO_BWIDTH)
		dev_info(dev, "Link Autonomous Bandwidth Management Status bit set\n");

	if (misc_stat & MSGF_MISC_SR_LINK_BWIDTH)
		dev_info(dev, "Link Bandwidth Management Status bit set\n");

	/* Clear misc interrupt status */
	nwl_bridge_writel(pcie, misc_stat, MSGF_MISC_STATUS);

	return IRQ_HANDLED;
}

static void nwl_pcie_leg_handler(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct nwl_pcie *pcie;
	unsigned long status;
	u32 bit;

	chained_irq_enter(chip, desc);
	pcie = irq_desc_get_handler_data(desc);

	while ((status = nwl_bridge_readl(pcie, MSGF_LEG_STATUS) &
				MSGF_LEG_SR_MASKALL) != 0) {
		for_each_set_bit(bit, &status, PCI_NUM_INTX)
			generic_handle_domain_irq(pcie->intx_irq_domain, bit);
	}

	chained_irq_exit(chip, desc);
}

static void nwl_pcie_handle_msi_irq(struct nwl_pcie *pcie, u32 status_reg)
{
	struct nwl_msi *msi = &pcie->msi;
	unsigned long status;
	u32 bit;

	while ((status = nwl_bridge_readl(pcie, status_reg)) != 0) {
		for_each_set_bit(bit, &status, 32) {
			nwl_bridge_writel(pcie, 1 << bit, status_reg);
			generic_handle_domain_irq(msi->dev_domain, bit);
		}
	}
}

static void nwl_pcie_msi_handler_high(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct nwl_pcie *pcie = irq_desc_get_handler_data(desc);

	chained_irq_enter(chip, desc);
	nwl_pcie_handle_msi_irq(pcie, MSGF_MSI_STATUS_HI);
	chained_irq_exit(chip, desc);
}

static void nwl_pcie_msi_handler_low(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct nwl_pcie *pcie = irq_desc_get_handler_data(desc);

	chained_irq_enter(chip, desc);
	nwl_pcie_handle_msi_irq(pcie, MSGF_MSI_STATUS_LO);
	chained_irq_exit(chip, desc);
}

static void nwl_mask_intx_irq(struct irq_data *data)
{
	struct nwl_pcie *pcie = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 mask;
	u32 val;

	mask = 1 << data->hwirq;
	raw_spin_lock_irqsave(&pcie->leg_mask_lock, flags);
	val = nwl_bridge_readl(pcie, MSGF_LEG_MASK);
	nwl_bridge_writel(pcie, (val & (~mask)), MSGF_LEG_MASK);
	raw_spin_unlock_irqrestore(&pcie->leg_mask_lock, flags);
}

static void nwl_unmask_intx_irq(struct irq_data *data)
{
	struct nwl_pcie *pcie = irq_data_get_irq_chip_data(data);
	unsigned long flags;
	u32 mask;
	u32 val;

	mask = 1 << data->hwirq;
	raw_spin_lock_irqsave(&pcie->leg_mask_lock, flags);
	val = nwl_bridge_readl(pcie, MSGF_LEG_MASK);
	nwl_bridge_writel(pcie, (val | mask), MSGF_LEG_MASK);
	raw_spin_unlock_irqrestore(&pcie->leg_mask_lock, flags);
}

static struct irq_chip nwl_intx_irq_chip = {
	.name = "nwl_pcie:legacy",
	.irq_enable = nwl_unmask_intx_irq,
	.irq_disable = nwl_mask_intx_irq,
	.irq_mask = nwl_mask_intx_irq,
	.irq_unmask = nwl_unmask_intx_irq,
};

static int nwl_intx_map(struct irq_domain *domain, unsigned int irq,
			irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &nwl_intx_irq_chip, handle_level_irq);
	irq_set_chip_data(irq, domain->host_data);
	irq_set_status_flags(irq, IRQ_LEVEL);

	return 0;
}

static const struct irq_domain_ops intx_domain_ops = {
	.map = nwl_intx_map,
	.xlate = pci_irqd_intx_xlate,
};

#ifdef CONFIG_PCI_MSI

#define NWL_MSI_FLAGS_REQUIRED (MSI_FLAG_USE_DEF_DOM_OPS	| \
				MSI_FLAG_USE_DEF_CHIP_OPS	| \
				MSI_FLAG_NO_AFFINITY)

#define NWL_MSI_FLAGS_SUPPORTED (MSI_GENERIC_FLAGS_MASK		| \
				 MSI_FLAG_MULTI_PCI_MSI)

static const struct msi_parent_ops nwl_msi_parent_ops = {
	.required_flags		= NWL_MSI_FLAGS_REQUIRED,
	.supported_flags	= NWL_MSI_FLAGS_SUPPORTED,
	.bus_select_token	= DOMAIN_BUS_PCI_MSI,
	.prefix			= "nwl-",
	.init_dev_msi_info	= msi_lib_init_dev_msi_info,
};

#endif

static void nwl_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct nwl_pcie *pcie = irq_data_get_irq_chip_data(data);
	phys_addr_t msi_addr = pcie->phys_pcie_reg_base;

	msg->address_lo = lower_32_bits(msi_addr);
	msg->address_hi = upper_32_bits(msi_addr);
	msg->data = data->hwirq;
}

static struct irq_chip nwl_irq_chip = {
	.name = "Xilinx MSI",
	.irq_compose_msi_msg = nwl_compose_msi_msg,
};

static int nwl_irq_domain_alloc(struct irq_domain *domain, unsigned int virq,
				unsigned int nr_irqs, void *args)
{
	struct nwl_pcie *pcie = domain->host_data;
	struct nwl_msi *msi = &pcie->msi;
	int bit;
	int i;

	mutex_lock(&msi->lock);
	bit = bitmap_find_free_region(msi->bitmap, INT_PCI_MSI_NR,
				      get_count_order(nr_irqs));
	if (bit < 0) {
		mutex_unlock(&msi->lock);
		return -ENOSPC;
	}

	for (i = 0; i < nr_irqs; i++) {
		irq_domain_set_info(domain, virq + i, bit + i, &nwl_irq_chip,
				    domain->host_data, handle_simple_irq,
				    NULL, NULL);
	}
	mutex_unlock(&msi->lock);
	return 0;
}

static void nwl_irq_domain_free(struct irq_domain *domain, unsigned int virq,
				unsigned int nr_irqs)
{
	struct irq_data *data = irq_domain_get_irq_data(domain, virq);
	struct nwl_pcie *pcie = irq_data_get_irq_chip_data(data);
	struct nwl_msi *msi = &pcie->msi;

	mutex_lock(&msi->lock);
	bitmap_release_region(msi->bitmap, data->hwirq,
			      get_count_order(nr_irqs));
	mutex_unlock(&msi->lock);
}

static const struct irq_domain_ops dev_msi_domain_ops = {
	.alloc  = nwl_irq_domain_alloc,
	.free   = nwl_irq_domain_free,
};

static int nwl_pcie_init_msi_irq_domain(struct nwl_pcie *pcie)
{
#ifdef CONFIG_PCI_MSI
	struct device *dev = pcie->dev;
	struct nwl_msi *msi = &pcie->msi;
	struct irq_domain_info info = {
		.fwnode		= dev_fwnode(dev),
		.ops		= &dev_msi_domain_ops,
		.host_data	= pcie,
		.size		= INT_PCI_MSI_NR,
	};

	msi->dev_domain  = msi_create_parent_irq_domain(&info, &nwl_msi_parent_ops);
	if (!msi->dev_domain) {
		dev_err(dev, "failed to create dev IRQ domain\n");
		return -ENOMEM;
	}
#endif
	return 0;
}

static void nwl_pcie_phy_power_off(struct nwl_pcie *pcie, int i)
{
	int err = phy_power_off(pcie->phy[i]);

	if (err)
		dev_err(pcie->dev, "could not power off phy %d (err=%d)\n", i,
			err);
}

static void nwl_pcie_phy_exit(struct nwl_pcie *pcie, int i)
{
	int err = phy_exit(pcie->phy[i]);

	if (err)
		dev_err(pcie->dev, "could not exit phy %d (err=%d)\n", i, err);
}

static int nwl_pcie_phy_enable(struct nwl_pcie *pcie)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(pcie->phy); i++) {
		ret = phy_init(pcie->phy[i]);
		if (ret)
			goto err;

		ret = phy_power_on(pcie->phy[i]);
		if (ret) {
			nwl_pcie_phy_exit(pcie, i);
			goto err;
		}
	}

	return 0;

err:
	while (i--) {
		nwl_pcie_phy_power_off(pcie, i);
		nwl_pcie_phy_exit(pcie, i);
	}

	return ret;
}

static void nwl_pcie_phy_disable(struct nwl_pcie *pcie)
{
	int i;

	for (i = ARRAY_SIZE(pcie->phy); i--;) {
		nwl_pcie_phy_power_off(pcie, i);
		nwl_pcie_phy_exit(pcie, i);
	}
}

static int nwl_pcie_init_irq_domain(struct nwl_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct device_node *node = dev->of_node;
	struct device_node *intc_node;

	intc_node = of_get_next_child(node, NULL);
	if (!intc_node) {
		dev_err(dev, "No legacy intc node found\n");
		return -EINVAL;
	}

	pcie->intx_irq_domain = irq_domain_create_linear(of_fwnode_handle(intc_node), PCI_NUM_INTX,
							 &intx_domain_ops, pcie);
	of_node_put(intc_node);
	if (!pcie->intx_irq_domain) {
		dev_err(dev, "failed to create IRQ domain\n");
		return -ENOMEM;
	}

	raw_spin_lock_init(&pcie->leg_mask_lock);
	nwl_pcie_init_msi_irq_domain(pcie);
	return 0;
}

static int nwl_pcie_enable_msi(struct nwl_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct nwl_msi *msi = &pcie->msi;
	unsigned long base;
	int ret;

	mutex_init(&msi->lock);

	/* Get msi_1 IRQ number */
	msi->irq_msi1 = platform_get_irq_byname(pdev, "msi1");
	if (msi->irq_msi1 < 0)
		return -EINVAL;

	irq_set_chained_handler_and_data(msi->irq_msi1,
					 nwl_pcie_msi_handler_high, pcie);

	/* Get msi_0 IRQ number */
	msi->irq_msi0 = platform_get_irq_byname(pdev, "msi0");
	if (msi->irq_msi0 < 0)
		return -EINVAL;

	irq_set_chained_handler_and_data(msi->irq_msi0,
					 nwl_pcie_msi_handler_low, pcie);

	/* Check for msii_present bit */
	ret = nwl_bridge_readl(pcie, I_MSII_CAPABILITIES) & MSII_PRESENT;
	if (!ret) {
		dev_err(dev, "MSI not present\n");
		return -EIO;
	}

	/* Enable MSII */
	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, I_MSII_CONTROL) |
			  MSII_ENABLE, I_MSII_CONTROL);

	/* Enable MSII status */
	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, I_MSII_CONTROL) |
			  MSII_STATUS_ENABLE, I_MSII_CONTROL);

	/* setup AFI/FPCI range */
	base = pcie->phys_pcie_reg_base;
	nwl_bridge_writel(pcie, lower_32_bits(base), I_MSII_BASE_LO);
	nwl_bridge_writel(pcie, upper_32_bits(base), I_MSII_BASE_HI);

	/*
	 * For high range MSI interrupts: disable, clear any pending,
	 * and enable
	 */
	nwl_bridge_writel(pcie, 0, MSGF_MSI_MASK_HI);

	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie,  MSGF_MSI_STATUS_HI) &
			  MSGF_MSI_SR_HI_MASK, MSGF_MSI_STATUS_HI);

	nwl_bridge_writel(pcie, MSGF_MSI_SR_HI_MASK, MSGF_MSI_MASK_HI);

	/*
	 * For low range MSI interrupts: disable, clear any pending,
	 * and enable
	 */
	nwl_bridge_writel(pcie, 0, MSGF_MSI_MASK_LO);

	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, MSGF_MSI_STATUS_LO) &
			  MSGF_MSI_SR_LO_MASK, MSGF_MSI_STATUS_LO);

	nwl_bridge_writel(pcie, MSGF_MSI_SR_LO_MASK, MSGF_MSI_MASK_LO);

	return 0;
}

static int nwl_pcie_bridge_init(struct nwl_pcie *pcie)
{
	struct device *dev = pcie->dev;
	struct platform_device *pdev = to_platform_device(dev);
	u32 breg_val, ecam_val;
	int err;

	breg_val = nwl_bridge_readl(pcie, E_BREG_CAPABILITIES) & BREG_PRESENT;
	if (!breg_val) {
		dev_err(dev, "BREG is not present\n");
		return breg_val;
	}

	/* Write bridge_off to breg base */
	nwl_bridge_writel(pcie, lower_32_bits(pcie->phys_breg_base),
			  E_BREG_BASE_LO);
	nwl_bridge_writel(pcie, upper_32_bits(pcie->phys_breg_base),
			  E_BREG_BASE_HI);

	/* Enable BREG */
	nwl_bridge_writel(pcie, ~BREG_ENABLE_FORCE & BREG_ENABLE,
			  E_BREG_CONTROL);

	/* Disable DMA channel registers */
	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, BRCFG_PCIE_RX0) |
			  CFG_DMA_REG_BAR, BRCFG_PCIE_RX0);

	/* Enable Ingress subtractive decode translation */
	nwl_bridge_writel(pcie, SET_ISUB_CONTROL, I_ISUB_CONTROL);

	/* Enable msg filtering details */
	nwl_bridge_writel(pcie, CFG_ENABLE_MSG_FILTER_MASK,
			  BRCFG_PCIE_RX_MSG_FILTER);

	/* This routes the PCIe DMA traffic to go through CCI path */
	if (of_dma_is_coherent(dev->of_node))
		nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, BRCFG_PCIE_RX1) |
				  CFG_PCIE_CACHE, BRCFG_PCIE_RX1);

	err = nwl_wait_for_link(pcie);
	if (err)
		return err;

	ecam_val = nwl_bridge_readl(pcie, E_ECAM_CAPABILITIES) & E_ECAM_PRESENT;
	if (!ecam_val) {
		dev_err(dev, "ECAM is not present\n");
		return ecam_val;
	}

	/* Enable ECAM */
	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, E_ECAM_CONTROL) |
			  E_ECAM_CR_ENABLE, E_ECAM_CONTROL);

	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, E_ECAM_CONTROL) |
			  (NWL_ECAM_MAX_SIZE << E_ECAM_SIZE_SHIFT),
			  E_ECAM_CONTROL);

	nwl_bridge_writel(pcie, lower_32_bits(pcie->phys_ecam_base),
			  E_ECAM_BASE_LO);
	nwl_bridge_writel(pcie, upper_32_bits(pcie->phys_ecam_base),
			  E_ECAM_BASE_HI);

	if (nwl_pcie_link_up(pcie))
		dev_info(dev, "Link is UP\n");
	else
		dev_info(dev, "Link is DOWN\n");

	/* Get misc IRQ number */
	pcie->irq_misc = platform_get_irq_byname(pdev, "misc");
	if (pcie->irq_misc < 0)
		return -EINVAL;

	err = devm_request_irq(dev, pcie->irq_misc,
			       nwl_pcie_misc_handler, IRQF_SHARED,
			       "nwl_pcie:misc", pcie);
	if (err) {
		dev_err(dev, "fail to register misc IRQ#%d\n",
			pcie->irq_misc);
		return err;
	}

	/* Disable all misc interrupts */
	nwl_bridge_writel(pcie, (u32)~MSGF_MISC_SR_MASKALL, MSGF_MISC_MASK);

	/* Clear pending misc interrupts */
	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, MSGF_MISC_STATUS) &
			  MSGF_MISC_SR_MASKALL, MSGF_MISC_STATUS);

	/* Enable all misc interrupts */
	nwl_bridge_writel(pcie, MSGF_MISC_SR_MASKALL, MSGF_MISC_MASK);

	/* Disable all INTX interrupts */
	nwl_bridge_writel(pcie, (u32)~MSGF_LEG_SR_MASKALL, MSGF_LEG_MASK);

	/* Clear pending INTX interrupts */
	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, MSGF_LEG_STATUS) &
			  MSGF_LEG_SR_MASKALL, MSGF_LEG_STATUS);

	/* Enable all INTX interrupts */
	nwl_bridge_writel(pcie, MSGF_LEG_SR_MASKALL, MSGF_LEG_MASK);

	/* Enable the bridge config interrupt */
	nwl_bridge_writel(pcie, nwl_bridge_readl(pcie, BRCFG_INTERRUPT) |
			  BRCFG_INTERRUPT_MASK, BRCFG_INTERRUPT);

	return 0;
}

static int nwl_pcie_parse_dt(struct nwl_pcie *pcie,
			     struct platform_device *pdev)
{
	struct device *dev = pcie->dev;
	struct resource *res;
	int i;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "breg");
	pcie->breg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pcie->breg_base))
		return PTR_ERR(pcie->breg_base);
	pcie->phys_breg_base = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcireg");
	pcie->pcireg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pcie->pcireg_base))
		return PTR_ERR(pcie->pcireg_base);
	pcie->phys_pcie_reg_base = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg");
	pcie->ecam_base = devm_pci_remap_cfg_resource(dev, res);
	if (IS_ERR(pcie->ecam_base))
		return PTR_ERR(pcie->ecam_base);
	pcie->phys_ecam_base = res->start;

	/* Get intx IRQ number */
	pcie->irq_intx = platform_get_irq_byname(pdev, "intx");
	if (pcie->irq_intx < 0)
		return pcie->irq_intx;

	irq_set_chained_handler_and_data(pcie->irq_intx,
					 nwl_pcie_leg_handler, pcie);


	for (i = 0; i < ARRAY_SIZE(pcie->phy); i++) {
		pcie->phy[i] = devm_of_phy_get_by_index(dev, dev->of_node, i);
		if (PTR_ERR(pcie->phy[i]) == -ENODEV) {
			pcie->phy[i] = NULL;
			break;
		}

		if (IS_ERR(pcie->phy[i]))
			return PTR_ERR(pcie->phy[i]);
	}

	return 0;
}

static const struct of_device_id nwl_pcie_of_match[] = {
	{ .compatible = "xlnx,nwl-pcie-2.11", },
	{}
};

static int nwl_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nwl_pcie *pcie;
	struct pci_host_bridge *bridge;
	int err;

	bridge = devm_pci_alloc_host_bridge(dev, sizeof(*pcie));
	if (!bridge)
		return -ENODEV;

	pcie = pci_host_bridge_priv(bridge);
	platform_set_drvdata(pdev, pcie);

	pcie->dev = dev;

	err = nwl_pcie_parse_dt(pcie, pdev);
	if (err) {
		dev_err(dev, "Parsing DT failed\n");
		return err;
	}

	pcie->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(pcie->clk))
		return PTR_ERR(pcie->clk);

	err = clk_prepare_enable(pcie->clk);
	if (err) {
		dev_err(dev, "can't enable PCIe ref clock\n");
		return err;
	}

	err = nwl_pcie_phy_enable(pcie);
	if (err) {
		dev_err(dev, "could not enable PHYs\n");
		goto err_clk;
	}

	err = nwl_pcie_bridge_init(pcie);
	if (err) {
		dev_err(dev, "HW Initialization failed\n");
		goto err_phy;
	}

	err = nwl_pcie_init_irq_domain(pcie);
	if (err) {
		dev_err(dev, "Failed creating IRQ Domain\n");
		goto err_phy;
	}

	bridge->sysdata = pcie;
	bridge->ops = &nwl_pcie_ops;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		err = nwl_pcie_enable_msi(pcie);
		if (err < 0) {
			dev_err(dev, "failed to enable MSI support: %d\n", err);
			goto err_phy;
		}
	}

	err = pci_host_probe(bridge);
	if (!err)
		return 0;

err_phy:
	nwl_pcie_phy_disable(pcie);
err_clk:
	clk_disable_unprepare(pcie->clk);
	return err;
}

static void nwl_pcie_remove(struct platform_device *pdev)
{
	struct nwl_pcie *pcie = platform_get_drvdata(pdev);

	nwl_pcie_phy_disable(pcie);
	clk_disable_unprepare(pcie->clk);
}

static struct platform_driver nwl_pcie_driver = {
	.driver = {
		.name = "nwl-pcie",
		.suppress_bind_attrs = true,
		.of_match_table = nwl_pcie_of_match,
	},
	.probe = nwl_pcie_probe,
	.remove = nwl_pcie_remove,
};
builtin_platform_driver(nwl_pcie_driver);
