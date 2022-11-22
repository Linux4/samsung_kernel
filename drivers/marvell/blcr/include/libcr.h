/* 
 * Berkeley Lab Checkpoint/Restart (BLCR) for Linux is Copyright (c)
 * 2008, The Regents of the University of California, through Lawrence
 * Berkeley National Laboratory (subject to receipt of any required
 * approvals from the U.S. Dept. of Energy).  All rights reserved.
 *
 * Portions may be copyrighted by others, as may be noted in specific
 * copyright notices within specific files.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: libcr.h,v 1.83.4.2 2009/02/18 03:41:26 phargrov Exp $
 */

#ifndef _CRLIB_H
#define _CRLIB_H	1

#include <features.h>
#include <sys/select.h> // for (struct timeval)
#include <errno.h>
#include <blcr_common.h>

__BEGIN_DECLS

// Version codes to identify caller's versions of the interface(s).  
// The library will adapt to versions older than itself.
// Client code can also use this to know what members to expect in
// the corresponding struct type.
typedef int cr_version_t;
#define CR_CHECKPOINT_ARGS_VERSION 1
#define CR_RESTART_ARGS_VERSION 1

// Maximum number of callbacks which can be registered
#define CR_MAX_CALLBACKS	0x1FFFFFF

// Type of callback functions
typedef int (*cr_callback_t)(void *);

// Integer type returned as an identifier from cr_init()
typedef int cr_client_id_t;

// Integer type returned as an identifier from cr_register_callback()
typedef int cr_callback_id_t;

// Type for signal-safe spinlocks
typedef unsigned int cr_spinlock_t;

// Type for cr_get_checkpoint_info()
struct cr_checkpoint_info {
	int		signal;		/* Signal that will be sent to the
					   process(es) when the checkpoint
					   has been taken, or zero if none
					   will be sent. */
	const char	*dest;		/* Path to the context file. */
	pid_t		requester;	/* pid of requester */
	pid_t		target;		/* Pid, pgid or sid of which chkpt is requested */
	cr_scope_t	scope;		/* scope of the request */
};

// Type for cr_get_restart_info()
struct cr_restart_info {
	pid_t		requester;	/* pid of cr_restart */
	const char	*src;		/* Path to the context file. */
};

// Values for return code from cr_status
enum {
	CR_STATE_IDLE,
	CR_STATE_PENDING,
	CR_STATE_ACTIVE
};

//  For usage examples see
//    tests/crut.c:crut_checkpoint_to_file()
//    tests/crut_util_libcr.c:crut_checkpoint_request()
//    util/cr_checkpoint/cr_checkpoint.c:real_main()
typedef struct cr_checkpoint_args {
    cr_version_t cr_version;
    cr_scope_t   cr_scope;
    pid_t        cr_target;

    int          cr_fd;
    int          cr_signal;
    unsigned int cr_timeout;    /* cr_secs in cr_chkpt_args */
    unsigned int cr_flags;
} cr_checkpoint_args_t;

//  For usage examples see
//    util/cr_restart/cr_restart.c:main()
//    tests/crut_wrapper.c:try_restart()
typedef struct cr_restart_args {
    cr_version_t cr_version;
    int          cr_fd;
    int          cr_signal;
    unsigned int cr_flags;
    struct cr_rstrt_relocate *cr_relocate;
} cr_restart_args_t;

// Opaque handles
typedef unsigned long cr_checkpoint_handle_t;
typedef unsigned long cr_restart_handle_t;

// Context flags for cr_{register,replace}_callback() and cr_replace_self().
#define CR_THREAD_CONTEXT	0x4000000
#define CR_SIGNAL_CONTEXT	0x2000000

// cr_client_id for use by callbacks
#define CR_ID_CALLBACK ((cr_client_id_t)(-1))

// Initializer for use with statically allocated spinlocks.
#define CR_SPINLOCK_INITIALIZER 0U

// Eval values for cr_register_hook()
typedef enum {
    // CR_HOOK_CONT_*: continuing after a checkpoint...
      // ... in a thread with signal-context callbacks and/or critical sections
	CR_HOOK_CONT_SIGNAL_CONTEXT = 0,
      // ... in a thread servicing thread-context callbacks
	CR_HOOK_CONT_THREAD_CONTEXT,
      // ... in a thread with no registered callbacks
	CR_HOOK_CONT_NO_CALLBACKS,

    // CR_HOOK_CONT_*: restarting from a checkpoint...
      // ... in a thread with signal-context callbacks and/or critical sections
	CR_HOOK_RSTRT_SIGNAL_CONTEXT,
      // ... in a thread servicing thread-context callbacks
	CR_HOOK_RSTRT_THREAD_CONTEXT,
      // ... in a thread with no registered callbacks
	CR_HOOK_RSTRT_NO_CALLBACKS, 

    // Not a valid value to pass to cr_register_hook:
	CR_NUM_HOOKS
} cr_hook_event_t;

// Function type for cr_register_hook()
typedef void (*cr_hook_fn_t)(cr_hook_event_t);
#define CR_HOOK_FN_ERROR ((cr_hook_fn_t)(~0UL))

// Values for scope argument to cr_hold_ctrl()
enum {
	CR_HOLD_SCOPE_THREAD,
	CR_HOLD_SCOPE_INIT,
	CR_HOLD_SCOPE_UNINIT
};

// cr_init()
//
// Initialize (per-thread) state for use with libcr.
//
// This function must be called once from each thread before it makes
// any other calls to libcr functions.  This is not needed from
// threads which will never call libcr functions, nor is it needed
// inside of thread-context callback code (which runs in a thread
// created by libcr).
//
// On success, returns a non-negative identifier, of type
// cr_client_id_t, to be used in any subsequent calls to
// cr_{enter,leave}_cs().  This identifier has only thread-local scope
// and must not be used from another thread.
// Returns negative values to indicate failure.
//
// This function is not reentrant - do not call it from signal context.
//
extern cr_client_id_t
cr_init(void);

// cr_initialize_checkpoint_args_t
//
// This initializer ensures that a client will get sane default values
// for fields they know nothing about even if linked to a library version
// newer than the one they were written and/or compiled for.
//
// Returns 0 on success, -1 on failure.
// Only likely failure is an incompatible library version,
// though that should be resolved at library link/load time.
//
// Never call cri_init_checkpoint_args_t() directly
#define cr_initialize_checkpoint_args_t(_cr_args) \
	cri_init_checkpoint_args_t(CR_CHECKPOINT_ARGS_VERSION, (_cr_args))

// cr_request_checkpoint
//
// Initiate a checkpoint of another process, process group, or session.
// Must call cr_initialize_checkpoint_args_t first.
// Use cr_wait_checkpoint(), cr_poll_checkpoint() or cr_poll_checkpoint_msg()
// to check or block for completion.
//
// IN:  args     See cr_checkpoint_args_t type for options.
// OUT: handle   Opaque output argument filled in by function call.
//
// Returns:
//   0 - checkpoint has been requested.
// < 0 - an error occurred (check errno for details)
//
// Most likely errno values when return < 0:
// ESRCH	 Request did not identify any existing pid, pgid or sid.
// EPERM	 One or more of the target processes are not checkpointable
// 		 by the current user.
// EBADF	 The destination (args->cr_fd) is not a valid file descriptor. 
// EINVAL	 Some field of the args argument is invalid.
// Other values may be possible.
// 
// Setting CR_CHKPT_ASYNC_ERR in args->cr_flags will defer reporting of
// nearly all errors in cr_request_checkpoint() until the cr_reap_checkpoint()
// call.  This allows for use of cr_log_checkpoint() to collect kernel
// messages that are associated with these error conditions.
//
extern int
cr_request_checkpoint(cr_checkpoint_args_t *args, cr_checkpoint_handle_t *handle);

// cr_forward_checkpoint
//
// Adds the given scope to a current checkpoint request.  Use this if there
// is some other process(es) that you want checkpointed consistently with
// your process
//
// Returns:
//   0 - the requested process(es) have been added to the checkpoint request.
// < 0 - an error occurred (check errno for details)
//
// Most likely errno values when return < 0:
// ESRCH	 Request did not identify any existing pid, pgid or sid.
// EPERM	 One or more of the target processes are not checkpointable
// 		 by the current user.
// EINVAL	 One of the arguments is invalid.
// CR_ENOTCB	 Called from other than a callback context.
// Other values may be possible.
//
// Note that it is not an error for cr_forward_checkpoint() to have a scope
// that overlaps the existing scope of the checkpoint (or to be identical).
// Processes in the overlap will be checkpointed exactly once.
// 
// This routine may only be called from a registered callback, and not
// after calling cr_checkpoint().
//
extern int
cr_forward_checkpoint(cr_scope_t cr_scope, pid_t cr_target);

// cr_wait_checkpoint
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_checkpoint()
// INOUT: timeval  Time limit, or NULL for unbounded wait.
//
// Returns:
// > 0 - checkpoint complete
//   0 - it's not done yet and the timeout (if any) has expired
// < 0 - an error occurred (see below for details)
//
// Waits until checkpoint completes, bounded by timeval (if non-NULL).
//
// The remaining timeout is written back to timeval if, and only if,
// select() would do so for the current Linux personality.  The contents
// of timeval are undefined on error.
//
// A negative return value indicates that an error occurred in trying to
// wait for completion, and is not an indication of the status of the
// checkpoint request.
// Most likely errno values when returning < 0:
// EINVAL	 No checkpoint request associated with the given handle.
// EINTR	 Call was interrupted by a signal.
// EFAULT	 'timeval' argument points outside caller's address space.
// Other values may be possible.
//
extern int
cr_wait_checkpoint(cr_checkpoint_handle_t *handle, struct timeval *timeout);

// cr_log_checkpoint
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_checkpoint()
// IN:    len      Size of allocated space.
// OUT:   msg      Space to return kernel messages.
//
// Returns:
// > 0 - space consumed by kernel messages.
//   0 - no kernel messages were logged.
// < 0 - an error occurred (see below for details)
//
// Collects messages logged by the kernel and associated with the checkpoint
// request.
//
// Up to 'len' bytes (including the terminating '\0' character) of kernel
// messages are copied to 'msg'.  Collecting the log messages does not destroy
// them.  So, the same messages may be retreived multiple times.
//
// Except in error cases, the return value is the number of bytes of storage
// required to retreive all the kernel messages logged for the given request,
// including the '\0' string terminator.  If the return value is larger than
// 'len', then the messages have been truncated.  In the case of 'len' equal
// to zero, 'msg' is ignored (and thus may safely be NULL) and the return
// value can be used to allocate a large enough buffer for a second call.
//
// Most likely errno values when returning < 0:
// EINVAL	 No checkpoint request associated with the given handle.
// EFAULT	 'msg' argument points outside caller's address space.
// Other values may be possible.
//
extern int
cr_log_checkpoint(cr_checkpoint_handle_t *handle, unsigned int len, char *msg);

// cr_reap_checkpoint
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_checkpoint()
//
// Returns:
// > 0 - checkpoint complete
// < 0 - an error occurred (see below for details)
//
// Retreives the status of a completed checkpoint request and releases the
// associated resources.
//
// If this call returns positive (success) or returns negative with an errno
// value other than EAGAIN, then the checkpoint is complete and the handle is
// invalid.  Further use of the handle w/o calling cr_request_checkpoint() to
// obtain a new one will result in undefined behavior.
//
// When checkpointing a set of processes which includes one's self, note:
// - When restarting from a checkpoint which includes one's self, releases
//   priot to 0.7.0 yielded, from cr_poll_checkpoint():
//      ((return==CR_POLL_CHKPT_ERR_PRE) && (errno==EINVAL)).
// - For this case, the current release yields
//      errno==CR_ERESTARTED
//   and this behavior is expected to continue in future releases.
// - Do not attempt to skip the cr_reap_checkpoint() to avoid this
//   particular error return.  Omitting the poll causes a resource leak.
//
// A negative return value with errno == EAGAIN indicates that the
// checkpoint request is NOT completed.  Use of cr_wait_checkpoint() is
// recommended (but not required) to check that a request is completed
// before calling cr_reap_checkpoint().
//
// A negative return value with errno == CR_ERESTARTED indicates that the
// checkpoint request was completed successfully.
//
// A negative return value with any other value for errno indicates that the
// checkpoint request was unsuccessful (was completed with errors):
// EINVAL	 No checkpoint request associated with the given handle.
// CR_ENOSUPPORT One or more of the target processes are not linked (or
// 		 LD_PRELOADed) with libcr.
// CR_ETEMPFAIL  One or more of the target processes indicated a
//               temporary inability to perform the requested checkpoint.
// CR_EPERMFAIL  One or more of the target processes indicated a
//               permanent inability to perform the requested checkpoint.
// ESRCH	 All target processes indicated that they should be 
//		 omitted from the checkpoint.
// EIO		 Problem writing the checkpoint output.
// ENOSPC	 Out of space on filesystem while writing the checkpoint.
// Other values may be possible.
//
// If cr_flags of the request included CR_CHKPT_ASYNC_ERR, then all the
// errno values listed in the description of cr_request_checkpoint() are
// also possible results from cr_reap_checkpoint().
//
// Note that even in the event of an error, it is possible that data has
// been written to the requested file descriptor.
//
extern int
cr_reap_checkpoint(cr_checkpoint_handle_t *handle);

// cr_poll_checkpoint_msg
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_checkpoint()
// INOUT: timeval  Time limit, or NULL for unbounded wait.
// OUT:   msg_p    Space to return pointer to kernel messages.
//
// Returns:
// > 0 - checkpoint complete
//   0 - it's not done yet and the timeout (if any) has expired
// < 0 - an error occurred (see below for details)
//
// Waits until checkpoint completes, bounded by timeval (if non-NULL), and
// collects kernel log messages and checkpoint request status.
//
// This call is roughly equivalent to:
//   rc = cr_wait_checkpoint(handle, timeval);
//   if (rc == 0) return 0; // Not done
//   if (rc < 0) return CR_POLL_CHKPT_ERR_PRE;
//   if (msg_p) {
//     int len = cr_log_checkpoint(handle, 0, NULL);
//     char *buf = *msg_p = malloc(len);
//     rc = cr_log_checkpoint(handle, len, buf);
//     if (rc < 0) return CR_POLL_CHKPT_ERR_LOG;
//   }
//   rc = cr_reap_checkpoint(handle);
//   if (rc < 0) return CR_POLL_CHKPT_ERR_POST;
//   return SOME_POSITIVE_VALUE; // Done
// with some additional error checking.
//
// For non-NULL 'msg_p', the caller is responsible for calling free(*msg_p)
// when the need for the collected messages is ended.
//
// If an error is detected from the cr_wait_checkpoint() call, then the
// negative value CR_POLL_CHKPT_ERR_PRE is returned.  See the documentation
// for cr_wait_checkpoint() for possible errno values and their meaning.
//
// If cr_wait_checkpoint() returns 0 to indicate the request is not done,
// then cr_poll_checkpoint_msg() also returns 0.
//
// If an error is detected from the cr_log_checkpoint() call, then the
// negative value CR_POLL_CHKPT_ERR_LOG is returned.  See the documentation
// for cr_log_checkpoint() for possible errno values and their meaning.
//
// If an error is detected from the cr_reap_checkpoint() call, then the
// negative value CR_POLL_CHKPT_ERR_POST is returned.  See the documentation
// for cr_reap_checkpoint() for possible errno values and their meaning.
//
#define CR_POLL_CHKPT_ERR_PRE  -1
#define CR_POLL_CHKPT_ERR_POST -2
#define CR_POLL_CHKPT_ERR_LOG  -3
extern int
cr_poll_checkpoint_msg(cr_checkpoint_handle_t *handle, struct timeval *timeout, char **msg_p);

// cr_poll_checkpoint
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_checkpoint()
// INOUT: timeval  Time limit, or NULL for unbounded wait.
//
// Calling
//   rc = cr_poll_checkpoint(handle, timeout);
// is equivalent to
//   rc = cr_poll_checkpoint_msg(handle, timeout, NULL);
//
extern int
cr_poll_checkpoint(cr_checkpoint_handle_t *handle, struct timeval *timeout);

// cr_initialize_restart_args_t
// THIS FUNCTION IS EXPERIMENTAL: USE AT YOUR OWN RISK
//
// This initializer ensures that a client will get sane default values
// for fields they know nothing about even if linked to a library version
// newer than the one they were written and/or compiled for.
//
// Returns 0 on success, -1 on failure.
// Only likely failure is an incompatible library version,
// though that should be resolved at library link/load time.
#define cr_initialize_restart_args_t(_cr_args) \
	cri_init_restart_args_t(CR_RESTART_ARGS_VERSION, (_cr_args))
/* Never call the following directly.
 * The public interface is via cr_initialize_restart_args_t()
 */
extern int cri_init_restart_args_t(cr_version_t, cr_restart_args_t *);

// cr_request_restart
// THIS FUNCTION IS EXPERIMENTAL: USE AT YOUR OWN RISK
//
// Initiate a restart from a given file descriptor.
// Use cr_wait_restart(), cr_poll_restart() or cr_poll_restart_msg() to
// check or block for completion.
//
// IN:  args     See cr_restart_args_t type for options.
// OUT: handle   Opaque output argument filled in by function call.
//
// Returns:
//   0 - restart has been requested.
// < 0 - an error occurred (check errno for details)
//
// Most likely errno values when return < 0:
// EBADF	 The source (args->cr_fd) is not a valid file descriptor. 
// EINVAL	 Some field of the args argument is invalid.
// XXX: THIS LIST IS KNOWN TO BE INCOMPLETE
// Other values may be possible.
//
// Setting CR_RSTRT_ASYNC_ERR in args->cr_flags will defer reporting of
// nearly all errors in cr_request_restart() until the cr_reap_restart()
// call.  This allows for use of cr_log_restart() to collect kernel messages
// that are associated with these error conditions.
// 
extern int cr_request_restart(cr_restart_args_t *args, cr_restart_handle_t *handle);

// cr_wait_restart
// THIS FUNCTION IS EXPERIMENTAL: USE AT YOUR OWN RISK
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_restart()
// INOUT: timeval  Time limit, or NULL for unbounded wait.
//
// Returns:
// > 0 - restart complete
//   0 - it's not done yet and the timeout (if any) has expired
// < 0 - an error occurred (see below for details)
//
// Waits until restart completes, bounded by timeval (if non-NULL).
//
// The remaining timeout is written back to timeval if, and only if,
// select() would do so for the current Linux personality.  The contents
// of timeval are undefined on error.
//
// A negative return value indicates that an error occurred in trying to
// wait for completion, and is not an indication of the status of the
// restart request.
// Most likely errno values when returning < 0:
// EINVAL	 No restart request associated with the given handle.
// EINTR	 Call was interrupted by a signal.
// EFAULT	 'timeval' argument points outside caller's address space.
// Other values may be possible.
//
extern int
cr_wait_restart(cr_restart_handle_t *handle, struct timeval *timeout);

// cr_log_restart
// THIS FUNCTION IS EXPERIMENTAL: USE AT YOUR OWN RISK
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_restart()
// IN:    len      Size of allocated space.
// OUT:   msg      Space to return kernel messages.
//
// Returns:
// > 0 - space consumed by kernel messages.
//   0 - no kernel messages were logged.
// < 0 - an error occurred (see below for details)
//
// Collects messages logged by the kernel and associated with the restart
// request.
//
// Up to 'len' bytes (including the terminating '\0' character) of kernel
// messages are copied to 'msg'.  Collecting the log messages does not destroy
// them.  So, the same messages may be retreived multiple times.
//
// Except in error cases, the return value is the number of bytes of storage
// required to retreive all the kernel messages logged for the given request,
// including the '\0' string terminator.  If the return value is larger than
// 'len', then the messages have been truncated.  In the case of 'len' equal
// to zero, 'msg' is ignored (and thus may safely be NULL) and the return
// value can be used to allocate a large enough buffer for a second call.
//
// Most likely errno values when returning < 0:
// EINVAL	 No restart request associated with the given handle.
// EFAULT	 'msg' argument points outside caller's address space.
// Other values may be possible.
//
extern int
cr_log_restart(cr_restart_handle_t *handle, unsigned int len, char *msg);

// cr_reap_restart
// THIS FUNCTION IS EXPERIMENTAL: USE AT YOUR OWN RISK
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_restart()
//
// Returns:
// > 0 - restart complete
// < 0 - an error occurred (see below for details)
//
// Retreives the status of a completed restart request and releases the
// associated resources.
//
// If this call returns positive (success) or returns negative with an errno
// value other than EAGAIN, then the restart is complete and the handle is
// invalid.  Further use of the handle w/o calling cr_request_restart() to
// obtain a new one will result in undefined behavior.
//
// A negative return value with errno == EAGAIN indicates that the
// restart request is NOT completed.  Use of cr_wait_restart() is
// recommended (but not required) to check that a request is completed
// before calling cr_reap_restart().
//
// A negative return value with any other value for errno indicates that the
// restart request was unsuccessful (was completed with errors):
// EINVAL	 No restart request associated with the given handle.
// EIO		 Problem reading the restart input.
// Other values may be possible.
//
// If cr_flags of the request included CR_RTSRT_ASYNC_ERR, then all the
// errno values listed in the description of cr_request_restart() are
// also possible results from cr_reap_restart().
//
// Note that even in the event of an error, it is possible that data has
// been left unread from the requested file descriptor.
//
extern int
cr_reap_restart(cr_restart_handle_t *handle);

// cr_poll_restart_msg
// THIS FUNCTION IS EXPERIMENTAL: USE AT YOUR OWN RISK
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_restart()
// INOUT: timeval  Time limit, or NULL for unbounded wait.
// OUT:   msg_p    Space to return pointer to kernel messages.
//
// Returns:
// > 0 - restart complete
//   0 - it's not done yet and the timeout (if any) has expired
// < 0 - an error occurred (see below for details)
//
// Waits until restart completes, bounded by timeval (if non-NULL), and
// collects kernel log messages and restart request status.
//
// This call is roughly equivalent to:
//   rc = cr_wait_restart(handle, timeval);
//   if (rc == 0) return 0; // Not done
//   if (rc < 0) return CR_POLL_RSTRT_ERR_PRE;
//   if (msg_p) {
//     int len = cr_log_restart(handle, 0, NULL);
//     char *buf = *msg_p = malloc(len);
//     rc = cr_log_restart(handle, len, buf);
//     if (rc < 0) return CR_POLL_RSTRT_ERR_LOG;
//   }
//   rc = cr_reap_restart(handle);
//   if (rc < 0) return CR_POLL_RSTRT_ERR_POST;
//   return SOME_POSITIVE_VALUE; // Done
// with some additional error checking.
//
// For non-NULL 'msg_p', the caller is responsible for calling free(*msg_p)
// when the need for the collected messages is ended.
//
// If an error is detected from the cr_wait_restart() call, then the
// negative value CR_POLL_RSTRT_ERR_PRE is returned.  See the documentation
// for cr_wait_restart() for possible errno values and their meaning.
//
// If cr_wait_restart() returns 0 to indicate the request is not done,
// then cr_poll_restart_msg() also returns 0.
//
// If an error is detected from the cr_log_restart() call, then the
// negative value CR_POLL_RSTRT_ERR_LOG is returned.  See the documentation
// for cr_log_restart() for possible errno values and their meaning.
//
// If an error is detected from the cr_reap_restart() call, then the
// negative value CR_POLL_RSTRT_ERR_POST is returned.  See the documentation
// for cr_reap_restart() for possible errno values and their meaning.
//
#define CR_POLL_RSTRT_ERR_PRE  -1
#define CR_POLL_RSTRT_ERR_POST -2
#define CR_POLL_RSTRT_ERR_LOG  -3
extern int
cr_poll_restart_msg(cr_restart_handle_t *handle, struct timeval *timeout, char **msg_p);

// cr_poll_restart
// THIS FUNCTION IS EXPERIMENTAL: USE AT YOUR OWN RISK
//
// INOUT: handle   Pointer to opaque value filled in by cr_request_restart()
// INOUT: timeval  Time limit, or NULL for unbounded wait.
//
// Calling
//   rc = cr_poll_restart(handle, timeout);
// is equivalent to
//   rc = cr_poll_restart_msg(handle, timeout, NULL);
//
extern int
cr_poll_restart(cr_restart_handle_t *handle, struct timeval *timeout);

// cr_register_callback()
//
// Register a checkpoint callback.
//
// Returns an identifier, of type cr_callback_id_t, to be used in any
// subsequent calls to cr_replace_callback().
//
// On error, the return value is -1.  The most likely errno values are:
// CR_ENOINIT	Calling thread has not called cr_init()
// ENOMEM	Insufficient free memory to complete the registration.
// ENOSPC	Given context has reached CR_MAX_CALLBACKS registrations.
// EINVAL	Invalid 'flags' argument.
// EBUSY	Called from inside a callback.
//
// The 'func' argument is the callback to invoke at checkpoint time.
//
// The supplied 'arg' argument will be passed to your callback when
// invoked.
//
// The 'flags' argument is a bit-wise OR of flags.  The flags must
// include exactly one of following to specify in which context the
// callback may be run.
//   CR_THREAD_CONTEXT:
//	This type of registration is shared among all threads in the
//	program.  Thus, the ids returned from this call have global
//	scope.  The callback will be invoked in a dedicated checkpoint
//	thread.  The callback will NEVER be invoked within signal
//	context.  When multiple thread-context callbacks are
//	registered, the order in which they run with respect to each
//	other is not specified, and it is possible that they may run
//	concurrently.  It is guaranteed, however, that all
//	thread-context callbacks will reach their cr_checkpoint()
//	calls before ANY of signal-context callbacks run.
//   CR_SIGNAL_CONTEXT:
//	This type of registration is per-thread.  Thus, the ids
//	returned from this call have only per-thread scope.  The
//	callback will be invoked in the same thread which registered
//	it.  The callback MIGHT be invoked within signal-handler
//	context.  So, the callback must be capable of running to
//	completion in signal context.  When multiple signal-context
//	callbacks are registered in a given thread, they are called in
//	the reverse of the order they were registered, as with
//	atexit().  Signal-context callbacks are not run until all
//	thread-context callbacks (if any) have called cr_checkpoint().
//
// This function is not reentrant - do not call it from signal context
// or from inside a callback.
//
extern cr_callback_id_t
cr_register_callback(cr_callback_t	func,
		     void*		arg,
		     int		flags);

// cr_replace_callback()
//
// Atomically replace one callback with another.
//
// Atomically (with respect to checkpoints and other cr_replace_*()
// calls) replaces the 'func', 'arg' and 'flags' associated with the
// given 'id'.  The new callback will be invoked in place of the
// function previously registered with this id, and in the same place
// in the ordering.
//
// There is currently no way to delete a callback id, but one can
// specify NULL for the 'func' argument to disable one.
//
// Returns 0 on success, -1 on failure.
//
// Most likely errno values when return is -1:
// CR_ENOINIT	Calling thread has not called cr_init()
// EINVAL	Invalid 'id' or 'flags' argument.
// EBUSY	Called from inside a callback.
//
// This call can not change the context associated with the 'id'.
// Therefore, the 'flags' argument must include the same one of
// CR_SIGNAL_CONTEXT or CR_THREAD_CONTEXT which was originally used to
// register this 'id'.
//
// For ids registered with CR_SIGNAL_CONTEXT, it only makes sense to
// call this function from the thread which originally registered this
// id because signal-context ids have only thread-local scope.
//
// For ids registered with CR_THREAD_CONTEXT the ids have global
// scope.  Therefore this function maybe called from any thread which
// has previously called cr_init().
//
// This function is not reentrant - do not call it from signal context
// or from inside a callback.
//
extern int
cr_replace_callback(cr_callback_id_t	id,
		    cr_callback_t	func,
		    void*		arg,
		    int			flags);

// cr_enter_cs()
//
// Enter a critical section.
//
// Critical sections allow blocks of code to run atomically with
// respect to checkpoints and the associated callbacks.
//
// If invoked within any context other than a callback this routine
// ensures that checkpoint requests arriving while execution is in the
// critical section will be deferred.  This call may block if a
// checkpoint has been requested or is in progress.  In this context
// the 'id' argument must be the identifier returned from cr_init() in
// the calling thread.
//
// If invoked from within a callback this routine returns immediately.
// This allows the same code to be run from both inside and outside of
// callbacks without causing deadlock, presumably taking the 'id' as
// an argument.  In this context the 'id' must be the value
// CR_ID_CALLBACK.
//
// Returns 0 on success, -1 on failure.
//
// Most likely errno values when return is -1:
// CR_ENOINIT	Calling thread has not called cr_init()
// EINVAL	Invalid 'id' argument.
//
// This routine is both thread-safe and reentrant (signal-safe).
//
extern int
cr_enter_cs(cr_client_id_t id);

// cr_tryenter_cs()
//
// Enter a critical section (non-blocking).
//
// This function is identical to cr_enter_cs() except that it never
// blocks (returning non-zero if cr_enter_cs() would have blocked).
//
// Returns:
//	Zero if the critical section has been entered (or when called
//	from within a callback).
//	Positive if the critical section could not be entered without
//	blocking.
//	-1 if an error occurred
//
// Most likely errno values when return is -1:
// CR_ENOINIT	Calling thread has not called cr_init()
// EINVAL	Invalid 'id' argument.
//
// A zero return must be balanced by an eventual cr_leave_cs(), while
// a non-zero return must NOT be balanced by a matching cr_leave_cs().
//
// This routine is both thread-safe and reentrant (signal-safe).
//
extern int
cr_tryenter_cs(cr_client_id_t id);

// cr_leave_cs()
//
// Leave a critical section.
//
// Leave a critical section previously entered by calling
// cs_enter_cs().  If this call leaves the only critical section
// currently active, then any pending checkpoint will be taken before
// this call returns.
//
// The 'id' argument must be the same one used in a preceding call to
// cr_enter_cs().  Calls to cr_enter_cs() and cr_leave_cs() must be
// balanced in the sense that the number of calls to cr_leave_cs()
// with a given id must never exceed the number of successful calls to
// cr_enter_cs() with the same id.  However, calls with distinct ids
// may be nested and/or overlapped arbitrarily.
//
// Returns 0 on success, -1 on failure.
//
// Most likely errno values when return is -1:
// CR_ENOINIT	Calling thread has not called cr_init()
// EINVAL	Invalid 'id' argument.
//
// This routine is both thread-safe and reentrant (signal-safe).
//
extern int
cr_leave_cs(cr_client_id_t id);

// cr_status()
//
// Query the status of the checkpoint system.
//
// If this routine is called outside of a critical section one must
// be aware that the state could change between the time cr_status()
// returns and the time the return value is used.
//
// Returns an integer:
//  CR_STATE_IDLE	No checkpoint is pending or active.
//  CR_STATE_PENDING	A checkpoint request is pending
//  CR_STATE_ACTIVE	A checkpoint is active.
//  -1			An error occurred
//
// Most likely errno values when return is -1:
// CR_ENOINIT	Calling thread has not called cr_init()
//
// This routine is both thread-safe and reentrant (signal-safe).
//
extern int
cr_status(void);

// cr_checkpoint()
//
// Proceed to next step in the checkpoint.
//
// Called inside a callback to indicate that checkpoint may proceed.
//
// Defined values for "flags" (mutually exclusive):
// CR_CHECKPOINT_READY (==0)
//      The normal case.
// CR_CHECKPOINT_OMIT
//      To continue the checkpoint, but exclude the current process.
//      Will also not run subsequent callbacks in the current group
//      (thread context, or signal context in this same thread).
// CR_CHECKPOINT_TEMP_FAILURE
// CR_CHECKPOINT_TEMP_FAILURE_CODE(error_code)
//      To abort the checkpoint and continue execution.
//      Will also not run subsequent callbacks in the current group
//      (thread context, or signal context in this same thread).
// CR_CHECKPOINT_PERM_FAILURE
// CR_CHECKPOINT_PERM_FAILURE_CODE(error_code)
//      To abort the checkpoint and kill all the processes involved in it.
//      Will also not run subsequent callbacks in the current group
//      (thread context, or signal context in this same thread).
//
// User-specified errors:
//	When the "flags" argument is CR_CHECKPOINT_TEMP_FAILURE_CODE(val)
//	or CR_CHECKPOINT_PERM_FAILURE_CODE(val), the "val" provided by
//	the caller replaces the default error codes of CR_ETEMPFAIL and
//	CR_EPERMFAIL.  If a non-zero value is specified in this manner
//	it will replace the CR_ETEMPFAIL or CR_EPERMFAIL values as:
//	  + the errno value returned by cr_poll_checkpoint()
//	  + the exit code from the cr_checkpoint utility (low 8 bits only)
//	  + the negative of the return value from cr_checkpoint()
//	Values are limited to 16-bits (1 through 65535), and a value of zero
//	will be be ignored (the default of CR_ETEMPFAIL or CR_EPERMFAIL will
//	be used instead).
//	If multiple callers of cr_checkpoint() indicate temporary or
//	permanent failure with different user-specified error values, then
//	exactly one of the provided values will be honored (but it is
//	undefined how the one is selected from the multiple callers).
//	BLCR does not provide any mechanism to distinguish how an error code
//	was generated.  Therefore, users of this facility are cautioned to
//	avoid use of values that have predefined meanings in errno.h or as
//	CR_E* error codes.  Additionally, care should be taken to consider
//	that the exit code from the cr_checkpoint utility preserves only the
//	8 least-significant bits of the error code.
//
// Returns:
//	Zero when continuing after taking a checkpoint. 
//	Positive when recovering from a checkpoint. 
//	Negative of error code if a checkpoint failed for some reason.
//	(man setjmp(3) for an analogue in POSIX.1).
//	Specific errors include:
//	-CR_ETEMPFAIL
//	    One or more callbacks (in any process in the checkpoint) passed
//	    CR_CHECKPOINT_TEMP_FAILURE to cr_checkpoint().
//	-CR_EPERMFAIL
//	    One or more callbacks (in any process in the checkpoint) passed
//	    CR_CHECKPOINT_PERM_FAILURE to cr_checkpoint().  Note that this
//	    should not be seen unless the kill has failed.
//	-CR_EOMITTED
//	    One or more callbacks in the current process passed
//	    CR_CHECKPOINT_OMIT to cr_checkpoint().
//	-CR_ENOTCB
//	    Called from other than a callback context.
//
// Example callback:
//	int callback(void* arg)
//	{
//	    int rc;
//
//	    /* do CHECKPOINT work here */
//
//	    rc = cr_checkpoint(0);
//	    if (rc < 0) {
//		/* deal with FAILURE here */
//	    } else if (rc) {
//		/* do RESTART work here */
//	    } else {
//		/* do CONTINUE work here */
//	    }
//
//	    return 0;
//	}
// 
// Note that if a callback returns without invoking cr_checkpoint(),
// then it will be invoked immediately after the return.
//
// Callbacks are expected to return zero on success.
// A callback may return nonzero to indicate a failure to CONTINUE or
// RESTART.  This is preferred over calling _exit(), but the exact
// behavior in this case is not yet defined.
//
// This routine may only be called from registered callbacks.
//
extern int
cr_checkpoint(int flags);

// cr_replace_self()
//
// Atomically replace the invoking callback with another.
//
// Atomically (with respect to checkpoints and other cr_replace_*()
// calls) replaces the 'func', 'arg' and 'flags' associated with the
// caller.
//
// There is currently no way to delete a callback id, but one can
// specify NULL for the 'func' argument to disable one.
//
// This call can not change the context associated with the callback.
// Therefore, the 'flags' argument must include the same one of
// CR_SIGNAL_CONTEXT or CR_THREAD_CONTEXT which was originally used to
// register the invoking callback.
//
// Returns 0 on success, -1 on failure.
//
// Most likely errno values when return is -1:
// CR_ENOTCB	Called from other than a callback context.
// EINVAL	Invalid 'flags' argument.
//
// This routine may only be called from registered callbacks.
//
extern int
cr_replace_self(cr_callback_t	func,
		void*		arg,
		int		flags);

// cr_get_checkpoint_info()
//
// Returns information about the active checkpoint request.
//
// The return value is a pointer to a structure in thread-local
// storage.  Therefore, reads of the structure are thread-safe
// within a running callback.  However, the values may change across
// calls to cr_checkpoint() if other callbacks run in the same thread.
// The contents of the structure are undefined when no checkpoint
// request is active.
//
// This routine may only be called from a registered callback, and not
// after calling cr_checkpoint().
//
// Returns NULL if called from other than a callback context.
extern const struct cr_checkpoint_info *
cr_get_checkpoint_info(void);

// cr_get_restart_info()
//
// Returns information about the active restart request.
//
// The return value is a pointer to a structure in thread-local
// storage.  Therefore, reads of the structure are thread-safe
// within a running callback.  However, the values may change across
// calls to cr_checkpoint() if other callbacks run in the same thread.
// The contents of the structure are undefined when no restart
// request is active.
//
// This routine may only be called from a registered callback, and not
// before calling cr_checkpoint().
//
// Returns NULL if called from other than a callback context.
extern const struct cr_restart_info *
cr_get_restart_info(void);

// cr_spinlock_init()
//
// Initialize a spinlock (to the unlocked state).
//
// This must be done before the first use.
//
// This routine is both thread-safe and reentrant (signal-safe).
//
extern void
cr_spinlock_init(cr_spinlock_t *);

// cr_spinlock_lock()
//
// Spin (busy wait) until the lock can be obtained.
//
// This routine is both thread-safe and reentrant (signal-safe).
//
extern void
cr_spinlock_lock(cr_spinlock_t *);

// cr_spinlock_unlock()
//
// Unlock a lock held by this process/thread.
//
// This routine is both thread-safe and reentrant (signal-safe).
//
extern void
cr_spinlock_unlock(cr_spinlock_t *);

// cr_spinlock_trylock()
//
// Try only once to obtain a lock.
//
// Returns non-zero if and only if the lock is obtained.
//
// This routine is both thread-safe and reentrant (signal-safe).
//
extern int
cr_spinlock_trylock(cr_spinlock_t *);

// cr_strerror()
//
// Returns a string describing the error code passed in the 'errnum' argument.
// It behaves similarly to the standard C strerror() function, except that it
// can handle BLCR-specific error codes in addition to the standard ones.
extern const char *
cr_strerror(int errnum);

// cr_register_hook()
//
// Registers a hook to be run any time a pre-defined event happens.
//
// Calling convention is modeled after signal():
// + First argument indicates for which event one is registering.
// + Second argument is the function to register.
//   The value NULL means don't run any hook, and is the default value
//   of all hooks (like SIG_DFL).
// + On success, the return value is the previous value registered.
//
// On failure, the return value is CR_HOOK_FN_ERROR.  In this case
// most likely errno values are:
// CR_ENOINIT	Calling thread has not called cr_init()
// EINVAL	Invalid 'event' or 'hook_fn' argument.
//
// The registrations persist until changed by another cr_register_hook() call.
//
// The registration is process-scoped (as opposed to thread-specific), but
// the hooks are run in all threads effected by a given event.
//
// The hook function is called with the event that has occurred, like the
// one argument passed to the handler registered by signal().
//
// Depending on the event, hooks might be invoked from signal context.
//
// See the cr_hook_event_t enum for a list of defined events.
//
// This function is thread-safe.
// This function is not reentrant - do not call it from signal context.
//
extern cr_hook_fn_t
cr_register_hook(cr_hook_event_t event, cr_hook_fn_t hook_fn); 

// cr_hold_ctrl()
// THIS FUNCTION IS EXPERIMENTAL: USE AT YOUR OWN RISK
//
// Controls post-callback hold behavior.
//
// By default, all threads of a process are "held" at a thread barrier
// following the running of all registered callbacks.  A thread which is
// held at this barrier will not resume normal execution after a checkpoint
// is written (CONTinue) or is resumed from (ReSTaRT) until all callbacks
// registered in all threads have run to completion.  However, this hold
// is optional, and a thread may be allowed to resume normal execution
// immediately after its own callbacks (if any) have completed.
//
// There are two global hold flags variables, plus each thread which has
// called cr_init() has a thread-local hold flags variable.
// Calls to cr_hold_ctrl() access one of these variables as determined by the
// 'scope' argument:
//    CR_HOLD_SCOPE_THREAD  This 'scope' value accesses the thread-local hold
//                          flags for the calling thread.
//    CR_HOLD_SCOPE_INIT    This 'scope' value accesses the hold flags for
//                          threads that *have* called cr_init(), but have
//                          a thread-local hold flags value of CR_HOLD_DFLT.
//    CR_HOLD_SCOPE_UNINIT  This 'scope' value accesses the hold flags for
//                          threads that have *not* called cr_init().
//
// The value of 'flags' may be any of the following:
//    CR_HOLD_NONE    Equal to zero.
//                    If 'flags' has this value then the effected thread(s)
//                    will not be held: they will resume normal execution
//                    as soon as their own callbacks (if any) have completed.
//    CR_HOLD_CONT    If set to this value, then the effected thread(s) will
//                    be held after a checkpoint is taken (corresponding to
//                    the CONTINUE branch of a callback) until all callbacks
//                    have run to completion.
//    CR_HOLD_RSTRT   If set to this value, then the effected thread(s) will
//                    be held after resuming from a checkpoint (corresponding
//                    to the RESTART branch of a callback) until all
//                    callbacks have run to completion.
//    CR_HOLD_BOTH    Equal to (CR_HOLD_CONT | CR_HOLD_RSTRT)
//                    The effected thread(s) are held on both the CONTINUE
//                    and RESTART paths.
//    CR_HOLD_READ    The current value of the hold flags variable selected
//                    by 'scope' is read and returned, but is not modified.
//    CR_HOLD_DFLT    This is valid only when 'scope'==CR_HOLD_SCOPE_THREAD.
//                    This value for 'flags' disables the thread-specific
//                    value, ensuring that the CR_HOLD_SCOPE_INIT hold flags
//                    variable will be used for the calling thread.
//
// Default values before any calls to cr_hold_ctrl() are:
//    For CR_HOLD_SCOPE_THREAD: CR_HOLD_DFLT (defers to CR_HOLD_SCOPE_INIT).
//    For CR_HOLD_SCOPE_INIT:   CR_HOLD_BOTH
//    For CR_HOLD_SCOPE_UNINIT: CR_HOLD_BOTH
//
// Returns previous value of selected hold flags on success, -1 on failure.
// Most likely errno values when returning -1:
// CR_ENOINIT	Calling thread has not called cr_init()
// EINVAL	Invalid 'scope' or 'flags' argument.
//
// This function maybe called from any thread which has previously called
// cr_init() (this includes any thread created by libcr to run callback in
// thread context).
// 
// If invoking cr_hold_ctrl() from a callback, please NOTE:
// + Calls made before the call to cr_checkpoint() and/or with a 'scope' of
//   CR_HOLD_SCOPE_THREAD are safe, and any value set will be used beginning
//   with the barrier following the active checkpoint or restart.
// + The remaining case (non-THREAD 'scope' after calling cr_checkpoint()),
//   is legal, but discouraged.  Unless some coordination among threads is
//   implemented outside of libcr, there is no guarantee in such a case
//   as to which threads will observe the old or new value.
//
// This routine is both thread-safe and reentrant (signal-safe).
//
// NOTE:
// The default hold behavior in the BLCR 0.7.X series differs from that in
// the 0.6.X series.  In 0.6.X threads that had not called cr_init() were
// not held at the thread barrier.  In 0.7.X a call to
//   cr_hold_ctrl(CR_HOLD_SCOPE_UNINIT, CR_HOLD_NONE);
// will restore the 0.6.X behavior for those applications that require it.
//
extern int
cr_hold_ctrl(int scope, int flags);

// cr_{inc,dec}_persist()
// THESE FUNCTIONS ARE EXPERIMENTAL: USE AT YOUR OWN RISK
//
// Increment/decrement TLD use count
//
// Calls to cr_init() create thread-local data (TLD).  Normally this TLD
// is freed automatically when a pthread exits or is canceled.  However,
// the POSIX threads specification leaves undefined the order in which
// TLD destructors are run.  This creates the possibility that a libcr
// function which requires a cr_init() call as a prerequisite could fail
// with errno=CR_ENOINIT if invoked from a client-provided TLD destructor.
//
// This pair of functions allow a client to increment and decrement a
// use count associated with the calling thread's libcr TLD.  This count
// begins at zero and the TLD can only be destroyed when the value is zero.
//
// Typical use of these function include a call to cr_inc_persist() before
// a client thread's first call to pthread_setspecific() for any key that
// has a destructor that invokes libcr functions (perhaps indirectly).
// This call should be made once per-key, per-thread - it should not be
// called for each pthread_setspecific().  Once the call has been made to
// increment the use count, the TLD will not be destroyed until after an
// equal number of calls to cr_dec_persist() in the same thread return the
// count to zero.  Therefore, the recommended use is to call cr_dec_persist()
// in the TLD destructor which invokes libcr functions.  A call to
// cr_dec_persist() which returns the use count to zero only makes the TLD
// eligible for destruction at thread exit, but will not immediately destroy
// it.  Therefore, one may call cr_dec_persist() at any time the need for
// TLD persistence ends.
//
// Note that failure to balance cr_inc_persist() calls from one thread with
// an equal number of cr_dec_persist() calls from the same thread will result
// in a memory leak of approximately 5KB per exited thread.
//
// Returns previous value of the use count on success, -1 on failure.
// On failure the use count is unchanged.
//
// Most likely errno values when returning -1:
// CR_ENOINIT	Calling thread has not called cr_init()
// ERANGE	The requested inc/dec would cause over/underflow
//
// These functions are not reentrant - do not call them from signal context.
//
extern int
cr_inc_persist(void);
extern int
cr_dec_persist(void);


// PRIVATE interfaces:
//
// The following are for internal use only and should never be called
// directly.  Use a documented public interface instead.

extern int  // The public interface is cr_initialize_checkpoint_args_t()
cri_init_checkpoint_args_t(cr_version_t, cr_checkpoint_args_t *);

// LEGACY interfaces:
//
// The following are DEPRECATED and therefore the corresponding documentation
// is not present.  You may consult the implementation of these functions for
// clues on how to replace them in your own code.

extern void cr_request(void) _CR_DEPRECATED;
extern void cr_request_file(const char *filename) _CR_DEPRECATED;
extern void cr_request_fd(int fd) _CR_DEPRECATED;

__END_DECLS

#endif
