/*
* This software program is licensed subject to the GNU General Public License
* (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

* (C) Copyright 2013 Marvell International Ltd.
* All Rights Reserved
*/
#ifndef _IML_MSA_H_
#define _IML_MSA_H_

#define SLOT_NUMBER 127

#define SLOT_SHIFT 13

#define SLOT_SIZE (1 << SLOT_SHIFT)

void init_ring_state(void);
int get_next_read_slot(void);
void slot_data_out_ring(int);
void slot_data_in_ring(int);
#endif
