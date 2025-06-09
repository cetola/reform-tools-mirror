/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright 2022 nanocodebug <nanocodebug@gmail.com>
 * Copyright 2023 Michael Fincham <michael@hotplate.co.nz>
 * Copyright 2024 Michal Suchánek <hramrach@gmail.com>
 * Copyright 2024-2025 Lucie Lukas Hartmann <lukas@mntre.com>
 * Copyright 2023-2025 Johannes Schauer Marin Rodrigues <josch@mister-muffin.de>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/math.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/backlight.h>
#include <linux/version.h>

static int lpc_probe(struct spi_device *spi);
static void lpc_remove(struct spi_device *spi);
static void lpc_power_off(void);
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
static ssize_t lpc_command(struct device *dev, char command, uint8_t arg1,
			   uint8_t *response);
static int get_bat_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val);

#define MNTRE_LPC_API_UNKNOWN 0
#define MNTRE_LPC_API_1 1
#define MNTRE_LPC_API_2 2

typedef struct lpc_driver_data {
	struct spi_device *spi;
	struct power_supply *bat;
	struct mutex lock;
	struct backlight_device *backlight;
	uint32_t api_version;
} lpc_driver_data;

static DEVICE_ATTR(status, 0444, show_status, NULL);
static DEVICE_ATTR(cells, 0444, show_cells, NULL);
static DEVICE_ATTR(firmware, 0444, show_firmware, NULL);
static DEVICE_ATTR(capacity, 0444, show_capacity, NULL);
static DEVICE_ATTR(uart, 0644, show_uart, store_uart);

static struct spi_board_info g_spi_board_info = {
	.modalias = "reform2_lpc",
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

static struct device *poweroff_device;

static int bl_get_brightness(struct backlight_device *bl)
{
	u16 brightness = bl->props.brightness;
	return brightness & 0x7f;
}

static int bl_update_status(struct backlight_device *bl)
{
	struct lpc_driver_data *data =
		(struct lpc_driver_data *)bl_get_data(bl);
	uint8_t buffer[8];
	lpc_command(&data->spi->dev, 'b', bl->props.brightness, buffer);
	return 0;
}

static const struct backlight_ops lpc_bl_ops = {
	.update_status = bl_update_status,
	.get_brightness = bl_get_brightness,
};

static struct backlight_device *lpc_create_backlight(struct device *dev,
						     void *data)
{
	struct backlight_properties props;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.brightness = 100;
	props.max_brightness = 100;

	return devm_backlight_device_register(dev,
					      "mnt_pocket_reform_backlight",
					      dev, data, &lpc_bl_ops, &props);
}

int (*__mnt_pocket_reform_get_panel_version)(void);

static uint32_t lpc_get_api_version(struct device *dev)
{
	int ret;
	uint32_t version;
	uint8_t str[9];

	ret = lpc_command(dev, 'f', 2, str);
	if (ret)
		return MNTRE_LPC_API_UNKNOWN;

	ret = kstrtou32(str, 10, &version);
	dev_info(dev, "version: %u (%s)\n", version, str);

	if (version > 20200000 && version < 20250526) {
		return MNTRE_LPC_API_1;
	} else if (version >= 20250526 && version <= 30000101) {
		return MNTRE_LPC_API_2;
	}
	return MNTRE_LPC_API_UNKNOWN;
}

static int lpc_probe(struct spi_device *spi)
{
	struct lpc_driver_data *data;
	int ret;

	spi->max_speed_hz = g_spi_board_info.max_speed_hz;
	spi->mode = g_spi_board_info.mode;
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret) {
		dev_err(&spi->dev, "spi_setup failed\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct lpc_driver_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&spi->dev, "kzalloc failed\n");
		return -ENOMEM;
	}

	data->spi = spi;
	mutex_init(&data->lock);
	spi_set_drvdata(spi, data);

	ret = device_create_file(&spi->dev, &dev_attr_status);
	if (ret)
		dev_err(&spi->dev, "device_create_file failed\n");

	ret = device_create_file(&spi->dev, &dev_attr_cells);
	if (ret)
		dev_err(&spi->dev, "device_create_file failed\n");

	ret = device_create_file(&spi->dev, &dev_attr_firmware);
	if (ret)
		dev_err(&spi->dev, "device_create_file failed\n");

	ret = device_create_file(&spi->dev, &dev_attr_capacity);
	if (ret)
		dev_err(&spi->dev, "device_create_file failed\n");

	ret = device_create_file(&spi->dev, &dev_attr_uart);
	if (ret)
		dev_err(&spi->dev, "device_create_file failed\n");

	psy_cfg.of_node = spi->dev.of_node;
	psy_cfg.drv_data = data;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	psy_cfg.no_wakeup_source = true;
	data->bat = power_supply_register(&spi->dev, &bat_desc, &psy_cfg);
#else
	data->bat = power_supply_register_no_ws(&spi->dev, &bat_desc, &psy_cfg);
#endif
	if (IS_ERR(data->bat)) {
		dev_err(&spi->dev, "power_supply_register_no_ws failed\n");
		return PTR_ERR(data->bat);
	}

	/* FIXME: this overwrites something else that has already claimed pm_power_off
	   on reform2 but it'll do for now */
	poweroff_device = &spi->dev;
	pm_power_off = lpc_power_off;

	/* for MNT Pocket Reform with Display Version 2, the
	   system controller has to control the backlight
	   directly via PWM, but it must not do that on
	   other versions of the display. */
	__mnt_pocket_reform_get_panel_version =
		(void *)__symbol_get("mnt_pocket_reform_get_panel_version");

	if (__mnt_pocket_reform_get_panel_version &&
	    __mnt_pocket_reform_get_panel_version() == 2) {
		dev_info(
			&spi->dev,
			"enabling backlight control for MNT Pocket Reform with Display Version 2.\n");
		data->backlight = lpc_create_backlight(&spi->dev, data);
		if (IS_ERR(data->backlight)) {
			dev_err(&spi->dev, "lpc_create_backlight failed\n");
		}
	}

	data->api_version = lpc_get_api_version(&spi->dev);

	return ret;
}

static void lpc_remove(struct spi_device *spi)
{
	struct lpc_driver_data *data =
		(struct lpc_driver_data *)spi_get_drvdata(spi);

	device_remove_file(&spi->dev, &dev_attr_status);
	device_remove_file(&spi->dev, &dev_attr_firmware);
	device_remove_file(&spi->dev, &dev_attr_cells);
	device_remove_file(&spi->dev, &dev_attr_capacity);
	device_remove_file(&spi->dev, &dev_attr_uart);

	power_supply_unregister(data->bat);

	if (pm_power_off == &lpc_power_off) {
		pm_power_off = NULL;
	}

	kfree(data);
}

static ssize_t lpc_command(struct device *dev, char command, uint8_t arg1,
			   uint8_t *response)
{
	int ret = 0;
	struct lpc_driver_data *data =
		(struct lpc_driver_data *)dev_get_drvdata(dev);

	int delays[3] = { 20, 20, 20 };
	if (data->api_version == 2) {
		/* newer LPC firmware doesn't need huge delays */
		/* because the response time is minimized */
		delays[0] = 2;
		delays[1] = 3;
		delays[2] = 0;
	}

	uint8_t cmd[4] = { 0xb5, command, arg1, 0x0 };

	mutex_lock(&data->lock);

	msleep(delays[0]);
	ret = spi_write(data->spi, cmd, 4);
	if (ret) {
		dev_err(dev, "lpc_command: %c/%d spi_write failed\n", command,
			arg1);
		mutex_unlock(&data->lock);
		return ret;
	}

	msleep(delays[1]);
	ret = spi_read(data->spi, response, 8);
	if (ret) {
		dev_err(dev, "lpc_command: %c/%d spi_read failed\n", command,
			arg1);
	}
	msleep(delays[2]);

	mutex_unlock(&data->lock);
	return ret;
}

static ssize_t show_uart(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	/* not yet implemented */
	return 0;
}

/* let LPC output bytes over UART to MNT Desktop Reform control panel */
static ssize_t store_uart(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	uint8_t discard[8];
	for (size_t i = 0; i < count; i++) {
		lpc_command(dev, 'z', buf[i], discard);
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
	struct lpc_driver_data *data;
	data = (struct lpc_driver_data *)dev_get_drvdata(dev);

	ret = lpc_command(dev, 'q', 0, buffer);
	if (ret)
		return 0;

	voltage = (int16_t)buffer[0] | ((int16_t)buffer[1] << 8);
	amps = (int16_t)buffer[2] | ((int16_t)buffer[3] << 8);
	percentage = buffer[4];
	status = buffer[5];

	return snprintf(buf, PAGE_SIZE,
			"%d.%dV %d.%dA %2d%% [status=%d] [API=%d]\n",
			voltage / 1000, voltage % 1000, amps / 1000,
			abs(amps % 1000), percentage, status,
			data->api_version);

	return snprintf(buf, PAGE_SIZE, "ok\n");
}

static ssize_t show_cells(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int ret = 0;
	uint8_t buffer[16];
	uint16_t cells[16];

	ret = lpc_command(dev, 'v', 0, buffer);
	if (ret)
		return 0;
	ret = lpc_command(dev, 'v', 1, &buffer[8]);
	if (ret)
		return 0;

	for (int s = 0; s < 16; s += 2) {
		uint16_t val = buffer[s] | buffer[s + 1] << 8;
		cells[s] = val / 1000;
		cells[s + 1] = val % 1000;
	}

	ret = snprintf(buf, PAGE_SIZE,
		       "%d.%d %d.%d %d.%d %d.%d %d.%d %d.%d %d.%d %d.%d\n",
		       cells[0], cells[1], cells[2], cells[3], cells[4],
		       cells[5], cells[6], cells[7], cells[8], cells[9],
		       cells[10], cells[11], cells[12], cells[13], cells[14],
		       cells[15]);

	return ret;
}

static ssize_t show_firmware(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	int ret = 0;
	uint8_t str1[9];
	uint8_t str2[9];
	uint8_t str3[9];

	ret = lpc_command(dev, 'f', 0, str1);
	if (ret)
		return 0;
	ret = lpc_command(dev, 'f', 1, str2);
	if (ret)
		return 0;
	ret = lpc_command(dev, 'f', 2, str3);
	if (ret)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s %s %s\n", str1, str2, str3);
}

static ssize_t show_capacity(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	uint8_t buffer[8];
	uint16_t cap_acc_mah, cap_min_mah, cap_max_mah;
	lpc_command(dev, 'c', 0, buffer);

	cap_acc_mah = buffer[0] | (buffer[1] << 8);
	cap_min_mah = buffer[2] | (buffer[3] << 8);
	cap_max_mah = buffer[4] | (buffer[5] << 8);

	return snprintf(buf, PAGE_SIZE, "[acc=%dmAh] [min=%dmAh] [max=%dmAh]\n",
			cap_acc_mah, cap_min_mah, cap_max_mah);
}

static void lpc_power_off(void)
{
	uint8_t buffer[8];
	lpc_command(poweroff_device, 'p', 1, buffer);
}

static int get_bat_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	uint8_t buffer[8];
	struct lpc_driver_data *data;
	struct device *dev;
	int milliamp, millivolt;

	data = (struct lpc_driver_data *)power_supply_get_drvdata(psy);
	dev = &data->spi->dev;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		ret = lpc_command(dev, 'q', 0, buffer);
		if (ret)
			return -EBUSY;

		int16_t ma16 = ((int16_t)buffer[2] | ((int16_t)buffer[3] << 8));
		milliamp = (int)ma16;
		if (milliamp < 0) {
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
		ret = lpc_command(dev, 'q', 0, buffer);
		if (ret)
			return -EBUSY;

		millivolt = (buffer[0] | buffer[1] << 8);
		if (millivolt < 5000 || millivolt >= 40000)
			return -EBUSY;

		val->intval = millivolt * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = lpc_command(dev, 'q', 0, buffer);
		if (ret)
			return -EBUSY;

		ma16 = (int16_t)buffer[2] | ((int16_t)buffer[3] << 8);
		milliamp = (int)ma16;
		if (milliamp < -20000 || milliamp >= 20000)
			return -EBUSY;

		/* negative current, battery is charging
		   reporting a negative value is out of spec */
		if (milliamp < 0)
			milliamp = 0;
		val->intval = milliamp * 1000;

		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		ret = lpc_command(dev, 'q', 0, buffer);
		if (ret)
			return -EBUSY;

		int gauge = buffer[4];
		if (gauge < 1 || gauge > 100)
			return -EBUSY;

		val->intval = gauge;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = lpc_command(dev, 'c', 0, buffer);
		if (ret)
			return -EBUSY;

		int milliamp_hours = (buffer[4] | buffer[5] << 8);
		val->intval = milliamp_hours * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = lpc_command(dev, 'c', 0, buffer);
		if (ret)
			return -EBUSY;

		val->intval = (buffer[0] | buffer[1] << 8) * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		ret = lpc_command(dev, 'c', 0, buffer);
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

static const struct of_device_id of_tis_spi_match[] = {
	{ .compatible = "mntre,lpc11u24", .data = 0 },
	{}
};
MODULE_DEVICE_TABLE(of, of_tis_spi_match);

static struct spi_device_id g_spi_dev_id_list[] = {
	{ "lpc11u24", 0 },
	{},
};
MODULE_DEVICE_TABLE(spi, g_spi_dev_id_list);

static struct spi_driver g_spi_driver = {
    .probe = lpc_probe,
    .remove = lpc_remove,
    .driver = {
        .of_match_table = of_match_ptr(of_tis_spi_match),
        .owner = THIS_MODULE,
        .name = "reform2_lpc",
    },
    .id_table = g_spi_dev_id_list,
};
module_spi_driver(g_spi_driver);

MODULE_DESCRIPTION("Reform 2 LPC Driver");
MODULE_LICENSE("GPL");
