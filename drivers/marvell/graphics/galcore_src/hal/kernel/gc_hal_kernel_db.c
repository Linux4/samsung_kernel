/****************************************************************************
*
*    Copyright (c) 2005 - 2015 by Vivante Corp.
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


#include "gc_hal_kernel_precomp.h"
#include "gc_hal_kernel_linux.h"

#ifdef LINUX
#include <linux/kernel.h>
#else
#include <stdio.h>
#endif

#define _GC_OBJ_ZONE    gcvZONE_DATABASE

/*******************************************************************************
***** Private fuctions ********************************************************/

#define _GetSlot(database, x) \
    (gctUINT32)(gcmPTR_TO_UINT64(x) % gcmCOUNTOF(database->list))

#define PulseEaterDB(index) dbPulse->data[index]
#define PulseIter gcmCOUNTOF(dbPulse->data[0].pulseCount)

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
    gcsDATABASE_PTR database;
    gctBOOL acquired = gcvFALSE;
    gctSIZE_T slot;
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

        /* Allocate a new database from the heap. */
        gcmkONERROR(gckOS_Allocate(Kernel->os,
                                   gcmSIZEOF(gcsDATABASE),
                                   &pointer));

        gckOS_ZeroMemory(pointer, gcmSIZEOF(gcsDATABASE));

        database = pointer;

        gcmkONERROR(gckOS_CreateMutex(Kernel->os, &database->counterMutex));
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

    /* Destory handle db. */
    gcmkVERIFY_OK(gckKERNEL_DestroyIntegerDatabase(Kernel, Database->handleDatabase));
    Database->handleDatabase = gcvNULL;
    gcmkVERIFY_OK(gckOS_DeleteMutex(Kernel->os, Database->handleDatabaseMutex));
    Database->handleDatabaseMutex = gcvNULL;

#if gcdPROCESS_ADDRESS_SPACE
    /* Destory process MMU. */
    gcmkVERIFY_OK(gckEVENT_DestroyMmu(Kernel->eventObj, Database->mmu, gcvKERNEL_PIXEL));
    Database->mmu = gcvNULL;
#endif

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
    IN gctUINT32 Slot,
    OUT gcsDATABASE_RECORD_PTR * Record
    )
{
    gceSTATUS status;
    gctBOOL acquired = gcvFALSE;
    gcsDATABASE_RECORD_PTR record = gcvNULL;

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

    /* Insert the record in the database. */
    record->next         = Database->list[Slot];
    Database->list[Slot] = record;

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
        if (Database->list[Slot] == record)
        {
            Database->list[Slot] = record->next;
        }
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
    gctUINT32 slot = _GetSlot(Database, Data);

    gcmkHEADER_ARG("Kernel=0x%x Database=0x%x Type=%d Data=0x%x",
                   Kernel, Database, Type, Data);

    /* Acquire the database mutex. */
    gcmkONERROR(
        gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));
    acquired = gcvTRUE;

    /* Scan the database for this record. */
    for (record = Database->list[slot], previous = gcvNULL;
         record != gcvNULL;
         record = record->next
    )
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
        Database->list[slot] = record->next;
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
    gctUINT32 slot = _GetSlot(Database, Data);

    gcmkHEADER_ARG("Kernel=0x%x Database=0x%x Type=%d Data=0x%x",
                   Kernel, Database, Type, Data);

    /* Acquire the database mutex. */
    gcmkONERROR(
        gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));
    acquired = gcvTRUE;

    /* Scan the database for this record. */
    for (record = Database->list[slot];
         record != gcvNULL;
         record = record->next
    )
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
    gctUINT32 i;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d", Kernel, ProcessID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Create a new database. */
    gcmkONERROR(gckKERNEL_NewDatabase(Kernel, ProcessID, &database));

    /* Initialize the database. */
    database->processID             = ProcessID;
    database->vidMem.bytes          = 0;
    database->vidMem.maxBytes       = 0;
    database->vidMem.totalBytes     = 0;
    database->nonPaged.bytes        = 0;
    database->nonPaged.maxBytes     = 0;
    database->nonPaged.totalBytes   = 0;
    database->contiguous.bytes      = 0;
    database->contiguous.maxBytes   = 0;
    database->contiguous.totalBytes = 0;
    database->mapMemory.bytes          = 0;
    database->mapMemory.maxBytes       = 0;
    database->mapMemory.totalBytes     = 0;
    database->mapUserMemory.bytes      = 0;
    database->mapUserMemory.maxBytes   = 0;
    database->mapUserMemory.totalBytes = 0;
    database->virtualCommandBuffer.bytes = 0;
    database->virtualCommandBuffer.maxBytes = 0;
    database->virtualCommandBuffer.totalBytes = 0;
    database->virtualContextBuffer.bytes = 0;
    database->virtualContextBuffer.maxBytes = 0;
    database->virtualContextBuffer.totalBytes = 0;

    for (i = 0; i < gcmCOUNTOF(database->list); i++)
    {
        database->list[i]              = gcvNULL;
    }

    for (i = 0; i < gcvSURF_NUM_TYPES; i++)
    {
        database->vidMemType[i].bytes = 0;
        database->vidMemType[i].maxBytes = 0;
        database->vidMemType[i].totalBytes = 0;
    }

    for (i = 0; i < gcvPOOL_NUMBER_OF_POOLS; i++)
    {
        database->vidMemPool[i].bytes = 0;
        database->vidMemPool[i].maxBytes = 0;
        database->vidMemPool[i].totalBytes = 0;
    }

    gcmkASSERT(database->handleDatabase == gcvNULL);
    gcmkONERROR(
        gckKERNEL_CreateIntegerDatabase(Kernel, &database->handleDatabase));

    gcmkASSERT(database->handleDatabaseMutex == gcvNULL);
    gcmkONERROR(
        gckOS_CreateMutex(Kernel->os, &database->handleDatabaseMutex));

#if gcdPROCESS_ADDRESS_SPACE
    gcmkASSERT(database->mmu == gcvNULL);
    gcmkONERROR(
        gckMMU_Construct(Kernel, gcdMMU_SIZE, &database->mmu));
#endif

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
    gctUINT32 vidMemType;
    gcePOOL vidMemPool;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d Type=%d Pointer=0x%x "
                   "Physical=0x%x Size=%lu",
                   Kernel, ProcessID, Type, Pointer, Physical, Size);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Decode type. */
    vidMemType = (Type & gcdDB_VIDEO_MEMORY_TYPE_MASK) >> gcdDB_VIDEO_MEMORY_TYPE_SHIFT;
    vidMemPool = (Type & gcdDB_VIDEO_MEMORY_POOL_MASK) >> gcdDB_VIDEO_MEMORY_POOL_SHIFT;

    Type &= gcdDATABASE_TYPE_MASK;

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
    gcmkONERROR(gckKERNEL_NewRecord(Kernel, database, _GetSlot(database, Pointer), &record));

    /* Initialize the record. */
    record->kernel   = Kernel;
    record->type     = Type;
    record->data     = Pointer;
    record->physical = Physical;
    record->bytes    = Size;

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

    case gcvDB_COMMAND_BUFFER:
        count = &database->virtualCommandBuffer;
        break;

    case gcvDB_CONTEXT:
        count = &database->virtualContextBuffer;
        break;

    default:
        count = gcvNULL;
        break;
    }

    gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, database->counterMutex, gcvINFINITE));

    if (count != gcvNULL)
    {
        /* Adjust counters. */
        count->totalBytes += Size;
        count->bytes      += Size;

        if (count->bytes > count->maxBytes)
        {
            count->maxBytes = count->bytes;
        }
    }

    if (Type == gcvDB_VIDEO_MEMORY)
    {
        count = &database->vidMemType[vidMemType];

        /* Adjust counters. */
        count->totalBytes += Size;
        count->bytes      += Size;

        if (count->bytes > count->maxBytes)
        {
            count->maxBytes = count->bytes;
        }

        count = &database->vidMemPool[vidMemPool];

        /* Adjust counters. */
        count->totalBytes += Size;
        count->bytes      += Size;

        if (count->bytes > count->maxBytes)
        {
            count->maxBytes = count->bytes;
        }
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, database->counterMutex));

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
    gctUINT32 vidMemType;
    gcePOOL vidMempool;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d Type=%d Pointer=0x%x",
                   Kernel, ProcessID, Type, Pointer);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Pointer != gcvNULL);

    /* Decode type. */
    vidMemType = (Type & gcdDB_VIDEO_MEMORY_TYPE_MASK) >> gcdDB_VIDEO_MEMORY_TYPE_SHIFT;
    vidMempool = (Type & gcdDB_VIDEO_MEMORY_POOL_MASK) >> gcdDB_VIDEO_MEMORY_POOL_SHIFT;

    Type &= gcdDATABASE_TYPE_MASK;

    /* Find the database. */
    gcmkONERROR(gckKERNEL_FindDatabase(Kernel, ProcessID, gcvFALSE, &database));

    /* Delete the record. */
    gcmkONERROR(
        gckKERNEL_DeleteRecord(Kernel, database, Type, Pointer, &bytes));

    gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, database->counterMutex, gcvINFINITE));

    /* Update counters. */
    switch (Type)
    {
    case gcvDB_VIDEO_MEMORY:
        database->vidMem.bytes -= bytes;
        database->vidMemType[vidMemType].bytes -= bytes;
        database->vidMemPool[vidMempool].bytes -= bytes;
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

    case gcvDB_COMMAND_BUFFER:
        database->virtualCommandBuffer.bytes -= bytes;
        break;

    case gcvDB_CONTEXT:
        database->virtualContextBuffer.bytes -= bytes;
        break;

    default:
        break;
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, database->counterMutex));

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

gctUINT32 _print_VIDMEM_by_pid(
    gckKERNEL Kernel,
    gcsDATABASE_RECORD_PTR * List,
    gctUINT Count,
    gctUINT ProcessID,
    gctPOINTER handleDatabase,
    gctPOINTER handleDatabaseMutex,
    gctPOINTER Buffer
)
{
    gcsDATABASE_RECORD_PTR record;
    gcuVIDMEM_NODE_PTR node;
    gckVIDMEM_HANDLE handleObject;
    gctINT32 size[gcvSURF_NUM_TYPES] = {0};
    gctINT32 sum  = 0;
    gctINT i;
    gctSTRING buf = (gctSTRING)Buffer;
    gctUINT32 len = 0;

    if (buf == gcvNULL)
    {
        gcmkPRINT("VIDMEM detail for PID %d \n", ProcessID);
    }
    else
    {
        len += sprintf(buf, "VIDMEM detail for PID %d \n", ProcessID);
    }

    for(i = 0; i < Count; i++)
    {
        record = List[i];
        while(record)
        {
            if(record->type == gcvDB_VIDEO_MEMORY)
            {
                gceSTATUS status = gcvSTATUS_OK;

                gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, handleDatabaseMutex, gcvINFINITE));

                status = (
                    gckKERNEL_QueryIntegerId(handleDatabase,
                                             gcmPTR2INT(record->data),
                                             (gctPOINTER *)&handleObject));

                if(gcmIS_ERROR(status))
                {
                    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, handleDatabaseMutex));
                    record = record->next;
                    continue;
                }

                node = handleObject->node->node;

                if((node->VidMem.memory != gcvNULL) &&
                   (node->VidMem.memory->object.type == gcvOBJ_VIDMEM))
                {
                    if(ProcessID == node->VidMem.processID)
                    {
                        size[node->VidMem.type] += node->VidMem.bytes;
                    }
                }
                else
                {
                    if(ProcessID == node->Virtual.processID)
                    {
                        size[node->Virtual.type] += node->Virtual.bytes;
                    }
                }

                gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, handleDatabaseMutex));
            }
            record = record->next;
        }
    }

    for(i = 0 ; i < gcvSURF_NUM_TYPES; i++)
    {
        if(size[i] >= 1024)
        {
            if (buf == gcvNULL)
            {
                gcmkPRINT("-- type %2d: %-16s, size %d KB \n", i, _gc_VIDMEM_type_name[i], size[i]/1024);
            }
            else
            {
                len += sprintf(buf + len, "-- type %2d: %-16s, size %d KB \n",
                               i, _gc_VIDMEM_type_name[i], size[i]/1024);
            }
        }
        else if(size[i] > 0)
        {
            if (buf == gcvNULL)
            {
                gcmkPRINT("-- type %2d: %-16s, size %d B \n", i, _gc_VIDMEM_type_name[i], size[i]);
            }
            else
            {
                len += sprintf(buf + len, "-- type %2d: %-16s, size %d B \n",
                               i, _gc_VIDMEM_type_name[i], size[i]);
            }
        }
        sum += size[i];
    }

    if (buf == gcvNULL)
    {
        gcmkPRINT("-- sum    :                   size %d KB \n", sum/1024);
    }
    else
    {
        len += sprintf(buf + len, "-- sum    :                   size %d KB \n",
                       sum/1024);
    }

    return len;
}

gctUINT32 _print_VIDMEM_by_type(
    gckKERNEL Kernel,
    gcsDATABASE_PTR * DataBase,
    gctSIZE_T Count,
    gctINT Type,
    gctPOINTER Buffer
)
{
    gcuVIDMEM_NODE_PTR node;
    gctINT32 size = 0;
    gctINT32 sum  = 0;
    gctINT i;
    gcsDATABASE_PTR database;
    gcsDATABASE_RECORD_PTR record;
    gctPOINTER handleDatabase;
    gctPOINTER handleDatabaseMutex;
    gckVIDMEM_HANDLE handleObject;
    gctSTRING buf = (gctSTRING)Buffer;
    gctUINT32 len = 0;

    if (buf == gcvNULL)
    {
        gcmkPRINT("VIDMEM detail for Type %d, %s \n", Type, _gc_VIDMEM_type_name[Type]);
    }
    else
    {
        len += sprintf(buf, "VIDMEM detail for Type %d, %s \n", Type, _gc_VIDMEM_type_name[Type]);
    }

    gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));

    for(i = 0; i < Count; i++)
    {
        database = DataBase[i];
        while(database)
        {
            gctINT j;

            size = 0;
            handleDatabase = database->handleDatabase;
            handleDatabaseMutex = database->handleDatabaseMutex;

            for(j = 0; j < gcmCOUNTOF((database->list)); j++)
            {
                record = database->list[j];
                while(record)
                {
                    if(record->type == gcvDB_VIDEO_MEMORY)
                    {
                        gceSTATUS status = gcvSTATUS_OK;

                        gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, handleDatabaseMutex, gcvINFINITE));

                        status = (
                            gckKERNEL_QueryIntegerId(handleDatabase,
                                                     gcmPTR2INT(record->data),
                                                     (gctPOINTER *)&handleObject));

                        if(gcmIS_ERROR(status))
                        {
                            gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, handleDatabaseMutex));
                            record = record->next;
                            continue;
                        }

                        node = handleObject->node->node;

                        if((node->VidMem.memory != gcvNULL) &&
                           (node->VidMem.memory->object.type == gcvOBJ_VIDMEM))
                        {
                            if((database->processID == node->VidMem.processID) &&
                               (node->VidMem.type == Type))
                            {
                                size += node->VidMem.bytes;
                            }
                        }
                        else
                        {
                            if((database->processID == node->Virtual.processID) &&
                               (node->Virtual.type == Type))
                            {
                                size += node->Virtual.bytes;
                            }
                        }

                        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, handleDatabaseMutex));
                    }
                    record = record->next;
                }
            }
            if(size >= 1024)
            {
                if (buf == gcvNULL)
                {
                    gcmkPRINT("  -- pid %-5d: size: %d KB\n", database->processID, size/1024);
                }
                else
                {
                    len += sprintf(buf + len, "  -- pid %-5d: size: %d KB\n", database->processID, size/1024);
                }
            }
            else if(size > 0)
            {
                if (buf == gcvNULL)
                {
                    gcmkPRINT("  -- pid %-5d: size: %d B\n", database->processID, size);
                }
                else
                {
                    len += sprintf(buf + len, "  -- pid %-5d: size: %d B\n", database->processID, size);
                }
            }
            database = database->next;
            sum += size;
        }
    }
    if(sum > 0)
    {
        if (buf == gcvNULL)
        {
            gcmkPRINT("  -- sum      : size: %d KB \n", sum/1024);
        }
        else
        {
            len += sprintf(buf + len, "  -- sum      : size: %d KB \n", sum/1024);
        }
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    return len;
}

gctUINT32 _print_VIDMEM_by_type_sum(
    gckKERNEL Kernel,
    gcsDATABASE_PTR * DataBase,
    gctSIZE_T Count,
    gctPOINTER Buffer
)
{
    gcuVIDMEM_NODE_PTR node;
    gctINT32 size[gcvSURF_NUM_TYPES] = {0};
    gctINT32 sum = 0;
    gctINT i;
    gcsDATABASE_PTR database;
    gcsDATABASE_RECORD_PTR record;
    gctPOINTER handleDatabase;
    gctPOINTER handleDatabaseMutex;
    gckVIDMEM_HANDLE handleObject;
    gctSTRING buf = (gctSTRING)Buffer;
    gctUINT32 len = 0;

    if (buf == gcvNULL)
    {
        gcmkPRINT("VIDMEM detail for all types:");
    }
    else
    {
        len += sprintf(buf, "VIDMEM detail for all types:\n");
    }

    gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));

    for(i = 0; i < Count; i++)
    {
        database = DataBase[i];
        while(database)
        {
            gctINT j;
            handleDatabase = database->handleDatabase;
            handleDatabaseMutex = database->handleDatabaseMutex;

            for(j = 0; j < gcmCOUNTOF((database->list)); j++)
            {
                record = database->list[j];
                while(record)
                {
                    if(record->type == gcvDB_VIDEO_MEMORY)
                    {
                        gceSTATUS status = gcvSTATUS_OK;

                        gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, handleDatabaseMutex, gcvINFINITE));

                        status = (
                            gckKERNEL_QueryIntegerId(handleDatabase,
                                                     gcmPTR2INT(record->data),
                                                     (gctPOINTER *)&handleObject));

                        if(gcmIS_ERROR(status))
                        {
                            gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, handleDatabaseMutex));
                            record = record->next;
                            continue;
                        }

                        node = handleObject->node->node;

                        if((node->VidMem.memory != gcvNULL) &&
                           (node->VidMem.memory->object.type == gcvOBJ_VIDMEM))
                        {
                            if (database->processID == node->VidMem.processID)
                            {
                                size[node->VidMem.type] += node->VidMem.bytes;
                            }
                        }
                        else
                        {
                            if (database->processID == node->Virtual.processID)
                            {
                                size[node->Virtual.type] += node->Virtual.bytes;
                            }
                        }

                        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, handleDatabaseMutex));
                    }
                    record = record->next;
                }
            }
            database = database->next;
        }
    }

    for(i = 0 ; i < gcvSURF_NUM_TYPES; i++)
    {
        if(size[i] >= 1024)
        {
            if (buf == gcvNULL)
            {
                gcmkPRINT("-- type %2d: %-16s, size %d KB \n", i, _gc_VIDMEM_type_name[i], size[i]/1024);
            }
            else
            {
                len += sprintf(buf + len, "-- type %2d: %-16s, size %d KB \n",
                               i, _gc_VIDMEM_type_name[i], size[i]/1024);
            }
        }
        else if(size[i] > 0)
        {
            if (buf == gcvNULL)
            {
                gcmkPRINT("-- type %2d: %-16s, size %d B \n", i, _gc_VIDMEM_type_name[i], size[i]);
            }
            else
            {
                len += sprintf(buf + len, "-- type %2d: %-16s, size %d B \n",
                               i, _gc_VIDMEM_type_name[i], size[i]);
            }
        }
        sum += size[i];
    }

    if (buf == gcvNULL)
    {
        gcmkPRINT("-- sum    :                   size %d KB \n", sum/1024);
    }
    else
    {
        len += sprintf(buf + len, "-- sum    :                   size %d KB \n", sum/1024);
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));

    return len;
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
    IN gctINT Value,
    OUT gctPOINTER Buffer,
    OUT gctUINT32_PTR Length
)
{
    gctUINT32 i;
    gcsDATABASE_PTR database;
    gctSTRING buf = (gctSTRING)Buffer;
    gctUINT32 len = 0;

    gcmkHEADER_ARG("Kernel=0x%x", Kernel);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    if(!Kernel->dbCreated)
    {
        gcmkFOOTER_NO();
        return gcvSTATUS_OK;
    }

    if(ListByPID)
    {
        gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));

        if(Value == -1)
        {
            for(i = 0; i < gcmCOUNTOF(Kernel->db->db); i++)
            {
                database = Kernel->db->db[i];

                while(database)
                {
                    len += _print_VIDMEM_by_pid(Kernel,
                                         database->list,
                                         gcmCOUNTOF(database->list),
                                         database->processID,
                                         database->handleDatabase,
                                         database->handleDatabaseMutex,
                                         (buf == gcvNULL) ? gcvNULL : (buf + len));
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
                len += _print_VIDMEM_by_pid(Kernel,
                                     database->list,
                                     gcmCOUNTOF(database->list),
                                     database->processID,
                                     database->handleDatabase,
                                     database->handleDatabaseMutex,
                                     (buf == gcvNULL) ? gcvNULL : (buf + len));
            }
            else
            {
                if (buf == gcvNULL)
                {
                    gcmkPRINT("No such process in memory record: %d \n", Value);
                }
                else
                {
                    len += sprintf(buf + len, "No such process in memory record: %d \n", Value);
                }
            }
        }

        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));
    }
    else
    {
        if(Value >= 0 && Value < gcvSURF_NUM_TYPES)
        {
            len += _print_VIDMEM_by_type(Kernel,
                                         Kernel->db->db,
                                         gcmCOUNTOF(Kernel->db->db),
                                         Value,
                                         (buf == gcvNULL) ? gcvNULL : (buf + len));
        }
        else if (Value == gcvSURF_NUM_TYPES)
        {
            len += _print_VIDMEM_by_type_sum(Kernel,
                                             Kernel->db->db,
                                             gcmCOUNTOF(Kernel->db->db),
                                             (buf == gcvNULL) ? gcvNULL : (buf + len));
        }
        else
        {
            for(i = 0; i < gcvSURF_NUM_TYPES; i++)
            {
                len += _print_VIDMEM_by_type(Kernel,
                                             Kernel->db->db,
                                             gcmCOUNTOF(Kernel->db->db),
                                             i,
                                             (buf == gcvNULL) ? gcvNULL : (buf + len));
            }
        }
    }

    if (Length)
    {
        *Length = len;
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
}

gceSTATUS
gckKERNEL_ShowProcessVidMemUsage(
    IN char * buf,
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    OUT gctUINT32_PTR Length
    )
{
    gcsDATABASE_PTR database;
    gctUINT32 slot;
    gctUINT32 i, len = 0;
    gctINT32 size[gcvSURF_NUM_TYPES] = {0};
    gctINT32 sum  = 0;

    gcmkHEADER_ARG("Kernel=0x%x", Kernel);
    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    if(!Kernel->dbCreated)
    {
        gcmkFOOTER_NO();
        return gcvSTATUS_OK;
    }

    gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, Kernel->db->dbMutex, gcvINFINITE));

    slot = ProcessID % gcmCOUNTOF(Kernel->db->db);
    database = Kernel->db->db[slot];
    while(database && database->processID != ProcessID)
    {
        database = database->next;
    }

    len += sprintf(buf+len, "GC memory usage details for pid %d\n", ProcessID);

    if(database && database->processID == ProcessID)
    {
        for(i = 0 ; i < gcvSURF_NUM_TYPES; i++)
        {
            size[i] = database->vidMemType[i].bytes;
            if(size[i] >= 1024)
            {
                len += sprintf(buf+len, "  - %-16s %d KB \n",
                               _gc_VIDMEM_type_name[i], size[i]/1024);
            }
            else if(size[i] > 0)
            {
                len += sprintf(buf+len, "  - %-16s %d B \n",
                               _gc_VIDMEM_type_name[i], size[i]);
            }
        }
        sum = database->vidMem.bytes + database->contiguous.bytes
             +database->nonPaged.bytes + database->virtualCommandBuffer.bytes
             +database->virtualContextBuffer.bytes;

        len += sprintf(buf+len, "  - %-16s %d KB \n",
                      "Others", (gctINT)(sum-database->vidMem.bytes)/1024);
        len += sprintf(buf+len, "  - %-16s %d KB \n",
                      "Sum",sum/1024);

    }
    else
    {
        len += sprintf(buf+len, "  - pid %d does not exist \n", ProcessID);
    }

    *Length = len;
    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->db->dbMutex));
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;
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
    gctBOOL asynchronous = gcvTRUE;
    gckVIDMEM_NODE nodeObject;
    gctPHYS_ADDR physical;
    gckKERNEL kernel = Kernel;
    gctUINT32 handle;
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

    for(i = 0; i < gcmCOUNTOF(database->list); i++)
    {

    /* Walk all records. */
    for (record = database->list[i]; record != gcvNULL; record = next)
    {
        /* Next next record. */
        next = record->next;

        /* Dispatch on record type. */
        switch (record->type)
        {
        case gcvDB_VIDEO_MEMORY:
            gcmkERR_BREAK(gckVIDMEM_HANDLE_Lookup(record->kernel,
                                                  ProcessID,
                                                  gcmPTR2INT32(record->data),
                                                  &nodeObject));

            /* Free the video memory. */
            gcmkVERIFY_OK(gckVIDMEM_HANDLE_Dereference(record->kernel,
                                                       ProcessID,
                                                       gcmPTR2INT32(record->data)));

            gcmkVERIFY_OK(gckVIDMEM_NODE_Dereference(record->kernel,
                                                     nodeObject));

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: VIDEO_MEMORY 0x%x (status=%d)",
                           record->data, status);
            break;

        case gcvDB_NON_PAGED:
            physical = gcmNAME_TO_PTR(record->physical);
            /* Unmap user logical memory first. */
            status = gckOS_UnmapUserLogical(Kernel->os,
                                            physical,
                                            record->bytes,
                                            record->data);

            /* Free the non paged memory. */
            status = gckEVENT_FreeNonPagedMemory(Kernel->eventObj,
                                                 record->bytes,
                                                 physical,
                                                 record->data,
                                                 gcvKERNEL_PIXEL);
            gcmRELEASE_NAME(record->physical);

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: NON_PAGED 0x%x, bytes=%lu (status=%d)",
                           record->data, record->bytes, status);
            break;

        case gcvDB_COMMAND_BUFFER:
            /* Free the command buffer. */
            status = gckEVENT_DestroyVirtualCommandBuffer(record->kernel->eventObj,
                                                          record->bytes,
                                                          gcmNAME_TO_PTR(record->physical),
                                                          record->data,
                                                          gcvKERNEL_PIXEL);
            gcmRELEASE_NAME(record->physical);

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: COMMAND_BUFFER 0x%x, bytes=%lu (status=%d)",
                           record->data, record->bytes, status);
            break;

        case gcvDB_CONTIGUOUS:
            physical = gcmNAME_TO_PTR(record->physical);
            /* Unmap user logical memory first. */
            status = gckOS_UnmapUserLogical(Kernel->os,
                                            physical,
                                            record->bytes,
                                            record->data);

            /* Free the contiguous memory. */
            status = gckEVENT_FreeContiguousMemory(Kernel->eventObj,
                                                   record->bytes,
                                                   physical,
                                                   record->data,
                                                   gcvKERNEL_PIXEL);
            gcmRELEASE_NAME(record->physical);

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
                                             gcmPTR2INT32(record->data));
#endif /* USE_NEW_LINUX_SIGNAL */

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: SIGNAL %d (status=%d)",
                           (gctINT)(gctUINTPTR_T)record->data, status);
            break;

        case gcvDB_VIDEO_MEMORY_LOCKED:
            handle = gcmPTR2INT32(record->data);

            gcmkERR_BREAK(gckVIDMEM_HANDLE_Lookup(record->kernel,
                                                  ProcessID,
                                                  handle,
                                                  &nodeObject));

            /* Unlock what we still locked */
            status = gckVIDMEM_Unlock(record->kernel,
                                      nodeObject,
                                      nodeObject->type,
                                      &asynchronous);

#if gcdENABLE_VG
            if (record->kernel->core == gcvCORE_VG)
            {
                if (gcmIS_SUCCESS(status) && (gcvTRUE == asynchronous))
                {
                    /* TODO: we maybe need to schedule a event here */
                    status = gckVIDMEM_Unlock(record->kernel,
                                              nodeObject,
                                              nodeObject->type,
                                              gcvNULL);
                }

                gcmkVERIFY_OK(gckVIDMEM_HANDLE_Dereference(record->kernel,
                                                           ProcessID,
                                                           handle));

                gcmkVERIFY_OK(gckVIDMEM_NODE_Dereference(record->kernel,
                                                         nodeObject));
            }
            else
#endif
            {
                gcmkVERIFY_OK(gckVIDMEM_HANDLE_Dereference(record->kernel,
                                                           ProcessID,
                                                           handle));

                if (gcmIS_SUCCESS(status) && (gcvTRUE == asynchronous))
                {
                    status = gckEVENT_Unlock(record->kernel->eventObj,
                                             gcvKERNEL_PIXEL,
                                             nodeObject,
                                             nodeObject->type);
                }
                else
                {
                    gcmkVERIFY_OK(gckVIDMEM_NODE_Dereference(record->kernel,
                                                             nodeObject));
                }
            }

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: VIDEO_MEMORY_LOCKED 0x%x (status=%d)",
                           record->data, status);
            break;

        case gcvDB_CONTEXT:
            /* TODO: Free the context */
            status = gckCOMMAND_Detach(Kernel->command, gcmNAME_TO_PTR(record->data));
            gcmRELEASE_NAME(record->data);

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: CONTEXT 0x%x (status=%d)",
                           record->data, status);
            break;

        case gcvDB_MAP_MEMORY:
            /* Unmap memory. */
            status = gckKERNEL_UnmapMemory(Kernel,
                                           record->physical,
                                           record->bytes,
                                           record->data);

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: MAP MEMORY %p (status=%d)",
                           record->data, status);
            break;

        case gcvDB_MAP_USER_MEMORY:
            /* TODO: Unmap user memory. */
            status = gckEVENT_UnmapUserMemory(record->kernel->eventObj,
                                              gcvKERNEL_PIXEL,
                                              record->physical,
                                              record->bytes,
                                              record->data,
                                              0);

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: MAP USER MEMORY %p (status=%d)",
                           record->data, status);
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

        case gcvDB_SHBUF:
            /* Free shared buffer. */
            status = gckKERNEL_DestroyShBuffer(Kernel,
                                               (gctSHBUF) record->data);

            gcmkTRACE_ZONE(gcvLEVEL_WARNING, gcvZONE_DATABASE,
                           "DB: SHBUF %u (status=%d)",
                           (gctUINT32)(gctUINTPTR_T) record->data, status);
            break;

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
    gcePOOL vidMemPool;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d Type=%d Info=0x%x",
                   Kernel, ProcessID, Type, Info);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Info != gcvNULL);

    /* Deocde pool. */
    vidMemPool = (Type & gcdDB_VIDEO_MEMORY_POOL_MASK) >> gcdDB_VIDEO_MEMORY_POOL_SHIFT;

    Type &= gcdDATABASE_TYPE_MASK;

    /* Find the database. */
    gcmkONERROR(
        gckKERNEL_FindDatabase(Kernel, ProcessID, LastProcessID, &database));


    gcmkVERIFY_OK(gckOS_AcquireMutex(Kernel->os, database->counterMutex, gcvINFINITE));

    /* Get pointer to counters. */
    switch (Type)
    {
    case gcvDB_VIDEO_MEMORY:
        if (vidMemPool != gcvPOOL_UNKNOWN)
        {
            gckOS_MemCopy(&Info->counters,
                          &database->vidMemPool[vidMemPool],
                          gcmSIZEOF(database->vidMemPool[vidMemPool]));
        }
        else
        {
            gckOS_MemCopy(&Info->counters,
                          &database->vidMem,
                          gcmSIZEOF(database->vidMem));
        }
        break;

    case gcvDB_NON_PAGED:
        gckOS_MemCopy(&Info->counters,
                                  &database->nonPaged,
                                  gcmSIZEOF(database->vidMem));
        break;

    case gcvDB_CONTIGUOUS:
        gckOS_MemCopy(&Info->counters,
                                  &database->contiguous,
                                  gcmSIZEOF(database->vidMem));
        break;

    case gcvDB_IDLE:
        Info->time                         = Kernel->db->idleTime[Kernel->core];
        Kernel->db->idleTime[Kernel->core] = 0;
        break;

    case gcvDB_MAP_MEMORY:
        gckOS_MemCopy(&Info->counters,
                                  &database->mapMemory,
                                  gcmSIZEOF(database->mapMemory));
        break;

    case gcvDB_MAP_USER_MEMORY:
        gckOS_MemCopy(&Info->counters,
                                  &database->mapUserMemory,
                                  gcmSIZEOF(database->mapUserMemory));
        break;

    case gcvDB_COMMAND_BUFFER:
        gckOS_MemCopy(&Info->counters,
                                  &database->virtualCommandBuffer,
                                  gcmSIZEOF(database->virtualCommandBuffer));
        break;

    default:
        break;
    }

    gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, database->counterMutex));

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

gceSTATUS
gckKERNEL_FindHandleDatbase(
    IN gckKERNEL Kernel,
    IN gctUINT32 ProcessID,
    OUT gctPOINTER * HandleDatabase,
    OUT gctPOINTER * HandleDatabaseMutex
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d",
                   Kernel, ProcessID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Find the database. */
    gcmkONERROR(gckKERNEL_FindDatabase(Kernel, ProcessID, gcvFALSE, &database));

    *HandleDatabase = database->handleDatabase;
    *HandleDatabaseMutex = database->handleDatabaseMutex;

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}

gceSTATUS
gckKERNEL_QueryPulseEaterIdleProfile(
    IN gckKERNEL      Kernel,
    IN gctBOOL        Start,
    IN gcePulseEaterDomain Domain,
    OUT gctUINT32_PTR BusyRatio,
    IN OUT gctUINT32_PTR TimeGap,
    OUT gctUINT32_PTR TotalTime)
{
    if(has_feat_pulse_eater_profiler())
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

            /* recordsNum > PULSE_EATER_COUNT*/
            if(dbPulse->moreRound)
            {
                diff    = endTime - PulseEaterDB(0).time + PulseEaterDB(0).sfPeriod;
            }
            /* recordsNum < PULSE_EATER_COUNT*/
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
    else
    {
        return gcvSTATUS_OK;
    }
}

#if gcdPROCESS_ADDRESS_SPACE
gceSTATUS
gckKERNEL_GetProcessMMU(
    IN gckKERNEL Kernel,
    OUT gckMMU * Mmu
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;
    gctUINT32 processID;

    gcmkONERROR(gckOS_GetProcessID(&processID));

    gcmkONERROR(gckKERNEL_FindDatabase(Kernel, processID, gcvFALSE, &database));

    *Mmu = database->mmu;

    return gcvSTATUS_OK;

OnError:
    return status;
}
#endif

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

void
_DumpCounter(
    IN gcsDATABASE_COUNTERS * Counter,
    IN gctCONST_STRING Name
    )
{
    gcmkPRINT("%s:", Name);
    gcmkPRINT("  Currently allocated : %10lld", Counter->bytes);
    gcmkPRINT("  Maximum allocated   : %10lld", Counter->maxBytes);
    gcmkPRINT("  Total allocated     : %10lld", Counter->totalBytes);
}

gceSTATUS
gckKERNEL_DumpVidMemUsage(
    IN gckKERNEL Kernel,
    IN gctINT32 ProcessID
    )
{
    gceSTATUS status;
    gcsDATABASE_PTR database;
    gcsDATABASE_COUNTERS * counter;
    gctUINT32 i = 0;

    static gctCONST_STRING surfaceTypes[] = {
        "UNKNOWN",
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
        "HIERARCHICAL_DEPTH",
    };

    gcmkHEADER_ARG("Kernel=0x%x ProcessID=%d",
                   Kernel, ProcessID);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);

    /* Find the database. */
    gcmkONERROR(
        gckKERNEL_FindDatabase(Kernel, ProcessID, gcvFALSE, &database));

    gcmkPRINT("VidMem Usage (Process %d):", ProcessID);

    /* Get pointer to counters. */
    counter = &database->vidMem;

    _DumpCounter(counter, "Total Video Memory");

    for (i = 0; i < gcvSURF_NUM_TYPES; i++)
    {
        counter = &database->vidMemType[i];

        _DumpCounter(counter, surfaceTypes[i]);
    }

    /* Success. */
    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    /* Return the status. */
    gcmkFOOTER();
    return status;
}
