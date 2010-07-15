/*
 * The original idea is from: Michael Brown <mbrown@fensystems.co.uk>.
 * Implemented into gPXE by: Guo-Fu Tseng <cooldavid@cooldavid.org>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/timer.h>
#include <gpxe/process.h>
#include <gpxe/activity.h>

/** @file
 *
 * Activities
 *
 * We implement a facility to keep the processes running while waiting
 * for some critical events to complete.
 */


/** Currently waiting activities */
static unsigned int num_activities = 0;

/**
 * Start an activity
 */
void activity_start ( void ) {
	num_activities++;
}

/**
 * Stop an activity
 */
void activity_stop ( void ) {
	num_activities--;
}

/**
 * Wait for activities to complete
 *
 * @v max_timeout	The max waiting time in tenths of a second.
 * @ret boolean		If all the activities are cleared.
 */
int activity_wait ( unsigned long max_timeout ) {
	max_timeout = currticks() + ( max_timeout * TICKS_PER_SEC ) / 10;
	while ( num_activities && currticks() < max_timeout ) {
		step();
	}
	return !num_activities;
}
