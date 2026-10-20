// SPDX-License-Identifier: GPL-2.0-only
/*
 * PP2106 keypad controller (LG Wine Smart / vfp)
 *
 * The controller scans a key matrix and reports every key press/release as a
 * single byte that the host clocks out over a bit-banged two-wire bus after
 * the controller pulls its IRQ line low:
 *
 *   bit 7     1 = release, 0 = press
 *   bits 6:4  row
 *   bits 3:0  column + 1
 *
 * Based on the LGE downstream driver.
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>

#define PP2106_BIT_DELAY_US	25

struct pp2106 {
	struct device *dev;
	struct input_dev *input;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *sda_gpio;
	struct gpio_desc *scl_gpio;
	struct gpio_desc *irq_gpio;
	unsigned int rows;
	unsigned int cols;
	unsigned int row_shift;
	unsigned short *keymap;
};

static void pp2106_delay(void)
{
	udelay(PP2106_BIT_DELAY_US);
}

static void pp2106_scl(struct pp2106 *kp, int val)
{
	gpiod_set_value(kp->scl_gpio, val);
	pp2106_delay();
}

static u8 pp2106_read_byte(struct pp2106 *kp)
{
	u8 data = 0;
	int i;

	/* Start: SDA low while SCL is high */
	gpiod_direction_output(kp->sda_gpio, 0);
	pp2106_delay();
	pp2106_scl(kp, 1);

	/* Start bit clock, then hand SDA over to the controller */
	pp2106_scl(kp, 0);
	gpiod_direction_input(kp->sda_gpio);
	pp2106_delay();
	pp2106_scl(kp, 1);

	/* 8 data bits, MSB first, sampled while SCL is low */
	for (i = 7; i >= 0; i--) {
		pp2106_scl(kp, 0);
		if (gpiod_get_value(kp->sda_gpio))
			data |= BIT(i);
		pp2106_scl(kp, 1);
	}

	/* Ack */
	gpiod_direction_output(kp->sda_gpio, 0);
	pp2106_delay();
	pp2106_scl(kp, 0);
	gpiod_direction_input(kp->sda_gpio);
	pp2106_scl(kp, 1);

	return data;
}

static irqreturn_t pp2106_irq(int irq, void *dev_id)
{
	struct pp2106 *kp = dev_id;
	unsigned int row, col, code;
	bool pressed;
	u8 data;

	data = pp2106_read_byte(kp);

	pressed = !(data & BIT(7));
	row = (data >> 4) & 0x7;
	col = data & 0xf;
	if (col)
		col--;

	dev_dbg(kp->dev, "data 0x%02x: row %u col %u %s\n", data, row, col,
		pressed ? "pressed" : "released");

	if (row >= kp->rows || col >= kp->cols)
		return IRQ_HANDLED;

	code = MATRIX_SCAN_CODE(row, col, kp->row_shift);
	input_event(kp->input, EV_MSC, MSC_SCAN, code);
	input_report_key(kp->input, kp->keymap[code], pressed);
	input_sync(kp->input);

	return IRQ_HANDLED;
}

static void pp2106_reset(struct pp2106 *kp)
{
	gpiod_set_value_cansleep(kp->reset_gpio, 1);
	msleep(25);
	gpiod_set_value_cansleep(kp->reset_gpio, 0);
	msleep(25);
}

static void pp2106_release_all(struct pp2106 *kp)
{
	struct input_dev *input = kp->input;
	unsigned int code;

	for_each_set_bit(code, input->key, KEY_CNT)
		input_report_key(input, code, 0);
	input_sync(input);
}

static int pp2106_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_desc *legacy_irq;
	struct pp2106 *kp;
	int irq, ret;

	kp = devm_kzalloc(dev, sizeof(*kp), GFP_KERNEL);
	if (!kp)
		return -ENOMEM;
	kp->dev = dev;

	kp->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(kp->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(kp->reset_gpio), "Failed to get reset gpio\n");

	kp->scl_gpio = devm_gpiod_get(dev, "scl", GPIOD_OUT_HIGH);
	if (IS_ERR(kp->scl_gpio))
		return dev_err_probe(dev, PTR_ERR(kp->scl_gpio), "Failed to get scl gpio\n");

	kp->sda_gpio = devm_gpiod_get(dev, "sda", GPIOD_IN);
	if (IS_ERR(kp->sda_gpio))
		return dev_err_probe(dev, PTR_ERR(kp->sda_gpio), "Failed to get sda gpio\n");

	kp->irq_gpio = devm_gpiod_get(dev, "irq", GPIOD_IN);
	if (IS_ERR(kp->irq_gpio))
		return dev_err_probe(dev, PTR_ERR(kp->irq_gpio), "Failed to get irq gpio\n");

	/*
	 * Older boards wire the interrupt to another pin. That pin is pulled
	 * down and left unconnected on newer boards, while the controller
	 * keeps its idle interrupt output high on the older ones.
	 */
	legacy_irq = devm_gpiod_get_optional(dev, "legacy-irq", GPIOD_IN);
	if (IS_ERR(legacy_irq))
		return dev_err_probe(dev, PTR_ERR(legacy_irq), "Failed to get legacy irq gpio\n");
	if (legacy_irq && gpiod_get_value(legacy_irq) == 1) {
		dev_info(dev, "old board revision, using legacy interrupt pin\n");
		devm_gpiod_put(dev, kp->irq_gpio);
		kp->irq_gpio = legacy_irq;
		gpiod_set_config(kp->irq_gpio,
				 pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_UP, 1));
	}

	irq = gpiod_to_irq(kp->irq_gpio);
	if (irq < 0)
		return dev_err_probe(dev, irq, "Failed to get interrupt\n");

	ret = matrix_keypad_parse_properties(dev, &kp->rows, &kp->cols);
	if (ret)
		return ret;
	kp->row_shift = get_count_order(kp->cols);

	kp->input = devm_input_allocate_device(dev);
	if (!kp->input)
		return -ENOMEM;

	kp->input->name = "pp2106-keypad";
	kp->input->phys = "pp2106/input0";
	kp->input->id.bustype = BUS_HOST;

	ret = matrix_keypad_build_keymap(NULL, NULL, kp->rows, kp->cols, NULL, kp->input);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to build keymap\n");
	kp->keymap = kp->input->keycode;

	input_set_capability(kp->input, EV_MSC, MSC_SCAN);

	ret = input_register_device(kp->input);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, kp);
	pp2106_reset(kp);

	ret = devm_request_threaded_irq(dev, irq, NULL, pp2106_irq,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"pp2106-keypad", kp);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to request interrupt\n");

	device_init_wakeup(dev, true);

	return 0;
}

static int pp2106_suspend(struct device *dev)
{
	struct pp2106 *kp = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(gpiod_to_irq(kp->irq_gpio));

	return 0;
}

static int pp2106_resume(struct device *dev)
{
	struct pp2106 *kp = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(gpiod_to_irq(kp->irq_gpio));

	/* The downstream driver resets the controller on every resume */
	pp2106_release_all(kp);
	pp2106_reset(kp);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(pp2106_pm_ops, pp2106_suspend, pp2106_resume);

static const struct of_device_id pp2106_of_match[] = {
	{ .compatible = "lge,pp2106-keypad" },
	{ }
};
MODULE_DEVICE_TABLE(of, pp2106_of_match);

static struct platform_driver pp2106_driver = {
	.probe = pp2106_probe,
	.driver = {
		.name = "pp2106-keypad",
		.of_match_table = pp2106_of_match,
		.pm = pm_sleep_ptr(&pp2106_pm_ops),
	},
};
module_platform_driver(pp2106_driver);

MODULE_DESCRIPTION("PP2106 keypad controller driver");
MODULE_LICENSE("GPL");
