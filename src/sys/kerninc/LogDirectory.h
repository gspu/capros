#ifndef LOGDIRECTORY_H
#define LOGDIRECTORY_H
/*
 * Copyright (C) 2008, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */


#include <kerninc/kernel.h>

/** \file LogDirectory.h
    \brief CapROS Kernel Log Directory Interface.

    The object directory keeps track of the most recent instance of
    objects written to the log. In the descriptions below, this location
    is called the "Primary Location".

    It will, in addition, keep up an additional location for the object
    to allow the journalize write operation to locate the version which
    will become current should a restart occur before another checkpoint.

    It provides operations to:
      1. Record new locations for objects
      2. Find the primary location for an object.
      3. Find all the objects whose primary locations are part of a 
         particular checkpoint generation.
      4. Remove all the entries of a particular generation and earlier
         generations.
      5. Find the most recent location for an object which is earlier
         than a specified generation number.

*/

extern LID logCursor;	// next place to write in the main log
extern LID logWrapPoint;	// end of main log
extern GenNum workingGenerationNumber;

/** The object information maintained by the Log Directory.
 */
typedef struct ObjectDescriptor {
  OID       oid;                /**<The Object ID of the object. */
  ObCount   allocCount;         /**<The allocation count of the object. */
  ObCount   callCount;          /**<The call count of the object. */
  LID       logLoc;             /**<The Log location ID where the object
				   is stored in the log. */
  uint8_t   type;               /**<The type of the object. */
} ObjectDescriptor;

/** Record the location of an object.

    The call includes the generation number so it may be used during
    restart. Object locations should be recorded in ascending order of
    generation number.

    Note: This routine invalidates all pointers returned by ld_findObject,
    ld_findFirstObject, or ld_findNextObject.

    @param[in] od The Object Descriptor for the object.
    @param[in] generation The log generation of the object.
*/
void
ld_recordLocation(const ObjectDescriptor *od, GenNum generation);

/** Find an object in the directory.

    This routine will return the primary location LID of the object
    in the working generation, the restart generation, or any of
    the earlier generations still in the directory.

    @param[in] oid The object ID to be located.
    @return A pointer to the ObjectDescriptor for the object or NULL if the
            object is not in the log. This pointer will be good until
	    a change is made to the log directory, either adding or
	    modifing an entry, or deleting a generation.
*/
const ObjectDescriptor *
ld_findObject(OID oid);


/** Find an object older than a given generation in the directory.

    This routine will return the primary location LID of the object
    if it is older than the given generation. It will return the previous
    primary location if the primary is of the given generation or younger,
    and the previous primary is older than the given generation.

    @param[in] oid The object ID to be located.
    @param[in] generation The generation the object must be older than.
    @return A pointer to the ObjectDescriptor for the object or NULL if the
            object is not in the log, or the log entry is younger or 
	    equal in age to generation.. This pointer will be good until
	    a change is made to the log directory, either adding or
	    modifing an entry, or deleting a generation.
*/
const ObjectDescriptor *
ld_findOldObject(OID oid, GenNum generation);


/** Find the first object of a generation.

    This routine starts a scan of all objects in a generation.
    od_nextObject continues the scan. There may be up to one scan in
    progress at any time for any particular generation. Only objects
    whose primary location is in the given generation will be returned.

    Note the the order of objects in a generation is undefined. If it needs
    to be defined for some reason, like optimizing migration, then that
    need will be an additional constraint on the implementation of the
    object directory or the use of the returned values.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the first object in a generation scan
            or NULL. This pointer will be good until
	    a change is made to the log directory, either adding or
	    modifing an entry, or deleting a generation.
*/
const ObjectDescriptor *
ld_findFirstObject(GenNum generation);

/** Find the next object of a generation.

    This routine continues the scan of all objects whose primary location
    is in a generation. See od_findFirstObject for more information.

    @param[in] generation The generation number to scan.
    @return The ObjectDescriptor of the next object in a generation scan
            or NULL. This pointer will be good until
	    a change is made to the log directory, either adding or
	    modifing an entry, or deleting a generation.
*/
const ObjectDescriptor *
ld_findNextObject(GenNum generation);

/** Remove all the objects in a generation, and all earlier generations,
    from the Log Directory.

    Note: This routine may need to be executed in smaller pieces to meet
    real-time requirements.

    Note: This routine invalidates all pointers returned by ld_findObject,
    ld_findOldObject, ld_findFirstObject, or ld_findNextObject.

    @param[in] generation The generation to clear.
*/
void
ld_clearGeneration(GenNum generation);

#endif /* LOGDIRECTORY_H */
