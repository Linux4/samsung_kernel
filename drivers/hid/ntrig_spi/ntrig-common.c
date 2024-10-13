/****************************************************************

Zoro Software.
D-Trig Digitizer modules files
Copyright (C) 2010, Dmitry Kuzminov

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

 This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

****************************************************************/
#include <linux/device.h>
#include <linux/module.h>

#include "ntrig-common.h"

/******************************************************************************/

/* NOTE: Static variables and global variables are automatically initialized to
 * 0 by the compiler. The kernel style checker tool (checkpatch.pl) complains
 * if they are explicitly initialized to 0 (or NULL) in their definition.
 */
int ntrig_debug_flag;
EXPORT_SYMBOL_GPL(ntrig_debug_flag);

void set_ntrig_debug_flag(char debug_flag)
{
	int flag;
	if ((debug_flag >= '0') && (debug_flag <= '9'))
		flag = debug_flag - '0';
	else if (debug_flag == 'a')
		flag = 10;
	else
		return;
	if (flag == ntrig_debug_flag)
		return;
	ntrig_debug_flag = flag;
	printk(KERN_DEBUG "%s: %s debug prints\n", MODULE_NAME,
		(flag ? "Enabling" : "Disabling"));
}

char get_ntrig_debug_flag_as_char()
{
	int flag = ntrig_debug_flag;
	if ((flag >= 0) && (flag <= 9))
		return flag + '0';
	else if (flag == 10)
		return 'a';
	return '0';
}

MODULE_PARM_DESC(debug, "Debug mode enable disable");
module_param_named(debug, ntrig_debug_flag, bool, 0644);

