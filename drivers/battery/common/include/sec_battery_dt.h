/*
 * sec_battery_dt.h
 * Samsung Mobile Battery Header, for device tree parsing
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __SEC_BATTERY_DT_H
#define __SEC_BATTERY_DT_H __FILE__

int sec_bat_parse_dt(struct device *dev, struct sec_battery_info *battery);
void sec_bat_parse_mode_dt(struct sec_battery_info *battery);
void sec_bat_parse_mode_dt_work(struct work_struct *work);

#endif /* __SEC_BATTERY_DT_H */
