#ifndef	_GPXE_ACTIVITY_H
#define _GPXE_ACTIVITY_H

/** @file
 *
 * gPXE activity API
 *
 * The activity API provides facility to keep the processes running
 * while waiting for some critical events to complete.
 *
 * It is suggested that we don't call activity_wait() within any
 * process's scope.
 */

FILE_LICENCE ( GPL2_OR_LATER );

extern void activity_start ( void );
extern void activity_stop ( void );

/* Default timeout for 1 second */
#define ACTIVITY_TIMEOUT 10

/**
 * max_timeout is in tenths of a second
 */
extern int activity_wait ( unsigned long max_timeout );

#endif
