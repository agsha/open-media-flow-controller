/* -*- Mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-

    libparted - a library for manipulating disk partitions
    Copyright (C) 2000, 2001 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Contributor:  Matt Wilson <msw@redhat.com>
*/

#include "config.h"

#include <string.h>

#include <parted/parted.h>
#include <parted/debug.h>
#include <parted/endian.h>

#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */

typedef struct {
	unsigned int   magic;        /* expect AIX_LABEL_MAGIC */
	unsigned int   fillbytes[127];
} AixLabel;

#define	AIX_LABEL_MAGIC		0xc9c2d4c1

static PedDiskType aix_disk_type;

static int
aix_probe (PedDevice *dev)
{
	PedDiskType*	disk_type;
	AixLabel	label;
	int		i;

	PED_ASSERT (dev != NULL, return 0);

	if (!ped_device_read (dev, &label, 0, 1))
		return 0;

	if (PED_BE32_TO_CPU (label.magic) != AIX_LABEL_MAGIC)
		return 0;

	return 1;
}

#ifndef DISCOVER_ONLY
static int
aix_clobber (PedDevice* dev)
{
	AixLabel label;

	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (aix_probe (dev), return 0);

	if (!ped_device_read (dev, &label, 0, 1))
		return 0;
	
	label.magic = 0;
	return ped_device_write (dev, &label, 0, 1);
}
#endif /* !DISCOVER_ONLY */

static PedDisk*
aix_alloc (PedDevice* dev)
{
	PedDisk*	disk;

        disk = _ped_disk_alloc (dev, &aix_disk_type);
	if (!disk)
		return NULL;

	return disk;
}

static PedDisk*
aix_duplicate (const PedDisk* disk)
{
	PedDisk*	new_disk;
       
	new_disk = ped_disk_new_fresh (disk->dev, &aix_disk_type);
	if (!new_disk)
		return NULL;

	return new_disk;
}

static void
aix_free (PedDisk *disk)
{
	_ped_disk_free (disk);
}

static int
aix_read (PedDisk* disk)
{
	ped_disk_delete_all (disk);
        ped_exception_throw (PED_EXCEPTION_NO_FEATURE,
                             PED_EXCEPTION_CANCEL,
                             _("Support for reading AIX disk labels is "
                               "is not implemented yet."));
        return 0;
}

#ifndef DISCOVER_ONLY
static int
aix_write (PedDisk* disk)
{
        ped_exception_throw (PED_EXCEPTION_NO_FEATURE,
                             PED_EXCEPTION_CANCEL,
                             _("Support for writing AIX disk labels is "
                               "is not implemented yet."));
	return 0;
}
#endif /* !DISCOVER_ONLY */

static PedPartition*
aix_partition_new (const PedDisk* disk, PedPartitionType part_type,
		   const PedFileSystemType* fs_type,
		   PedSector start, PedSector end)
{
	PedPartition*		part;

        ped_exception_throw (PED_EXCEPTION_NO_FEATURE,
                             PED_EXCEPTION_CANCEL,
                             _("Support for adding partitions to AIX disk "
                               "labels is not implemented yet."));
        return NULL;
}

static PedPartition*
aix_partition_duplicate (const PedPartition* part)
{
	PedPartition*		new_part;

        ped_exception_throw (PED_EXCEPTION_NO_FEATURE,
                             PED_EXCEPTION_CANCEL,
                             _("Support for duplicating partitions in AIX "
                               "disk labels is not implemented yet."));
        return NULL;
}

static void
aix_partition_destroy (PedPartition* part)
{
	PED_ASSERT (part != NULL, return);

	_ped_partition_free (part);
}

static int
aix_partition_set_system (PedPartition* part, const PedFileSystemType* fs_type)
{
        ped_exception_throw (PED_EXCEPTION_NO_FEATURE,
                             PED_EXCEPTION_CANCEL,
                             _("Support for setting system type of partitions "
                               "in AIX disk labels is not implemented yet."));
	return 0;
}

static int
aix_partition_set_flag (PedPartition* part, PedPartitionFlag flag, int state)
{
        ped_exception_throw (PED_EXCEPTION_NO_FEATURE,
                             PED_EXCEPTION_CANCEL,
                             _("Support for setting flags "
                               "in AIX disk labels is not implemented yet."));
        return 0;
}

static int
aix_partition_get_flag (const PedPartition* part, PedPartitionFlag flag)
{
        return 0;
}


static int
aix_partition_is_flag_available (const PedPartition* part,
				 PedPartitionFlag flag)
{
        return 0;
}


static int
aix_get_max_primary_partition_count (const PedDisk* disk)
{
	return 4;
}

static int
aix_partition_align (PedPartition* part, const PedConstraint* constraint)
{
        PED_ASSERT (part != NULL, return 0);

        return 1;
}

static int
aix_partition_enumerate (PedPartition* part)
{
	return 1;
}

static int
aix_alloc_metadata (PedDisk* disk)
{
	return 1;
}

static PedDiskOps aix_disk_ops = {
	probe:			aix_probe,
#ifndef DISCOVER_ONLY
	clobber:		aix_clobber,
#else
	clobber:		NULL,
#endif
	alloc:			aix_alloc,
	duplicate:		aix_duplicate,
	free:			aix_free,
	read:			aix_read,
#ifndef DISCOVER_ONLY
	write:			aix_write,
#else
	write:			NULL,
#endif

	partition_new:		aix_partition_new,
	partition_duplicate:	aix_partition_duplicate,
	partition_destroy:	aix_partition_destroy,
	partition_set_system:	aix_partition_set_system,
	partition_set_flag:	aix_partition_set_flag,
	partition_get_flag:	aix_partition_get_flag,
	partition_is_flag_available:	aix_partition_is_flag_available,
	partition_align:	aix_partition_align,
	partition_enumerate:	aix_partition_enumerate,
	alloc_metadata:		aix_alloc_metadata,
	get_max_primary_partition_count:
				aix_get_max_primary_partition_count,

	partition_set_name:		NULL,
	partition_get_name:		NULL,
};

static PedDiskType aix_disk_type = {
	next:		NULL,
	name:		"aix",
	ops:		&aix_disk_ops,
	features:	0
};

void
ped_disk_aix_init ()
{
	PED_ASSERT (sizeof (AixLabel) == 512, return);
	ped_register_disk_type (&aix_disk_type);
}

void
ped_disk_aix_done ()
{
	ped_unregister_disk_type (&aix_disk_type);
}
