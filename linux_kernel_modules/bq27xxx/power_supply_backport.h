#ifndef POWER_SUPPLY_BACKPORT_H
#define POWER_SUPPLY_BACKPORT_H

/*
 * This is the recommended struct to manage static battery parameters,
 * populated by power_supply_get_battery_info(). Most platform drivers should
 * use these for consistency.
 * Its field names must correspond to elements in enum power_supply_property.
 * The default field value is -EINVAL.
 * Power supply class itself doesn't use this.
 */
struct power_supply_battery_info {
	int energy_full_design_uwh;	    /* microWatt-hours */
	int charge_full_design_uah;	    /* microAmp-hours */
	int voltage_min_design_uv;	    /* microVolts */
	int precharge_current_ua;	    /* microAmps */
	int charge_term_current_ua;	    /* microAmps */
	int constant_charge_current_max_ua; /* microAmps */
	int constant_charge_voltage_max_uv; /* microVolts */
};

extern int power_supply_get_battery_info(struct power_supply *psy,
					 struct power_supply_battery_info *info);

#endif /* POWER_SUPPLY_BACKPORT_H */
