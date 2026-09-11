/* template file: rcd_scp.tmp.h

	This file is part of the rcode project.

	Copyright (C) 2019-2025 Tomasz Pawlak,
	e-mail: tomasz.pawlak@wp.eu

	rcd_scp.tmp.h v4.0

	License: GNU Lesser General Public License version 3 (LGPLv3+)

	The rcd_scp.tmp.h is free software; you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 3 of the License, or (at your
	option) any later version.

	The rcd_scp.tmp.h is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
	License for more details.

	Full text of the GNU Lesser General Public License is available at:
	www.gnu.org/licenses
*/

#ifndef %rcd_bname%_RCD_SCP_H
#define %rcd_bname%_RCD_SCP_H 1

#include "%path%rcode.h"

#define %rcd_bname%_RCD_AUTOGEN_MODE %rcdg_mode%

// 'rcd_lib' is resolved to '#define <base_name>_RCD_LINK_LIB <libname>'
// if RCDGEN_LNK_LIB env. var is set or --link-lib|-lib opion is used,
// otherwise it is replaced with single space.
// If <base_name>_RCD_LINK_LIB is defined, then target library header include
// path also must be provided, to substitute the 'lib_hdr' keyword:
// RCDGEN_LIB_HDR env. var must be set or --lib-hdr|-hdr opion can be used.

%rcd_lib%

#ifdef %rcd_bname%_RCD_LINK_LIB
	#define m_rcd_concat(_pfx, _fn) _pfx ## _fn
	#define m_rcd_libfn(_pfx, _fn) m_rcd_concat(_pfx, _fn)

	#include %lib_hdr%

	#if %rcd_bname%_RCD_AUTOGEN_MODE != RCD_MODE_FULL
		#error "RCD_AUTOGEN: link-lib option requires RCD_MODE_FULL"
	#endif

	#ifndef %link_lib_md%
		#error "RCD_AUTOGEN: link-lib option requires '%link_lib_md%' to be defined"
	#elif %link_lib_md% != RCD_MODE_FULL
		#error "RCD_AUTOGEN:link-lib option requires %link_lib_md% == RCD_MODE_FULL"
	#endif
#endif

#include <sys/cdefs.h>
__BEGIN_DECLS

/* Besides returning the scope pointer, each call to this function
	updates the rcd_scope_t.vmsg pointer.
	The vmsg struct is stored in TLS, so its pointer can't be initialized statically.
	The alternative would be to store all the scope data in TLS, but this becomes
	problematic if the rcd_msg_t message array is large. */
struct rcd_scope*
%rcd_bname%_rcdGetScopePtr() __attribute__((pure));

/* return stack size as number of rcode slots
*/
uint32_t
%rcd_bname%_rcdGetStackSize() __attribute__((const));

/* Set sub-scope scope.
	This fn must be called for each thread context.
*/
void
%rcd_bname%_rcdSetSubScopePtr(struct rcd_scope* scp);

/* Reset rcode stacks (including linked sub-scope stacks)
*/
void
%rcd_bname%_rcdResetStack();

/* returns last rcode from stack or RCD_OK if stack is empty
*/
rcode
%rcd_bname%_rcdGetStatus();

/* Push rcode on scope stack.
*/
void
%rcd_bname%_rcdPushRcode(rcode retU);

#ifndef %rcd_bname%_RCD_LINK_LIB
/* xxx_RCD_LINK_LIB undefined: built-in functions
*/

/* Assembles null-terminated string with all rcodes and messages stored
	on scope stack - the string is stored in *buff.
	Returns length of the string, incl. null byte, -1 for error.
	The function uses xxx_rcdGetMsg() internally, which is resolved to
	RCD_PRINT_BUF() in basic and dummy modes.
	if scp == NULL, use *this* scope ptr. */
int
%rcd_bname%_rcdGetCallStack(struct rcd_scope* scp, char* buf, int bsz)
__nonnull((2));

/* Assembles null-terminated rcode description in the *buff,
	returns length of the string, excluding null byte, -1 for error.
	Resolved to RCD_PRINT_BUF() in basic and dummy modes.
	if scp == NULL, use *this* scope ptr. */
int
%rcd_bname%_rcdGetMsg(struct rcd_scope* scp, rcode retU, char* buf, int bsz)
__nonnull((3));

#else
/*
	xxx_RCD_LINK_LIB defined: use functions already implemented in linked library.
	NOTE: NULL scope arg refers to linked lib scope, see xxx_RCD_GET_CALL_STACK()
	and xxx_RCD_GET_MSG()
*/

__always_inline __nonnull()
int
%rcd_bname%_rcdGetCallStack(struct rcd_scope* scp, char* buf, int bsz) {
	return m_rcd_libfn(%rcd_bname%_RCD_LINK_LIB, _rcdGetCallStack) (scp, buf, bsz);
}

__always_inline __nonnull()
int
%rcd_bname%_rcdGetMsg(struct rcd_scope* scp, rcode retU, char* buf, int bsz) {
	return m_rcd_libfn(%rcd_bname%_RCD_LINK_LIB, _rcdGetMsg) (scp, retU, buf, bsz);
}

#endif /* _RCD_LINK_LIB */

/* if scp == NULL, then *this* scope ptr is used.
*/
int
%rcd_bname%_rcdGetMinMsgBufSz(struct rcd_scope* scp);


/* general-purpose fn, specialized macros are defined below */
void
%rcd_bname%_rcdSetMsg(rcode, const char* fmt, ... )
__attribute__(( format(printf, 2, 3) ));


/* push fault + return */
#define %rcd_bname%_RCD_PUSH_RETURN(_rcd) \
	{ \
		%rcd_bname%_rcdPushRcode(_rcd); \
		return (_rcd); \
	}

/* set & push fault */
#define %rcd_bname%_RCD_SET_PUSH_FAULT(_rcd) \
	{ \
		RCD_SET_FAULT(_rcd); \
		%rcd_bname%_rcdPushRcode(_rcd); \
	}

/* set & push fault + return */
#define %rcd_bname%_RCD_SET_PUSH_FAULT_RETURN(_rcd) \
	{ %rcd_bname%_RCD_SET_PUSH_FAULT(_rcd); return (_rcd); }


/* set & push fault MSG */
#define %rcd_bname%_RCD_SET_PUSH_FAULT_MSG(_rcd, _msg) \
	{ \
		RCD_SET_FAULT_MSG(_rcd, _msg); \
		%rcd_bname%_rcdPushRcode(_rcd); \
	}

/* set & push fault MSG + return */
#define %rcd_bname%_RCD_SET_PUSH_FAULT_MSG_RETURN(_rcd, _msg) \
	{ %rcd_bname%_RCD_SET_PUSH_FAULT_MSG(_rcd, _msg); return (_rcd); }

/* set fault VMSG (volatile message)
	NOTE: changing the rcode before returning invalidates the message. */
#define %rcd_bname%_RCD_SET_FAULT_VMSG( _rcd, _fmt, ... ) \
	{ \
		_rcd = RCD_SET_VAL( RCD_UNIT, __LINE__, RCD_FVMSG ); \
		%rcd_bname%_rcdSetMsg(_rcd, _fmt, ##__VA_ARGS__); \
	}

/* set & push fault VMSG */
#define %rcd_bname%_RCD_SET_PUSH_FAULT_VMSG(_rcd, _fmt, ...) \
	{ \
		%rcd_bname%_RCD_SET_FAULT_VMSG(_rcd, _fmt, ##__VA_ARGS__) \
		%rcd_bname%_rcdPushRcode(_rcd); \
	}

/* set fault VMSG + return */
#define %rcd_bname%_RCD_RETURN_FAULT_VMSG( _rcd, _fmt, ... ) \
	{ \
		%rcd_bname%_RCD_SET_FAULT_VMSG(_rcd, _fmt, ##__VA_ARGS__) \
		return (_rcd); \
	}

/* set & push fault VMSG + return */
#define %rcd_bname%_RCD_SET_PUSH_FAULT_VMSG_RETURN( _rcd, _fmt, ... ) \
	{ \
		%rcd_bname%_RCD_SET_PUSH_FAULT_VMSG(_rcd, _fmt, ##__VA_ARGS__); \
		return (_rcd); \
	}

/* get message associated with rcode */
#define %rcd_bname%_RCD_GET_MSG(_rcd, _buf, _bsz) \
	%rcd_bname%_rcdGetMsg(%rcd_bname%_rcdGetScopePtr(), (_rcd), (_buf), (_bsz))

/* get Call Stack for *this* scope */
#define %rcd_bname%_RCD_GET_CALL_STACK(_buf, _bsz) \
	%rcd_bname%_rcdGetCallStack( %rcd_bname%_rcdGetScopePtr(), (_buf), (_bsz) )


__END_DECLS

#endif /* %rcd_bname%_RCD_SCP_H */
