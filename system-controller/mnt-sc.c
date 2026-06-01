// SPDX-License-Identifier: GPL-2.0-only
/*
 * MNT Reform System Controller
 *
 * Copyright 2022 nanocodebug <nanocodebug@gmail.com>
 * Copyright 2023 Michael Fincham <michael@hotplate.co.nz>
 * Copyright 2024 Michal Suchánek <hramrach@gmail.com>
 * Copyright 2024-2026 Lucie Lukas Hartmann <lukas@mntre.com>
 * Copyright 2023-2025 Johannes Schauer Marin Rodrigues <josch@mister-muffin.de>
 */

#define DEBUG

#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/slab.h>
#include <linux/math.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/backlight.h>
#include <linux/version.h>
#include <linux/suspend.h>
#include <linux/reboot.h>

#define MNTSC_API_UNKNOWN 0
#define MNTSC_API_V1 1
#define MNTSC_API_V2 2
#define MNTSC_API_V3 3

/* array size for mntsc response buffers */
#define MNTSC_RESP_SZ 8

struct mntsc_driver_data {
	struct spi_device *spi;
	struct power_supply *bat;
	struct mutex lock;
	struct backlight_device *backlight;
	struct gpio_chip gc;
	struct notifier_block suspend_notifier;
	uint32_t api_version;
};

static int mntsc_power_off(struct sys_off_data *data);
static ssize_t show_status(struct device *dev, struct device_attribute *attr,
			   char *buf);
static ssize_t show_cells(struct device *dev, struct device_attribute *attr,
			  char *buf);
static ssize_t show_firmware(struct device *dev, struct device_attribute *attr,
			     char *buf);
static ssize_t show_capacity(struct device *dev, struct device_attribute *attr,
			     char *buf);
static ssize_t show_uart(struct device *dev, struct device_attribute *attr,
			 char *buf);
static ssize_t store_uart(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count);
static ssize_t sc_cmdresp(struct mntsc_driver_data *mntsc, char command,
			   uint8_t arg1, uint8_t *response);
static int get_bat_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val);

// Send a command and discard the response
#define sc_cmd(mntsc, cmd, arg1) sc_cmdresp(mntsc, cmd, arg1, NULL)

static DEVICE_ATTR(status, 0444, show_status, NULL);
static DEVICE_ATTR(cells, 0444, show_cells, NULL);
static DEVICE_ATTR(firmware, 0444, show_firmware, NULL);
static DEVICE_ATTR(capacity, 0444, show_capacity, NULL);
static DEVICE_ATTR(uart, 0644, show_uart, store_uart);

static struct spi_board_info mntsc_board_info = {
	.modalias = "mntsc",
	.max_speed_hz = 4000000,
	.bus_num = 0,
	.chip_select = 0,
	.mode = SPI_MODE_1,
};

static enum power_supply_property bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_EMPTY,
	POWER_SUPPLY_PROP_PRESENT,
};

static struct power_supply_desc bat_desc = {
	.name = "BAT0",
	.properties = bat_props,
	.num_properties = ARRAY_SIZE(bat_props),
	.get_property = get_bat_property,
	.type = POWER_SUPPLY_TYPE_BATTERY,
};

static struct power_supply_config psy_cfg = {};

static int bl_get_brightness(struct backlight_device *bl)
{
	u16 brightness = bl->props.brightness;
	return brightness & 0x7f;
}

static int bl_update_status(struct backlight_device *bl)
{
	struct mntsc_driver_data *mntsc = (struct mntsc_driver_data *)bl_get_data(bl);

	sc_cmd(mntsc, 'b', bl->props.brightness);
	return 0;
}

static const struct backlight_ops mntsc_bl_ops = {
	.update_status = bl_update_status,
	.get_brightness = bl_get_brightness,
};

static struct backlight_device *mntsc_create_backlight(struct device *dev,
						     void *data)
{
	struct backlight_properties props;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.brightness = 100;
	props.max_brightness = 100;

	return devm_backlight_device_register(dev,
					      "mntsc_backlight",
					      dev, data, &mntsc_bl_ops, &props);
}

static uint8_t mntsc_calc_checksum(uint8_t *buffer, int len)
{
	uint8_t sum = 0;
	for (int i = 0; i < len - 1; i++) {
		sum = sum ^ buffer[i];
	}
	return sum;
}

static int mntsc_confirm_checksum(uint8_t *buffer, int len)
{
	return (buffer[len - 1] == mntsc_calc_checksum(buffer, len));
}

static uint32_t mntsc_get_api_version(struct device *dev)
{
	int ret;
	uint32_t version;
	uint8_t str[MNTSC_RESP_SZ + 1] = { 0 };
	struct mntsc_driver_data *mntsc =
		(struct mntsc_driver_data *)dev_get_drvdata(dev);

	ret = sc_cmdresp(mntsc, 'f', 2, str);
	if (ret)
		return MNTSC_API_UNKNOWN;

	ret = kstrtou32(str, 10, &version);

	ret = MNTSC_API_UNKNOWN;
	if (version > 20200000 && version < 20250526) {
		ret = MNTSC_API_V1;
	} else if (version >= 20250526 && version < 20260315) {
		ret = MNTSC_API_V2;
	} else if (version >= 20260315 && version <= 30000101) {
		ret = MNTSC_API_V3;
	}
	dev_dbg(dev, "raw version: %u (%s), mntsc API version: %d.\n", version,
		 str, ret);
	return ret;
}

static int mntsc_gpio_set(struct gpio_chip *gc, unsigned int offset, int value)
{
	int ret;
	struct mntsc_driver_data *mntsc =
		(struct mntsc_driver_data *)dev_get_drvdata(gc->parent);

	dev_info(gc->parent, "%s: %s <- %d\n", __func__, gc->names[offset], value);
	ret = sc_cmd(mntsc, value ? 'G' : 'g', offset);
	if (ret) {
		dev_err(gc->parent, "%s: %d <- %d error %d\n", __func__, offset,
			value, ret);
		return ret;
	}

	return 0;
}

static int mntsc_gpio_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	return GPIO_LINE_DIRECTION_OUT;
}


static int lpc_suspend_cb(struct notifier_block *nb, unsigned long action,
			void *_data)
{
	struct mntsc_driver_data *data = container_of(nb, struct mntsc_driver_data, suspend_notifier);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		dev_info(&data->spi->dev, "%s: set brightness %u\n", __func__, 0);
		sc_cmd(data, 'b', 0);
		break;
	case PM_POST_SUSPEND:
		dev_info(&data->spi->dev, "%s: set brightness %u\n", __func__, data->backlight->props.brightness);
		sc_cmd(data, 'b', data->backlight->props.brightness);
		break;
	}

	return NOTIFY_DONE;
}

static int mntsc_probe(struct spi_device *spi)
{
	struct mntsc_driver_data *data;
	struct device_node *backlight;
	int ret;

	spi->max_speed_hz = mntsc_board_info.max_speed_hz;
	spi->mode = mntsc_board_info.mode;
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret) {
		dev_err(&spi->dev, "spi_setup failed.\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&spi->dev, sizeof(struct mntsc_driver_data),
			    GFP_KERNEL);
	if (data == NULL) {
		dev_err(&spi->dev, "devm_kzalloc failed.\n");
		return -ENOMEM;
	}

	data->spi = spi;
	mutex_init(&data->lock);
	spi_set_drvdata(spi, data);

	ret = device_create_file(&spi->dev, &dev_attr_status);
	if (ret) {
		dev_err(&spi->dev,
			"device_create_file dev_attr_status failed.\n");
		return ret;
	}

	ret = device_create_file(&spi->dev, &dev_attr_cells);
	if (ret) {
		dev_err(&spi->dev,
			"device_create_file dev_attr_cells failed.\n");
		return ret;
	}

	ret = device_create_file(&spi->dev, &dev_attr_firmware);
	if (ret) {
		dev_err(&spi->dev,
			"device_create_file dev_attr_firmware failed.\n");
		return ret;
	}

	ret = device_create_file(&spi->dev, &dev_attr_capacity);
	if (ret) {
		dev_err(&spi->dev,
			"device_create_file dev_attr_capacity failed.\n");
		return ret;
	}

	ret = device_create_file(&spi->dev, &dev_attr_uart);
	if (ret) {
		dev_err(&spi->dev,
			"device_create_file dev_attr_uart failed.\n");
		return ret;
	}

	data->api_version = mntsc_get_api_version(&spi->dev);
	dev_info(&spi->dev, "MNT System Controller v%d\n", data->api_version);

	psy_cfg.fwnode = dev_fwnode(&spi->dev);
	psy_cfg.drv_data = data;
	data->bat = devm_power_supply_register(&spi->dev, &bat_desc, &psy_cfg);
	if (IS_ERR(data->bat)) {
		dev_err(&spi->dev, "dev_power_supply_register failed.\n");
		return PTR_ERR(data->bat);
	}

	/* register mntsc as poweroff handler */
	ret = devm_register_sys_off_handler(&spi->dev,
					    SYS_OFF_MODE_POWER_OFF_PREPARE,
					    SYS_OFF_PRIO_FIRMWARE,
					    mntsc_power_off, data);
	if (ret) {
		dev_err(&spi->dev, "devm_register_sys_off_handler failed.\n");
		return ret;
	}

	/* Register backlight device if we have a backlight node */
	backlight = of_get_child_by_name(spi->dev.of_node, "backlight");
	if (backlight && of_device_is_available(backlight)) {
		dev_dbg(
			&spi->dev,
			"enabling backlight control for MNT Pocket Reform with Display Version 2.\n");
		data->backlight = mntsc_create_backlight(&spi->dev, data);
		if (IS_ERR(data->backlight)) {
			dev_err(&spi->dev, "mntsc_create_backlight failed.\n");
		}

		data->suspend_notifier.notifier_call = lpc_suspend_cb;
		register_pm_notifier(&data->suspend_notifier);
	}

	data->gc.request = gpiochip_generic_request;
	data->gc.free = gpiochip_generic_free;
	data->gc.base = -1;
	data->gc.set = mntsc_gpio_set;
	data->gc.get_direction = mntsc_gpio_get_direction;
	data->gc.ngpio = 3;
	data->gc.label = dev_name(&spi->dev);
	data->gc.parent = &spi->dev;
	data->gc.owner = THIS_MODULE;
	data->gc.can_sleep = true;
	data->gc.names =
		(const char *const[]){ "disp_reset", "3v3_en", "1v1_en" };

	return ret;
}

static void mntsc_remove(struct spi_device *spi)
{
	device_remove_file(&spi->dev, &dev_attr_status);
	device_remove_file(&spi->dev, &dev_attr_firmware);
	device_remove_file(&spi->dev, &dev_attr_cells);
	device_remove_file(&spi->dev, &dev_attr_capacity);
	device_remove_file(&spi->dev, &dev_attr_uart);
}

static ssize_t sc_cmdresp(struct mntsc_driver_data *mntsc, char command,
			   uint8_t arg1, uint8_t *response)
{
	static int delays[3] = { 2, 10, 0 };
	u8 resp[MNTSC_RESP_SZ] = { 0 };
	int ret;

	uint8_t cmd[4] = { 0xb5, command, arg1, 0x0 };
	cmd[3] = mntsc_calc_checksum(cmd, 4);

	mutex_lock(&mntsc->lock);

	msleep(delays[0]);
	ret = spi_write(mntsc->spi, cmd, 4);
	if (ret) {
		dev_err(&mntsc->spi->dev,
			"sc_command: %c/%d spi_write failed.\n", command,
			arg1);
		mutex_unlock(&mntsc->lock);
		return ret;
	}

	msleep(delays[1]);
	ret = spi_read(mntsc->spi, resp, 8);
	if (ret) {
		dev_err(&mntsc->spi->dev, "sc_command: %c/%d spi_read failed.\n",
			command, arg1);
	}
	msleep(delays[2]);
	mutex_unlock(&mntsc->lock);

	dev_info(&mntsc->spi->dev, "%02x%02x%02x%02x %02x%02x%02x%02x\n",
		resp[0], resp[1], resp[2], resp[3], resp[4], resp[5], resp[6], resp[7]);

	if (!mntsc_confirm_checksum(resp, MNTSC_RESP_SZ)) {
		dev_dbg(&mntsc->spi->dev,
			"sc_command: %c/%d checksum mismatch: %02x!=%02x",
			command, arg1, mntsc_calc_checksum(resp, MNTSC_RESP_SZ),
			resp[MNTSC_RESP_SZ - 1]);
		return -EAGAIN;
	}

	if (response)
		memcpy(response, resp, MNTSC_RESP_SZ);

	return ret;
}

static ssize_t sc_cmdresp_retry(struct mntsc_driver_data *mntsc, char command,
			   uint8_t arg1, uint8_t *response)
{
	int ret = -EAGAIN, i;
	for (i = 0; i < 3 && ret == -EAGAIN; i++)
		ret = sc_cmdresp(mntsc, command, arg1, response);

	if (i == 3 && ret == -EAGAIN)
		dev_err(&mntsc->spi->dev, "cmd failed after %d retries!", i);

	return ret;
}

static ssize_t show_uart(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	/* not yet implemented */
	return 0;
}

/* let mntsc output bytes over UART to MNT Desktop Reform control panel */
static ssize_t store_uart(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct mntsc_driver_data *mntsc =
		(struct mntsc_driver_data *)dev_get_drvdata(dev);
	for (size_t i = 0; i < count; i++) {
		sc_cmd(mntsc, 'z', buf[i]);
	}
	return count;
}

static ssize_t show_status(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int ret = 0;
	uint8_t buffer[8];
	int16_t voltage;
	int16_t amps;
	uint8_t percentage;
	uint8_t status;
	struct mntsc_driver_data *mntsc =
		(struct mntsc_driver_data *)dev_get_drvdata(dev);

	ret = sc_cmdresp_retry(mntsc, 'q', 0, buffer);
	if (ret)
		return 0;

	voltage = (int16_t)buffer[0] | ((int16_t)buffer[1] << 8);
	amps = (int16_t)buffer[2] | ((int16_t)buffer[3] << 8);
	percentage = buffer[4];
	status = buffer[5];

	return snprintf(buf, PAGE_SIZE,
			"%d.%dV %d.%dA %2d%% [status=%d] [API=%d]\n",
			voltage / 1000, voltage % 1000, amps / 1000,
			abs(amps % 1000), percentage, status, mntsc->api_version);

	return snprintf(buf, PAGE_SIZE, "ok\n");
}

static ssize_t show_cells(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int ret = 0;
	uint8_t buffer[MNTSC_RESP_SZ * 2];
	uint16_t cells[16];
	struct mntsc_driver_data *mntsc =
		(struct mntsc_driver_data *)dev_get_drvdata(dev);

	ret = sc_cmdresp_retry(mntsc, 'v', 0, buffer);
	if (ret)
		return 0;
	ret = sc_cmdresp_retry(mntsc, 'v', 1, &buffer[8]);
	if (ret)
		return 0;

	for (int s = 0; s < 16; s += 2) {
		uint16_t val = buffer[s] | buffer[s + 1] << 8;
		cells[s] = val / 1000;
		cells[s + 1] = val % 1000;
	}

	ret = snprintf(
		buf, PAGE_SIZE,
		"%d.%03d %d.%03d %d.%03d %d.%03d %d.%03d %d.%03d %d.%03d %d.%03d\n",
		cells[0], cells[1], cells[2], cells[3], cells[4], cells[5],
		cells[6], cells[7], cells[8], cells[9], cells[10], cells[11],
		cells[12], cells[13], cells[14], cells[15]);

	return ret;
}

static ssize_t show_firmware(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int ret = 0;
	uint8_t str1[MNTSC_RESP_SZ], str2[MNTSC_RESP_SZ], str3[MNTSC_RESP_SZ];
	struct mntsc_driver_data *mntsc =
		(struct mntsc_driver_data *)dev_get_drvdata(dev);

	ret = sc_cmdresp_retry(mntsc, 'f', 0, str1);
	if (ret)
		return 0;
	ret = sc_cmdresp_retry(mntsc, 'f', 1, str2);
	if (ret)
		return 0;
	ret = sc_cmdresp_retry(mntsc, 'f', 2, str3);
	if (ret)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s %s %s\n", str1, str2, str3);
}

static ssize_t show_capacity(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	uint8_t buffer[MNTSC_RESP_SZ];
	uint16_t cap_acc_mah, cap_min_mah, cap_max_mah;
	struct mntsc_driver_data *mntsc =
		(struct mntsc_driver_data *)dev_get_drvdata(dev);

	sc_cmdresp_retry(mntsc, 'c', 0, buffer);

	cap_acc_mah = buffer[0] | (buffer[1] << 8);
	cap_min_mah = buffer[2] | (buffer[3] << 8);
	cap_max_mah = buffer[4] | (buffer[5] << 8);

	return snprintf(buf, PAGE_SIZE, "[acc=%dmAh] [min=%dmAh] [max=%dmAh]\n",
			cap_acc_mah, cap_min_mah, cap_max_mah);
}

static int mntsc_power_off(struct sys_off_data *data)
{
	struct mntsc_driver_data *mntsc = (struct mntsc_driver_data *)data->cb_data;

	/* try to shut down power, forever */
	while (true) {
		sc_cmd(mntsc, 'p', 1);
		msleep(100);
	}
	return 0;
}

static int get_bat_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	uint8_t buffer[MNTSC_RESP_SZ];
	int milliamp, millivolt;
	struct mntsc_driver_data *mntsc =
		(struct mntsc_driver_data *)power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		ret = sc_cmdresp_retry(mntsc, 'q', 0, buffer);
		if (ret)
			return -EBUSY;

		int16_t ma16 = ((int16_t)buffer[2] | ((int16_t)buffer[3] << 8));
		milliamp = (int)ma16;

		if (milliamp < -100) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else if (milliamp <= 100) {
			if (buffer[4] == 100) {
				val->intval = POWER_SUPPLY_STATUS_FULL;
			} else {
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
		} else {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sc_cmdresp_retry(mntsc, 'q', 0, buffer);
		if (ret)
			return -EBUSY;

		millivolt = (buffer[0] | buffer[1] << 8);
		if (millivolt < 5000 || millivolt >= 40000)
			millivolt = 0;

		val->intval = millivolt * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sc_cmdresp_retry(mntsc, 'q', 0, buffer);
		if (ret)
			return -EBUSY;

		ma16 = (int16_t)buffer[2] | ((int16_t)buffer[3] << 8);
		milliamp = (int)ma16;
		if (milliamp < -20000 || milliamp >= 20000)
			return 0;

		/* clamp noise around zero mA currents */
		if (milliamp >= -5 && milliamp <= 5) {
			milliamp = 0;
		}

		/* system controller and linux disagree on which sign
		 * means charging and which means discharging */
		val->intval = -milliamp * 1000;

		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		ret = sc_cmdresp_retry(mntsc, 'q', 0, buffer);
		if (ret)
			return -EBUSY;

		val->intval = buffer[4] > 6 ? buffer[4] : 6;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = sc_cmdresp_retry(mntsc, 'c', 0, buffer);
		if (ret)
			return -EBUSY;

		int milliamp_hours = (buffer[4] | buffer[5] << 8);
		val->intval = milliamp_hours * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = sc_cmdresp_retry(mntsc, 'c', 0, buffer);
		if (ret)
			return -EBUSY;

		val->intval = (buffer[0] | buffer[1] << 8) * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		ret = sc_cmdresp_retry(mntsc, 'c', 0, buffer);
		if (ret)
			return -EBUSY;

		val->intval = (buffer[2] | buffer[3] << 8) * 1000;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;

	default:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct of_device_id of_mntre_sc_match[] = {
	{ .compatible = "mntre,system-controller", .data = 0 },
	{}
};
MODULE_DEVICE_TABLE(of, of_mntre_sc_match);

static struct spi_device_id mntsc_spi_dev_id[] = {
	{ "mntre-system-controller", 0 },
	{},
};
MODULE_DEVICE_TABLE(spi, mntsc_spi_dev_id);

static struct spi_driver mntre_sc = {
	.probe = mntsc_probe,
	.remove = mntsc_remove,
	.driver = {
		.of_match_table = of_match_ptr(of_mntre_sc_match),
		.owner = THIS_MODULE,
		.name = "mnt_syscon",
	},
	.id_table = mntsc_spi_dev_id,
};
module_spi_driver(mntre_sc);

MODULE_DESCRIPTION("Reform 2 mntsc Driver");
MODULE_LICENSE("GPL");
