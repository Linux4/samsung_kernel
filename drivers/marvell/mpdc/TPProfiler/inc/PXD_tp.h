/*
** (C) Copyright 2009 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd. 
** under the terms of the GNU General Public License Version 2, June 1991 (the "License"). 
** You may use, redistribute and/or modify this File in accordance with the terms and 
** conditions of the License, a copy of which is available along with the File in the 
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place, 
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES 
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.  
** The License provides additional details about this warranty disclaimer.
*/

/* (C) Copyright 2010 Marvell International Ltd. All Rights Reserved */

/* defines the constants and structures for thread profiler */
#ifndef _TP_PROFILER_STRUCT_H
#define _TP_PROFILER_STRUCT_H

#include "pxdTypes.h"
#include "TPProfilerDef.h"

#ifdef _cplusplus
extern "C" {
#endif

/* Following are the payload types */
#define TP_MODULES             1
#define TP_EVENTS              2
#define TP_SUMMARY             3           // obselete, but don't remove this line
#define TP_CPU_USAGE           4

#define PX_MAX_PATH    512

typedef PXD32_Byte TP_FULLPATH[PX_MAX_PATH];

typedef struct PXD32_TPSettings_s
{
	PXD32_Word duration;           /* duration in seconds */
	PXD32_Word bufferSize;         /* buffer size in KB */
	PXD32_Word options;
	PXD32_Word appEnabled;         /* none 0 means the application settings enabled */
	                               /* 0 means application settings disabled */
	PXD32_Word imageEnabled;       /* same as appEnabled but for image settings */
	
	struct Application_s 
	{
		TP_FULLPATH path;
		TP_FULLPATH workingDir;
		PXD32_Byte  parameter[512];
		PXD32_Word  options;
	} appLaunched;

	struct Image_s 
	{
		TP_FULLPATH path;
	} imageWaited;

} PXD32_TPSettings;

/*
 * Following macros define the flag of the module
 */

/* This module is a exe */
#define TP_MODULE_RECORD_FLAG_EXE                      0x00000001

/* This module is a global module */
/* Global module is a module mapped to the same virtual address in all processes */
#define TP_MODULE_RECORD_FLAG_GLOBAL_MODULE            0x00000002

/* This module is the first module of the process */
#define TP_MODULE_RECORD_FLAG_FIRST_MODULE_IN_PROCESS  0x00000004

/* This process has the same pid as the auto launched application */
#define TP_MODULE_RECORD_FLAG_AUTO_LAUNCH_PID          0x00000008

typedef struct PXD32_TP_Module_s
{
	PXD32_Word    length;           /* total record length */
	PXD32_Word    pid;              /* pid of the process which loads the module */
	PXD32_DWord   timestamp;        /* when the module is loaded */
	PXD32_Word    flag;             /* process attribute flag */
	PXD32_Word    loadAddr;         /* load address */
	PXD32_Word    codeSize;         /* code size in virtual memory */
	PXD32_Word    reserved;         /* reserved for future */
	PXD32_Word    nameOffset;       /* file name offset in the file path string */
	PXD32_Byte    pathName[1];      /* file path and name string */
} PXD32_TP_Module;

typedef enum TPEventType_e
{
//	TP_EVENT_THREAD_ENUM                   = 0,    /* for PXD32_TP_Thread_Enum, obsolete */
	TP_EVENT_THREAD_CREATE                 = 1,    /* for PXD32_TP_Thread_Create */
	TP_EVENT_THREAD_EXIT                   = 2,    /* for PXD32_TP_Thread_Exit */
	TP_EVENT_THREAD_JOIN_BEGIN             = 3,    /* for PXD32_TP_Thread_Join_Begin */
	TP_EVENT_THREAD_JOIN_END               = 4,    /* for PXD32_TP_Thread_Join_End */
	TP_EVENT_THREAD_WAIT_SYNC_BEGIN        = 5,    /* for PXD32_TP_Thread_Wait_SyncObj_Begin */
	TP_EVENT_THREAD_WAIT_SYNC_END          = 6,    /* for PXD32_TP_Thread_Wait_SyncObj_End */ 
	TP_EVENT_THREAD_TRY_WAIT_SYNC_BEGIN    = 7,    /* for PXD32_TP_Thread_Try_Wait_SyncObj_Begin */
	TP_EVENT_THREAD_TRY_WAIT_SYNC_END      = 8,    /* for PXD32_TP_Thread_Try_Wait_SyncObj_End */ 
	TP_EVENT_THREAD_TIMED_WAIT_SYNC_BEGIN  = 9,    /* for PXD32_TP_Thread_Timed_Wait_SyncObj_Begin */
	TP_EVENT_THREAD_TIMED_WAIT_SYNC_END    = 10,   /* for PXD32_TP_Thread_Timed_Wait_SyncObj_End */ 
//	TP_EVENT_THREAD_SPIN_SYNC_BEGIN        = 11,   /* for PXD32_TP_Thread_Spin_SyncObj_Begin, obsolete */
//	TP_EVENT_THREAD_SPIN_SYNC_END          = 12,   /* for PXD32_TP_Thread_Spin_SyncObj_End, obsolete */
//	TP_EVENT_THREAD_TRY_SPIN_SYNC_BEGIN    = 13,   /* for PXD32_TP_Thread_Try_Spin_SyncObj_Begin, obsolete */
//	TP_EVENT_THREAD_TRY_SPIN_SYNC_END      = 14,   /* for PXD32_TP_Thread_Try_Spin_SyncObj_End, obsolete */ 
//	TP_EVENT_THREAD_TIMED_SPIN_SYNC_BEGIN  = 15,   /* for PXD32_TP_Thread_Timed_Spin_SyncObj_Begin, obsolete */
//	TP_EVENT_THREAD_TIMED_SPIN_SYNC_END    = 16,   /* for PXD32_TP_Thread_Timed_Spin_SyncObj_End, obsolete */ 
	TP_EVENT_THREAD_RELEASE_SYNC           = 17,   /* for PXD32_TP_Thread_Release_SyncObj */
	TP_EVENT_THREAD_BROADCAST_RELEASE_SYNC = 18,   /* for PXD32_TP_Thread_Broadcast_Release_SyncObj */
	TP_EVENT_SYNC_CREATE                   = 19,   /* for PXD32_TP_SyncObj_Create */
	TP_EVENT_SYNC_OPEN                     = 20,   /* for PXD32_TP_SyncObj_Open */
	TP_EVENT_SYNC_DESTROY                  = 21,   /* for PXD32_TP_SyncObj_Destroy */
	TP_EVENT_THREAD_PRIORITY_CHANGE        = 22,   /* for PXD32_TP_Thread_Priority_Change */
	TP_EVENT_THREAD_CHANGE_CPU             = 23    /* for PXD32_TP_Thread_Change_CPU */
}TPEventType;


/* All the synchronization object type with busy wait should be smaller than TP_SYNC_OBJ_TYPE_BUSY_WAIT_MAX */
#define TP_SYNC_OBJ_TYPE_BUSY_WAIT_MIN         1
#define TP_SYNC_OBJ_TYPE_BUSY_WAIT_MAX         1000
#define TP_SYNC_OBJ_TYPE_WAIT_MIN              TP_SYNC_OBJ_TYPE_BUSY_WAIT_MAX + 1
#define TP_SYNC_OBJ_TYPE_WAIT_MAX              65535  

typedef enum TPSyncObjType_e
{
	TP_SYNC_OBJ_TYPE_SPIN_LOCK             = TP_SYNC_OBJ_TYPE_BUSY_WAIT_MIN + 0,     /* Spin Lock */
	TP_SYNC_OBJ_TYPE_MUTEX                 = TP_SYNC_OBJ_TYPE_WAIT_MIN + 0,          /* Mutex */
	TP_SYNC_OBJ_TYPE_SEMAPHORE             = TP_SYNC_OBJ_TYPE_WAIT_MIN + 1,          /* Semaphore */
	TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE    = TP_SYNC_OBJ_TYPE_WAIT_MIN + 2,          /* Condition Variable */
	TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE         = TP_SYNC_OBJ_TYPE_WAIT_MIN + 3,          /* Message Queue */
	TP_SYNC_OBJ_TYPE_RW_LOCK               = TP_SYNC_OBJ_TYPE_WAIT_MIN + 4,          /* Read/Write Lock */
	TP_SYNC_OBJ_TYPE_RW_SEMAPHORE          = TP_SYNC_OBJ_TYPE_WAIT_MIN + 5,          /* Read/Write Semaphore */
}TPSyncObjType;


/* Following macros are for Syncronization Object Subtype, they can be combined when using */
#define TP_SYNC_OBJ_SUBTYPE_RW_READ            0x1           /* If the type is RW Lock/Semaphore, the operation is read lock/unlock */
#define TP_SYNC_OBJ_SUBTYPE_RW_WRITE           0x2           /* If the type is RW Lock/Semaphore, the operation is write lock/unlock */
#define TP_SYNC_OBJ_SUBTYPE_PROC_SHARED        0x4           /* The synchronization object is shared between processes */

/* This structure represents the TP_EVENTS payload */
/* ------------------------------------------------------------------  */
/* | event type |    the data for correponding event type structure |  */
/* ------------------------------------------------------------------  */

typedef struct PXD32_TP_Event_s
{
	PXD32_Word   eventType;        /* which event is recorded */ 
	PXD32_Word   dataSize;         /* size of the following data */   
	PXD32_Byte   eventData[1];     /* following spaces are for the event type structure */
}PXD32_TP_Event;

typedef struct TP_Event_Context_s
{
	PXD32_Word   pid;               /* Id of the calling process */
	PXD32_Word   tid;               /* Id of the calling thread */
	PXD32_Word   pc;                /* Where the event is triggered */  
	PXD32_DWord  ts;                /* When the event is triggered */
} TP_Event_Context;

#if 0
#define THREAD_STATE_RUNNING         0        /* the thread is running or waiting for run */
#define THREAD_STATE_BLOCKED         1        /* the thread is blocked on some event */

/* The following structure is for the TP_INIT_THREADS payload */
typedef struct PXD32_TP_Thread_Enum_s
{
	PXD32_Word   pid;               /* id of the process */
	PXD32_Word   tid;               /* id of the thread */
	PXD32_Word   state;             /* state of the thread */
	PXD32_Word   priority;          /* priority of the thread */
}PXD32_TP_Thread_Enum;
#endif

/* A new thread is created */
/* For example, posix pthread_create() */
typedef struct PXD32_TP_Thread_Create_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       newPid;            /* pid of the new thread */
	PXD32_Word       newTid;            /* Id of the new thread */
	PXD32_Word       priority;          /* Priority of the new thread */ 
}PXD32_TP_Thread_Create;

/* This structure represents that one thread is to exit */
/* For example, posix pthread_exit() or just return from thread function */
typedef struct PXD32_TP_Thread_Exit_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       returnValue;       /* The return value of the thread */
}PXD32_TP_Thread_Exit;

/* This structure represents that one thread is waiting for another thread to be exited */
/* For example, posix pthread_join() is called */
typedef struct PXD32_TP_Thread_Join_Begin_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       targetTid;         /* Id of the target thread to be waited for */
}PXD32_TP_Thread_Join_Begin;


/* The wait/join/spin function is returned successfully */
#define WAIT_RETURN_SUCCESS           0

/* The wait/join/spin function is returned with timedout */
#define WAIT_RETURN_TIMEDOUT          1  

/* The wait/join/spin function is returned with other errors */
#define WAIT_RETURN_ERROR             2  

/* This structure represents that one thread has finished waiting for another thread to be exited */
/* For example, posix pthread_join() is returned */
typedef struct PXD32_TP_Thread_Join_End_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       targetTid;         /* Id of the target thread to be waited for */
	PXD32_Word       threadReturn;      /* The return value of thread */
	PXD32_Word       returnValue;       /* The result of the thread join function, see definition of the WAIT_RETURN_* macros */
}PXD32_TP_Thread_Join_End;

/* This structure represents that a synchronization object waiting function is called */
/* For example posix pthread_mutex_lock() is called */
typedef struct PXD32_TP_Thread_Wait_SyncObj_Begin_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
}PXD32_TP_Thread_Wait_SyncObj_Begin;

/* This structure represents that a synchronization object waiting function is returned */
/* For example posix pthread_mutex_lock() is returned */
typedef struct PXD32_TP_Thread_Wait_SyncObj_End_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
	PXD32_Word       returnValue;       /* The resul of the thread wait function, see definition of the WAIT_RETURN_* macros */
}PXD32_TP_Thread_Wait_SyncObj_End;

/* This structure represents that a synchronization object trying waiting function is called */
/* For example posix pthread_mutex_trylock() is called */
typedef struct PXD32_TP_Thread_Try_Wait_SyncObj_Begin_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
}PXD32_TP_Thread_Try_Wait_SyncObj_Begin;

/* This structure represents that a synchronization object trying waiting function is returned */
/* For example posix pthread_mutex_trylock() is returned */
typedef struct PXD32_TP_Thread_Try_Wait_SyncObj_End_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
	PXD32_Word       returnValue;       /* The resul of the thread wait function, see definition of the WAIT_RETURN_* macros */
}PXD32_TP_Thread_Try_Wait_SyncObj_End;

/* This structure represents that a synchronization object timed waiting function is called */
/* For example posix pthread_mutex_timedlock() is called */
typedef struct PXD32_TP_Thread_Timed_Wait_SyncObj_Begin_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
}PXD32_TP_Thread_Timed_Wait_SyncObj_Begin;

/* This structure represents that a synchronization object timed waiting function is returned */
/* For example posix pthread_mutex_timedlock() is returned */
typedef struct PXD32_TP_Thread_Timed_Wait_SyncObj_End_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
	PXD32_Word       returnValue;       /* The return value of the thread wait function, see definition of the WAIT_RETURN_* macros */
}PXD32_TP_Thread_Timed_Wait_SyncObj_End;

/* This structure represents that a synchronization object release function is called */
/* For example posix pthread_mutex_unlock() is called */
typedef struct PXD32_TP_Thread_Release_SyncObj_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
}PXD32_TP_Thread_Release_SyncObj;

/* This structure represents that a synchronization object broadcast release function is called */
/* For example posix pthread_cond_broadcast() is called */
typedef struct PXD32_TP_Thread_Broadcast_Release_SyncObj_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
}PXD32_TP_Thread_Broadcast_Release_SyncObj;

/* This structure represents that a synchronization object create function is successfully called */
/* For example posix pthread_mutex_init() is called */
typedef struct PXD32_TP_SyncObj_Create_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the created synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
	PXD32_Word       initCount;         /* initial count value for the synchronization object */
	PXD32_Word       maxCount;          /* max count value for the synchronization object */
	PXD32_Byte       name[1];           /* name for the synchronization object */
}PXD32_TP_SyncObj_Create;

/* This structure represents that a synchronization object open function is successfully called */
/* For example posix pthread_mutex_init() is called */
typedef struct PXD32_TP_SyncObj_Open_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the created synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
	PXD32_Byte       name[1];           /* name for the synchronization object */
}PXD32_TP_SyncObj_Open;

/* This structure represents that a synchronization object delete function is successfully called */
/* For example posix pthread_mutex_destroy() is called */
typedef struct PXD32_TP_SyncObj_Destroy_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       objectId;          /* id of the synchronization object */
	PXD32_Word       objectType;        /* type of the synchronization object, see enum TPSyncObjType */
	PXD32_Word       objectSubType;     /* subtype of the synchronization object, see macro TP_SYNC_OBJ_SUBTYPE_* */
}PXD32_TP_SyncObj_Destroy;

/* This structure represents that a thread priority is changed */
typedef struct PXD32_TP_Thread_Priority_Change_s
{
	TP_Event_Context context;           /* context of the event */
	PXD32_Word       oldPriority;       /* the old priority */
	PXD32_Word       newPriority;       /* the new priority */
} PXD32_TP_Thread_Priority_Change;

typedef struct PXD32_TP_Thread_Change_CPU_s
{
	PXD32_Word   pid;               /* Id of the process which the thread belongs to */
	PXD32_Word   tid;               /* Id of the switching thread */
	PXD32_DWord  timestamp;         /* when does this happen */
	PXD32_Word   newCpu;            /* To which cpu the thread switches */
}PXD32_TP_Thread_Change_CPU;

typedef struct PXD32_TP_CPU_Usage_s
{
	PXD32_DWord   timestamp;        /* timestamp */
	PXD32_Word    usage[16];        /* CPU usage for each CPU, maximum to 16 CPUs */
}PXD32_TP_CPU_Usage;

typedef struct PXD32_TP_Summary_s
{
	PXD32_Time startTime;           /* start time */
	PXD32_Time stopTime;            /* stop time */
}PXD32_TP_Summary;


   
#ifdef _cplusplus
}
#endif

#endif /* _TP_PROFILER_STRUCT_H */
