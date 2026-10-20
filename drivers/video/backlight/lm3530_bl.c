// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal TI LM3530 backlight driver: manual mode, I2C brightness control.
 * (drivers/leds/leds-lm3530.c only takes platform data.)
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define LM3530_GEN_CONFIG	0x10
#define LM3530_BRT_CTRL		0xa0
#define LM3530_MAX_BRIGHTNESS	127

struct lm3530_bl {
	struct regmap *regmap;
	struct gpio_desc *enable_gpio;
	u32 gen_config;
	bool enabled;
};

static int lm3530_bl_update_status(struct backlight_device *bl)
{
	struct lm3530_bl *ctx = bl_get_data(bl);
	int brightness = backlight_get_brightness(bl);
	int ret;

	if (!brightness) {
		if (!ctx->enabled)
			return 0;
		regmap_write(ctx->regmap, LM3530_GEN_CONFIG, 0);
		gpiod_set_value_cansleep(ctx->enable_gpio, 0);
		ctx->enabled = false;
		return 0;
	}

	if (!ctx->enabled) {
		gpiod_set_value_cansleep(ctx->enable_gpio, 1);
		usleep_range(1000, 2000);
		ret = regmap_write(ctx->regmap, LM3530_BRT_CTRL, 0);
		if (ret)
			return ret;
		ret = regmap_write(ctx->regmap, LM3530_GEN_CONFIG, ctx->gen_config);
		if (ret)
			return ret;
		ctx->enabled = true;
	}

	return regmap_write(ctx->regmap, LM3530_BRT_CTRL, brightness);
}

static const struct backlight_ops lm3530_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3530_bl_update_status,
};

static const struct regmap_config lm3530_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int lm3530_bl_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.scale = BACKLIGHT_SCALE_NON_LINEAR,
		.max_brightness = LM3530_MAX_BRIGHTNESS,
		.brightness = 64,
	};
	struct backlight_device *bl;
	struct lm3530_bl *ctx;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->regmap = devm_regmap_init_i2c(client, &lm3530_regmap_config);
	if (IS_ERR(ctx->regmap))
		return PTR_ERR(ctx->regmap);

	ctx->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->enable_gpio),
				     "Failed to get enable gpio\n");

	/*
	 * General config: I2C enable, linear mapping, full-scale current.
	 * Bit 5 (PWM input enable) would scale the output by the PWM pin duty
	 * cycle, so leave it off unless something drives that pin.
	 */
	ctx->gen_config = 0x17;
	of_property_read_u32(dev->of_node, "ti,general-config", &ctx->gen_config);
	of_property_read_u32(dev->of_node, "default-brightness", &props.brightness);
	props.brightness = min_t(u32, props.brightness, LM3530_MAX_BRIGHTNESS);

	bl = devm_backlight_device_register(dev, dev_name(dev), dev, ctx,
					    &lm3530_bl_ops, &props);
	if (IS_ERR(bl))
		return dev_err_probe(dev, PTR_ERR(bl), "Failed to register backlight\n");

	i2c_set_clientdata(client, bl);

	return backlight_update_status(bl);
}

static const struct of_device_id lm3530_bl_of_match[] = {
	{ .compatible = "ti,lm3530-backlight" },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3530_bl_of_match);

static struct i2c_driver lm3530_bl_driver = {
	.driver = {
		.name = "lm3530-backlight",
		.of_match_table = lm3530_bl_of_match,
	},
	.probe = lm3530_bl_probe,
};
module_i2c_driver(lm3530_bl_driver);

MODULE_DESCRIPTION("TI LM3530 backlight driver");
MODULE_LICENSE("GPL");
