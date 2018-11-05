/*
 * This file contains backported features of the power supply subsystem from 4.18-rc8
 */
#include <linux/device.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include "power_supply_backport.h"

int power_supply_get_battery_info(struct power_supply *psy,
				  struct power_supply_battery_info *info)
{
	struct device_node *battery_np;
	const char *value;
	int err;

	info->energy_full_design_uwh         = -EINVAL;
	info->charge_full_design_uah         = -EINVAL;
	info->voltage_min_design_uv          = -EINVAL;
	info->precharge_current_ua           = -EINVAL;
	info->charge_term_current_ua         = -EINVAL;
	info->constant_charge_current_max_ua = -EINVAL;
	info->constant_charge_voltage_max_uv = -EINVAL;

	if (!psy->of_node) {
		dev_warn(psy->dev, "%s currently only supports devicetree\n",
			 __func__);
		return -ENXIO;
	}

	battery_np = of_parse_phandle(psy->of_node, "monitored-battery", 0);
	if (!battery_np)
		return -ENODEV;

	err = of_property_read_string(battery_np, "compatible", &value);
	if (err)
		return err;

	if (strcmp("simple-battery", value))
		return -ENODEV;

	/* The property and field names below must correspond to elements
	 * in enum power_supply_property. For reasoning, see
	 * Documentation/power/power_supply_class.txt.
	 */

	of_property_read_u32(battery_np, "energy-full-design-microwatt-hours",
			     &info->energy_full_design_uwh);
	of_property_read_u32(battery_np, "charge-full-design-microamp-hours",
			     &info->charge_full_design_uah);
	of_property_read_u32(battery_np, "voltage-min-design-microvolt",
			     &info->voltage_min_design_uv);
	of_property_read_u32(battery_np, "precharge-current-microamp",
			     &info->precharge_current_ua);
	of_property_read_u32(battery_np, "charge-term-current-microamp",
			     &info->charge_term_current_ua);
	of_property_read_u32(battery_np, "constant_charge_current_max_microamp",
			     &info->constant_charge_current_max_ua);
	of_property_read_u32(battery_np, "constant_charge_voltage_max_microvolt",
			     &info->constant_charge_voltage_max_uv);

	return 0;
}
EXPORT_SYMBOL_GPL(power_supply_get_battery_info);
MODULE_LICENSE("GPL");
