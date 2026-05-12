/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright 2022 nanocodebug <nanocodebug@gmail.com>
 * Copyright 2023 Michael Fincham <michael@hotplate.co.nz>
 * Copyright 2024 Michal Suchánek <hramrach@gmail.com>
 * Copyright 2024-2026 Lucie Lukas Hartmann <lukas@mntre.com>
 * Copyright 2023-2025 Johannes Schauer Marin Rodrigues <josch@mister-muffin.de>
 */

#include <asm-generic/errno-base.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/math.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/version.h>

#define MNTRE_LPC_API_UNKNOWN 0
#define MNTRE_LPC_API_V1 1
#define MNTRE_LPC_API_V2 2
#define MNTRE_LPC_API_V3 3

/* array size for lpc response buffers */
#define LPC_RES_SZ 9

typedef struct lpc_driver_data {
	struct spi_device *spi;
	struct power_supply *bat;
	struct mutex lock;
	struct backlight_device *backlight;
	uint32_t api_version;
} lpc_driver_data;

static int lpc_probe(struct spi_device *spi);
static void lpc_remove(struct spi_device *spi);
static int lpc_power_off(struct sys_off_data *data);
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
static ssize_t lpc_command(struct lpc_driver_data *lpc, char* cmd,
			   uint8_t *response);
static int get_bat_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val);

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

static int bl_get_brightness(struct backlight_device *bl)
{
	u16 brightness = bl->props.brightness;
	return brightness & 0x7f;
}

static int bl_update_status(struct backlight_device *bl)
{
	struct lpc_driver_data *lpc = (struct lpc_driver_data *)bl_get_data(bl);
	uint8_t buffer[LPC_RES_SZ];
	char cmd[32];
	snprintf(cmd, 32, "(set-blgt %d)", bl->props.brightness);
	lpc_command(lpc, cmd, buffer);
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

static uint8_t lpc_calc_checksum(uint8_t *buffer, int len)
{
	uint8_t sum = 0;
	for (int i = 0; i < len - 1; i++) {
		sum = sum ^ buffer[i];
	}
	return sum;
}

static int lpc_confirm_checksum(uint8_t *buffer, int len)
{
	return (buffer[len - 1] == lpc_calc_checksum(buffer, len));
}

static uint32_t lpc_get_api_version(struct device *dev)
{
	int ret;
	uint32_t version;
	uint8_t str[LPC_RES_SZ];
	struct lpc_driver_data *lpc =
		(struct lpc_driver_data *)dev_get_drvdata(dev);

	ret = MNTRE_LPC_API_V3;
	return ret;
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
		dev_err(&spi->dev, "spi_setup failed.\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&spi->dev, sizeof(struct lpc_driver_data),
			    GFP_KERNEL);
	if (data == NULL) {
		dev_err(&spi->dev, "devm_kzalloc failed.\n");
		return -ENOMEM;
	}

	data->spi = spi;
	mutex_init(&data->lock);
	spi_set_drvdata(spi, data);

	ret = device_create_file(&spi->dev, &dev_attr_status);
	if (ret)
		dev_err(&spi->dev,
			"device_create_file dev_attr_status failed.\n");

	ret = device_create_file(&spi->dev, &dev_attr_cells);
	if (ret)
		dev_err(&spi->dev,
			"device_create_file dev_attr_cells failed.\n");

	ret = device_create_file(&spi->dev, &dev_attr_firmware);
	if (ret)
		dev_err(&spi->dev,
			"device_create_file dev_attr_firmware failed.\n");

	ret = device_create_file(&spi->dev, &dev_attr_capacity);
	if (ret)
		dev_err(&spi->dev,
			"device_create_file dev_attr_capacity failed.\n");

	ret = device_create_file(&spi->dev, &dev_attr_uart);
	if (ret)
		dev_err(&spi->dev,
			"device_create_file dev_attr_uart failed.\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
	psy_cfg.fwnode = dev_fwnode(&spi->dev);
#else
	psy_cfg.of_node = spi->dev.of_node;
#endif
	psy_cfg.drv_data = data;
	data->bat = devm_power_supply_register(&spi->dev, &bat_desc, &psy_cfg);
	if (IS_ERR(data->bat)) {
		dev_err(&spi->dev, "dev_power_supply_register failed.\n");
		return PTR_ERR(data->bat);
	}

	/* register lpc as poweroff handler */
	ret = devm_register_sys_off_handler(&spi->dev,
					    SYS_OFF_MODE_POWER_OFF_PREPARE,
					    SYS_OFF_PRIO_FIRMWARE,
					    lpc_power_off, data);
	if (ret) {
		dev_err(&spi->dev, "devm_register_sys_off_handler failed.\n");
		return ret;
	}

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
			"enabling backlight control for MNT Pocket Reform with "
			"Display Version 2.\n");
		data->backlight = lpc_create_backlight(&spi->dev, data);
		if (IS_ERR(data->backlight)) {
			dev_err(&spi->dev, "lpc_create_backlight failed.\n");
		}
	}

	data->api_version = lpc_get_api_version(&spi->dev);

	spi_controller_get(spi->controller);

	return ret;
}

static void lpc_remove(struct spi_device *spi)
{
	device_remove_file(&spi->dev, &dev_attr_status);
	device_remove_file(&spi->dev, &dev_attr_firmware);
	device_remove_file(&spi->dev, &dev_attr_cells);
	device_remove_file(&spi->dev, &dev_attr_capacity);
	device_remove_file(&spi->dev, &dev_attr_uart);

	// spi_controller_put(spi->controller);
}

/* response[] has to have a size of at least 8 bytes! */
static ssize_t lpc_command(struct lpc_driver_data *lpc, char* cmd, uint8_t *response)
{
	int ret = 0;
	memset(response, 0, LPC_RES_SZ);

	mutex_lock(&lpc->lock);
	//dev_err(&lpc->spi->dev, "lpc_command: %s\n", cmd);
	int len = strlen(cmd);
	for (int i = 0; i < len; i+=8) {
		uint8_t xfer[9];
		memset(xfer, 0, 9);
		int remain = 8;
		if (i + 8 >= len) {
			remain = len - i;
		}
		memcpy(xfer, &cmd[i], remain);
		ret = spi_write(lpc->spi, xfer, 8);
		udelay(200);
	}
	//ret = spi_write(lpc->spi, cmd, len);
	//dev_err(&lpc->spi->dev, "lpc: sent [%s]\n", cmd);

#define RX_SZ 32
	int done = 0;
	int delayed = 0;
	int vcount = 0;
	char rxbuf[RX_SZ];
	memset(rxbuf, 0, RX_SZ);
	int paren = 0;
	while (!done) {
		uint8_t c = 0;
		ret = spi_read(lpc->spi, &c, 1);
		if (ret) {
			dev_err(&lpc->spi->dev, "lpc: spi_read failed (%d).\n", ret);
			break;
		}
		if (c == '(') {
			paren++;
		}
		if (c == ')') {
			paren--;

			if (paren == 0) {
				// response received
				if (vcount >= 5) {
					if (strncmp(rxbuf, "u64 ", 4)) {
						uint64_t r_u64;
						if (!kstrtoull(&rxbuf[5], 10, &r_u64)) {
							dev_err(&lpc->spi->dev, "lpc: parsed u64: %llu.\n", r_u64);
							*(uint64_t*)response = r_u64;
						}
					} else if (strncmp(rxbuf, "i64 ", 4)) {
						int64_t r_i64;
						if (!kstrtoull(&rxbuf[5], 10, &r_i64)) {
							dev_err(&lpc->spi->dev, "lpc: parsed i64: %lld.\n", r_i64);
							*(int64_t*)response = r_i64;
						}
					} else if (strncmp(rxbuf, "f64 ", 4)) {
						dev_err(&lpc->spi->dev, "lpc: can't parse f64 response yet.\n");
					}
				}
				break;
			}
		}
		if (c == 0 || c == 0xff) {
			//dev_err(&lpc->spi->dev, "lpc: 00 @ %d/%d.\n", count, vcount);
			udelay(10);
			delayed++;
		} else if (paren == 1) {
			rxbuf[vcount] = c;
			vcount++;
		}
		if (vcount >= RX_SZ) {
			dev_err(&lpc->spi->dev, "lpc: max read %d.\n", vcount);
			break;
		}
		if (delayed >= 1000) {
			dev_err(&lpc->spi->dev, "lpc: timeout %d.\n", delayed);
			break;
		}
	}
	dev_err(&lpc->spi->dev, "lpc: rxbuf [%s]\n", rxbuf);
	//msleep(500);

	mutex_unlock(&lpc->lock);
	return ret;
}

static char uart_resp[9];

static ssize_t show_uart(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	uart_resp[8] = 0;
	return snprintf(buf, PAGE_SIZE, "%s", uart_resp);
}

/* let LPC output bytes over UART to MNT Desktop Reform control panel */
static ssize_t store_uart(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct lpc_driver_data *lpc =
		(struct lpc_driver_data *)dev_get_drvdata(dev);
	uint8_t discard[8];
	char cmd[64];
	snprintf(cmd, (count > 64 ? count : 64), "%s", buf);
	cmd[63] = 0;
	lpc_command(lpc, cmd, uart_resp);
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
	struct lpc_driver_data *lpc =
		(struct lpc_driver_data *)dev_get_drvdata(dev);

	ret = lpc_command(lpc, "(0q)", buffer);
	if (ret)
		return 0;

	voltage = (int16_t)buffer[0] | ((int16_t)buffer[1] << 8);
	amps = (int16_t)buffer[2] | ((int16_t)buffer[3] << 8);
	percentage = buffer[4];
	status = buffer[5];

	return snprintf(buf, PAGE_SIZE,
			"%d.%dV %d.%dA %2d%% [status=%d] [API=%d]\n",
			voltage / 1000, voltage % 1000, amps / 1000,
			abs(amps % 1000), percentage, status, lpc->api_version);
}

static ssize_t show_cells(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int ret = 0;
	uint8_t buffer[LPC_RES_SZ * 2];
	uint16_t cells[16];
	struct lpc_driver_data *lpc =
		(struct lpc_driver_data *)dev_get_drvdata(dev);

	ret = lpc_command(lpc, "(cell-mv 0)", buffer);
	if (ret)
		return 0;
	ret = lpc_command(lpc, "(cell-mv 1)", &buffer[8]);
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
	uint8_t str1[LPC_RES_SZ], str2[LPC_RES_SZ], str3[LPC_RES_SZ];
	struct lpc_driver_data *lpc =
		(struct lpc_driver_data *)dev_get_drvdata(dev);

	ret = lpc_command(lpc, "(0f)", str1);
	if (ret)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%s\n", str1);
}

static ssize_t show_capacity(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	uint8_t buffer[LPC_RES_SZ];
	uint16_t cap_acc_mah, cap_min_mah, cap_max_mah;
	struct lpc_driver_data *lpc =
		(struct lpc_driver_data *)dev_get_drvdata(dev);

	lpc_command(lpc, "(0c)", buffer);

	cap_acc_mah = buffer[0] | (buffer[1] << 8);
	cap_min_mah = buffer[2] | (buffer[3] << 8);
	cap_max_mah = buffer[4] | (buffer[5] << 8);

	return snprintf(buf, PAGE_SIZE, "[acc=%dmAh] [min=%dmAh] [max=%dmAh]\n",
			cap_acc_mah, cap_min_mah, cap_max_mah);
}

static int lpc_power_off(struct sys_off_data *data)
{
	uint8_t buffer[LPC_RES_SZ];
	struct lpc_driver_data *lpc = (struct lpc_driver_data *)data->cb_data;

	/* try to shut down power, forever */
	while (true) {
		lpc_command(lpc, "(0p)", buffer);
		msleep(100);
	}
	return 0;
}

static int get_bat_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	uint8_t buffer[LPC_RES_SZ];
	int milliamp, millivolt;
	struct lpc_driver_data *lpc =
		(struct lpc_driver_data *)power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		ret = lpc_command(lpc, "(0q)", buffer);
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
		ret = lpc_command(lpc, "(0q)", buffer);
		if (ret)
			return -EBUSY;

		millivolt = (buffer[0] | buffer[1] << 8);
		if (millivolt < 5000 || millivolt >= 40000)
			millivolt = 0;

		val->intval = millivolt * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = lpc_command(lpc, "(0q)", buffer);
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
		ret = lpc_command(lpc, "(0q)", buffer);
		if (ret)
			return -EBUSY;

		/* don't trigger upower emergency shutdown in case
     * of faulty data
     * (normally happens at 5% or less) */
		int gauge = buffer[4];
		if (gauge < 6)
			gauge = 6;
		if (gauge > 100)
			gauge = 100;

		val->intval = gauge;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = lpc_command(lpc, "(0c)", buffer);
		if (ret)
			return -EBUSY;

		int milliamp_hours = (buffer[4] | buffer[5] << 8);
		val->intval = milliamp_hours * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = lpc_command(lpc, "(0c)", buffer);
		if (ret)
			return -EBUSY;

		val->intval = (buffer[0] | buffer[1] << 8) * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		ret = lpc_command(lpc, "(0c)", buffer);
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
    .driver =
        {
            .of_match_table = of_match_ptr(of_tis_spi_match),
            .owner = THIS_MODULE,
            .name = "reform2_lpc",
        },
    .id_table = g_spi_dev_id_list,
};
module_spi_driver(g_spi_driver);

MODULE_DESCRIPTION("Reform 2 LPC Driver");
MODULE_LICENSE("GPL");
