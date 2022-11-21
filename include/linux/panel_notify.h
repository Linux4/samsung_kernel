/*
 * =================================================================
 *
 *	Description: Header file for samsung panel notify
 *	Company: Samsung Electronics
 *
 * ================================================================
 *
 *
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2018, Samsung Electronics. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __PANEL_NOTIFY_H__
#define __PANEL_NOTIFY_H__

enum panel_notifier_event_t {
	PANEL_EVENT_BL_CHANGED,
};

struct panel_bl_event_data {
	int bl_level;
	int aor_data;
};

extern int panel_notifier_register(struct notifier_block *nb);
extern int panel_notifier_unregister(struct notifier_block *nb);
extern int panel_notifier_call_chain(unsigned long val, void *v);

#endif /*__PANEL_NOTIFY_H__*/
