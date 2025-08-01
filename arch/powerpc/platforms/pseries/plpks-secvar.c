// SPDX-License-Identifier: GPL-2.0-only

// Secure variable implementation using the PowerVM LPAR Platform KeyStore (PLPKS)
//
// Copyright 2022, 2023 IBM Corporation
// Authors: Russell Currey
//          Andrew Donnellan
//          Nayna Jain

#define pr_fmt(fmt) "secvar: "fmt

#include <linux/printk.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/nls.h>
#include <asm/machdep.h>
#include <asm/secvar.h>
#include <asm/plpks.h>

// Config attributes for sysfs
#define PLPKS_CONFIG_ATTR(name, fmt, func)			\
	static ssize_t name##_show(struct kobject *kobj,	\
				   struct kobj_attribute *attr,	\
				   char *buf)			\
	{							\
		return sysfs_emit(buf, fmt, func());		\
	}							\
	static struct kobj_attribute attr_##name = __ATTR_RO(name)

PLPKS_CONFIG_ATTR(version, "%u\n", plpks_get_version);
PLPKS_CONFIG_ATTR(max_object_size, "%u\n", plpks_get_maxobjectsize);
PLPKS_CONFIG_ATTR(total_size, "%u\n", plpks_get_totalsize);
PLPKS_CONFIG_ATTR(used_space, "%u\n", plpks_get_usedspace);
PLPKS_CONFIG_ATTR(supported_policies, "%08x\n", plpks_get_supportedpolicies);
PLPKS_CONFIG_ATTR(signed_update_algorithms, "%016llx\n", plpks_get_signedupdatealgorithms);

static const struct attribute *config_attrs[] = {
	&attr_version.attr,
	&attr_max_object_size.attr,
	&attr_total_size.attr,
	&attr_used_space.attr,
	&attr_supported_policies.attr,
	&attr_signed_update_algorithms.attr,
	NULL,
};

static u32 get_policy(const char *name)
{
	if ((strcmp(name, "db") == 0) ||
	    (strcmp(name, "dbx") == 0) ||
	    (strcmp(name, "grubdb") == 0) ||
	    (strcmp(name, "grubdbx") == 0) ||
	    (strcmp(name, "sbat") == 0))
		return (PLPKS_WORLDREADABLE | PLPKS_SIGNEDUPDATE);
	else
		return PLPKS_SIGNEDUPDATE;
}

static const char * const plpks_var_names_static[] = {
	"PK",
	"moduledb",
	"trustedcadb",
	NULL,
};

static const char * const plpks_var_names_dynamic[] = {
	"PK",
	"KEK",
	"db",
	"dbx",
	"grubdb",
	"grubdbx",
	"sbat",
	"moduledb",
	"trustedcadb",
	NULL,
};

static int plpks_get_variable(const char *key, u64 key_len, u8 *data,
			      u64 *data_size)
{
	struct plpks_var var = {0};
	int rc = 0;

	// We subtract 1 from key_len because we don't need to include the
	// null terminator at the end of the string
	var.name = kcalloc(key_len - 1, sizeof(wchar_t), GFP_KERNEL);
	if (!var.name)
		return -ENOMEM;
	rc = utf8s_to_utf16s(key, key_len - 1, UTF16_LITTLE_ENDIAN, (wchar_t *)var.name,
			     key_len - 1);
	if (rc < 0)
		goto err;
	var.namelen = rc * 2;

	var.os = PLPKS_VAR_LINUX;
	if (data) {
		var.data = data;
		var.datalen = *data_size;
	}
	rc = plpks_read_os_var(&var);

	if (rc)
		goto err;

	*data_size = var.datalen;

err:
	kfree(var.name);
	if (rc && rc != -ENOENT) {
		pr_err("Failed to read variable '%s': %d\n", key, rc);
		// Return -EIO since userspace probably doesn't care about the
		// specific error
		rc = -EIO;
	}
	return rc;
}

static int plpks_set_variable(const char *key, u64 key_len, u8 *data,
			      u64 data_size)
{
	struct plpks_var var = {0};
	int rc = 0;
	u64 flags;

	// Secure variables need to be prefixed with 8 bytes of flags.
	// We only want to perform the write if we have at least one byte of data.
	if (data_size <= sizeof(flags))
		return -EINVAL;

	// We subtract 1 from key_len because we don't need to include the
	// null terminator at the end of the string
	var.name = kcalloc(key_len - 1, sizeof(wchar_t), GFP_KERNEL);
	if (!var.name)
		return -ENOMEM;
	rc = utf8s_to_utf16s(key, key_len - 1, UTF16_LITTLE_ENDIAN, (wchar_t *)var.name,
			     key_len - 1);
	if (rc < 0)
		goto err;
	var.namelen = rc * 2;

	// Flags are contained in the first 8 bytes of the buffer, and are always big-endian
	flags = be64_to_cpup((__be64 *)data);

	var.datalen = data_size - sizeof(flags);
	var.data = data + sizeof(flags);
	var.os = PLPKS_VAR_LINUX;
	var.policy = get_policy(key);

	// Unlike in the read case, the plpks error code can be useful to
	// userspace on write, so we return it rather than just -EIO
	rc = plpks_signed_update_var(&var, flags);

err:
	kfree(var.name);
	return rc;
}

/*
 * Return the key management mode.
 *
 * SB_VERSION is defined as a "1 byte unsigned integer value", taking values
 * starting from 1. It is owned by the Partition Firmware and its presence
 * indicates that the key management mode is dynamic. Any failure in
 * reading SB_VERSION defaults the key management mode to static. The error
 * codes -ENOENT or -EPERM are expected in static key management mode. An
 * unexpected error code will have to be investigated. Only signed variables
 * have null bytes in their names, SB_VERSION does not.
 *
 * Return 0 to indicate that the key management mode is static. Otherwise
 * return the SB_VERSION value to indicate that the key management mode is
 * dynamic.
 */
static u8 plpks_get_sb_keymgmt_mode(void)
{
	u8 mode;
	ssize_t rc;
	struct plpks_var var = {
		.component = NULL,
		.name = "SB_VERSION",
		.namelen = 10,
		.datalen = 1,
		.data = &mode,
	};

	rc = plpks_read_fw_var(&var);
	if (rc) {
		if (rc != -ENOENT && rc != -EPERM)
			pr_info("Error %ld reading SB_VERSION from firmware\n", rc);
		mode = 0;
	}
	return mode;
}

/*
 * PLPKS dynamic secure boot doesn't give us a format string in the same way
 * OPAL does. Instead, report the format using the SB_VERSION variable in the
 * keystore. The string, made up by us, takes the form of either
 * "ibm,plpks-sb-v<n>" or "ibm,plpks-sb-v0", based on the key management mode,
 * and return the length of the secvar format property.
 */
static ssize_t plpks_secvar_format(char *buf, size_t bufsize)
{
	u8 mode;

	mode = plpks_get_sb_keymgmt_mode();
	return snprintf(buf, bufsize, "ibm,plpks-sb-v%hhu", mode);
}

static int plpks_max_size(u64 *max_size)
{
	// The max object size reported by the hypervisor is accurate for the
	// object itself, but we use the first 8 bytes of data on write as the
	// signed update flags, so the max size a user can write is larger.
	*max_size = (u64)plpks_get_maxobjectsize() + sizeof(u64);

	return 0;
}

static const struct secvar_operations plpks_secvar_ops_static = {
	.get = plpks_get_variable,
	.set = plpks_set_variable,
	.format = plpks_secvar_format,
	.max_size = plpks_max_size,
	.config_attrs = config_attrs,
	.var_names = plpks_var_names_static,
};

static const struct secvar_operations plpks_secvar_ops_dynamic = {
	.get = plpks_get_variable,
	.set = plpks_set_variable,
	.format = plpks_secvar_format,
	.max_size = plpks_max_size,
	.config_attrs = config_attrs,
	.var_names = plpks_var_names_dynamic,
};

static int plpks_secvar_init(void)
{
	u8 mode;

	if (!plpks_is_available())
		return -ENODEV;

	mode = plpks_get_sb_keymgmt_mode();
	if (mode)
		return set_secvar_ops(&plpks_secvar_ops_dynamic);
	return set_secvar_ops(&plpks_secvar_ops_static);
}
machine_device_initcall(pseries, plpks_secvar_init);
