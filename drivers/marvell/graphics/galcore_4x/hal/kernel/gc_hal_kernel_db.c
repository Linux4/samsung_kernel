/****************************************************************************
*
*    Copyright (c) 2005 - 2012 by Vivante Corp.
*    
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the license, or
*    (at your option) any later version.
*    
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*    
*    You should have received a copy of the GNU General Public License
*    along with this program; if not write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*
*****************************************************************************/



#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_precomp.h"

#define _GC_OBJ_ZONE    gcvZONE_DATABASE

#define RECORD_HASH_TABLE_SIZE 4096
/*******************************************************************************
***** Private fuctions ********************************************************/

static
gctUINT32 _gcRecordHash( gctUINT32 a)
{
       a = (a+0x7ed55d16) + (a<<12);
       a = (a^0xc761c23c) ^ (a>>19);
       a = (a+0x165667b1) + (a<<5);
       a = (a+0xd3a2646c) ^ (a<<9);
       a = (a+0xfd7046c5) + (a<<3);
       a = (a^0xb55a4f09) ^ (a>>16);
       return a % RECORD_HASH_TABLE_SIZE;
}

#if MRVL_PULSE_EATER_COUNT_NUM
#define PulseEaterDB(index) dbPulse->data[index]
#define PulseIter gcmCOUNTOF(dbPulse->data[0].pulseCount)
#endif

/*******************************************************************************
**  gckKERNEL_NewDatabase
**
**  Create a new database structure and insert it to the head of the hash list.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          ProcessID that identifies the database.
**
**  OUTPUT:
**
**      gcsDATABASE_PTR * Database
**          Pointer to a variable receiving the database structure pointer on
**          success.
*/
static gceSTATUS
gckKERNEL_NewDatabase(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    OUT gcsDATABASE_PTR * Database
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database = gcvNULL;
    gctBOOL acquired = gcvFALSE;
    gctSIZE_T slot;
    gctBOOL newAllocation = gcvFALSE;
    gcsDATABASE_PTR existingDatabase;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d", Kernel, ProcessID);

    /* Acquire the database mutex. */
    gcmkONERROR(gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));
    acquired = gcvTRUE;

    /* Compute the hash for the database. */
    slot = ProcessID % gcmCOUNTOF(Kernel->db->db);

    /* Walk the hash list. */
    for (existingDatabase = Kernel->db->db[slot];
         existingDatabase != gcvNULL;
         existingDatabase = existingDatabase->next)
    {
        if (existingDatabase->processID == ProcessID)
        {
            /* One process can't be added twice. */
            gcmkONERROR(gcvSTATUS_NOT_SUPPORTED);
        }
    }

    if (Kernel->db->freeDatabase != gcvNULL)
    {
        /* Allocate a database from the free list. */
        database             = Kernel->db->freeDatabase;
        Kernel->db->freeDatabase = database->next;
    }
    else
    {
        gctPOINTER pointer = gcvNULL;
        newAllocation = gcvTRUE;

        /* Allocate a new database from the heap. */
        gcmkONERROR(gckOS_Allocate(Kernel->os,
                                   gcmSIZEOF(gcsDATABASE),
                                   &pointer));

        database = pointer;

        database->htable = gcvNULL;
        gcmkONERROR(gckOS_Allocate(Kernel->os,
                                    gcmSIZEOF(gctPOINTER)*RECORD_HASH_TABLE_SIZE,
                                    (gctPOINTER *)&database->htable));
    }

    /* Insert the database into the hash. */
    database->next   = Kernel->db->db[slot];
    Kernel->db->db[slot] = database;

    /* Save the hash slot. */
    database->slot = slot;

    /* Release the database mutex. */
    gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    /* Return the database. */
    *Database = database;

    /* Success. */
    gcmkFOOTER_ARG("*Database=0x%x", *Database);
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the database mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));
    }

    if(newAllocation)
    {
        if(database)
        {
            if(database->htable)
            {
                gcmkVERIFY_OK(gckOS_Free(Kernel->os, database->htable));
            }
            gcmkVERIFY_OK(gckOS_Free(Kernel->os, database));
        }
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_FindDatabase
**
**  Find a database identified by a process ID and move it to the head of the
**  hash list.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          ProcessID that identifies the database.
**
**      gctBOOL LastProcessID
**          gcvTRUE if searching for the last known process ID.  gcvFALSE if
**          we need to search for the process ID specified by the ProcessID
**          argument.
**
**  OUTPUT:
**
**      gcsDATABASE_PTR * Database
**          Pointer to a variable receiving the database structure pointer on
**          success.
*/
gceSTATUS
gckKERNEL_FindDatabase(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    IN gctBOOL LastProcessID,
    OUT gcsDATABASE_PTR * Database
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database, previous;
    gctSIZE_T slot;
    gctBOOL acquired = gcvFALSE;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d LastProcessID=%d",
                   Kernel, ProcessID, LastProcessID);

    /* Compute the hash for the database. */
    slot = ProcessID % gcmCOUNTOF(Kernel->db->db);

    /* Acquire the database mutex. */
    gcmkONERROR(
        gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));
    acquired = gcvTRUE;

    /* Check whether we are getting the last known database. */
    if (LastProcessID)
    {
        /* Use last database. */
        database = Kernel->db->lastDatabase;

        if (database == gcvNULL)
        {
            /* Database not found. */
            gcmkONERROR(gcvSTATUS_INVALID_DATA);
        }
    }
    else
    {
        /* Walk the hash list. */
        for (previous = gcvNULL, database = Kernel->db->db[slot];
             database != gcvNULL;
             database = database->next)
        {
            if (database->processID == ProcessID)
            {
                /* Found it! */
                break;
            }

            previous = database;
        }

        if (database == gcvNULL)
        {
            /* Database not found. */
            gcmkONERROR(gcvSTATUS_INVALID_DATA);
        }

        if (previous != gcvNULL)
        {
            /* Move database to the head of the hash list. */
            previous->next   = database->next;
            database->next   = Kernel->db->db[slot];
            Kernel->db->db[slot] = database;
        }
    }

    /* Release the database mutex. */
    gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    /* Return the database. */
    *Database = database;

    /* Success. */
    gcmkFOOTER_ARG("*Database=0x%x", *Database);
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the database mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_DeleteDatabase
**
**  Remove a database from the hash list and delete its structure.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gcsDATABASE_PTR Database
**          Pointer to the database structure to remove.
**
**  OUTPUT:
**
**      Nothing.
*/
static gceSTATUS
gckKERNEL_DeleteDatabase(
    IN gckKERNEL Kernel,
    IN gcsDATABASE_PTR Database
    )
{
    gceSTATUS status;
    gctBOOL acquired = gcvFALSE;
    gcsDATABASE_PTR database;

    gcmkHEADER_ARG("Kernel=0x%x Database=0x%x", Kernel, Database);

    /* Acquire the database mutex. */
    gcmkONERROR(
        gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));
    acquired = gcvTRUE;

    /* Check slot value. */
    gcmkVERIFY_ARGUMENT(Database->slot < gcmCOUNTOF(Kernel->db->db));

    if (Database->slot < gcmCOUNTOF(Kernel->db->db))
    {
        /* Check if database if the head of the hash list. */
        if (Kernel->db->db[Database->slot] == Database)
        {
            /* Remove the database from the hash list. */
            Kernel->db->db[Database->slot] = Database->next;
        }
        else
        {
            /* Walk the has list to find the database. */
            for (database = Kernel->db->db[Database->slot];
                 database != gcvNULL;
                 database = database->next
            )
            {
                /* Check if the next list entry is this database. */
                if (database->next == Database)
                {
                    /* Remove the database from the hash list. */
                    database->next = Database->next;
                    break;
                }
            }

            if (database == gcvNULL)
            {
                /* Ouch!  Something got corrupted. */
                gcmkONERROR(gcvSTATUS_INVALID_DATA);
            }
        }
    }

    if (Kernel->db->lastDatabase != gcvNULL)
    {
        /* Insert database to the free list. */
        Kernel->db->lastDatabase->next = Kernel->db->freeDatabase;
        Kernel->db->freeDatabase       = Kernel->db->lastDatabase;
    }

    /* Keep database as the last database. */
    Kernel->db->lastDatabase = Database;

    /* Release the database mutex. */
    gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the database mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_NewRecord
**
**  Create a new database record structure and insert it to the head of the
**  database.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gcsDATABASE_PTR Database
**          Pointer to a database structure.
**
**  OUTPUT:
**
**      gcsDATABASE_RECORD_PTR * Record
**          Pointer to a variable receiving the database record structure
**          pointer on success.
*/
static gceSTATUS
gckKERNEL_NewRecord(
    IN gckKERNEL Kernel,
    IN gcsDATABASE_PTR Database,
    IN gctPOINTER Data,
    OUT gcsDATABASE_RECORD_PTR * Record
    )
{
    gceSTATUS status;
    gctBOOL acquired = gcvFALSE;
    gcsDATABASE_RECORD_PTR record = gcvNULL;
    gctUINT32 recordslot;

    gcmkHEADER_ARG("Kernel=0x%x Database=0x%x", Kernel, Database);

    /* Acquire the database mutex. */
    gcmkONERROR(
        gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));
    acquired = gcvTRUE;

    if (Kernel->db->freeRecord != gcvNULL)
    {
        /* Allocate the record from the free list. */
        record             = Kernel->db->freeRecord;
        Kernel->db->freeRecord = record->next;
    }
    else
    {
        gctPOINTER pointer = gcvNULL;

        /* Allocate the record from the heap. */
        gcmkONERROR(gckOS_Allocate(Kernel->os,
                                   gcmSIZEOF(gcsDATABASE_RECORD),
                                   &pointer));

        record = pointer;
    }

    recordslot = _gcRecordHash((gctUINT32)Data);

    record->next = Database->htable[recordslot];
    Database->htable[recordslot] = record;

    /* Release the database mutex. */
    gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    /* Return the record. */
    *Record = record;

    /* Success. */
    gcmkFOOTER_ARG("*Record=0x%x", *Record);
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the database mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));
    }
    if (record != gcvNULL)
    {
        gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Kernel->os, record));
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_DeleteRecord
**
**  Remove a database record from the database and delete its structure.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gcsDATABASE_PTR Database
**          Pointer to a database structure.
**
**      gceDATABASE_TYPE Type
**          Type of the record to remove.
**
**      gctPOINTER Data
**          Data of the record to remove.
**
**  OUTPUT:
**
**      gctSIZE_T_PTR Bytes
**          Pointer to a variable that receives the size of the record deleted.
**          Can be gcvNULL if the size is not required.
*/
static gceSTATUS
gckKERNEL_DeleteRecord(
    IN gckKERNEL Kernel,
    IN gcsDATABASE_PTR Database,
    IN gceDATABASE_TYPE Type,
    IN gctPOINTER Data,
    OUT gctSIZE_T_PTR Bytes OPTIONAL
    )
{
    gceSTATUS status;
    gctBOOL acquired = gcvFALSE;
    gcsDATABASE_RECORD_PTR record, previous;
    gctUINT32 recordslot = 0;

    gcmkHEADER_ARG("Kernel=0x%x Database=0x%x Type=%d Data=0x%x",
                   Kernel, Database, Type, Data);

    /* Acquire the database mutex. */
    gcmkONERROR(
        gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));
    acquired = gcvTRUE;

    recordslot = _gcRecordHash((gctUINT32)Data);

    for(record = Database->htable[recordslot], previous = gcvNULL;
        record != gcvNULL;
        record = record->next)
    {
        if ((record->type == Type)
        &&  (record->data == Data)
        )
        {
            /* Found it! */
            break;
        }

        previous = record;
    }

    if (record == gcvNULL)
    {
        /* Ouch!  This record is not found? */
        gcmkONERROR(gcvSTATUS_INVALID_DATA);
    }

    if (Bytes != gcvNULL)
    {
        /* Return size of record. */
        *Bytes = record->bytes;
    }

    /* Remove record from database. */
    if (previous == gcvNULL)
    {
        Database->htable[recordslot] = record->next;
    }
    else
    {
        previous->next = record->next;
    }

    /* Insert record in free list. */
    record->next       = Kernel->db->freeRecord;
    Kernel->db->freeRecord = record;

    /* Release the database mutex. */
    gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    /* Success. */
    gcmkFOOTER_ARG("*Bytes=%lu", gcmOPT_VALUE(Bytes));
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the database mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_FindRecord
**
**  Find a database record from the database.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gcsDATABASE_PTR Database
**          Pointer to a database structure.
**
**      gceDATABASE_TYPE Type
**          Type of the record to remove.
**
**      gctPOINTER Data
**          Data of the record to remove.
**
**  OUTPUT:
**
**      gctSIZE_T_PTR Bytes
**          Pointer to a variable that receives the size of the record deleted.
**          Can be gcvNULL if the size is not required.
*/
static gceSTATUS
gckKERNEL_FindRecord(
    IN gckKERNEL Kernel,
    IN gcsDATABASE_PTR Database,
    IN gceDATABASE_TYPE Type,
    IN gctPOINTER Data,
    OUT gcsDATABASE_RECORD_PTR Record
    )
{
    gceSTATUS status;
    gctBOOL acquired = gcvFALSE;
    gcsDATABASE_RECORD_PTR record;
    gctUINT32 recordslot = 0;
    gcmkHEADER_ARG("Kernel=0x%x Database=0x%x Type=%d Data=0x%x",
                   Kernel, Database, Type, Data);

    /* Acquire the database mutex. */
    gcmkONERROR(
        gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));
    acquired = gcvTRUE;

    recordslot = _gcRecordHash((gctUINT32)Data);

    for(record = Database->htable[recordslot];
        record != gcvNULL;
        record = record->next)
    {
        if ((record->type == Type)
        &&  (record->data == Data)
        )
        {
            /* Found it! */
            break;
        }
    }

    if (record == gcvNULL)
    {
        /* Ouch!  This record is not found? */
        gcmkONERROR(gcvSTATUS_INVALID_DATA);
    }

    if (Record != gcvNULL)
    {
        /* Return information of record. */
        gcmkONERROR(
            gckOS_MemCopy(Record, record, sizeof(gcsDATABASE_RECORD)));
    }

    /* Release the database mutex. */
    gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    /* Success. */
    gcmkFOOTER_ARG("Record=0x%x", Record);
    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        /* Release the database mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}


/*******************************************************************************
***** Public API **************************************************************/

/*******************************************************************************
**  gckKERNEL_CreateProcessDB
**
**  Create a new process database.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          Process ID used to identify the database.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_CreateProcessDB(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database = gcvNULL;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d", Kernel, ProcessID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Create a new database. */
    gcmkONERROR(gckKERNEL_NewDatabase(Kernel, ProcessID, &database));

    /* Initialize the database. */
    database->processID             = ProcessID;
    database->vidMem.bytes          = 0;
    database->vidMem.maxBytes       = 0;
    database->vidMem.periodMaxBytes = 0;
    database->vidMem.totalBytes     = 0;
    database->nonPaged.bytes        = 0;
    database->nonPaged.maxBytes     = 0;
    database->nonPaged.periodMaxBytes = 0;
    database->nonPaged.totalBytes   = 0;
    database->contiguous.bytes      = 0;
    database->contiguous.maxBytes   = 0;
    database->contiguous.periodMaxBytes = 0;
    database->contiguous.totalBytes = 0;
    database->mapMemory.bytes          = 0;
    database->mapMemory.maxBytes       = 0;
    database->mapMemory.periodMaxBytes = 0;
    database->mapMemory.totalBytes     = 0;
    database->mapUserMemory.bytes      = 0;
    database->mapUserMemory.maxBytes   = 0;
    database->mapUserMemory.periodMaxBytes       = 0;
    database->mapUserMemory.totalBytes = 0;
    database->conAllocSucc          = 0;
    database->conAllocFail          = 0;
    database->conAllocTime          = 0;
    database->virtAllocSucc         = 0;
    database->virtAllocFail         = 0;
    database->virtAllocTime         = 0;
    database->ionAllocSucc          = 0;
    database->ionAllocFail          = 0;
    database->ionAllocTime          = 0;
    database->resAllocSucc          = 0;
    database->resAllocFail          = 0;
    database->resAllocTime          = 0;
    database->list                  = gcvNULL;
    gckOS_ZeroMemory(database->htable, gcmSIZEOF(gctPOINTER)*RECORD_HASH_TABLE_SIZE);

#if gcdSECURE_USER
    {
        gctINT slot;
        gcskSECURE_CACHE * cache = &database->cache;

        /* Setup the linked list of cache nodes. */
        for (slot = 1; slot <= gcdSECURE_CACHE_SLOTS; ++slot)
        {
            cache->cache[slot].logical = gcvNULL;

#if gcdSECURE_CACHE_METHOD != gcdSECURE_CACHE_TABLE
            cache->cache[slot].prev = &cache->cache[slot - 1];
            cache->cache[slot].next = &cache->cache[slot + 1];
#   endif
#if gcdSECURE_CACHE_METHOD == gcdSECURE_CACHE_HASH
            cache->cache[slot].nextHash = gcvNULL;
            cache->cache[slot].prevHash = gcvNULL;
#   endif
        }

#if gcdSECURE_CACHE_METHOD != gcdSECURE_CACHE_TABLE
        /* Setup the head and tail of the cache. */
        cache->cache[0].next    = &cache->cache[1];
        cache->cache[0].prev    = &cache->cache[gcdSECURE_CACHE_SLOTS];
        cache->cache[0].logical = gcvNULL;

        /* Fix up the head and tail pointers. */
        cache->cache[0].next->prev = &cache->cache[0];
        cache->cache[0].prev->next = &cache->cache[0];
#   endif

#if gcdSECURE_CACHE_METHOD == gcdSECURE_CACHE_HASH
        /* Zero out the hash table. */
        for (slot = 0; slot < gcmCOUNTOF(cache->hash); ++slot)
        {
            cache->hash[slot].logical  = gcvNULL;
            cache->hash[slot].nextHash = gcvNULL;
        }
#   endif

        /* Initialize cache index. */
        cache->cacheIndex = gcvNULL;
        cache->cacheFree  = 1;
        cache->cacheStamp = 0;
    }
#endif

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_AddProcessDB
**
**  Add a record to a process database.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          Process ID used to identify the database.
**
**      gceDATABASE_TYPE TYPE
**          Type of the record to add.
**
**      gctPOINTER Pointer
**          Data of the record to add.
**
**      gctPHYS_ADDR Physical
**          Physical address of the record to add.
**
**      gctSIZE_T Size
**          Size of the record to add.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_AddProcessDB(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    IN gceDATABASE_TYPE Type,
    IN gctPOINTER Pointer,
    IN gctPHYS_ADDR Physical,
    IN gctSIZE_T Size
    )
{
    gceSTATUS status;
    gctBOOL acquired = gcvFALSE;
    gcsDATABASE_PTR database;
    gcsDATABASE_RECORD_PTR record = gcvNULL;
    gcsDATABASE_COUNTERS * count;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d Type=%d Pointer=0x%x "
                   "Physical=0x%x Size=%lu",
                   Kernel, ProcessID, Type, Pointer, Physical, Size);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Special case the idle record. */
    if (Type == gcvDB_IDLE)
    {
        gctUINT64 time;

        /* Get the current profile time. */
        gcmkONERROR(gckOS_GetProfileTick(&time));

        /* Grab the mutex. */
        gcmkONERROR(gckOS_AcquireMutex(Kernel->os, Kernel->db->profMutex, gcvINFINITE));
        acquired = gcvTRUE;

        if ((ProcessID == 0) && (Kernel->db->lastIdle[Kernel->core] != 0))
        {
            /* Out of idle, adjust time it was idle. */
            Kernel->db->idleTime[Kernel->core] += time - Kernel->db->lastIdle[Kernel->core];
            Kernel->db->lastIdle[Kernel->core]  = 0;
            Kernel->db->lastBusy[Kernel->core]  = time;
            Kernel->db->isIdle[Kernel->core]    = gcvFALSE;
            Kernel->db->switchCount[Kernel->core]++;
            gckKERNEL_AddProfNode(Kernel, Kernel->db->isIdle[Kernel->core], gckOS_ProfileToMS(time));
            /*gcmkPRINT("@@[core-%d][%6d]      Busy", Kernel->core, Kernel->db->switchCount[Kernel->core]);*/
        }
        else if ((ProcessID == 1) && (Kernel->db->lastBusy[Kernel->core] != 0))
        {
            /* Save current idle time. */
            Kernel->db->lastIdle[Kernel->core] = time;
            Kernel->db->lastBusy[Kernel->core]  = 0;
            Kernel->db->isIdle[Kernel->core]   = gcvTRUE;
            Kernel->db->switchCount[Kernel->core]++;
            gckKERNEL_AddProfNode(Kernel, Kernel->db->isIdle[Kernel->core], gckOS_ProfileToMS(time));
            /*gcmkPRINT("##[core-%d][%6d] Idle", Kernel->core, Kernel->db->switchCount[Kernel->core]);*/
        }

        /* Release the mutex. */
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->profMutex));
        acquired = gcvFALSE;

#if gcdDYNAMIC_SPEED
        {
            /* Test for first call. */
            if (Kernel->db->lastSlowdown[Kernel->core] == 0)
            {
                /* Save milliseconds. */
                Kernel->db->lastSlowdown[Kernel->core]     = time;
                Kernel->db->lastSlowdownIdle[Kernel->core] = Kernel->db->idleTime[Kernel->core];
            }
            else
            {
                /* Compute ellapsed time in milliseconds. */
                gctUINT delta = gckOS_ProfileToMS(time - Kernel->db->lastSlowdown[Kernel->core]);

                /* Test for end of period. */
                if (delta >= gcdDYNAMIC_SPEED)
                {
                    /* Compute number of idle milliseconds. */
                    gctUINT idle = gckOS_ProfileToMS(
                        Kernel->db->idleTime[Kernel->core]  - Kernel->db->lastSlowdownIdle[Kernel->core]);

                    /* Broadcast to slow down the GPU. */
                    gcmkONERROR(gckOS_BroadcastCalibrateSpeed(Kernel->os,
                                                              Kernel->hardware,
                                                              idle,
                                                              delta));

                    /* Save current time. */
                    Kernel->db->lastSlowdown[Kernel->core]     = time;
                    Kernel->db->lastSlowdownIdle[Kernel->core] = Kernel->db->idleTime[Kernel->core];
                }
            }
        }
#endif

        /* Success. */
        gcmkFOOTER_NO();
        return gcvSTATUS_OK;
    }

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(Pointer != gcvNULL);

    /* Find the database. */
    gcmkONERROR(gckKERNEL_FindDatabase(Kernel, ProcessID, gcvFALSE, &database));

    /* Create a new record in the database. */
    gcmkONERROR(gckKERNEL_NewRecord(Kernel, database, Pointer, &record));

    /* Initialize the record. */
    record->kernel   = Kernel;
    record->type     = Type;
    record->data     = Pointer;
    record->physical = Physical;
    record->bytes    = Size;
    record->flag     = Kernel->dbflag;

    /* Get pointer to counters. */
    switch (Type)
    {
    case gcvDB_VIDEO_MEMORY:
        count = &database->vidMem;
        break;

    case gcvDB_NON_PAGED:
        count = &database->nonPaged;
        break;

    case gcvDB_CONTIGUOUS:
        count = &database->contiguous;
        break;

    case gcvDB_MAP_MEMORY:
        count = &database->mapMemory;
        break;

    case gcvDB_MAP_USER_MEMORY:
        count = &database->mapUserMemory;
        break;

    default:
        count = gcvNULL;
        break;
    }

    if (count != gcvNULL)
    {
        /* Adjust counters. */
        count->totalBytes += Size;
        count->bytes      += Size;

        if (count->bytes > count->maxBytes)
        {
            count->maxBytes = count->bytes;
        }
        if (count->bytes > count->periodMaxBytes)
        {
            count->periodMaxBytes = count->bytes;
        }
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (acquired == gcvTRUE)
    {
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->profMutex));
    }

    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_RemoveProcessDB
**
**  Remove a record from a process database.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          Process ID used to identify the database.
**
**      gceDATABASE_TYPE TYPE
**          Type of the record to remove.
**
**      gctPOINTER Pointer
**          Data of the record to remove.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_RemoveProcessDB(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    IN gceDATABASE_TYPE Type,
    IN gctPOINTER Pointer
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;
    gctSIZE_T bytes = 0;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d Type=%d Pointer=0x%x",
                   Kernel, ProcessID, Type, Pointer);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Pointer != gcvNULL);

    /* Find the database. */
    gcmkONERROR(gckKERNEL_FindDatabase(Kernel, ProcessID, gcvFALSE, &database));

    /* Delete the record. */
    gcmkONERROR(
        gckKERNEL_DeleteRecord(Kernel, database, Type, Pointer, &bytes));

    /* Update counters. */
    switch (Type)
    {
    case gcvDB_VIDEO_MEMORY:
        database->vidMem.bytes -= bytes;
        break;

    case gcvDB_NON_PAGED:
        database->nonPaged.bytes -= bytes;
        break;

    case gcvDB_CONTIGUOUS:
        database->contiguous.bytes -= bytes;
        break;

    case gcvDB_MAP_MEMORY:
        database->mapMemory.bytes -= bytes;
        break;

    case gcvDB_MAP_USER_MEMORY:
        database->mapUserMemory.bytes -= bytes;
        break;

    default:
        break;
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_FindProcessDB
**
**  Find a record from a process database.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          Process ID used to identify the database.
**
**      gceDATABASE_TYPE TYPE
**          Type of the record to remove.
**
**      gctPOINTER Pointer
**          Data of the record to remove.
**
**  OUTPUT:
**
**      gcsDATABASE_RECORD_PTR Record
**          Copy of record.
*/
gceSTATUS
gckKERNEL_FindProcessDB(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    IN gctUINT32 ThreadID,
    IN gceDATABASE_TYPE Type,
    IN gctPOINTER Pointer,
    OUT gcsDATABASE_RECORD_PTR Record
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d Type=%d Pointer=0x%x",
                   Kernel, ProcessID, ThreadID, Type, Pointer);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Pointer != gcvNULL);

    /* Find the database. */
    gcmkONERROR(gckKERNEL_FindDatabase(Kernel, ProcessID, gcvFALSE, &database));

    /* Find the record. */
    gcmkONERROR(
        gckKERNEL_FindRecord(Kernel, database, Type, Pointer, Record));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

static gctSTRING _gc_VIDMEM_type_name[]={
    "TYPE_UNKNOWN",
    "INDEX",
    "VERTEX",
    "TEXTURE",
    "RENDER_TARGET",
    "DEPTH",
    "BITMAP",
    "TILE_STATUS",
    "IMAGE",
    "MASK",
    "SCISSOR",
    "HIERARCHICAL_DEPTH"
};

void _print_VIDMEM_by_pid(
    gcsDATABASE_PTR database,
    gctUINT ProcessID
)
{
    gcuVIDMEM_NODE_PTR node;
    gcsDATABASE_RECORD_PTR record;
    gctINT32 size[gcvSURF_NUM_TYPES] = {0};
    gctINT i, j;

    gcmkPRINT("VIDMEM detail for PID %d \n", ProcessID);

    for(j = 0; j < RECORD_HASH_TABLE_SIZE; j++)
    {
        /* go through each hash table slot */
        for(record = database->htable[j]; record != gcvNULL; record = record->next)
        {
            if(record->type == gcvDB_VIDEO_MEMORY)
            {
                node = (gcuVIDMEM_NODE_PTR)record->data;
                if((node->VidMem.memory != gcvNULL) &&
                   (node->VidMem.memory->object.type == gcvOBJ_VIDMEM))
                {
                    size[node->VidMem.surfType] += node->VidMem.bytes;
                }
                else
                {
                    size[node->Virtual.surfType] += node->Virtual.bytes;
                }
            }
        }
    }

    for(i = 0 ; i < gcvSURF_NUM_TYPES; i++)
    {
        if(size[i] >= 1024)
        {
            gcmkPRINT("-- type %2d: %-16s, size %d KB \n", i, _gc_VIDMEM_type_name[i], size[i]/1024);
        }
        else if(size[i] > 0)
        {
            gcmkPRINT("-- type %2d: %-16s, size %d B \n", i, _gc_VIDMEM_type_name[i], size[i]);
        }
    }
}

void _print_VIDMEM_by_type(
    gcsDATABASE_PTR DataBase[16],
    gctINT Type
)
{
    gcuVIDMEM_NODE_PTR node;
    gctINT32 size = 0;
    gctINT i, j;
    gcsDATABASE_PTR database;
    gcsDATABASE_RECORD_PTR record;

    gcmkPRINT("VIDMEM detail for Type %d, %s \n", Type, _gc_VIDMEM_type_name[Type]);

    for(i = 0; i < 16; i++)
    {
        database = DataBase[i];
        while(database)
        {
            size = 0;

            for(j = 0; j < RECORD_HASH_TABLE_SIZE; j++)
            {
                /* go through each hash table slot */
                for(record = database->htable[j]; record != gcvNULL; record = record->next)
                {
                    if(record->type == gcvDB_VIDEO_MEMORY)
                    {
                        node = (gcuVIDMEM_NODE_PTR)record->data;
                        if((node->VidMem.memory != gcvNULL) &&
                           (node->VidMem.memory->object.type == gcvOBJ_VIDMEM))
                        {
                            if(node->VidMem.surfType == Type)
                            {
                                size += node->VidMem.bytes;
                            }
                        }
                        else
                        {
                            if(node->Virtual.surfType == Type)
                            {
                                size += node->Virtual.bytes;
                            }
                        }
                    }
                }
            }

            if(size >= 1024)
            {
                gcmkPRINT("  -- pid %-5d: size: %d KB\n", database->processID, size/1024);
            }
            else if(size > 0)
            {
                gcmkPRINT("  -- pid %-5d: size: %d B\n", database->processID, size);
            }
            database = database->next;
        }
    }
}

/*******************************************************************************
**  gckKERNEL_ShowVidMemUsageDetails
**
**  Print out a process video memory details from the database.
**  Usage:
**      echo pid > mem_stats
**        Print out Video memory detailed information and list by process
**      echo pid 105 > mem_stats
**        Print out Video memory detailed information for pid 105
**      echo type > mem_stats
**        Print out video memory detailed information and list by memory type
**      echo type 4 > mem_stats
**        Print out video memory detailed information for type 4
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctBOOL ListByPID
**          Flag indicates whether list by PID or type.
**
**      gctUINT32 Value
**          The second input value. If list by PID, it is PID, else it is type.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_ShowVidMemUsageDetails(
    IN gckKERNEL Kernel,
    IN gctBOOL ListByPID,
    IN gctINT Value
)
{
    gctUINT32 i;
    gcsDATABASE_PTR database;

    gcmkHEADER_ARG("Kernel=0x%x", Kernel);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    if(ListByPID)
    {
        if(Value == -1)
        {
            for(i = 0; i < gcmCOUNTOF(Kernel->db->db); i++)
            {
                database = Kernel->db->db[i];
                while(database)
                {
                    _print_VIDMEM_by_pid(database, database->processID);
                    database = database->next;
                }
            }
        }
        else
        {
            gctINT slot = Value % gcmCOUNTOF(Kernel->db->db);
            database = Kernel->db->db[slot];

            while(database && database->processID != Value)
            {
                database = database->next;
            }
            if(database)
            {
                _print_VIDMEM_by_pid(database, database->processID);
            }
            else
            {
                gcmkPRINT("No such process in memory record: %d \n", Value);
            }
        }
    }
    else
    {
        if(Value >= 0 && Value < gcvSURF_NUM_TYPES)
        {
            _print_VIDMEM_by_type(Kernel->db->db, Value);
        }
        else
        {
            for(i = 0; i < gcvSURF_NUM_TYPES; i++)
            {
                _print_VIDMEM_by_type(Kernel->db->db, i);
            }
        }
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**  gckKERNEL_ShowVidMemPeriodUsage
**
**  Print out max video memory use in a period.
**
**  Usage:
**      echo period 0 > mem_stats
**        Start period and begin to count
**      echo period 1 > mem_stats
**        Print out Video memory max use from start
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 Value
**          The second input value. If is 0, restart the period, if is 1, print the usage.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_ShowVidMemPeriodUsage(
    IN gckKERNEL Kernel,
    IN gctINT Value
)
{
    gctUINT32 i;
    gctSIZE_T size;
    gcsDATABASE_PTR database;

    gcmkHEADER_ARG("Kernel=0x%x", Kernel);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    if(0 == Value)
    {
        for(i = 0; i < 16; i++)
        {
            database = Kernel->db->db[i];
            while(database)
            {
                database->vidMem.periodMaxBytes = database->vidMem.bytes;
                database->conAllocSucc          = 0;
                database->conAllocFail          = 0;
                database->conAllocTime          = 0;
                database->virtAllocSucc         = 0;
                database->virtAllocFail         = 0;
                database->virtAllocTime         = 0;
                database->ionAllocSucc          = 0;
                database->ionAllocFail          = 0;
                database->ionAllocTime          = 0;
                database->resAllocSucc          = 0;
                database->resAllocFail          = 0;
                database->resAllocTime          = 0;

                database = database->next;
            }
        }
        gcmkPRINT("A new period begin.\n");
    }
    else if(1 == Value)
    {
        size = 0;

        gcmkPRINT("In current period:\n");

        for(i = 0; i < 16; i++)
        {
            database = Kernel->db->db[i];
            while(database)
            {
                if(0 < database->vidMem.periodMaxBytes
                   || 0 < database->conAllocFail
                   || 0 < database->virtAllocFail
                   || 0 < database->ionAllocFail
                   || 0 < database->resAllocFail)
                {
                    gcmkPRINT("  --PID %d:\n"
                            "    --Max allocted mem bytes %lu B\n"
                            "    --Continuous alloc success  %u\n"
                            "                       fail     %u\n"
                            "                       duration %u ms\n"
                            "    --Virtual    alloc success  %u\n"
                            "                       fail     %u\n"
                            "                       duration %u ms\n"
                            "    --Ion        alloc success  %u\n"
                            "                       fail     %u\n"
                            "                       duration %u ms\n"
                            "    --Reserved   alloc success  %u\n"
                            "                       fail     %u\n"
                            "                       duration %u ms\n"
                            , database->processID
                            , database->vidMem.periodMaxBytes
                            , database->conAllocSucc, database->conAllocFail, database->conAllocTime
                            , database->virtAllocSucc, database->virtAllocFail, database->virtAllocTime
                            , database->ionAllocSucc, database->ionAllocFail, database->ionAllocTime
                            , database->resAllocSucc, database->resAllocFail, database->resAllocTime
                    );
                    size += database->vidMem.periodMaxBytes;
                }
                database = database->next;
            }
        }

        if(size >= 10240)
        {
            gcmkPRINT("  Total period max mem: %lu KB\n", size/1024);
        }
        else
        {
            gcmkPRINT("  Total period max mem: %lu B\n", size);
        }
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**  gckKERNEL_ShowProcessMemUsage
**
**  Print out a process database when release, including used bytes, max bytes
**  and total bytes for all types of memory.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          Process ID used to identify the database.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_ShowProcessMemUsage(
    IN gckKERNEL Kernel,
    gctUINT32 ProcessID
)
{
    gctUINT32 slot;
    gcsDATABASE_PTR database;

    gcmkHEADER_ARG("Kernel=0x%X", Kernel);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(ProcessID > 0);

    /* Compute the hash for the database. */
    slot = ProcessID % gcmCOUNTOF(Kernel->db->db);

    database = Kernel->db->db[slot];
    while(database)
    {
        if(database->processID == ProcessID)
        {
            break;
        }
        database = database->next;
    }
    if(database && database->processID == ProcessID)
    {
        gcmkPRINT("Process %d released: \n", database->processID);

        gcmkPRINT(" -- VidMem:         used bytes %7d KB, max bytes %7d KB, total bytes %7d KB\n",
              (gctINT)database->vidMem.bytes/1024,
              (gctINT)database->vidMem.maxBytes/1024,
              (gctINT)database->vidMem.totalBytes/1024);
        gcmkPRINT(" -- NonPaged Mem:   used bytes %7d KB, max bytes %7d KB, total bytes %7d KB\n",
              (gctINT)database->nonPaged.bytes/1024,
              (gctINT)database->nonPaged.maxBytes/1024,
              (gctINT)database->nonPaged.totalBytes/1024);
        gcmkPRINT(" -- Contiguous Mem: used bytes %7d KB, max bytes %7d KB, total bytes %7d KB\n",
              (gctINT)database->contiguous.bytes/1024,
              (gctINT)database->contiguous.maxBytes/1024,
              (gctINT)database->contiguous.totalBytes/1024);
        gcmkPRINT(" -- MapUserMemory:  used bytes %7d KB, max bytes %7d KB, total bytes %7d KB\n",
              (gctINT)database->mapUserMemory.bytes/1024,
              (gctINT)database->mapUserMemory.maxBytes/1024,
              (gctINT)database->mapUserMemory.totalBytes/1024);
        gcmkPRINT(" -- MapMemory:      used bytes %7d KB, max bytes %7d KB, total bytes %7d KB\n",
              (gctINT)database->mapMemory.bytes/1024,
              (gctINT)database->mapMemory.maxBytes/1024,
              (gctINT)database->mapMemory.totalBytes/1024);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**  gckKERNEL_ShowProcessMemUsage
**
**  Print out vide memory record details in Database, and save into a file.
**
**  Usage:
**      echo database > mem_stats
**        The result would be save in Filename
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 Type
**          Video memory type. If invalid value, all types would be print out.
**
**      gctCHAR* Filename
**          Filename for the results to save.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_PrintVIDMEMProcessDB(
    IN gckKERNEL Kernel,
    IN gceSURF_TYPE Type,
    IN gctCHAR* Filename
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;
    gcsDATABASE_RECORD_PTR record;
    gctPOINTER fp;
    gctSIZE_T fs;
    char buf[1000];
    gctINT len = 0;
    gcuVIDMEM_NODE_PTR node;
    gctINT i, j;
    gctBOOL bFirstFound = gcvFALSE;

    gcmkHEADER_ARG("Kernel=0x%x", Kernel);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Open file for writing */
    gcmkONERROR(gckOS_OpenFile(Kernel->os, Filename, &fp, &fs));

    if((Type >= gcvSURF_TYPE_UNKNOWN) && (Type < gcvSURF_NUM_TYPES))
    {
        len = sprintf(buf, "\nVideo memory details for type %d\n", Type);
    }
    else
    {
        len = sprintf(buf, "\nVideo memory details for all types\n");
    }
    gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));

    for (i = 0; i < gcmCOUNTOF(Kernel->db->db); i++)
    {
        database = Kernel->db->db[i];
        while(database)
        {
            bFirstFound = gcvFALSE;
            /* Walk all records. */
            for(j = 0; j < RECORD_HASH_TABLE_SIZE; j++)
            {
                record = database->htable[j];

                /* only print the info when first non-empty record found */
                if(record && !bFirstFound)
                {
                    bFirstFound = gcvTRUE;
                    len = sprintf(buf, "\npid: %d\n  %-32s| %5s | %5s | %s\n",
                                  database->processID, "function name", "line", "type", "bytes");
                    gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));
                }

                for(; record != gcvNULL; record = record->next)
                {
                    if(record->type == gcvDB_VIDEO_MEMORY)
                     {
                         len = 0;
                         node = (gcuVIDMEM_NODE_PTR)record->data;

                         if(node->VidMem.memory->object.type == gcvOBJ_VIDMEM)
                         {
                             if((Type >= gcvSURF_TYPE_UNKNOWN) && (Type < gcvSURF_NUM_TYPES))
                             {
                                 if(Type == node->VidMem.surfType)
                                 {
                                     len = sprintf(buf, "  %-32s| %5d | %5d | %d\n",
                                             node->VidMem.funcName,
                                             node->VidMem.line,
                                             node->VidMem.surfType,
                                             (gctUINT)record->bytes);
                                 }
                             }
                             else
                             {
                                 len = sprintf(buf, "  %-32s| %5d | %5d | %d\n",
                                             node->VidMem.funcName,
                                             node->VidMem.line,
                                             node->VidMem.surfType,
                                             (gctUINT)record->bytes);
                             }
                         }
                         else
                         {
                             if((Type >= gcvSURF_TYPE_UNKNOWN) && (Type < gcvSURF_NUM_TYPES))
                             {
                                 if(Type == node->Virtual.surfType)
                                 {
                                     len = sprintf(buf, "  %-32s| %5d | %5d | %d\n",
                                             node->Virtual.funcName,
                                             node->Virtual.line,
                                             node->Virtual.surfType,
                                             (gctUINT)record->bytes);
                                 }
                             }
                             else
                             {
                                 len = sprintf(buf, "  %-32s| %5d | %5d | %d\n",
                                             node->Virtual.funcName,
                                             node->Virtual.line,
                                             node->Virtual.surfType,
                                             (gctUINT)record->bytes);
                             }
                         }
                         if(len > 0)
                         {
                             gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));
                         }
                     }
                }
            }
            database = database->next;
        }
    }

    len = sprintf(buf, "\n------------------------ end ---------------------------\n");
    gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));

    gcmkONERROR(gckOS_CloseFile(Kernel->os, fp, fs));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

gctSTRING _gc_DB_type_name[]=
{
    "",
    "VIDEO_MEMORY",
    "NON_PAGED",
    "CONTIGUOUS",
    "SIGNAL",
    "VIDEO_MEMORY_LOCKED",
    "CONTEXT",
    "IDLE",
    "MAP_MEMORY",
    "SHARED_INFO",
    "MAP_USER_MEMORY"
};

/*******************************************************************************
**  gckKERNEL_PrintFlagedProcessDB
**
**  Print out the flaged records in Database, and save into a file.
**
**  Usage:
**      echo flag > mem_stats
**  The new added records between two input "echo flag" would be saved into file.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctCHAR* Filename
**          Filename for the results to save.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_PrintFlagedProcessDB(
    IN gckKERNEL Kernel,
    IN gctCHAR* Filename
)
{
    gceSTATUS status;
    gcsDATABASE_PTR database;
    gcsDATABASE_RECORD_PTR record;
    gctPOINTER fp;
    gctSIZE_T fs;
    char buf[1000];
    gctINT i, len = 0;
    gcuVIDMEM_NODE_PTR node;
    gctINT j;

    gcmkHEADER_ARG("Kernel=0x%x", Kernel);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Open file for writing */
    gcmkONERROR(gckOS_OpenFile(Kernel->os, Filename, &fp, &fs));

    for (i = 0; i < gcmCOUNTOF(Kernel->db->db); i++)
    {
        database = Kernel->db->db[i];
        while(database)
        {
            len = sprintf(buf, "\npid: %d, All flaged records in DB:\n"
                               " %-24s | %10s | %10s | %5s \n",
               database->processID, "record type", "record data", "record physical", "record bytes");
            gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));

            for(j = 0; j < RECORD_HASH_TABLE_SIZE; j++)
            {
                record = database->htable[j];
                while(record != gcvNULL)
                {
                    if(record->flag)
                    {
                        len = sprintf(buf, "  %-24s | %10x | %10x | %5d \n",
                                             _gc_DB_type_name[record->type],
                                            (gctUINT)record->data,
                                            (gctUINT)record->physical,
                                            (gctUINT)record->bytes);
                        gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));
                    }

                    record = record->next;
                }
            }

            /* print out VIDMEM records detail */

            len = sprintf(buf, "\npid: %d, VIDMEM records detail:\n  %-32s| %5s | %5s | %s\n",
                              database->processID, "function name", "line", "type", "bytes");
            gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));

            for(j = 0; j < RECORD_HASH_TABLE_SIZE; j++)
            {
                record = database->htable[j];

                while(record)
                {
                    if(record->type == gcvDB_VIDEO_MEMORY && record->flag)
                    {
                        node = (gcuVIDMEM_NODE_PTR)record->data;

                        if(node->VidMem.memory->object.type == gcvOBJ_VIDMEM)
                        {
                            len = sprintf(buf, "  %-32s| %5d | %5d | %d\n",
                                            node->VidMem.funcName,
                                            node->VidMem.line,
                                            node->VidMem.surfType,
                                            (gctUINT)record->bytes);
                        }
                        else
                        {
                            len = sprintf(buf, "  %-32s| %5d | %5d | %d\n",
                                            node->Virtual.funcName,
                                            node->Virtual.line,
                                            node->Virtual.surfType,
                                            (gctUINT)record->bytes);
                        }
                        gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));
                    }
                    /* Clear the record flag */
                    if(record->flag)
                    {
                        record->flag = gcvFALSE;
                    }
                    record = record->next;
                }
            }

            database = database->next;
        }
    }

    len = sprintf(buf, "\n------------------------ end ---------------------------\n");
    gcmkONERROR(gckOS_WriteFile(Kernel->os, fp, buf, len));

    gcmkONERROR(gckOS_CloseFile(Kernel->os, fp, fs));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_DestroyProcessDB
**
**  Destroy a process database.  If the database contains any records, the data
**  inside those records will be deleted as well.  This aids in the cleanup if
**  a process has died unexpectedly or has memory leaks.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          Process ID used to identify the database.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_DestroyProcessDB(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;
    gcsDATABASE_RECORD_PTR record, next;
    gctBOOL asynchronous;
    gctUINT32 i;
    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d", Kernel, ProcessID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Find the database. */
    gcmkONERROR(gckKERNEL_FindDatabase(Kernel, ProcessID, gcvFALSE, &database));

    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DATABASE,
                   "DB(%d): VidMem: total=%lu max=%lu",
                   ProcessID, database->vidMem.totalBytes,
                   database->vidMem.maxBytes);
    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DATABASE,
                   "DB(%d): NonPaged: total=%lu max=%lu",
                   ProcessID, database->nonPaged.totalBytes,
                   database->nonPaged.maxBytes);
    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DATABASE,
                   "DB(%d): Contiguous: total=%lu max=%lu",
                   ProcessID, database->contiguous.totalBytes,
                   database->contiguous.maxBytes);
    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DATABASE,
                   "DB(%d): Idle time=%llu",
                   ProcessID, Kernel->db->idleTime[Kernel->core]);
    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DATABASE,
                   "DB(%d): Map: total=%lu max=%lu",
                   ProcessID, database->mapMemory.totalBytes,
                   database->mapMemory.maxBytes);
    gcmkTRACE_ZONE(gcvLEVEL_INFO, gcvZONE_DATABASE,
                   "DB(%d): Map: total=%lu max=%lu",
                   ProcessID, database->mapUserMemory.totalBytes,
                   database->mapUserMemory.maxBytes);

    if (database->list != gcvNULL)
    {
        gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                       "Process %d has entries in its database:",
                       ProcessID);
    }

    /* Walk all records. */

    for(i = 0; i < RECORD_HASH_TABLE_SIZE; i++)
    {
        for(record = database->htable[i]; record != gcvNULL; record = next)
        {
            /* Next next record. */
            next = record->next;

            /* Dispatch on record type. */
            switch (record->type)
            {
            case gcvDB_VIDEO_MEMORY:
                /* Free the video memory. */
                status = gckVIDMEM_Free(record->data);

                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: VIDEO_MEMORY 0x%x (status=%d)",
                               record->data, status);
                break;

            case gcvDB_NON_PAGED:
                /* Unmap user logical memory first. */
                status = gckOS_UnmapUserLogical(Kernel->os,
                                                record->physical,
                                                record->bytes,
                                                record->data);

                /* Free the non paged memory. */
                status = gckOS_FreeNonPagedMemory(Kernel->os,
                                                  record->bytes,
                                                  record->physical,
                                                  record->data);
                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: NON_PAGED 0x%x, bytes=%lu (status=%d)",
                               record->data, record->bytes, status);
                break;

            case gcvDB_CONTIGUOUS:
                /* Unmap user logical memory first. */
                status = gckOS_UnmapUserLogical(Kernel->os,
                                                record->physical,
                                                record->bytes,
                                                record->data);

                /* Free the contiguous memory. */
                status = gckEVENT_FreeContiguousMemory(Kernel->eventObj,
                                                       record->bytes,
                                                       record->physical,
                                                       record->data,
                                                       gcvKERNEL_PIXEL);

                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: CONTIGUOUS 0x%x bytes=%lu (status=%d)",
                               record->data, record->bytes, status);
                break;

            case gcvDB_SIGNAL:
#if USE_NEW_LINUX_SIGNAL
                status = gcvSTATUS_NOT_SUPPORTED;
#else
                /* Free the user signal. */
                status = gckOS_DestroyUserSignal(Kernel->os,
                                                 gcmPTR2INT(record->data));
#endif /* USE_NEW_LINUX_SIGNAL */

                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: SIGNAL %d (status=%d)",
                           (gctINT)(gctUINTPTR_T)record->data, status);
                break;

            case gcvDB_VIDEO_MEMORY_LOCKED:

                /* Unlock what we still locked */
                status = gckVIDMEM_Unlock(record->kernel,
                                          record->data,
                                          ProcessID,
                                          gcvSURF_TYPE_UNKNOWN,
                                          &asynchronous,
                                          gcvTRUE);

                if (gcmIS_SUCCESS(status) && (gcvTRUE == asynchronous))
                {
                    /* TODO: we maybe need to schedule a event here */
                    status = gckVIDMEM_Unlock(record->kernel,
                                              record->data,
                                              ProcessID,
                                              gcvSURF_TYPE_UNKNOWN,
                                              gcvNULL,
                                              gcvTRUE);
                }

                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: VIDEO_MEMORY_LOCKED 0x%x (status=%d)",
                               record->data, status);
                break;

            case gcvDB_CONTEXT:
                /* TODO: Free the context */
                status = gckCOMMAND_Detach(Kernel->command, record->data);

                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: CONTEXT 0x%x (status=%d)",
                               record->data, status);
                break;

            case gcvDB_MAP_MEMORY:
                /* Unmap memory. */
                status = gckOS_UnmapMemoryEx(Kernel->os,
                                            record->physical,
                                            record->bytes,
                                            record->data,
                                            ProcessID);

                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: MAP MEMORY %d (status=%d)",
                               gcmPTR2INT(record->data), status);
                break;

            case gcvDB_MAP_USER_MEMORY:
                /* TODO: Unmap user memory. */
                status = gckOS_UnmapUserMemory(Kernel->os,
                                               Kernel->core,
                                               record->physical,
                                               record->bytes,
                                               record->data,
                                               0);

                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: MAP USER MEMORY %d (status=%d)",
                               gcmPTR2INT(record->data), status);
                break;

            case gcvDB_SHARED_INFO:
                status = gckOS_FreeMemory(Kernel->os, record->physical);
                break;

#if gcdANDROID_NATIVE_FENCE_SYNC
            case gcvDB_SYNC_POINT:
                /* Free the user signal. */
                status = gckOS_DestroySyncPoint(Kernel->os,
                                                (gctSYNC_POINT) record->data);

                gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                               "DB: SYNC POINT %d (status=%d)",
                               (gctINT)(gctUINTPTR_T)record->data, status);
                break;
#endif

            default:
                gcmkTRACE_ZONE(gcvLEVEL_ERROR, gcvZONE_DATABASE,
                               "DB: Correcupted record=0x%08x type=%d",
                               record, record->type);
                break;
            }

            /* Delete the record. */
            gcmkONERROR(gckKERNEL_DeleteRecord(Kernel,
                                               database,
                                               record->type,
                                               record->data,
                                               gcvNULL));
        }
    }

    /* Delete the database. */
    gcmkONERROR(gckKERNEL_DeleteDatabase(Kernel, database));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_QueryProcessDB
**
**  Query a process database for the current usage of a particular record type.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          Process ID used to identify the database.
**
**      gctBOOL LastProcessID
**          gcvTRUE if searching for the last known process ID.  gcvFALSE if
**          we need to search for the process ID specified by the ProcessID
**          argument.
**
**      gceDATABASE_TYPE Type
**          Type of the record to query.
**
**  OUTPUT:
**
**      gcuDATABASE_INFO * Info
**          Pointer to a variable that receives the requested information.
*/
gceSTATUS
gckKERNEL_QueryProcessDB(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    IN gctBOOL LastProcessID,
    IN gceDATABASE_TYPE Type,
    OUT gcuDATABASE_INFO * Info
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d Type=%d Info=0x%x",
                   Kernel, ProcessID, Type, Info);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Info != gcvNULL);

    /* Find the database. */
    gcmkONERROR(
        gckKERNEL_FindDatabase(Kernel, ProcessID, LastProcessID, &database));

    /* Get pointer to counters. */
    switch (Type)
    {
    case gcvDB_VIDEO_MEMORY:
        gcmkONERROR(gckOS_MemCopy(&Info->counters,
                                  &database->vidMem,
                                  gcmSIZEOF(database->vidMem)));
        break;

    case gcvDB_NON_PAGED:
        gcmkONERROR(gckOS_MemCopy(&Info->counters,
                                  &database->nonPaged,
                                  gcmSIZEOF(database->vidMem)));
        break;

    case gcvDB_CONTIGUOUS:
        gcmkONERROR(gckOS_MemCopy(&Info->counters,
                                  &database->contiguous,
                                  gcmSIZEOF(database->vidMem)));
        break;

    case gcvDB_IDLE:
        Info->time                         = Kernel->db->idleTime[Kernel->core];
        Kernel->db->idleTime[Kernel->core] = 0;
        break;

    case gcvDB_MAP_MEMORY:
        gcmkONERROR(gckOS_MemCopy(&Info->counters,
                                  &database->mapMemory,
                                  gcmSIZEOF(database->mapMemory)));
        break;

    case gcvDB_MAP_USER_MEMORY:
        gcmkONERROR(gckOS_MemCopy(&Info->counters,
                                  &database->mapUserMemory,
                                  gcmSIZEOF(database->mapUserMemory)));
        break;

    default:
        break;
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**  gckKERNEL_QueryDutyCycleDB
**
**  Query duty cycle from current kernel
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**  OUTPUT:
**
**      gcuDATABASE_INFO * Info
**          Pointer to a variable that receives the requested information.
*/
gceSTATUS
gckKERNEL_QueryDutyCycleDB(
    IN gckKERNEL Kernel,
    IN gctBOOL   Start,
    OUT gcuDATABASE_INFO * Info
    )
{
    gctUINT64 time;
    gcmkHEADER_ARG("Kernel=0x%x Info=0x%x", Kernel, Info);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Info != gcvNULL);

    gcmkVERIFY_OK(gckOS_GetProfileTick(&time));
    Info->time = Kernel->db->idleTime[Kernel->core];
    Info->dutycycle = 0xFFFFFFFF;

    if(Start)
    {
        Kernel->db->idleTime[Kernel->core] = 0;
        /* exclude the first slice when query in idle. */
        if(Kernel->db->isIdle[Kernel->core] == gcvTRUE)
        {
             Kernel->db->lastIdle[Kernel->core] = time;
        }
        Kernel->db->beginTime[Kernel->core] = time;
        Kernel->db->endTime[Kernel->core]   = 0;
    }
    else
    {
        /* count in the last slice when query in idle. */
        if(Kernel->db->isIdle[Kernel->core] == gcvTRUE)
        {
            Info->time += time - Kernel->db->lastIdle[Kernel->core];
        }
        Kernel->db->endTime[Kernel->core]   = time;
    }

    if ((Kernel->db->beginTime[Kernel->core] != 0) && (Kernel->db->endTime[Kernel->core] != 0))
    {
        if (Kernel->db->beginTime[Kernel->core] > Kernel->db->endTime[Kernel->core])
        {
            if (!Start)
                gcmkPRINT("%s(%d): Warning: please set the start and end point of cycle!", __func__, __LINE__);
        }
        else
        {
            gctUINT32 percentage;
            gctUINT64 timeDiff;

            timeDiff        = Kernel->db->endTime[Kernel->core] - Kernel->db->beginTime[Kernel->core];
            /* statistics is wrong somewhere, just return to ignore this time. */
            if(Info->time > timeDiff)
            {
                gcmkPRINT("%s(%d): [%s] idle:%d, diff:%d", __func__, __LINE__, Kernel->core ? "2D" : "3D",
                            gckOS_ProfileToMS(Info->time), gckOS_ProfileToMS(timeDiff));

                return gcvSTATUS_OK;
            }
            percentage      = 100 - 100 * gckOS_ProfileToMS(Info->time) / gckOS_ProfileToMS(timeDiff);
            Info->dutycycle = percentage;
        }
    }

#if 0
    if (Info->dutycycle != 0xFFFFFFFF)
        gcmkPRINT("%s(%d): [%s] %d->%d %d, %u%%",
                __func__, __LINE__,
                Kernel->core ? "2D" : "3D",
                gckOS_ProfileToMS(Kernel->db->beginTime[Kernel->core]),
                gckOS_ProfileToMS(Kernel->db->endTime[Kernel->core]),
                gckOS_ProfileToMS(Info->time),
                Info->dutycycle
                );
#endif
    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**  gckKERNEL_AddProfNode
**
**  Add a profiling node to node-array.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctBOOL Idle
**          Current gpu status
**
**      gctUINT32 Time
**          Current time
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_AddProfNode(
    IN gckKERNEL Kernel,
    IN gctBOOL   Idle,
    IN gctUINT32 Time
    )
{
    gceCORE         core;
    gctUINT32       index;
    gckProfNode_PTR profNode = gcvNULL;

    gcmkHEADER_ARG("Kernel=0x%x Idle=%d Time=%u", Kernel, Idle, Time);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Advanced the node index. */
    core                             = Kernel->core;
    Kernel->db->lastNodeIndex[core]  = (Kernel->db->lastNodeIndex[core] + 1) % gcdPROFILE_NODES_NUM;
    index                            = Kernel->db->lastNodeIndex[core];
    profNode                         = (core == gcvCORE_MAJOR) ? (&Kernel->db->profNode[0]) : (&Kernel->db->profNode2D[0]);

    /* Store profile data to current node. */
    profNode[index].idle            = Idle;
    profNode[index].tick            = Time;
    profNode[index].core            = core;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

/*******************************************************************************
**  gckKERNEL_QueryLastProfNode
**
**  Return the last index and head address of profiling node array
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**  OUTPUT:
**
**      gctUINT32_PTR Index
**          the last index of profNode
**
**      gckProfNode_PTR * ProfNode
**          the head address of profiling nodes array
**
*/
gceSTATUS
gckKERNEL_QueryLastProfNode(
    IN gckKERNEL Kernel,
    OUT gctUINT32_PTR Index,
    OUT gckProfNode_PTR * ProfNode
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL acquired;
    gceCORE core;

    gcmkHEADER_ARG("Kernel=0x%x", Kernel);
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Verify arguments */
    gcmkVERIFY_ARGUMENT(Index != gcvNULL);
    gcmkVERIFY_ARGUMENT(ProfNode != gcvNULL);

    if(Index == gcvNULL || ProfNode == gcvNULL)
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
        goto OnError;
    }

    gcmkONERROR(gckOS_AcquireMutex(Kernel->os, Kernel->db->profMutex, gcvINFINITE));
    acquired = gcvTRUE;

    core = Kernel->core;
    *Index = Kernel->db->lastNodeIndex[core];

    /*
        return corresponding head address of prof nodes,
        if not exsits, just return NULL
    */
    if(core == gcvCORE_MAJOR)
        *ProfNode = &Kernel->db->profNode[0];
    else if(core == gcvCORE_2D)
        *ProfNode = &Kernel->db->profNode2D[0];
    else
        *ProfNode = gcvNULL;

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->profMutex));
    acquired = gcvFALSE;

OnError:
    gcmkFOOTER_NO();

    if(acquired)
    {
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->profMutex));
    }

    return status;
}
/*******************************************************************************
**  gckKERNEL_QueryIdleProfile
**
**  Add a profiling node to node-array.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 Time
**          Value of current time
**
**  OUTPUT:
**
**      gctUINT32_PTR Timeslice
**          Pointer to the time slice
**
**      gctUINT32_PTR IdleTime
**          Pointer to total idle time within Timeslice
*/

#define gcdDEBUG_IDLE_PROFILE       0

gceSTATUS
gckKERNEL_QueryIdleProfile(
    IN     gckKERNEL     Kernel,
    IN     gctUINT32     Time,
    IN OUT gctUINT32_PTR Timeslice,
    OUT    gctUINT32_PTR IdleTime
    )
{
    gctUINT32       i;
    gceCORE         core;
    gctUINT32       idleTime = 0;
    gctUINT32       currentTick;
    gckProfNode_PTR profNode = gcvNULL;
    gctUINT32       firstIndex, lastIndex, iterIndex, endIndex;
    gckProfNode_PTR firstNode, lastNode, iterNode, nextIterNode, endNode;
#if gcdDEBUG_IDLE_PROFILE
    gctUINT32       prelastIndex;
    gckProfNode_PTR prelastNode;
#endif

    gcmkHEADER_ARG("Kernel=0x%x Timeslice=0x%x IdleTime=0x%x", Kernel, Timeslice, IdleTime);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Timeslice != gcvNULL);
    gcmkVERIFY_ARGUMENT(IdleTime != gcvNULL);

    currentTick = Time;
    core        = Kernel->core;
    profNode    = (core == gcvCORE_MAJOR) ? (&Kernel->db->profNode[0]) : (&Kernel->db->profNode2D[0]);
    if (profNode == gcvNULL)
    {
        gcmkPRINT("error: fail to get the profNode pointer!!");
        return gcvSTATUS_INVALID_ADDRESS;
    }

    lastIndex   = Kernel->db->lastNodeIndex[core];
    firstIndex  = (lastIndex + 1) % gcdPROFILE_NODES_NUM;
    lastNode    = &profNode[lastIndex];
    firstNode   = &profNode[firstIndex];

#if gcdDEBUG_IDLE_PROFILE
    prelastIndex = (lastIndex + gcdPROFILE_NODES_NUM -1) % gcdPROFILE_NODES_NUM;
    prelastNode  = &profNode[prelastIndex];
    gcmkPRINT("%s(%d): core: %d, [%#x:%d:%d:%d, %#x:%d:%d:%d, %#x:%d:%d:%d], %d, switchCount:%llu\n",
                __FUNCTION__, __LINE__,
                core,
                prelastNode, prelastNode->tick, prelastNode->idle, prelastIndex,
                lastNode, lastNode->tick, lastNode->idle, lastIndex,
                firstNode, firstNode->tick, firstNode->idle, firstIndex,
                currentTick, Kernel->db->switchCount[core]);
#endif

    /* Short path: there is no records in Node list. */
    if (lastNode->tick == 0)
    {
#if gcdDEBUG_IDLE_PROFILE
        gcmkPRINT("%s(%d): no records in profNode, return!!\n", __FUNCTION__, __LINE__);
#endif
        /* Fill with the start status: 3D->busy, 2D->idle. */
        *IdleTime = Kernel->db->isIdle[core] ? *Timeslice : 0;
        return gcvSTATUS_OK;
    }

    /*
     *                   cur'   cur
     *  first       last  |      |
     *  |____________|____|______|
     *
     * Case A:
     *     no node within time window, that means status hasnt been
     *     changed since lastNode
     */
    if(currentTick >= (lastNode->tick + *Timeslice))
    {
        if(lastNode->idle)
        {
            idleTime += *Timeslice;
        }
    }
    /*
     *  cur'  cur
     *  |      |    first       last
     *  |______|____|____________|
     *
     * Case B:
     *     time window is out of range
     */
    else if(currentTick < firstNode->tick)
    {
        idleTime = 0;
        *Timeslice = 0;
    }
    else
    {
        /*
         *   cur'
         *    |   first      last
         *  __|___|___________|
         *
         * Case C:
         *     lower bound of time window cur' is out of range,
         *     thus just cut it to the FIRST profNode
         */
        if((firstNode->tick != 0) && (currentTick <= (firstNode->tick + *Timeslice)))
        {
            iterIndex = firstIndex;
            iterNode  = &profNode[iterIndex];
        }
        /*
         *       cur'
         *  first  | i  last
         *  |______|_|___|
         *
         * Case D:
         *     find first node within the time window
         *             cur' <= iterNode
         *
         */
        else
        {
            for(i = 0; i < gcdPROFILE_NODES_NUM; i++)
            {
                iterIndex = (firstIndex + i) % gcdPROFILE_NODES_NUM;
                iterNode  = &profNode[iterIndex];
                if(currentTick <= (iterNode->tick + *Timeslice))
                {
                    break;
                }
            }
        }
        gcmkASSERT(currentTick <= (iterNode->tick + *Timeslice));

        /*
         *           cur
         *  first  i |   last
         *  |______|_|____|
         *
         * Case E:
         *     find last node within the time window
         *             cur >= endNode
         *
         */
        if(currentTick < lastNode->tick)
        {
            for(i = 0; i < gcdPROFILE_NODES_NUM; i++)
            {
                endIndex = (lastIndex + gcdPROFILE_NODES_NUM - i) % gcdPROFILE_NODES_NUM;
                endNode  = &profNode[endIndex];
                if(currentTick >= endNode->tick)
                {
                    break;
                }
            }
        }
        /*
         *                cur
         *  first   last   |
         *  |________|_____|
         *
         * Case F:
         *     normal case
         */
        else
        {
            endIndex = lastIndex;
            endNode  = &profNode[endIndex];
        }
        gcmkASSERT(currentTick >= endNode->tick);

#if gcdDEBUG_IDLE_PROFILE
        gcmkPRINT("%s(%d): %d:%d:%d -> %d:%d:%d, timeslice:%d\n", __FUNCTION__, __LINE__, iterIndex, iterNode->idle, iterNode->tick, endIndex, endNode->idle, endNode->tick, *Timeslice);
#endif
        /*
         *     cur'  cur
         * end  |     |    iter
         *  |_ _|_____|_____|
         *
         * Corner case:
         *
         * FIXED: time window within two contiguous node,
         *        but index is reversed for start and end node
         *        which is correct by using above logic that makes:
         *            cur >= endNode && cur' <= iterNode
         */
        if(iterNode->tick > endNode->tick &&
            (currentTick >= (*Timeslice + endNode->tick)) &&
            (currentTick <= iterNode->tick))
        {
            if(endNode->idle)
            {
                idleTime += *Timeslice;
            }
            else
            {
                idleTime = 0;
            }
            goto bailout;
        }

        /* startTick time to the first node. */
        if(!iterNode->idle)
        {
            idleTime += iterNode->tick + *Timeslice - currentTick;
#if gcdDEBUG_IDLE_PROFILE
            gcmkPRINT("%s(%d): startTick-> idletime:%d\n", __FUNCTION__, __LINE__, idleTime);
#endif
        }

        /* last node to currentTick time */
        if(endNode->idle)
        {
            idleTime += currentTick - endNode->tick;
#if gcdDEBUG_IDLE_PROFILE
            gcmkPRINT("%s(%d): lastTick-> idletime:%d\n", __FUNCTION__, __LINE__, idleTime);
#endif
        }

        /* sum up the total time from first node to last node. */
        for(i = 0; i < gcdPROFILE_NODES_NUM; i++ )
        {
            if(iterIndex == endIndex)
                break;

            if(iterNode->idle)
            {
                nextIterNode = &profNode[(iterIndex + 1) % gcdPROFILE_NODES_NUM];
                gcmkASSERT(nextIterNode->tick != 0 && nextIterNode->idle == gcvFALSE);
                idleTime    += nextIterNode->tick - iterNode->tick;
#if gcdDEBUG_IDLE_PROFILE
                gcmkPRINT("%s(%d): midTick-> idletime:%d\n", __FUNCTION__, __LINE__, idleTime);
#endif
            }

            iterIndex = (iterIndex + 1) % gcdPROFILE_NODES_NUM;
            iterNode  = &profNode[iterIndex];
        }

    }

bailout:
    *IdleTime = idleTime;
#if gcdDEBUG_IDLE_PROFILE
    gcmkPRINT("%s(%d): totalTick-> idletime:%d, Timeslice:%d\n", __FUNCTION__, __LINE__, idleTime, *Timeslice);
#endif

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

#if gcdSECURE_USER
/*******************************************************************************
**  gckKERNEL_GetProcessDBCache
**
**  Get teh secure cache from a process database.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to a gckKERNEL object.
**
**      gctUINT32 ProcessID
**          Process ID used to identify the database.
**
**  OUTPUT:
**
**      gcskSECURE_CACHE_PTR * Cache
**          Pointer to a variable that receives the secure cache pointer.
*/
gceSTATUS
gckKERNEL_GetProcessDBCache(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    OUT gcskSECURE_CACHE_PTR * Cache
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d", Kernel, ProcessID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Cache != gcvNULL);

    /* Find the database. */
    gcmkONERROR(gckKERNEL_FindDatabase(Kernel, ProcessID, gcvFALSE, &database));

    /* Return the pointer to the cache. */
    *Cache = &database->cache;

    /* Success. */
    gcmkFOOTER_ARG("*Cache=0x%x", *Cache);
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}
#endif

#if MRVL_PULSE_EATER_COUNT_NUM
gceSTATUS
gckKERNEL_QueryPulseEaterIdleProfile(
    IN gckKERNEL      Kernel,
    IN gctBOOL        Start,
    IN gcePulseEaterDomain Domain,
    OUT gctUINT32_PTR BusyRatio,
    IN OUT gctUINT32_PTR TimeGap,
    OUT gctUINT32_PTR TotalTime)
{
    gckHARDWARE hardware;
    gckPulseEaterDB dbPulse;
    gctUINT64 tick;
    gceSTATUS status = gcvSTATUS_OK;

    gcmkHEADER_ARG("Kernel=0x%x Start=%d", Kernel, Start);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(BusyRatio != gcvNULL);
    gcmkVERIFY_ARGUMENT(TimeGap != gcvNULL);
    gcmkVERIFY_ARGUMENT(TotalTime != gcvNULL);

    hardware = Kernel->hardware;

    gcmkVERIFY_OK(gckOS_AcquireMutex(hardware->os, hardware->pulseEaterMutex, gcvINFINITE));

    dbPulse = hardware->pulseEaterDB[Domain];
    gcmkVERIFY_OK(gckOS_GetProfileTick(&tick));

    /* this path is for gpufreq polling*/
    if(*TimeGap == DEF_MIN_SAMPLING_RATE)
    {
        gctINT index, pole = -1, j = 0, k = 0;
        gctUINT32 curTime, busyRatio = 0, totalTime = 0;
        gctINT32 diff;

        curTime = gckOS_ProfileToMS(tick);
        index   = dbPulse->lastIndex?(dbPulse->lastIndex -1):(gcmCOUNTOF(dbPulse->data)-1);
        diff    = (gctUINT32)(curTime - PulseEaterDB(index).time);

        do
        {
            gctINT i = 0, tmp = index;

            /*find the record in Sampling Window*/
            if(diff > DEF_MIN_SAMPLING_RATE || j == gcmCOUNTOF(dbPulse->data))
            {
                 break;
            }

            j++;
            if(pole > 0)
            {
                if(((PulseEaterDB(pole).time)*10 - (PulseEaterDB(index).time)*10) < PulseEaterDB(index).hwMs)
                {
                    /*polling interval is less than one hardware period
                                   which causes the data is useless,
                                   update index and continue*/
                    index = (tmp > 0)? (--tmp):(gcmCOUNTOF(dbPulse->data)-1);
                    continue;
                }
            }

            while(i < PulseIter)
            {
                busyRatio += PulseEaterDB(index).pulseCount[i];
                i++;
            }
            totalTime += PulseEaterDB(index).sfPeriod;
            k++;

            /*index couldn't be negative*/
            pole = index;
            index = (tmp > 0)? (--tmp):(gcmCOUNTOF(dbPulse->data)-1);
            diff    = (gctUINT32)(curTime - dbPulse->data[index].time);
        }while(gcvTRUE);

        *TotalTime = totalTime;
        *BusyRatio = (k==0)?0:(((busyRatio * 100)/(64*4*k))* totalTime)/100;
    }
    /*set start time*/
    else if(Start)
    {
        gctUINT32 startTime;

        /*Avoid re-type "echo 1 0 > .../dutycycle"*/
        if(dbPulse->startTime == 0)
        {
            startTime = gckOS_ProfileToMS(tick);

            dbPulse->startTime    = startTime;
            //gckOS_StartTimer(hardware->os, hardware->pulseEaterTimer, hardware->sfPeriod);
        }
        else
        {
            status = gcvSTATUS_INVALID_REQUEST;
        }
    }
    /*set end time and calculate idle Time and Time Gap*/
    else if(dbPulse->startTime != 0)
    {
        gctUINT32 timeGap, diff, endTime, i = 0;
        gctUINT32_PTR busyRatio = gcvNULL, totalTime = gcvNULL;

        endTime= gckOS_ProfileToMS(tick);

        busyRatio = BusyRatio;
        totalTime = TotalTime;

        timeGap = endTime - dbPulse->startTime;

        /* recordsNum > MRVL_PULSE_EATER_COUNT_NUM*/
        if(dbPulse->moreRound)
        {
            diff    = endTime - PulseEaterDB(0).time + PulseEaterDB(0).sfPeriod;
        }
        /* recordsNum < MRVL_PULSE_EATER_COUNT_NUM*/
        else
        {
            diff    = timeGap;
        }

        gckHARDWARE_QueryPulseEaterIdleProfile(hardware, endTime, &diff, Domain);

        while(i < PulseIter)
        {
            busyRatio[i]         = dbPulse->busyRatio[i];
            totalTime[i]         = dbPulse->totalTime[i];
            dbPulse->busyRatio[i] = 0;
            dbPulse->totalTime[i]= 0;
            i++;
        }

        dbPulse->moreRound = gcvFALSE;
        dbPulse->startTime = 0;
        dbPulse->startIndex = ~0;

        *TimeGap  = timeGap;
    }
    else
    {
        status = gcvSTATUS_INVALID_ARGUMENT;
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(hardware->os, hardware->pulseEaterMutex));

    /* Success. */
    gcmkFOOTER_ARG("*IdleTime=%d, *TimeGap=%d", *BusyRatio, *TimeGap);
    return status;
}
#endif

gceSTATUS
gckKERNEL_DumpProcessDB(
    IN gckKERNEL Kernel
    )
{
    gcsDATABASE_PTR database;
    gctINT i, pid;
    gctUINT8 name[24];

    gcmkHEADER_ARG("Kernel=0x%x", Kernel);

    /* Acquire the database mutex. */
    gcmkVERIFY_OK(
        gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));

    gcmkPRINT("**************************\n");
    gcmkPRINT("***  PROCESS DB DUMP   ***\n");
    gcmkPRINT("**************************\n");

    gcmkPRINT_N(8, "%-8s%s\n", "PID", "NAME");
    /* Walk the databases. */
    for (i = 0; i < gcmCOUNTOF(Kernel->db->db); ++i)
    {
        for (database = Kernel->db->db[i];
             database != gcvNULL;
             database = database->next)
        {
            pid = database->processID;

            gcmkVERIFY_OK(gckOS_ZeroMemory(name, gcmSIZEOF(name)));

            gcmkVERIFY_OK(gckOS_GetProcessNameByPid(pid, gcmSIZEOF(name), name));

            gcmkPRINT_N(8, "%-8d%s\n", pid, name);
        }
    }

    /* Release the database mutex. */
    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}
