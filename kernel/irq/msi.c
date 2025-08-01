// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014 Intel Corp.
 * Author: Jiang Liu <jiang.liu@linux.intel.com>
 *
 * This file is licensed under GPLv2.
 *
 * This file contains common code to support Message Signaled Interrupts for
 * PCI compatible and non PCI compatible devices.
 */
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/msi.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/xarray.h>

#include "internals.h"

/**
 * struct msi_device_data - MSI per device data
 * @properties:		MSI properties which are interesting to drivers
 * @mutex:		Mutex protecting the MSI descriptor store
 * @__domains:		Internal data for per device MSI domains
 * @__iter_idx:		Index to search the next entry for iterators
 */
struct msi_device_data {
	unsigned long			properties;
	struct mutex			mutex;
	struct msi_dev_domain		__domains[MSI_MAX_DEVICE_IRQDOMAINS];
	unsigned long			__iter_idx;
};

/**
 * struct msi_ctrl - MSI internal management control structure
 * @domid:	ID of the domain on which management operations should be done
 * @first:	First (hardware) slot index to operate on
 * @last:	Last (hardware) slot index to operate on
 * @nirqs:	The number of Linux interrupts to allocate. Can be larger
 *		than the range due to PCI/multi-MSI.
 */
struct msi_ctrl {
	unsigned int			domid;
	unsigned int			first;
	unsigned int			last;
	unsigned int			nirqs;
};

/* Invalid Xarray index which is outside of any searchable range */
#define MSI_XA_MAX_INDEX	(ULONG_MAX - 1)
/* The maximum domain size */
#define MSI_XA_DOMAIN_SIZE	(MSI_MAX_INDEX + 1)

static void msi_domain_free_locked(struct device *dev, struct msi_ctrl *ctrl);
static unsigned int msi_domain_get_hwsize(struct device *dev, unsigned int domid);
static inline int msi_sysfs_create_group(struct device *dev);
static int msi_domain_prepare_irqs(struct irq_domain *domain, struct device *dev,
				   int nvec, msi_alloc_info_t *arg);

/**
 * msi_alloc_desc - Allocate an initialized msi_desc
 * @dev:	Pointer to the device for which this is allocated
 * @nvec:	The number of vectors used in this entry
 * @affinity:	Optional pointer to an affinity mask array size of @nvec
 *
 * If @affinity is not %NULL then an affinity array[@nvec] is allocated
 * and the affinity masks and flags from @affinity are copied.
 *
 * Return: pointer to allocated &msi_desc on success or %NULL on failure
 */
static struct msi_desc *msi_alloc_desc(struct device *dev, int nvec,
				       const struct irq_affinity_desc *affinity)
{
	struct msi_desc *desc = kzalloc(sizeof(*desc), GFP_KERNEL);

	if (!desc)
		return NULL;

	desc->dev = dev;
	desc->nvec_used = nvec;
	if (affinity) {
		desc->affinity = kmemdup_array(affinity, nvec, sizeof(*desc->affinity), GFP_KERNEL);
		if (!desc->affinity) {
			kfree(desc);
			return NULL;
		}
	}
	return desc;
}

static void msi_free_desc(struct msi_desc *desc)
{
	kfree(desc->affinity);
	kfree(desc);
}

static int msi_insert_desc(struct device *dev, struct msi_desc *desc,
			   unsigned int domid, unsigned int index)
{
	struct msi_device_data *md = dev->msi.data;
	struct xarray *xa = &md->__domains[domid].store;
	unsigned int hwsize;
	int ret;

	hwsize = msi_domain_get_hwsize(dev, domid);

	if (index == MSI_ANY_INDEX) {
		struct xa_limit limit = { .min = 0, .max = hwsize - 1 };
		unsigned int index;

		/* Let the xarray allocate a free index within the limit */
		ret = xa_alloc(xa, &index, desc, limit, GFP_KERNEL);
		if (ret)
			goto fail;

		desc->msi_index = index;
		return 0;
	} else {
		if (index >= hwsize) {
			ret = -ERANGE;
			goto fail;
		}

		desc->msi_index = index;
		ret = xa_insert(xa, index, desc, GFP_KERNEL);
		if (ret)
			goto fail;
		return 0;
	}
fail:
	msi_free_desc(desc);
	return ret;
}

/**
 * msi_domain_insert_msi_desc - Allocate and initialize a MSI descriptor and
 *				insert it at @init_desc->msi_index
 *
 * @dev:	Pointer to the device for which the descriptor is allocated
 * @domid:	The id of the interrupt domain to which the desriptor is added
 * @init_desc:	Pointer to an MSI descriptor to initialize the new descriptor
 *
 * Return: 0 on success or an appropriate failure code.
 */
int msi_domain_insert_msi_desc(struct device *dev, unsigned int domid,
			       struct msi_desc *init_desc)
{
	struct msi_desc *desc;

	lockdep_assert_held(&dev->msi.data->mutex);

	desc = msi_alloc_desc(dev, init_desc->nvec_used, init_desc->affinity);
	if (!desc)
		return -ENOMEM;

	/* Copy type specific data to the new descriptor. */
	desc->pci = init_desc->pci;

	return msi_insert_desc(dev, desc, domid, init_desc->msi_index);
}

static bool msi_desc_match(struct msi_desc *desc, enum msi_desc_filter filter)
{
	switch (filter) {
	case MSI_DESC_ALL:
		return true;
	case MSI_DESC_NOTASSOCIATED:
		return !desc->irq;
	case MSI_DESC_ASSOCIATED:
		return !!desc->irq;
	}
	WARN_ON_ONCE(1);
	return false;
}

static bool msi_ctrl_valid(struct device *dev, struct msi_ctrl *ctrl)
{
	unsigned int hwsize;

	if (WARN_ON_ONCE(ctrl->domid >= MSI_MAX_DEVICE_IRQDOMAINS ||
			 (dev->msi.domain &&
			  !dev->msi.data->__domains[ctrl->domid].domain)))
		return false;

	hwsize = msi_domain_get_hwsize(dev, ctrl->domid);
	if (WARN_ON_ONCE(ctrl->first > ctrl->last ||
			 ctrl->first >= hwsize ||
			 ctrl->last >= hwsize))
		return false;
	return true;
}

static void msi_domain_free_descs(struct device *dev, struct msi_ctrl *ctrl)
{
	struct msi_desc *desc;
	struct xarray *xa;
	unsigned long idx;

	lockdep_assert_held(&dev->msi.data->mutex);

	if (!msi_ctrl_valid(dev, ctrl))
		return;

	xa = &dev->msi.data->__domains[ctrl->domid].store;
	xa_for_each_range(xa, idx, desc, ctrl->first, ctrl->last) {
		xa_erase(xa, idx);

		/* Leak the descriptor when it is still referenced */
		if (WARN_ON_ONCE(msi_desc_match(desc, MSI_DESC_ASSOCIATED)))
			continue;
		msi_free_desc(desc);
	}
}

/**
 * msi_domain_free_msi_descs_range - Free a range of MSI descriptors of a device in an irqdomain
 * @dev:	Device for which to free the descriptors
 * @domid:	Id of the domain to operate on
 * @first:	Index to start freeing from (inclusive)
 * @last:	Last index to be freed (inclusive)
 */
void msi_domain_free_msi_descs_range(struct device *dev, unsigned int domid,
				     unsigned int first, unsigned int last)
{
	struct msi_ctrl ctrl = {
		.domid	= domid,
		.first	= first,
		.last	= last,
	};

	msi_domain_free_descs(dev, &ctrl);
}

/**
 * msi_domain_add_simple_msi_descs - Allocate and initialize MSI descriptors
 * @dev:	Pointer to the device for which the descriptors are allocated
 * @ctrl:	Allocation control struct
 *
 * Return: 0 on success or an appropriate failure code.
 */
static int msi_domain_add_simple_msi_descs(struct device *dev, struct msi_ctrl *ctrl)
{
	struct msi_desc *desc;
	unsigned int idx;
	int ret;

	lockdep_assert_held(&dev->msi.data->mutex);

	if (!msi_ctrl_valid(dev, ctrl))
		return -EINVAL;

	for (idx = ctrl->first; idx <= ctrl->last; idx++) {
		desc = msi_alloc_desc(dev, 1, NULL);
		if (!desc)
			goto fail_mem;
		ret = msi_insert_desc(dev, desc, ctrl->domid, idx);
		if (ret)
			goto fail;
	}
	return 0;

fail_mem:
	ret = -ENOMEM;
fail:
	msi_domain_free_descs(dev, ctrl);
	return ret;
}

void __get_cached_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	*msg = entry->msg;
}

void get_cached_msi_msg(unsigned int irq, struct msi_msg *msg)
{
	struct msi_desc *entry = irq_get_msi_desc(irq);

	__get_cached_msi_msg(entry, msg);
}
EXPORT_SYMBOL_GPL(get_cached_msi_msg);

static void msi_device_data_release(struct device *dev, void *res)
{
	struct msi_device_data *md = res;
	int i;

	for (i = 0; i < MSI_MAX_DEVICE_IRQDOMAINS; i++) {
		msi_remove_device_irq_domain(dev, i);
		WARN_ON_ONCE(!xa_empty(&md->__domains[i].store));
		xa_destroy(&md->__domains[i].store);
	}
	dev->msi.data = NULL;
}

/**
 * msi_setup_device_data - Setup MSI device data
 * @dev:	Device for which MSI device data should be set up
 *
 * Return: 0 on success, appropriate error code otherwise
 *
 * This can be called more than once for @dev. If the MSI device data is
 * already allocated the call succeeds. The allocated memory is
 * automatically released when the device is destroyed.
 */
int msi_setup_device_data(struct device *dev)
{
	struct msi_device_data *md;
	int ret, i;

	if (dev->msi.data)
		return 0;

	md = devres_alloc(msi_device_data_release, sizeof(*md), GFP_KERNEL);
	if (!md)
		return -ENOMEM;

	ret = msi_sysfs_create_group(dev);
	if (ret) {
		devres_free(md);
		return ret;
	}

	for (i = 0; i < MSI_MAX_DEVICE_IRQDOMAINS; i++)
		xa_init_flags(&md->__domains[i].store, XA_FLAGS_ALLOC);

	/*
	 * If @dev::msi::domain is set and is a global MSI domain, copy the
	 * pointer into the domain array so all code can operate on domain
	 * ids. The NULL pointer check is required to keep the legacy
	 * architecture specific PCI/MSI support working.
	 */
	if (dev->msi.domain && !irq_domain_is_msi_parent(dev->msi.domain))
		md->__domains[MSI_DEFAULT_DOMAIN].domain = dev->msi.domain;

	mutex_init(&md->mutex);
	dev->msi.data = md;
	devres_add(dev, md);
	return 0;
}

/**
 * __msi_lock_descs - Lock the MSI descriptor storage of a device
 * @dev:	Device to operate on
 *
 * Internal function for guard(msi_descs_lock). Don't use in code.
 */
void __msi_lock_descs(struct device *dev)
{
	mutex_lock(&dev->msi.data->mutex);
}
EXPORT_SYMBOL_GPL(__msi_lock_descs);

/**
 * __msi_unlock_descs - Unlock the MSI descriptor storage of a device
 * @dev:	Device to operate on
 *
 * Internal function for guard(msi_descs_lock). Don't use in code.
 */
void __msi_unlock_descs(struct device *dev)
{
	/* Invalidate the index which was cached by the iterator */
	dev->msi.data->__iter_idx = MSI_XA_MAX_INDEX;
	mutex_unlock(&dev->msi.data->mutex);
}
EXPORT_SYMBOL_GPL(__msi_unlock_descs);

static struct msi_desc *msi_find_desc(struct msi_device_data *md, unsigned int domid,
				      enum msi_desc_filter filter)
{
	struct xarray *xa = &md->__domains[domid].store;
	struct msi_desc *desc;

	xa_for_each_start(xa, md->__iter_idx, desc, md->__iter_idx) {
		if (msi_desc_match(desc, filter))
			return desc;
	}
	md->__iter_idx = MSI_XA_MAX_INDEX;
	return NULL;
}

/**
 * msi_domain_first_desc - Get the first MSI descriptor of an irqdomain associated to a device
 * @dev:	Device to operate on
 * @domid:	The id of the interrupt domain which should be walked.
 * @filter:	Descriptor state filter
 *
 * Must be called with the MSI descriptor mutex held, i.e. msi_lock_descs()
 * must be invoked before the call.
 *
 * Return: Pointer to the first MSI descriptor matching the search
 *	   criteria, NULL if none found.
 */
struct msi_desc *msi_domain_first_desc(struct device *dev, unsigned int domid,
				       enum msi_desc_filter filter)
{
	struct msi_device_data *md = dev->msi.data;

	if (WARN_ON_ONCE(!md || domid >= MSI_MAX_DEVICE_IRQDOMAINS))
		return NULL;

	lockdep_assert_held(&md->mutex);

	md->__iter_idx = 0;
	return msi_find_desc(md, domid, filter);
}
EXPORT_SYMBOL_GPL(msi_domain_first_desc);

/**
 * msi_next_desc - Get the next MSI descriptor of a device
 * @dev:	Device to operate on
 * @domid:	The id of the interrupt domain which should be walked.
 * @filter:	Descriptor state filter
 *
 * The first invocation of msi_next_desc() has to be preceeded by a
 * successful invocation of __msi_first_desc(). Consecutive invocations are
 * only valid if the previous one was successful. All these operations have
 * to be done within the same MSI mutex held region.
 *
 * Return: Pointer to the next MSI descriptor matching the search
 *	   criteria, NULL if none found.
 */
struct msi_desc *msi_next_desc(struct device *dev, unsigned int domid,
			       enum msi_desc_filter filter)
{
	struct msi_device_data *md = dev->msi.data;

	if (WARN_ON_ONCE(!md || domid >= MSI_MAX_DEVICE_IRQDOMAINS))
		return NULL;

	lockdep_assert_held(&md->mutex);

	if (md->__iter_idx >= (unsigned long)MSI_MAX_INDEX)
		return NULL;

	md->__iter_idx++;
	return msi_find_desc(md, domid, filter);
}
EXPORT_SYMBOL_GPL(msi_next_desc);

/**
 * msi_domain_get_virq - Lookup the Linux interrupt number for a MSI index on a interrupt domain
 * @dev:	Device to operate on
 * @domid:	Domain ID of the interrupt domain associated to the device
 * @index:	MSI interrupt index to look for (0-based)
 *
 * Return: The Linux interrupt number on success (> 0), 0 if not found
 */
unsigned int msi_domain_get_virq(struct device *dev, unsigned int domid, unsigned int index)
{
	struct msi_desc *desc;
	bool pcimsi = false;
	struct xarray *xa;

	if (!dev->msi.data)
		return 0;

	if (WARN_ON_ONCE(index > MSI_MAX_INDEX || domid >= MSI_MAX_DEVICE_IRQDOMAINS))
		return 0;

	/* This check is only valid for the PCI default MSI domain */
	if (dev_is_pci(dev) && domid == MSI_DEFAULT_DOMAIN)
		pcimsi = to_pci_dev(dev)->msi_enabled;

	guard(msi_descs_lock)(dev);
	xa = &dev->msi.data->__domains[domid].store;
	desc = xa_load(xa, pcimsi ? 0 : index);
	if (desc && desc->irq) {
		/*
		 * PCI-MSI has only one descriptor for multiple interrupts.
		 * PCI-MSIX and platform MSI use a descriptor per
		 * interrupt.
		 */
		if (!pcimsi)
			return desc->irq;
		if (index < desc->nvec_used)
			return desc->irq + index;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(msi_domain_get_virq);

#ifdef CONFIG_SYSFS
static struct attribute *msi_dev_attrs[] = {
	NULL
};

static const struct attribute_group msi_irqs_group = {
	.name	= "msi_irqs",
	.attrs	= msi_dev_attrs,
};

static inline int msi_sysfs_create_group(struct device *dev)
{
	return devm_device_add_group(dev, &msi_irqs_group);
}

static ssize_t msi_mode_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	/* MSI vs. MSIX is per device not per interrupt */
	bool is_msix = dev_is_pci(dev) ? to_pci_dev(dev)->msix_enabled : false;

	return sysfs_emit(buf, "%s\n", is_msix ? "msix" : "msi");
}

static void msi_sysfs_remove_desc(struct device *dev, struct msi_desc *desc)
{
	struct device_attribute *attrs = desc->sysfs_attrs;
	int i;

	if (!attrs)
		return;

	desc->sysfs_attrs = NULL;
	for (i = 0; i < desc->nvec_used; i++) {
		if (attrs[i].show)
			sysfs_remove_file_from_group(&dev->kobj, &attrs[i].attr, msi_irqs_group.name);
		kfree(attrs[i].attr.name);
	}
	kfree(attrs);
}

static int msi_sysfs_populate_desc(struct device *dev, struct msi_desc *desc)
{
	struct device_attribute *attrs;
	int ret, i;

	attrs = kcalloc(desc->nvec_used, sizeof(*attrs), GFP_KERNEL);
	if (!attrs)
		return -ENOMEM;

	desc->sysfs_attrs = attrs;
	for (i = 0; i < desc->nvec_used; i++) {
		sysfs_attr_init(&attrs[i].attr);
		attrs[i].attr.name = kasprintf(GFP_KERNEL, "%d", desc->irq + i);
		if (!attrs[i].attr.name) {
			ret = -ENOMEM;
			goto fail;
		}

		attrs[i].attr.mode = 0444;
		attrs[i].show = msi_mode_show;

		ret = sysfs_add_file_to_group(&dev->kobj, &attrs[i].attr, msi_irqs_group.name);
		if (ret) {
			attrs[i].show = NULL;
			goto fail;
		}
	}
	return 0;

fail:
	msi_sysfs_remove_desc(dev, desc);
	return ret;
}

#if defined(CONFIG_PCI_MSI_ARCH_FALLBACKS) || defined(CONFIG_PCI_XEN)
/**
 * msi_device_populate_sysfs - Populate msi_irqs sysfs entries for a device
 * @dev:	The device (PCI, platform etc) which will get sysfs entries
 */
int msi_device_populate_sysfs(struct device *dev)
{
	struct msi_desc *desc;
	int ret;

	msi_for_each_desc(desc, dev, MSI_DESC_ASSOCIATED) {
		if (desc->sysfs_attrs)
			continue;
		ret = msi_sysfs_populate_desc(dev, desc);
		if (ret)
			return ret;
	}
	return 0;
}

/**
 * msi_device_destroy_sysfs - Destroy msi_irqs sysfs entries for a device
 * @dev:		The device (PCI, platform etc) for which to remove
 *			sysfs entries
 */
void msi_device_destroy_sysfs(struct device *dev)
{
	struct msi_desc *desc;

	msi_for_each_desc(desc, dev, MSI_DESC_ALL)
		msi_sysfs_remove_desc(dev, desc);
}
#endif /* CONFIG_PCI_MSI_ARCH_FALLBACK || CONFIG_PCI_XEN */
#else /* CONFIG_SYSFS */
static inline int msi_sysfs_create_group(struct device *dev) { return 0; }
static inline int msi_sysfs_populate_desc(struct device *dev, struct msi_desc *desc) { return 0; }
static inline void msi_sysfs_remove_desc(struct device *dev, struct msi_desc *desc) { }
#endif /* !CONFIG_SYSFS */

static struct irq_domain *msi_get_device_domain(struct device *dev, unsigned int domid)
{
	struct irq_domain *domain;

	lockdep_assert_held(&dev->msi.data->mutex);

	if (WARN_ON_ONCE(domid >= MSI_MAX_DEVICE_IRQDOMAINS))
		return NULL;

	domain = dev->msi.data->__domains[domid].domain;
	if (!domain)
		return NULL;

	if (WARN_ON_ONCE(irq_domain_is_msi_parent(domain)))
		return NULL;

	return domain;
}

static unsigned int msi_domain_get_hwsize(struct device *dev, unsigned int domid)
{
	struct msi_domain_info *info;
	struct irq_domain *domain;

	domain = msi_get_device_domain(dev, domid);
	if (domain) {
		info = domain->host_data;
		return info->hwsize;
	}
	/* No domain, default to MSI_XA_DOMAIN_SIZE */
	return MSI_XA_DOMAIN_SIZE;
}

static inline void irq_chip_write_msi_msg(struct irq_data *data,
					  struct msi_msg *msg)
{
	data->chip->irq_write_msi_msg(data, msg);
}

static void msi_check_level(struct irq_domain *domain, struct msi_msg *msg)
{
	struct msi_domain_info *info = domain->host_data;

	/*
	 * If the MSI provider has messed with the second message and
	 * not advertized that it is level-capable, signal the breakage.
	 */
	WARN_ON(!((info->flags & MSI_FLAG_LEVEL_CAPABLE) &&
		  (info->chip->flags & IRQCHIP_SUPPORTS_LEVEL_MSI)) &&
		(msg[1].address_lo || msg[1].address_hi || msg[1].data));
}

/**
 * msi_domain_set_affinity - Generic affinity setter function for MSI domains
 * @irq_data:	The irq data associated to the interrupt
 * @mask:	The affinity mask to set
 * @force:	Flag to enforce setting (disable online checks)
 *
 * Intended to be used by MSI interrupt controllers which are
 * implemented with hierarchical domains.
 *
 * Return: IRQ_SET_MASK_* result code
 */
int msi_domain_set_affinity(struct irq_data *irq_data,
			    const struct cpumask *mask, bool force)
{
	struct irq_data *parent = irq_data->parent_data;
	struct msi_msg msg[2] = { [1] = { }, };
	int ret;

	ret = parent->chip->irq_set_affinity(parent, mask, force);
	if (ret >= 0 && ret != IRQ_SET_MASK_OK_DONE) {
		BUG_ON(irq_chip_compose_msi_msg(irq_data, msg));
		msi_check_level(irq_data->domain, msg);
		irq_chip_write_msi_msg(irq_data, msg);
	}

	return ret;
}

static int msi_domain_activate(struct irq_domain *domain,
			       struct irq_data *irq_data, bool early)
{
	struct msi_msg msg[2] = { [1] = { }, };

	BUG_ON(irq_chip_compose_msi_msg(irq_data, msg));
	msi_check_level(irq_data->domain, msg);
	irq_chip_write_msi_msg(irq_data, msg);
	return 0;
}

static void msi_domain_deactivate(struct irq_domain *domain,
				  struct irq_data *irq_data)
{
	struct msi_msg msg[2];

	memset(msg, 0, sizeof(msg));
	irq_chip_write_msi_msg(irq_data, msg);
}

static int msi_domain_alloc(struct irq_domain *domain, unsigned int virq,
			    unsigned int nr_irqs, void *arg)
{
	struct msi_domain_info *info = domain->host_data;
	struct msi_domain_ops *ops = info->ops;
	irq_hw_number_t hwirq = ops->get_hwirq(info, arg);
	int i, ret;

	if (irq_find_mapping(domain, hwirq) > 0)
		return -EEXIST;

	if (domain->parent) {
		ret = irq_domain_alloc_irqs_parent(domain, virq, nr_irqs, arg);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < nr_irqs; i++) {
		ret = ops->msi_init(domain, info, virq + i, hwirq + i, arg);
		if (ret < 0) {
			if (ops->msi_free) {
				for (i--; i >= 0; i--)
					ops->msi_free(domain, info, virq + i);
			}
			irq_domain_free_irqs_top(domain, virq, nr_irqs);
			return ret;
		}
	}

	return 0;
}

static void msi_domain_free(struct irq_domain *domain, unsigned int virq,
			    unsigned int nr_irqs)
{
	struct msi_domain_info *info = domain->host_data;
	int i;

	if (info->ops->msi_free) {
		for (i = 0; i < nr_irqs; i++)
			info->ops->msi_free(domain, info, virq + i);
	}
	irq_domain_free_irqs_top(domain, virq, nr_irqs);
}

static int msi_domain_translate(struct irq_domain *domain, struct irq_fwspec *fwspec,
				irq_hw_number_t *hwirq, unsigned int *type)
{
	struct msi_domain_info *info = domain->host_data;

	/*
	 * This will catch allocations through the regular irqdomain path except
	 * for MSI domains which really support this, e.g. MBIGEN.
	 */
	if (!info->ops->msi_translate)
		return -ENOTSUPP;
	return info->ops->msi_translate(domain, fwspec, hwirq, type);
}

#ifdef CONFIG_GENERIC_IRQ_DEBUGFS
static void msi_domain_debug_show(struct seq_file *m, struct irq_domain *d,
				  struct irq_data *irqd, int ind)
{
	struct msi_desc *desc = irqd ? irq_data_get_msi_desc(irqd) : NULL;

	if (!desc)
		return;

	seq_printf(m, "\n%*saddress_hi: 0x%08x", ind + 1, "", desc->msg.address_hi);
	seq_printf(m, "\n%*saddress_lo: 0x%08x", ind + 1, "", desc->msg.address_lo);
	seq_printf(m, "\n%*smsg_data:   0x%08x\n", ind + 1, "", desc->msg.data);
}
#endif

static const struct irq_domain_ops msi_domain_ops = {
	.alloc		= msi_domain_alloc,
	.free		= msi_domain_free,
	.activate	= msi_domain_activate,
	.deactivate	= msi_domain_deactivate,
	.translate	= msi_domain_translate,
#ifdef CONFIG_GENERIC_IRQ_DEBUGFS
	.debug_show     = msi_domain_debug_show,
#endif
};

static irq_hw_number_t msi_domain_ops_get_hwirq(struct msi_domain_info *info,
						msi_alloc_info_t *arg)
{
	return arg->hwirq;
}

static int msi_domain_ops_prepare(struct irq_domain *domain, struct device *dev,
				  int nvec, msi_alloc_info_t *arg)
{
	memset(arg, 0, sizeof(*arg));
	return 0;
}

static void msi_domain_ops_teardown(struct irq_domain *domain, msi_alloc_info_t *arg)
{
}

static void msi_domain_ops_set_desc(msi_alloc_info_t *arg,
				    struct msi_desc *desc)
{
	arg->desc = desc;
}

static int msi_domain_ops_init(struct irq_domain *domain,
			       struct msi_domain_info *info,
			       unsigned int virq, irq_hw_number_t hwirq,
			       msi_alloc_info_t *arg)
{
	irq_domain_set_hwirq_and_chip(domain, virq, hwirq, info->chip,
				      info->chip_data);
	if (info->handler && info->handler_name) {
		__irq_set_handler(virq, info->handler, 0, info->handler_name);
		if (info->handler_data)
			irq_set_handler_data(virq, info->handler_data);
	}
	return 0;
}

static struct msi_domain_ops msi_domain_ops_default = {
	.get_hwirq		= msi_domain_ops_get_hwirq,
	.msi_init		= msi_domain_ops_init,
	.msi_prepare		= msi_domain_ops_prepare,
	.msi_teardown		= msi_domain_ops_teardown,
	.set_desc		= msi_domain_ops_set_desc,
};

static void msi_domain_update_dom_ops(struct msi_domain_info *info)
{
	struct msi_domain_ops *ops = info->ops;

	if (ops == NULL) {
		info->ops = &msi_domain_ops_default;
		return;
	}

	if (!(info->flags & MSI_FLAG_USE_DEF_DOM_OPS))
		return;

	if (ops->get_hwirq == NULL)
		ops->get_hwirq = msi_domain_ops_default.get_hwirq;
	if (ops->msi_init == NULL)
		ops->msi_init = msi_domain_ops_default.msi_init;
	if (ops->msi_prepare == NULL)
		ops->msi_prepare = msi_domain_ops_default.msi_prepare;
	if (ops->msi_teardown == NULL)
		ops->msi_teardown = msi_domain_ops_default.msi_teardown;
	if (ops->set_desc == NULL)
		ops->set_desc = msi_domain_ops_default.set_desc;
}

static void msi_domain_update_chip_ops(struct msi_domain_info *info)
{
	struct irq_chip *chip = info->chip;

	BUG_ON(!chip || !chip->irq_mask || !chip->irq_unmask);
	if (!chip->irq_set_affinity && !(info->flags & MSI_FLAG_NO_AFFINITY))
		chip->irq_set_affinity = msi_domain_set_affinity;
}

static struct irq_domain *__msi_create_irq_domain(struct fwnode_handle *fwnode,
						  struct msi_domain_info *info,
						  unsigned int flags,
						  struct irq_domain *parent)
{
	struct irq_domain *domain;

	if (info->hwsize > MSI_XA_DOMAIN_SIZE)
		return NULL;

	/*
	 * Hardware size 0 is valid for backwards compatibility and for
	 * domains which are not backed by a hardware table. Grant the
	 * maximum index space.
	 */
	if (!info->hwsize)
		info->hwsize = MSI_XA_DOMAIN_SIZE;

	msi_domain_update_dom_ops(info);
	if (info->flags & MSI_FLAG_USE_DEF_CHIP_OPS)
		msi_domain_update_chip_ops(info);

	domain = irq_domain_create_hierarchy(parent, flags | IRQ_DOMAIN_FLAG_MSI, 0,
					     fwnode, &msi_domain_ops, info);

	if (domain) {
		irq_domain_update_bus_token(domain, info->bus_token);
		domain->dev = info->dev;
		if (info->flags & MSI_FLAG_PARENT_PM_DEV)
			domain->pm_dev = parent->pm_dev;
	}

	return domain;
}

/**
 * msi_create_irq_domain - Create an MSI interrupt domain
 * @fwnode:	Optional fwnode of the interrupt controller
 * @info:	MSI domain info
 * @parent:	Parent irq domain
 *
 * Return: pointer to the created &struct irq_domain or %NULL on failure
 */
struct irq_domain *msi_create_irq_domain(struct fwnode_handle *fwnode,
					 struct msi_domain_info *info,
					 struct irq_domain *parent)
{
	return __msi_create_irq_domain(fwnode, info, 0, parent);
}

/**
 * msi_create_parent_irq_domain - Create an MSI-parent interrupt domain
 * @info:		MSI irqdomain creation info
 * @msi_parent_ops:	MSI parent callbacks and configuration
 *
 * Return: pointer to the created &struct irq_domain or %NULL on failure
 */
struct irq_domain *msi_create_parent_irq_domain(struct irq_domain_info *info,
						const struct msi_parent_ops *msi_parent_ops)
{
	struct irq_domain *d;

	info->hwirq_max		= max(info->hwirq_max, info->size);
	info->size		= info->hwirq_max;
	info->domain_flags	|= IRQ_DOMAIN_FLAG_MSI_PARENT;
	info->bus_token		= msi_parent_ops->bus_select_token;

	d = irq_domain_instantiate(info);
	if (IS_ERR(d))
		return NULL;

	d->msi_parent_ops = msi_parent_ops;
	return d;
}
EXPORT_SYMBOL_GPL(msi_create_parent_irq_domain);

/**
 * msi_parent_init_dev_msi_info - Delegate initialization of device MSI info down
 *				  in the domain hierarchy
 * @dev:		The device for which the domain should be created
 * @domain:		The domain in the hierarchy this op is being called on
 * @msi_parent_domain:	The IRQ_DOMAIN_FLAG_MSI_PARENT domain for the child to
 *			be created
 * @msi_child_info:	The MSI domain info of the IRQ_DOMAIN_FLAG_MSI_DEVICE
 *			domain to be created
 *
 * Return: true on success, false otherwise
 *
 * This is the most complex problem of per device MSI domains and the
 * underlying interrupt domain hierarchy:
 *
 * The device domain to be initialized requests the broadest feature set
 * possible and the underlying domain hierarchy puts restrictions on it.
 *
 * That's trivial for a simple parent->child relationship, but it gets
 * interesting with an intermediate domain: root->parent->child.  The
 * intermediate 'parent' can expand the capabilities which the 'root'
 * domain is providing. So that creates a classic hen and egg problem:
 * Which entity is doing the restrictions/expansions?
 *
 * One solution is to let the root domain handle the initialization that's
 * why there is the @domain and the @msi_parent_domain pointer.
 */
bool msi_parent_init_dev_msi_info(struct device *dev, struct irq_domain *domain,
				  struct irq_domain *msi_parent_domain,
				  struct msi_domain_info *msi_child_info)
{
	struct irq_domain *parent = domain->parent;

	if (WARN_ON_ONCE(!parent || !parent->msi_parent_ops ||
			 !parent->msi_parent_ops->init_dev_msi_info))
		return false;

	return parent->msi_parent_ops->init_dev_msi_info(dev, parent, msi_parent_domain,
							 msi_child_info);
}

/**
 * msi_create_device_irq_domain - Create a device MSI interrupt domain
 * @dev:		Pointer to the device
 * @domid:		Domain id
 * @template:		MSI domain info bundle used as template
 * @hwsize:		Maximum number of MSI table entries (0 if unknown or unlimited)
 * @domain_data:	Optional pointer to domain specific data which is set in
 *			msi_domain_info::data
 * @chip_data:		Optional pointer to chip specific data which is set in
 *			msi_domain_info::chip_data
 *
 * Return: True on success, false otherwise
 *
 * There is no firmware node required for this interface because the per
 * device domains are software constructs which are actually closer to the
 * hardware reality than any firmware can describe them.
 *
 * The domain name and the irq chip name for a MSI device domain are
 * composed by: "$(PREFIX)$(CHIPNAME)-$(DEVNAME)"
 *
 * $PREFIX:   Optional prefix provided by the underlying MSI parent domain
 *	      via msi_parent_ops::prefix. If that pointer is NULL the prefix
 *	      is empty.
 * $CHIPNAME: The name of the irq_chip in @template
 * $DEVNAME:  The name of the device
 *
 * This results in understandable chip names and hardware interrupt numbers
 * in e.g. /proc/interrupts
 *
 * PCI-MSI-0000:00:1c.0     0-edge  Parent domain has no prefix
 * IR-PCI-MSI-0000:00:1c.4  0-edge  Same with interrupt remapping prefix 'IR-'
 *
 * IR-PCI-MSIX-0000:3d:00.0 0-edge  Hardware interrupt numbers reflect
 * IR-PCI-MSIX-0000:3d:00.0 1-edge  the real MSI-X index on that device
 * IR-PCI-MSIX-0000:3d:00.0 2-edge
 *
 * On IMS domains the hardware interrupt number is either a table entry
 * index or a purely software managed index but it is guaranteed to be
 * unique.
 *
 * The domain pointer is stored in @dev::msi::data::__irqdomains[]. All
 * subsequent operations on the domain depend on the domain id.
 *
 * The domain is automatically freed when the device is removed via devres
 * in the context of @dev::msi::data freeing, but it can also be
 * independently removed via @msi_remove_device_irq_domain().
 */
bool msi_create_device_irq_domain(struct device *dev, unsigned int domid,
				  const struct msi_domain_template *template,
				  unsigned int hwsize, void *domain_data,
				  void *chip_data)
{
	struct irq_domain *domain, *parent = dev->msi.domain;
	const struct msi_parent_ops *pops;
	struct fwnode_handle *fwnode;

	if (!irq_domain_is_msi_parent(parent))
		return false;

	if (domid >= MSI_MAX_DEVICE_IRQDOMAINS)
		return false;

	struct msi_domain_template *bundle __free(kfree) =
		kmemdup(template, sizeof(*bundle), GFP_KERNEL);
	if (!bundle)
		return false;

	bundle->info.hwsize = hwsize;
	bundle->info.chip = &bundle->chip;
	bundle->info.ops = &bundle->ops;
	bundle->info.data = domain_data;
	bundle->info.chip_data = chip_data;
	bundle->info.alloc_data = &bundle->alloc_info;
	bundle->info.dev = dev;

	pops = parent->msi_parent_ops;
	snprintf(bundle->name, sizeof(bundle->name), "%s%s-%s",
		 pops->prefix ? : "", bundle->chip.name, dev_name(dev));
	bundle->chip.name = bundle->name;

	/*
	 * Using the device firmware node is required for wire to MSI
	 * device domains so that the existing firmware results in a domain
	 * match.
	 * All other device domains like PCI/MSI use the named firmware
	 * node as they are not guaranteed to have a fwnode. They are never
	 * looked up and always handled in the context of the device.
	 */
	struct fwnode_handle *fwnode_alloced __free(irq_domain_free_fwnode) = NULL;

	if (!(bundle->info.flags & MSI_FLAG_USE_DEV_FWNODE))
		fwnode = fwnode_alloced = irq_domain_alloc_named_fwnode(bundle->name);
	else
		fwnode = dev->fwnode;

	if (!fwnode)
		return false;

	if (msi_setup_device_data(dev))
		return false;

	guard(msi_descs_lock)(dev);
	if (WARN_ON_ONCE(msi_get_device_domain(dev, domid)))
		return false;

	if (!pops->init_dev_msi_info(dev, parent, parent, &bundle->info))
		return false;

	domain = __msi_create_irq_domain(fwnode, &bundle->info, IRQ_DOMAIN_FLAG_MSI_DEVICE, parent);
	if (!domain)
		return false;

	dev->msi.data->__domains[domid].domain = domain;

	if (msi_domain_prepare_irqs(domain, dev, hwsize, &bundle->alloc_info)) {
		dev->msi.data->__domains[domid].domain = NULL;
		irq_domain_remove(domain);
		return false;
	}

	/* @bundle and @fwnode_alloced are now in use. Prevent cleanup */
	retain_and_null_ptr(bundle);
	retain_and_null_ptr(fwnode_alloced);
	return true;
}

/**
 * msi_remove_device_irq_domain - Free a device MSI interrupt domain
 * @dev:	Pointer to the device
 * @domid:	Domain id
 */
void msi_remove_device_irq_domain(struct device *dev, unsigned int domid)
{
	struct fwnode_handle *fwnode = NULL;
	struct msi_domain_info *info;
	struct irq_domain *domain;

	guard(msi_descs_lock)(dev);
	domain = msi_get_device_domain(dev, domid);
	if (!domain || !irq_domain_is_msi_device(domain))
		return;

	dev->msi.data->__domains[domid].domain = NULL;
	info = domain->host_data;

	info->ops->msi_teardown(domain, info->alloc_data);

	if (irq_domain_is_msi_device(domain))
		fwnode = domain->fwnode;
	irq_domain_remove(domain);
	irq_domain_free_fwnode(fwnode);
	kfree(container_of(info, struct msi_domain_template, info));
}

/**
 * msi_match_device_irq_domain - Match a device irq domain against a bus token
 * @dev:	Pointer to the device
 * @domid:	Domain id
 * @bus_token:	Bus token to match against the domain bus token
 *
 * Return: True if device domain exists and bus tokens match.
 */
bool msi_match_device_irq_domain(struct device *dev, unsigned int domid,
				 enum irq_domain_bus_token bus_token)
{
	struct msi_domain_info *info;
	struct irq_domain *domain;

	guard(msi_descs_lock)(dev);
	domain = msi_get_device_domain(dev, domid);
	if (domain && irq_domain_is_msi_device(domain)) {
		info = domain->host_data;
		return info->bus_token == bus_token;
	}
	return false;
}

static int msi_domain_prepare_irqs(struct irq_domain *domain, struct device *dev,
				   int nvec, msi_alloc_info_t *arg)
{
	struct msi_domain_info *info = domain->host_data;
	struct msi_domain_ops *ops = info->ops;

	return ops->msi_prepare(domain, dev, nvec, arg);
}

/*
 * Carefully check whether the device can use reservation mode. If
 * reservation mode is enabled then the early activation will assign a
 * dummy vector to the device. If the PCI/MSI device does not support
 * masking of the entry then this can result in spurious interrupts when
 * the device driver is not absolutely careful. But even then a malfunction
 * of the hardware could result in a spurious interrupt on the dummy vector
 * and render the device unusable. If the entry can be masked then the core
 * logic will prevent the spurious interrupt and reservation mode can be
 * used. For now reservation mode is restricted to PCI/MSI.
 */
static bool msi_check_reservation_mode(struct irq_domain *domain,
				       struct msi_domain_info *info,
				       struct device *dev)
{
	struct msi_desc *desc;

	switch(domain->bus_token) {
	case DOMAIN_BUS_PCI_MSI:
	case DOMAIN_BUS_PCI_DEVICE_MSI:
	case DOMAIN_BUS_PCI_DEVICE_MSIX:
	case DOMAIN_BUS_VMD_MSI:
		break;
	default:
		return false;
	}

	if (!(info->flags & MSI_FLAG_MUST_REACTIVATE))
		return false;

	if (info->flags & MSI_FLAG_NO_MASK)
		return false;

	/*
	 * Checking the first MSI descriptor is sufficient. MSIX supports
	 * masking and MSI does so when the can_mask attribute is set.
	 */
	desc = msi_first_desc(dev, MSI_DESC_ALL);
	return desc->pci.msi_attrib.is_msix || desc->pci.msi_attrib.can_mask;
}

static int msi_handle_pci_fail(struct irq_domain *domain, struct msi_desc *desc,
			       int allocated)
{
	switch(domain->bus_token) {
	case DOMAIN_BUS_PCI_MSI:
	case DOMAIN_BUS_PCI_DEVICE_MSI:
	case DOMAIN_BUS_PCI_DEVICE_MSIX:
	case DOMAIN_BUS_VMD_MSI:
		if (IS_ENABLED(CONFIG_PCI_MSI))
			break;
		fallthrough;
	default:
		return -ENOSPC;
	}

	/* Let a failed PCI multi MSI allocation retry */
	if (desc->nvec_used > 1)
		return 1;

	/* If there was a successful allocation let the caller know */
	return allocated ? allocated : -ENOSPC;
}

#define VIRQ_CAN_RESERVE	0x01
#define VIRQ_ACTIVATE		0x02

static int msi_init_virq(struct irq_domain *domain, int virq, unsigned int vflags)
{
	struct irq_data *irqd = irq_domain_get_irq_data(domain, virq);
	int ret;

	if (!(vflags & VIRQ_CAN_RESERVE)) {
		irqd_clr_can_reserve(irqd);

		/*
		 * If the interrupt is managed but no CPU is available to
		 * service it, shut it down until better times. Note that
		 * we only do this on the !RESERVE path as x86 (the only
		 * architecture using this flag) deals with this in a
		 * different way by using a catch-all vector.
		 */
		if ((vflags & VIRQ_ACTIVATE) &&
		    irqd_affinity_is_managed(irqd) &&
		    !cpumask_intersects(irq_data_get_affinity_mask(irqd),
					cpu_online_mask)) {
			    irqd_set_managed_shutdown(irqd);
			    return 0;
		    }
	}

	if (!(vflags & VIRQ_ACTIVATE))
		return 0;

	ret = irq_domain_activate_irq(irqd, vflags & VIRQ_CAN_RESERVE);
	if (ret)
		return ret;
	/*
	 * If the interrupt uses reservation mode, clear the activated bit
	 * so request_irq() will assign the final vector.
	 */
	if (vflags & VIRQ_CAN_RESERVE)
		irqd_clr_activated(irqd);
	return 0;
}

static int populate_alloc_info(struct irq_domain *domain, struct device *dev,
			       unsigned int nirqs, msi_alloc_info_t *arg)
{
	struct msi_domain_info *info = domain->host_data;

	/*
	 * If the caller has provided a template alloc info, use that. Once
	 * all users of msi_create_irq_domain() have been eliminated, this
	 * should be the only source of allocation information, and the
	 * prepare call below should be finally removed.
	 */
	if (!info->alloc_data)
		return msi_domain_prepare_irqs(domain, dev, nirqs, arg);

	*arg = *info->alloc_data;
	return 0;
}

static int __msi_domain_alloc_irqs(struct device *dev, struct irq_domain *domain,
				   struct msi_ctrl *ctrl)
{
	struct xarray *xa = &dev->msi.data->__domains[ctrl->domid].store;
	struct msi_domain_info *info = domain->host_data;
	struct msi_domain_ops *ops = info->ops;
	unsigned int vflags = 0, allocated = 0;
	msi_alloc_info_t arg = { };
	struct msi_desc *desc;
	unsigned long idx;
	int i, ret, virq;

	ret = populate_alloc_info(domain, dev, ctrl->nirqs, &arg);
	if (ret)
		return ret;

	/*
	 * This flag is set by the PCI layer as we need to activate
	 * the MSI entries before the PCI layer enables MSI in the
	 * card. Otherwise the card latches a random msi message.
	 */
	if (info->flags & MSI_FLAG_ACTIVATE_EARLY)
		vflags |= VIRQ_ACTIVATE;

	/*
	 * Interrupt can use a reserved vector and will not occupy
	 * a real device vector until the interrupt is requested.
	 */
	if (msi_check_reservation_mode(domain, info, dev))
		vflags |= VIRQ_CAN_RESERVE;

	xa_for_each_range(xa, idx, desc, ctrl->first, ctrl->last) {
		if (!msi_desc_match(desc, MSI_DESC_NOTASSOCIATED))
			continue;

		/* This should return -ECONFUSED... */
		if (WARN_ON_ONCE(allocated >= ctrl->nirqs))
			return -EINVAL;

		if (ops->prepare_desc)
			ops->prepare_desc(domain, &arg, desc);

		ops->set_desc(&arg, desc);

		virq = __irq_domain_alloc_irqs(domain, -1, desc->nvec_used,
					       dev_to_node(dev), &arg, false,
					       desc->affinity);
		if (virq < 0)
			return msi_handle_pci_fail(domain, desc, allocated);

		for (i = 0; i < desc->nvec_used; i++) {
			irq_set_msi_desc_off(virq, i, desc);
			irq_debugfs_copy_devname(virq + i, dev);
			ret = msi_init_virq(domain, virq + i, vflags);
			if (ret)
				return ret;
		}
		if (info->flags & MSI_FLAG_DEV_SYSFS) {
			ret = msi_sysfs_populate_desc(dev, desc);
			if (ret)
				return ret;
		}
		allocated++;
	}
	return 0;
}

static int msi_domain_alloc_simple_msi_descs(struct device *dev,
					     struct msi_domain_info *info,
					     struct msi_ctrl *ctrl)
{
	if (!(info->flags & MSI_FLAG_ALLOC_SIMPLE_MSI_DESCS))
		return 0;

	return msi_domain_add_simple_msi_descs(dev, ctrl);
}

static int __msi_domain_alloc_locked(struct device *dev, struct msi_ctrl *ctrl)
{
	struct msi_domain_info *info;
	struct msi_domain_ops *ops;
	struct irq_domain *domain;
	int ret;

	if (!msi_ctrl_valid(dev, ctrl))
		return -EINVAL;

	domain = msi_get_device_domain(dev, ctrl->domid);
	if (!domain)
		return -ENODEV;

	info = domain->host_data;

	ret = msi_domain_alloc_simple_msi_descs(dev, info, ctrl);
	if (ret)
		return ret;

	ops = info->ops;
	if (ops->domain_alloc_irqs)
		return ops->domain_alloc_irqs(domain, dev, ctrl->nirqs);

	return __msi_domain_alloc_irqs(dev, domain, ctrl);
}

static int msi_domain_alloc_locked(struct device *dev, struct msi_ctrl *ctrl)
{
	int ret = __msi_domain_alloc_locked(dev, ctrl);

	if (ret)
		msi_domain_free_locked(dev, ctrl);
	return ret;
}

/**
 * msi_domain_alloc_irqs_range_locked - Allocate interrupts from a MSI interrupt domain
 * @dev:	Pointer to device struct of the device for which the interrupts
 *		are allocated
 * @domid:	Id of the interrupt domain to operate on
 * @first:	First index to allocate (inclusive)
 * @last:	Last index to allocate (inclusive)
 *
 * Must be invoked from within a msi_lock_descs() / msi_unlock_descs()
 * pair. Use this for MSI irqdomains which implement their own descriptor
 * allocation/free.
 *
 * Return: %0 on success or an error code.
 */
int msi_domain_alloc_irqs_range_locked(struct device *dev, unsigned int domid,
				       unsigned int first, unsigned int last)
{
	struct msi_ctrl ctrl = {
		.domid	= domid,
		.first	= first,
		.last	= last,
		.nirqs	= last + 1 - first,
	};

	return msi_domain_alloc_locked(dev, &ctrl);
}

/**
 * msi_domain_alloc_irqs_range - Allocate interrupts from a MSI interrupt domain
 * @dev:	Pointer to device struct of the device for which the interrupts
 *		are allocated
 * @domid:	Id of the interrupt domain to operate on
 * @first:	First index to allocate (inclusive)
 * @last:	Last index to allocate (inclusive)
 *
 * Return: %0 on success or an error code.
 */
int msi_domain_alloc_irqs_range(struct device *dev, unsigned int domid,
				unsigned int first, unsigned int last)
{

	guard(msi_descs_lock)(dev);
	return msi_domain_alloc_irqs_range_locked(dev, domid, first, last);
}
EXPORT_SYMBOL_GPL(msi_domain_alloc_irqs_range);

/**
 * msi_domain_alloc_irqs_all_locked - Allocate all interrupts from a MSI interrupt domain
 *
 * @dev:	Pointer to device struct of the device for which the interrupts
 *		are allocated
 * @domid:	Id of the interrupt domain to operate on
 * @nirqs:	The number of interrupts to allocate
 *
 * This function scans all MSI descriptors of the MSI domain and allocates interrupts
 * for all unassigned ones. That function is to be used for MSI domain usage where
 * the descriptor allocation is handled at the call site, e.g. PCI/MSI[X].
 *
 * Return: %0 on success or an error code.
 */
int msi_domain_alloc_irqs_all_locked(struct device *dev, unsigned int domid, int nirqs)
{
	struct msi_ctrl ctrl = {
		.domid	= domid,
		.first	= 0,
		.last	= msi_domain_get_hwsize(dev, domid) - 1,
		.nirqs	= nirqs,
	};

	return msi_domain_alloc_locked(dev, &ctrl);
}

static struct msi_map __msi_domain_alloc_irq_at(struct device *dev, unsigned int domid,
						unsigned int index,
						const struct irq_affinity_desc *affdesc,
						union msi_instance_cookie *icookie)
{
	struct msi_ctrl ctrl = { .domid	= domid, .nirqs = 1, };
	struct irq_domain *domain;
	struct msi_map map = { };
	struct msi_desc *desc;
	int ret;

	domain = msi_get_device_domain(dev, domid);
	if (!domain) {
		map.index = -ENODEV;
		return map;
	}

	desc = msi_alloc_desc(dev, 1, affdesc);
	if (!desc) {
		map.index = -ENOMEM;
		return map;
	}

	if (icookie)
		desc->data.icookie = *icookie;

	ret = msi_insert_desc(dev, desc, domid, index);
	if (ret) {
		map.index = ret;
		return map;
	}

	ctrl.first = ctrl.last = desc->msi_index;

	ret = __msi_domain_alloc_irqs(dev, domain, &ctrl);
	if (ret) {
		map.index = ret;
		msi_domain_free_locked(dev, &ctrl);
	} else {
		map.index = desc->msi_index;
		map.virq = desc->irq;
	}
	return map;
}

/**
 * msi_domain_alloc_irq_at - Allocate an interrupt from a MSI interrupt domain at
 *			     a given index - or at the next free index
 *
 * @dev:	Pointer to device struct of the device for which the interrupts
 *		are allocated
 * @domid:	Id of the interrupt domain to operate on
 * @index:	Index for allocation. If @index == %MSI_ANY_INDEX the allocation
 *		uses the next free index.
 * @affdesc:	Optional pointer to an interrupt affinity descriptor structure
 * @icookie:	Optional pointer to a domain specific per instance cookie. If
 *		non-NULL the content of the cookie is stored in msi_desc::data.
 *		Must be NULL for MSI-X allocations
 *
 * This requires a MSI interrupt domain which lets the core code manage the
 * MSI descriptors.
 *
 * Return: struct msi_map
 *
 *	On success msi_map::index contains the allocated index number and
 *	msi_map::virq the corresponding Linux interrupt number
 *
 *	On failure msi_map::index contains the error code and msi_map::virq
 *	is %0.
 */
struct msi_map msi_domain_alloc_irq_at(struct device *dev, unsigned int domid, unsigned int index,
				       const struct irq_affinity_desc *affdesc,
				       union msi_instance_cookie *icookie)
{
	guard(msi_descs_lock)(dev);
	return __msi_domain_alloc_irq_at(dev, domid, index, affdesc, icookie);
}

/**
 * msi_device_domain_alloc_wired - Allocate a "wired" interrupt on @domain
 * @domain:	The domain to allocate on
 * @hwirq:	The hardware interrupt number to allocate for
 * @type:	The interrupt type
 *
 * This weirdness supports wire to MSI controllers like MBIGEN.
 *
 * @hwirq is the hardware interrupt number which is handed in from
 * irq_create_fwspec_mapping(). As the wire to MSI domain is sparse, but
 * sized in firmware, the hardware interrupt number cannot be used as MSI
 * index. For the underlying irq chip the MSI index is irrelevant and
 * all it needs is the hardware interrupt number.
 *
 * To handle this the MSI index is allocated with MSI_ANY_INDEX and the
 * hardware interrupt number is stored along with the type information in
 * msi_desc::cookie so the underlying interrupt chip and domain code can
 * retrieve it.
 *
 * Return: The Linux interrupt number (> 0) or an error code
 */
int msi_device_domain_alloc_wired(struct irq_domain *domain, unsigned int hwirq,
				  unsigned int type)
{
	unsigned int domid = MSI_DEFAULT_DOMAIN;
	union msi_instance_cookie icookie = { };
	struct device *dev = domain->dev;
	struct msi_map map = { };

	if (WARN_ON_ONCE(!dev || domain->bus_token != DOMAIN_BUS_WIRED_TO_MSI))
		return -EINVAL;

	icookie.value = ((u64)type << 32) | hwirq;

	guard(msi_descs_lock)(dev);
	if (WARN_ON_ONCE(msi_get_device_domain(dev, domid) != domain))
		map.index = -EINVAL;
	else
		map = __msi_domain_alloc_irq_at(dev, domid, MSI_ANY_INDEX, NULL, &icookie);
	return map.index >= 0 ? map.virq : map.index;
}

static void __msi_domain_free_irqs(struct device *dev, struct irq_domain *domain,
				   struct msi_ctrl *ctrl)
{
	struct xarray *xa = &dev->msi.data->__domains[ctrl->domid].store;
	struct msi_domain_info *info = domain->host_data;
	struct irq_data *irqd;
	struct msi_desc *desc;
	unsigned long idx;
	int i;

	xa_for_each_range(xa, idx, desc, ctrl->first, ctrl->last) {
		/* Only handle MSI entries which have an interrupt associated */
		if (!msi_desc_match(desc, MSI_DESC_ASSOCIATED))
			continue;

		/* Make sure all interrupts are deactivated */
		for (i = 0; i < desc->nvec_used; i++) {
			irqd = irq_domain_get_irq_data(domain, desc->irq + i);
			if (irqd && irqd_is_activated(irqd))
				irq_domain_deactivate_irq(irqd);
		}

		irq_domain_free_irqs(desc->irq, desc->nvec_used);
		if (info->flags & MSI_FLAG_DEV_SYSFS)
			msi_sysfs_remove_desc(dev, desc);
		desc->irq = 0;
	}
}

static void msi_domain_free_locked(struct device *dev, struct msi_ctrl *ctrl)
{
	struct msi_domain_info *info;
	struct msi_domain_ops *ops;
	struct irq_domain *domain;

	if (!msi_ctrl_valid(dev, ctrl))
		return;

	domain = msi_get_device_domain(dev, ctrl->domid);
	if (!domain)
		return;

	info = domain->host_data;
	ops = info->ops;

	if (ops->domain_free_irqs)
		ops->domain_free_irqs(domain, dev);
	else
		__msi_domain_free_irqs(dev, domain, ctrl);

	if (ops->msi_post_free)
		ops->msi_post_free(domain, dev);

	if (info->flags & MSI_FLAG_FREE_MSI_DESCS)
		msi_domain_free_descs(dev, ctrl);
}

/**
 * msi_domain_free_irqs_range_locked - Free a range of interrupts from a MSI interrupt domain
 *				       associated to @dev with msi_lock held
 * @dev:	Pointer to device struct of the device for which the interrupts
 *		are freed
 * @domid:	Id of the interrupt domain to operate on
 * @first:	First index to free (inclusive)
 * @last:	Last index to free (inclusive)
 */
void msi_domain_free_irqs_range_locked(struct device *dev, unsigned int domid,
				       unsigned int first, unsigned int last)
{
	struct msi_ctrl ctrl = {
		.domid	= domid,
		.first	= first,
		.last	= last,
	};
	msi_domain_free_locked(dev, &ctrl);
}

/**
 * msi_domain_free_irqs_range - Free a range of interrupts from a MSI interrupt domain
 *				associated to @dev
 * @dev:	Pointer to device struct of the device for which the interrupts
 *		are freed
 * @domid:	Id of the interrupt domain to operate on
 * @first:	First index to free (inclusive)
 * @last:	Last index to free (inclusive)
 */
void msi_domain_free_irqs_range(struct device *dev, unsigned int domid,
				unsigned int first, unsigned int last)
{
	guard(msi_descs_lock)(dev);
	msi_domain_free_irqs_range_locked(dev, domid, first, last);
}
EXPORT_SYMBOL_GPL(msi_domain_free_irqs_all);

/**
 * msi_domain_free_irqs_all_locked - Free all interrupts from a MSI interrupt domain
 *				     associated to a device
 * @dev:	Pointer to device struct of the device for which the interrupts
 *		are freed
 * @domid:	The id of the domain to operate on
 *
 * Must be invoked from within a msi_lock_descs() / msi_unlock_descs()
 * pair. Use this for MSI irqdomains which implement their own vector
 * allocation.
 */
void msi_domain_free_irqs_all_locked(struct device *dev, unsigned int domid)
{
	msi_domain_free_irqs_range_locked(dev, domid, 0,
					  msi_domain_get_hwsize(dev, domid) - 1);
}

/**
 * msi_domain_free_irqs_all - Free all interrupts from a MSI interrupt domain
 *			      associated to a device
 * @dev:	Pointer to device struct of the device for which the interrupts
 *		are freed
 * @domid:	The id of the domain to operate on
 */
void msi_domain_free_irqs_all(struct device *dev, unsigned int domid)
{
	guard(msi_descs_lock)(dev);
	msi_domain_free_irqs_all_locked(dev, domid);
}

/**
 * msi_device_domain_free_wired - Free a wired interrupt in @domain
 * @domain:	The domain to free the interrupt on
 * @virq:	The Linux interrupt number to free
 *
 * This is the counterpart of msi_device_domain_alloc_wired() for the
 * weird wired to MSI converting domains.
 */
void msi_device_domain_free_wired(struct irq_domain *domain, unsigned int virq)
{
	struct msi_desc *desc = irq_get_msi_desc(virq);
	struct device *dev = domain->dev;

	if (WARN_ON_ONCE(!dev || !desc || domain->bus_token != DOMAIN_BUS_WIRED_TO_MSI))
		return;

	guard(msi_descs_lock)(dev);
	if (WARN_ON_ONCE(msi_get_device_domain(dev, MSI_DEFAULT_DOMAIN) != domain))
		return;
	msi_domain_free_irqs_range_locked(dev, MSI_DEFAULT_DOMAIN, desc->msi_index,
					  desc->msi_index);
}

/**
 * msi_get_domain_info - Get the MSI interrupt domain info for @domain
 * @domain:	The interrupt domain to retrieve data from
 *
 * Return: the pointer to the msi_domain_info stored in @domain->host_data.
 */
struct msi_domain_info *msi_get_domain_info(struct irq_domain *domain)
{
	return (struct msi_domain_info *)domain->host_data;
}

/**
 * msi_device_has_isolated_msi - True if the device has isolated MSI
 * @dev: The device to check
 *
 * Isolated MSI means that HW modeled by an irq_domain on the path from the
 * initiating device to the CPU will validate that the MSI message specifies an
 * interrupt number that the device is authorized to trigger. This must block
 * devices from triggering interrupts they are not authorized to trigger.
 * Currently authorization means the MSI vector is one assigned to the device.
 *
 * This is interesting for securing VFIO use cases where a rouge MSI (eg created
 * by abusing a normal PCI MemWr DMA) must not allow the VFIO userspace to
 * impact outside its security domain, eg userspace triggering interrupts on
 * kernel drivers, a VM triggering interrupts on the hypervisor, or a VM
 * triggering interrupts on another VM.
 */
bool msi_device_has_isolated_msi(struct device *dev)
{
	struct irq_domain *domain = dev_get_msi_domain(dev);

	for (; domain; domain = domain->parent)
		if (domain->flags & IRQ_DOMAIN_FLAG_ISOLATED_MSI)
			return true;
	return arch_is_isolated_msi();
}
EXPORT_SYMBOL_GPL(msi_device_has_isolated_msi);
