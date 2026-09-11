/* template file: rcd_alias.h

	This file is part of the rcode project.

	Copyright (C) 2019-2025 Tomasz Pawlak,
	e-mail: tomasz.pawlak@wp.eu

	rcd_alias.h v4.0

	License: GNU Lesser General Public License version 3 (LGPLv3+)

	The rcd_alias.h is free software; you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 3 of the License, or (at your
	option) any later version.

	The rcd_alias.h is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
	License for more details.

	Full text of the GNU Lesser General Public License is available at:
	www.gnu.org/licenses
*/

#ifndef RCD_ALIAS_H
#define RCD_ALIAS_H 1

#include "rcode.h"

#include <sys/cdefs.h>
__BEGIN_DECLS

/* local hidden aliases of public functions, useful for shared objects
	to avoid PLT calls */

struct rcd_scope*
_rcdGetScopePtr_() __attribute__((pure));

uint32_t
_rcdGetStackSize_() __attribute__((const));

void
_rcdSetSubScopePtr_(struct rcd_scope* scp);

void
_rcdPushRcode_(rcode retU);

int
_rcdGetCallStack_(struct rcd_scope* scp, char* buf, int bsz)
__nonnull((2));

void
_rcdResetStack_();

rcode
_rcdGetStatus_();

int
_rcdGetMsg_(struct rcd_scope* scp, rcode retU, char* buf, int bsz)
__nonnull((3));

void
_rcdSetMsg_(rcode, const char* fmt, ... ) __attribute__(( format(printf, 2, 3) ));

int
_rcdGetMinMsgBufSz_(struct rcd_scope* scp);

/* alternative macros which are referencing aliases */

/* push fault + return */
#define RCD_PUSH_RETURN(_rcd) \
	{ \
		_rcdPushRcode_(_rcd); \
		return (_rcd); \
	}

/* set & push fault */
#define RCD_SET_PUSH_FAULT(_rcd) \
	{ \
		RCD_SET_FAULT(_rcd); \
		_rcdPushRcode_(_rcd); \
	}

/* set & push fault + return */
#define RCD_SET_PUSH_FAULT_RETURN(_rcd) \
	{ RCD_SET_PUSH_FAULT(_rcd); return (_rcd); }


/* set & push fault MSG */
#define RCD_SET_PUSH_FAULT_MSG(_rcd, _msg) \
	{ \
		RCD_SET_FAULT_MSG(_rcd, _msg); \
		_rcdPushRcode_(_rcd); \
	}

/* set & push fault MSG + return */
#define RCD_SET_PUSH_FAULT_MSG_RETURN(_rcd, _msg) \
	{ RCD_SET_PUSH_FAULT_MSG(_rcd, _msg); return (_rcd); }

/* set fault VMSG (volatile message) */
#define RCD_SET_FAULT_VMSG( _rcd, _fmt, ... ) \
	{ \
		_rcd = RCD_SET_VAL( RCD_UNIT, __LINE__, RCD_FVMSG ); \
		_rcdSetMsg_(_rcd, _fmt, ##__VA_ARGS__); \
	}

/* set & push fault VMSG */
#define RCD_SET_PUSH_FAULT_VMSG(_rcd, _fmt, ...) \
	{ \
		RCD_SET_FAULT_VMSG(_rcd, _fmt, ##__VA_ARGS__) \
		_rcdPushRcode_(_rcd); \
	}

/* set fault VMSG + return */
#define RCD_RETURN_FAULT_VMSG( _rcd, _fmt, ... ) \
	{ \
		RCD_SET_FAULT_VMSG(_rcd, _fmt, ##__VA_ARGS__) \
		return (_rcd); \
	}

/* set & push VMSG + return */
#define RCD_SET_PUSH_FAULT_VMSG_RETURN( _rcd, _fmt, ... ) \
	{ RCD_SET_PUSH_FAULT_VMSG(_rcd, _fmt, ##__VA_ARGS__); return (_rcd); }

/* get local msg */
#define RCD_GET_MSG(_rcd, _buf, _bsz) \
	_rcdGetMsg_(NULL, (_rcd), (_buf), (_bsz))

__END_DECLS

#endif /* RCD_ALIAS_H */
