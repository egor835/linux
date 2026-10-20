// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 Egor 835 <egor.su.8@gmail.com>
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved.

#include <linux/delay.h>
#include <linux/mod_devicetable.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct lg_vfp_tovis {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data *supplies;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;
};

static const struct regulator_bulk_data lg_vfp_tovis_supplies[] = {
	{ .supply = "vdd" },
	{ .supply = "vddio" },
};

static inline
struct lg_vfp_tovis *to_lg_vfp_tovis(struct drm_panel *panel)
{
	return container_of_const(panel, struct lg_vfp_tovis, panel);
}

static void lg_vfp_tovis_reset(struct lg_vfp_tovis *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(2000, 3000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(120);
}

static int lg_vfp_tovis_on(struct lg_vfp_tovis *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xf8, 0x21, 0x04);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x21, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_ADDRESS_MODE, 0x08);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb4, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb6, 0x02, 0x22, 0x3b);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xc0, 0x0a, 0x0a);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xc1, 0x44);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xc2, 0x02);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xc5,
					 0x00, 0x42, 0x80, 0x00);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xe0,
					 0x05, 0x08, 0x0a, 0x00, 0x0c, 0x03,
					 0x2d, 0x56, 0x4a, 0x09, 0x11, 0x0d,
					 0x27, 0x2d, 0x0f);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xe1,
					 0x00, 0x0d, 0x15, 0x03, 0x0e, 0x02,
					 0x38, 0x43, 0x52, 0x09, 0x11, 0x0e,
					 0x34, 0x36, 0x0f);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x2a,
					 0x00, 0x00, 0x01, 0x3f);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x2b,
					 0x00, 0x00, 0x01, 0xdf);
	mipi_dsi_dcs_set_pixel_format_multi(&dsi_ctx, 0x66);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb7, 0xc6);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb1, 0xa0, 0x10);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xf7,
					 0xa9, 0x51, 0x2c, 0x82);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb5,
					 0x02, 0x02, 0x0a, 0x04);
	mipi_dsi_dcs_set_display_brightness_multi(&dsi_ctx, 0x00ff);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY,
				     0x24);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_POWER_SAVE, 0x01);
	mipi_dsi_usleep_range(&dsi_ctx, 5000, 6000);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcc, 0x01);
	mipi_dsi_dcs_set_tear_on_multi(&dsi_ctx, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x44, 0x00, 0x00);
	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_MEMORY_START);
	mipi_dsi_msleep(&dsi_ctx, 80);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 60);

	return dsi_ctx.accum_err;
}

static int lg_vfp_tovis_off(struct lg_vfp_tovis *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 50);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int lg_vfp_tovis_prepare(struct drm_panel *panel)
{
	struct lg_vfp_tovis *ctx = to_lg_vfp_tovis(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(lg_vfp_tovis_supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	gpiod_set_value_cansleep(ctx->enable_gpio, 1);
	usleep_range(1000, 2000);

	lg_vfp_tovis_reset(ctx);

	ret = lg_vfp_tovis_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		gpiod_set_value_cansleep(ctx->enable_gpio, 0);
		regulator_bulk_disable(ARRAY_SIZE(lg_vfp_tovis_supplies), ctx->supplies);
		return ret;
	}

	return 0;
}

static int lg_vfp_tovis_unprepare(struct drm_panel *panel)
{
	struct lg_vfp_tovis *ctx = to_lg_vfp_tovis(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = lg_vfp_tovis_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	gpiod_set_value_cansleep(ctx->enable_gpio, 0);
	regulator_bulk_disable(ARRAY_SIZE(lg_vfp_tovis_supplies), ctx->supplies);

	return 0;
}

static const struct drm_display_mode lg_vfp_tovis_mode = {
	.clock = (320 + 49 + 8 + 14) * (480 + 14 + 4 + 14) * 60 / 1000,
	.hdisplay = 320,
	.hsync_start = 320 + 49,
	.hsync_end = 320 + 49 + 8,
	.htotal = 320 + 49 + 8 + 14,
	.vdisplay = 480,
	.vsync_start = 480 + 14,
	.vsync_end = 480 + 14 + 4,
	.vtotal = 480 + 14 + 4 + 14,
	.width_mm = 49,
	.height_mm = 73,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int lg_vfp_tovis_get_modes(struct drm_panel *panel,
				       struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &lg_vfp_tovis_mode);
}

static const struct drm_panel_funcs lg_vfp_tovis_panel_funcs = {
	.prepare = lg_vfp_tovis_prepare,
	.unprepare = lg_vfp_tovis_unprepare,
	.get_modes = lg_vfp_tovis_get_modes,
};

static int lg_vfp_tovis_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lg_vfp_tovis *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct lg_vfp_tovis, panel,
				   &lg_vfp_tovis_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ret = devm_regulator_bulk_get_const(dev,
					    ARRAY_SIZE(lg_vfp_tovis_supplies),
					    lg_vfp_tovis_supplies,
					    &ctx->supplies);
	if (ret < 0)
		return ret;

	ctx->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->enable_gpio),
				     "Failed to get enable-gpios\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 1;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void lg_vfp_tovis_remove(struct mipi_dsi_device *dsi)
{
	struct lg_vfp_tovis *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id lg_vfp_tovis_of_match[] = {
	{ .compatible = "lg,vfp-panel-tovis" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lg_vfp_tovis_of_match);

static struct mipi_dsi_driver lg_vfp_tovis_driver = {
	.probe = lg_vfp_tovis_probe,
	.remove = lg_vfp_tovis_remove,
	.driver = {
		.name = "panel-lg-vfp-tovis",
		.of_match_table = lg_vfp_tovis_of_match,
	},
};
module_mipi_dsi_driver(lg_vfp_tovis_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>");
MODULE_DESCRIPTION("DRM driver for the TOVIS HVGA cmd mode panel of the LG Wine Smart");
MODULE_LICENSE("GPL");
