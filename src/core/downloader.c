/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/job.h>
#include <ipxe/uaccess.h>
#include <ipxe/umalloc.h>
#include <ipxe/image.h>
#include <ipxe/downloader.h>

/** @file
 *
 * Image downloader
 *
 */

/** A downloader */
struct downloader {
	/** Reference count for this object */
	struct refcnt refcnt;

	/** Job control interface */
	struct interface job;
	/** Data transfer interface */
	struct interface xfer;

	/** Image to contain downloaded file */
	struct image *image;
	/** Current position within image buffer */
	size_t pos;
	/** Image registration routine */
	int ( * register_image ) ( struct image *image );
};

/**
 * Free downloader object
 *
 * @v refcnt		Downloader reference counter
 */
static void downloader_free ( struct refcnt *refcnt ) {
	struct downloader *downloader =
		container_of ( refcnt, struct downloader, refcnt );

	image_put ( downloader->image );
	free ( downloader );
}

/**
 * Terminate download
 *
 * @v downloader	Downloader
 * @v rc		Reason for termination
 */
static void downloader_finished ( struct downloader *downloader, int rc ) {

	/* Register image if download was successful */
	if ( rc == 0 )
		rc = downloader->register_image ( downloader->image );

	/* Shut down interfaces */
	intf_shutdown ( &downloader->xfer, rc );
	intf_shutdown ( &downloader->job, rc );
}

/**
 * Ensure that download buffer is large enough for the specified size
 *
 * @v downloader	Downloader
 * @v len		Required minimum size
 * @ret rc		Return status code
 */
static int downloader_ensure_size ( struct downloader *downloader,
				    size_t len ) {
	userptr_t new_buffer;

	/* If buffer is already large enough, do nothing */
	if ( len <= downloader->image->len )
		return 0;

	DBGC ( downloader, "Downloader %p extending to %zd bytes\n",
	       downloader, len );

	/* Extend buffer */
	new_buffer = urealloc ( downloader->image->data, len );
	if ( ! new_buffer ) {
		DBGC ( downloader, "Downloader %p could not extend buffer to "
		       "%zd bytes\n", downloader, len );
		return -ENOBUFS;
	}
	downloader->image->data = new_buffer;
	downloader->image->len = len;

	return 0;
}

/****************************************************************************
 *
 * Job control interface
 *
 */

/**
 * Report progress of download job
 *
 * @v downloader	Downloader
 * @v progress		Progress report to fill in
 */
static void downloader_progress ( struct downloader *downloader,
				  struct job_progress *progress ) {

	/* This is not entirely accurate, since downloaded data may
	 * arrive out of order (e.g. with multicast protocols), but
	 * it's a reasonable first approximation.
	 */
	progress->completed = downloader->pos;
	progress->total = downloader->image->len;
}

/****************************************************************************
 *
 * Data transfer interface
 *
 */

/**
 * Handle received data
 *
 * @v downloader	Downloader
 * @v iobuf		Datagram I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int downloader_xfer_deliver ( struct downloader *downloader,
				     struct io_buffer *iobuf,
				     struct xfer_metadata *meta ) {
	size_t len;
	size_t max;
	int rc;

	/* Calculate new buffer position */
	if ( meta->whence != SEEK_CUR )
		downloader->pos = 0;
	downloader->pos += meta->offset;

	/* Ensure that we have enough buffer space for this data */
	len = iob_len ( iobuf );
	max = ( downloader->pos + len );
	if ( ( rc = downloader_ensure_size ( downloader, max ) ) != 0 )
		goto done;

	/* Copy data to buffer */
	copy_to_user ( downloader->image->data, downloader->pos,
		       iobuf->data, len );

	/* Update current buffer position */
	downloader->pos += len;

 done:
	free_iob ( iobuf );
	return rc;
}

/** Downloader data transfer interface operations */
static struct interface_operation downloader_xfer_operations[] = {
	INTF_OP ( xfer_deliver, struct downloader *, downloader_xfer_deliver ),
	INTF_OP ( intf_close, struct downloader *, downloader_finished ),
};

/** Downloader data transfer interface descriptor */
static struct interface_descriptor downloader_xfer_desc =
	INTF_DESC ( struct downloader, xfer, downloader_xfer_operations );

/****************************************************************************
 *
 * Job control interface
 *
 */

/** Downloader job control interface operations */
static struct interface_operation downloader_job_op[] = {
	INTF_OP ( job_progress, struct downloader *, downloader_progress ),
	INTF_OP ( intf_close, struct downloader *, downloader_finished ),
};

/** Downloader job control interface descriptor */
static struct interface_descriptor downloader_job_desc =
	INTF_DESC ( struct downloader, job, downloader_job_op );

/****************************************************************************
 *
 * Instantiator
 *
 */

/**
 * Instantiate a downloader
 *
 * @v job		Job control interface
 * @v image		Image to fill with downloaded file
 * @v register_image	Image registration routine
 * @v type		Location type to pass to xfer_open()
 * @v ...		Remaining arguments to pass to xfer_open()
 * @ret rc		Return status code
 *
 * Instantiates a downloader object to download the specified URI into
 * the specified image object.  If the download is successful, the
 * image registration routine @c register_image() will be called.
 */
int create_downloader ( struct interface *job, struct image *image,
			int ( * register_image ) ( struct image *image ),
			int type, ... ) {
	struct downloader *downloader;
	va_list args;
	int rc;

	/* Allocate and initialise structure */
	downloader = zalloc ( sizeof ( *downloader ) );
	if ( ! downloader )
		return -ENOMEM;
	ref_init ( &downloader->refcnt, downloader_free );
	intf_init ( &downloader->job, &downloader_job_desc,
		    &downloader->refcnt );
	intf_init ( &downloader->xfer, &downloader_xfer_desc,
		    &downloader->refcnt );
	downloader->image = image_get ( image );
	downloader->register_image = register_image;
	va_start ( args, type );

	/* Instantiate child objects and attach to our interfaces */
	if ( ( rc = xfer_vopen ( &downloader->xfer, type, args ) ) != 0 )
		goto err;

	/* Attach parent interface, mortalise self, and return */
	intf_plug_plug ( &downloader->job, job );
	ref_put ( &downloader->refcnt );
	va_end ( args );
	return 0;

 err:
	downloader_finished ( downloader, rc );
	ref_put ( &downloader->refcnt );
	va_end ( args );
	return rc;
}
