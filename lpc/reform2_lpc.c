#include <linux/module.h>
#include <linux/slab.h>
#include <linux/math.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/of.h>

/* abs_diff was only added to math.h in linux 6.6 */
#ifndef abs_diff
#define abs_diff(a, b)                                 \
	({                                             \
		typeof(a) __a = (a);                   \
		typeof(b) __b = (b);                   \
		(void)(&__a == &__b);                  \
		__a > __b ? (__a - __b) : (__b - __a); \
	})
#endif

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

static ssize_t lpc_command(struct device *dev, char command, uint8_t arg1,
			  uint8_t *response);
static int get_bat_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  union power_supply_propval *val);

#define VOL_JUMP 1000000
#define CAP_JUMP 1000000
#define CUR_JUMP 1000000

typedef struct lpc_driver_data {
	struct spi_device *spi;
	struct power_supply *bat;
	struct mutex lock;
	int last_batt_cap;
	int last_batt_vol;
	int last_batt_cur;
} lpc_driver_data;

static DEVICE_ATTR(status, 0444, show_status, NULL);
static DEVICE_ATTR(cells, 0444, show_cells, NULL);
static DEVICE_ATTR(firmware, 0444, show_firmware, NULL);
static DEVICE_ATTR(capacity, 0444, show_capacity, NULL);

static struct spi_board_info g_spi_board_info = {
	.modalias = "reform2_lpc",
	.max_speed_hz = 400000,
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

static int lpc_probe(struct spi_device *spi)
{
	struct lpc_driver_data *data;
	int ret;

	printk(KERN_INFO "%s: probing ...\n", "reform2_lpc");

	spi->max_speed_hz = g_spi_board_info.max_speed_hz;
	spi->mode = g_spi_board_info.mode;
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret) {
		printk(KERN_ERR "%s: spi_setup failed\n", __func__);
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct lpc_driver_data), GFP_KERNEL);
	if (data == NULL) {
		printk(KERN_ERR "%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	data->spi = spi;
	mutex_init(&data->lock);
	spi_set_drvdata(spi, data);

	ret = device_create_file(&spi->dev, &dev_attr_status);
	if (ret) {
		printk(KERN_ERR "%s: device_create_file failed\n", __func__);
	}

	ret = device_create_file(&spi->dev, &dev_attr_cells);
	if (ret) {
		printk(KERN_ERR "%s: device_create_file failed\n", __func__);
	}

	ret = device_create_file(&spi->dev, &dev_attr_firmware);
	if (ret) {
		printk(KERN_ERR "%s: device_create_file failed\n", __func__);
	}

	ret = device_create_file(&spi->dev, &dev_attr_capacity);
	if (ret) {
		printk(KERN_ERR "%s: device_create_file failed\n", __func__);
	}

	psy_cfg.of_node = spi->dev.of_node;
	psy_cfg.drv_data = data;

	data->bat = power_supply_register_no_ws(&spi->dev, &bat_desc, &psy_cfg);
	if (IS_ERR(data->bat)) {
		printk(KERN_ERR "%s: power_supply_register_no_ws failed\n",
		       __func__);
		return PTR_ERR(data->bat);
	}

	// this overwrites something else that has already claimed pm_power_off on reform2 but it'll do for now
	poweroff_device = &spi->dev;
	pm_power_off = lpc_power_off;

	return ret;
}

static void lpc_remove(struct spi_device *spi)
{
	struct lpc_driver_data *data =
		(struct lpc_driver_data *)spi_get_drvdata(spi);

	printk(KERN_INFO "%s: removing ... \n", "reform2_lpc");

	device_remove_file(&spi->dev, &dev_attr_status);
	device_remove_file(&spi->dev, &dev_attr_firmware);
	device_remove_file(&spi->dev, &dev_attr_cells);
	device_remove_file(&spi->dev, &dev_attr_capacity);

	power_supply_unregister(data->bat);

	if (pm_power_off == &lpc_power_off) {
		pm_power_off = NULL;
	}

	kfree(data);
}

static ssize_t show_status(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	uint8_t buffer[8];
	int16_t voltage;
	int16_t amps;
	uint8_t percentage;
	uint8_t status;
	int ret = 0;

	ret = lpc_command(dev, 'q', 0, buffer);
	if (ret) {
		printk(KERN_INFO "%s: lpc_command failed\n", __func__);
	}

	voltage = (int16_t)buffer[0] | ((int16_t)buffer[1] << 8);
	amps = (int16_t)buffer[2] | ((int16_t)buffer[3] << 8);
	percentage = buffer[4];
	status = buffer[5];

	return snprintf(buf, PAGE_SIZE, "%d.%d %d.%d %2d%% %d", voltage / 1000,
			voltage % 1000, amps / 1000, abs(amps % 1000),
			percentage, status);
}

static ssize_t show_cells(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	uint8_t buffer[8];
	uint16_t cells[8];
	ssize_t wroteChars = 0;
	int ret = 0;

	ret = lpc_command(dev, 'v', 0, buffer);
	if (ret) {
		printk(KERN_INFO "%s: lpc_command failed\n", __func__);
	}

	for (uint8_t s = 0; s < 4; s++) {
		cells[s] = buffer[s * 2] | buffer[(s * 2) + 1] << 8;
	}

	ret = lpc_command(dev, 'v', 1, buffer);
	if (ret) {
		printk(KERN_INFO "%s: lpc_command failed\n", __func__);
	}

	for (uint8_t s = 0; s < 4; s++) {
		cells[s + 4] = buffer[s * 2] | buffer[(s * 2) + 1] << 8;
	}

	for (uint8_t s = 0; s < 8; s++) {
		ret = snprintf(buf + wroteChars, PAGE_SIZE - wroteChars,
			       "%d.%d ", cells[s] / 1000, cells[s] % 1000);
		if (ret != -1) {
			wroteChars += ret;
		}
	}

	// drop the trailing whitespace
	if (wroteChars > 0) {
		wroteChars--;
	}

	return wroteChars;
}

static ssize_t show_firmware(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	uint8_t str1[9];
	uint8_t str2[9];
	uint8_t str3[9];
	int ret = 0;

	ret = lpc_command(dev, 'f', 0, str1);
	if (ret) {
		printk(KERN_INFO "%s: lpc_command failed\n", __func__);
	}

	ret = lpc_command(dev, 'f', 1, str2);
	if (ret) {
		printk(KERN_INFO "%s: lpc_command failed\n", __func__);
	}

	ret = lpc_command(dev, 'f', 2, str3);
	if (ret) {
		printk(KERN_INFO "%s: lpc_command failed\n", __func__);
	}

	str1[8] = '\0';
	str2[8] = '\0';
	str3[8] = '\0';

	return snprintf(buf, PAGE_SIZE, "%s %s %s", str1, str2, str3);
}

static ssize_t show_capacity(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	uint8_t buffer[8];
	int ret = 0;
	uint16_t cap_accu_mah, cap_min_mah, cap_max_mah;
	ret = lpc_command(dev, 'c', 0, buffer);
	if (ret) {
		printk(KERN_INFO "%s: lpc_command failed\n", __func__);
	}

	cap_accu_mah = buffer[0] | (buffer[1] << 8);
	cap_min_mah = buffer[2] | (buffer[3] << 8);
	cap_max_mah = buffer[4] | (buffer[5] << 8);

	return snprintf(buf, PAGE_SIZE, "%d %d %d", cap_accu_mah, cap_min_mah,
			cap_max_mah);
}

static ssize_t lpc_command(struct device *dev, char command, uint8_t arg1,
			  uint8_t *responseBuffer)
{
	struct lpc_driver_data *data =
		(struct lpc_driver_data *)dev_get_drvdata(dev);
	uint8_t commandBuffer[4] = { 0xB5, command, arg1, 0x0 };
	int ret = 0;

	mutex_lock(&data->lock);

	ret = spi_write(data->spi, commandBuffer, 4);
	if (ret) {
		printk(KERN_INFO "%s: spi_write failed\n", __func__);
	}
	msleep(50);

	ret = spi_read(data->spi, responseBuffer, 8);
	if (ret) {
		printk(KERN_INFO "%s: spi_read failed\n", __func__);
	}
	msleep(50);
	mutex_unlock(&data->lock);

	return ret;
}

static void lpc_power_off(void)
{
	int ret = 0;
	uint8_t buffer[8];

	ret = lpc_command(poweroff_device, 'p', 1, buffer);
}

static int get_bat_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  union power_supply_propval *val)
{
	int ret = 0;
	uint8_t buffer[8];
	struct lpc_driver_data *data;
	struct device *dev;
	int16_t amp;

	data = (struct lpc_driver_data *)power_supply_get_drvdata(psy);
	dev = &(data->spi->dev);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		ret = lpc_command(dev, 'q', 0, buffer);
		if (ret) {
			printk(KERN_INFO "%s: lpc_command failed\n", __func__);
		}
		amp = (int16_t)buffer[2] | ((int16_t)buffer[3] << 8);
		if (amp < 0) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else if (amp == 0) {
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
		if (ret) {
			printk(KERN_INFO "%s: lpc_command failed\n", __func__);
			ret = -EINVAL;
		}
		val->intval = (buffer[0] | buffer[1] << 8) * 1000;
		if (data->last_batt_vol &&
		    (abs_diff(val->intval, data->last_batt_vol) > VOL_JUMP)) {
			printk(KERN_INFO "%s: Voltage jump from %i to %i\n",
			       __func__, data->last_batt_vol, val->intval);
			val->intval = data->last_batt_vol +
				      (val->intval > data->last_batt_vol ?
					       VOL_JUMP :
					       -VOL_JUMP);
			printk(KERN_INFO "%s: Clamping to %i\n", __func__,
			       val->intval);
		}
		data->last_batt_vol = val->intval;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = lpc_command(dev, 'q', 0, buffer);
		if (ret) {
			printk(KERN_INFO "%s: lpc_command failed\n", __func__);
			ret = -EINVAL;
		}
		amp = (int16_t)buffer[2] | ((int16_t)buffer[3] << 8);
		// negative current, battery is charging
		// reporting a negative value is out of spec
		if (amp < 0) {
			amp = 0;
		}
		val->intval = amp * 1000;
		if (data->last_batt_cur &&
		    (abs_diff(val->intval, data->last_batt_cur) > CUR_JUMP)) {
			printk(KERN_INFO "%s: Current jump from %i to %i\n",
			       __func__, data->last_batt_cur, val->intval);
			val->intval = data->last_batt_cur +
				      (val->intval > data->last_batt_cur ?
					       CUR_JUMP :
					       -CUR_JUMP);
			printk(KERN_INFO "%s: Clamping to %i\n", __func__,
			       val->intval);
		}
		data->last_batt_cur = val->intval;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		ret = lpc_command(dev, 'q', 0, buffer);
		if (ret) {
			printk(KERN_INFO "%s: lpc_command failed\n", __func__);
			ret = -EINVAL;
		}
		val->intval = buffer[4];
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = lpc_command(dev, 'c', 0, buffer);
		if (ret) {
			printk(KERN_INFO "%s: lpc_command failed\n", __func__);
			ret = -EINVAL;
		}
		val->intval = (buffer[4] | buffer[5] << 8) * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = lpc_command(dev, 'c', 0, buffer);
		if (ret) {
			printk(KERN_INFO "%s: lpc_command failed\n", __func__);
			ret = -EINVAL;
		}
		val->intval = (buffer[0] | buffer[1] << 8) * 1000;
		if (data->last_batt_cap &&
		    (abs_diff(val->intval, data->last_batt_cap) > CAP_JUMP)) {
			printk(KERN_INFO "%s: Charge jump from %i to %i\n",
			       __func__, data->last_batt_cap, val->intval);
			val->intval = data->last_batt_cap +
				      (val->intval > data->last_batt_cap ?
					       CAP_JUMP :
					       -CAP_JUMP);
			printk(KERN_INFO "%s: Clamping to %i\n", __func__,
			       val->intval);
		}
		data->last_batt_cap = val->intval;
		break;

	case POWER_SUPPLY_PROP_CHARGE_EMPTY:
		ret = lpc_command(dev, 'c', 0, buffer);
		if (ret) {
			printk(KERN_INFO "%s: lpc_command failed\n", __func__);
			ret = -EINVAL;
		}
		val->intval = (buffer[2] | buffer[3] << 8) * 1000;
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
