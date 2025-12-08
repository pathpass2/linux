// SPDX-License-Identifier: GPL-2.0-only
// Author: Eralp Çelebi <eralp.celebi.personal@gmail.com>
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
// Copyright (c) 2022, The Linux Foundation. All rights reserved.

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

static const char * const regulator_names[] = {
	"vddio",
	"vddpos",
	"vddneg",
};

static const unsigned long regulator_enable_loads[] = {
	62000,
	100000,
	100000
};

struct ss_notch_fhd_ea8074 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;

	struct regulator_bulk_data supplies[ARRAY_SIZE(regulator_names)];

	struct gpio_desc *reset_gpio;
};

static inline
struct ss_notch_fhd_ea8074 *to_ss_notch_fhd_ea8074(struct drm_panel *panel)
{
	return container_of(panel, struct ss_notch_fhd_ea8074, panel);
}

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static void ss_notch_fhd_ea8074_reset(struct ss_notch_fhd_ea8074 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int ss_notch_fhd_ea8074_on(struct ss_notch_fhd_ea8074 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	usleep_range(10000, 11000);

	ret = mipi_dsi_dcs_set_page_address(dsi, 0x0000, 0x08c7);
	if (ret < 0) {
		dev_err(dev, "Failed to set page address: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	dsi_dcs_write_seq(dsi, 0xef, 0xf0, 0x31, 0x00, 0x33, 0x31, 0x14, 0x35);
	dsi_dcs_write_seq(dsi, 0xb0, 0x01);
	dsi_dcs_write_seq(dsi, 0xbb, 0x03);
	dsi_dcs_write_seq(dsi, 0xb0, 0x4f);
	dsi_dcs_write_seq(dsi, 0xcb, 0x00);
	dsi_dcs_write_seq(dsi, 0xb0, 0x6b);
	dsi_dcs_write_seq(dsi, 0xcb, 0x00);
	dsi_dcs_write_seq(dsi, 0xf7, 0x03);
	dsi_dcs_write_seq(dsi, 0xb0, 0x05);
	dsi_dcs_write_seq(dsi, 0xb1, 0x10);
	dsi_dcs_write_seq(dsi, 0xb0, 0x02);
	dsi_dcs_write_seq(dsi, 0xd5, 0x02, 0x17, 0x54, 0x14);
	dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	dsi_dcs_write_seq(dsi, 0xfc, 0x5a, 0x5a);
	dsi_dcs_write_seq(dsi, 0xd2, 0x9f, 0xf0);
	dsi_dcs_write_seq(dsi, 0xb0, 0x0e);
	dsi_dcs_write_seq(dsi, 0xd2, 0x70);
	dsi_dcs_write_seq(dsi, 0xb0, 0x04);
	dsi_dcs_write_seq(dsi, 0xd2, 0x20);
	dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	dsi_dcs_write_seq(dsi, 0xfc, 0xa5, 0xa5);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0000);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x20);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x00);
	msleep(110);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ss_notch_fhd_ea8074_disable(struct drm_panel *panel)
{
	struct ss_notch_fhd_ea8074 *ctx = to_ss_notch_fhd_ea8074(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ctx->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(ctx->dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(17000, 18000);

	ret = mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	return 0;
}

static int ss_notch_fhd_ea8074_prepare(struct drm_panel *panel)
{
	struct ss_notch_fhd_ea8074 *ctx = to_ss_notch_fhd_ea8074(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	ss_notch_fhd_ea8074_reset(ctx);

	ret = ss_notch_fhd_ea8074_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	return 0;
}

static int ss_notch_fhd_ea8074_unprepare(struct drm_panel *panel)
{
	struct ss_notch_fhd_ea8074 *ctx = to_ss_notch_fhd_ea8074(panel);
	int ret;

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ret = regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret)
		dev_err(panel->dev, "Failed to disable regulators: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	return 0;
}

static const struct drm_display_mode ss_notch_fhd_ea8074_mode = {
	.clock = (1080 + 56 + 18 + 56) * (2248 + 26 + 12 + 24) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 56,
	.hsync_end = 1080 + 56 + 18,
	.htotal = 1080 + 56 + 18 + 56,
	.vdisplay = 2248,
	.vsync_start = 2248 + 26,
	.vsync_end = 2248 + 26 + 12,
	.vtotal = 2248 + 26 + 12 + 24,
	.width_mm = 68,
	.height_mm = 142,
};

static int ss_notch_fhd_ea8074_get_modes(struct drm_panel *panel,
					 struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &ss_notch_fhd_ea8074_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs ss_notch_fhd_ea8074_panel_funcs = {
	.prepare = ss_notch_fhd_ea8074_prepare,
        .disable = ss_notch_fhd_ea8074_disable,
	.unprepare = ss_notch_fhd_ea8074_unprepare,
	.get_modes = ss_notch_fhd_ea8074_get_modes,
};

static int ss_notch_fhd_ea8074_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ss_notch_fhd_ea8074 *ctx;
	int i, ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
		
	for (i = 0; i < ARRAY_SIZE(ctx->supplies); i++)
		ctx->supplies[i].supply = regulator_names[i];
	
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	for (i = 0; i < ARRAY_SIZE(ctx->supplies); i++) {
		ret = regulator_set_load(ctx->supplies[i].consumer,
						regulator_enable_loads[i]);
		if (ret)
			return dev_err_probe(dev, ret,
						 "Failed to set regulator load\n");
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &ss_notch_fhd_ea8074_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void ss_notch_fhd_ea8074_remove(struct mipi_dsi_device *dsi)
{
	struct ss_notch_fhd_ea8074 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);


}

static const struct of_device_id ss_notch_fhd_ea8074_of_match[] = {
	{ .compatible = "samsung,ea8074" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ss_notch_fhd_ea8074_of_match);

static struct mipi_dsi_driver ss_notch_fhd_ea8074_driver = {
	.probe = ss_notch_fhd_ea8074_probe,
	.remove = ss_notch_fhd_ea8074_remove,
	.driver = {
		.name = "panel-samsung-ea8074",
		.of_match_table = ss_notch_fhd_ea8074_of_match,
	},
};
module_mipi_dsi_driver(ss_notch_fhd_ea8074_driver);

MODULE_AUTHOR("Eralp Çelebi <eralp.celebi.personal@gmail.com>");
MODULE_DESCRIPTION("DRM driver for the Samsung EA8074 Panel");
