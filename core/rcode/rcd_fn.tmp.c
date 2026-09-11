/* rcode template file: rcd_fn.tmp.c */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "%rcd_bname%_rcd_scope.h"

#include <sys/cdefs.h>
__BEGIN_DECLS

// rcd_autogen: full mode (2)

//no PLT calls: include rcd_alias.h
#ifdef __cplusplus
	#define __RCD_NTH ,nothrow
#else
	#define __RCD_NTH
#endif

#define __RCD_ALIAS(__fn, __alias) \
	extern __typeof(__fn) __alias \
	__attribute__((alias (#__fn), visibility ("hidden") __RCD_NTH ))

#define __RCD_ALIAS_PURE(__fn, __alias) \
	extern __typeof(__fn) __alias \
	__attribute__((alias (#__fn), visibility ("hidden"), pure __RCD_NTH ))

#define __RCD_ALIAS_CONST(__fn, __alias) \
	extern __typeof(__fn) __alias \
	__attribute__((alias (#__fn), visibility ("hidden"), const __RCD_NTH ))

__RCD_ALIAS_PURE (%rcd_bname%_rcdGetScopePtr , _rcdGetScopePtr_ );
__RCD_ALIAS_CONST(%rcd_bname%_rcdGetStackSize, _rcdGetStackSize_);

__RCD_ALIAS(%rcd_bname%_rcdSetMsg        , _rcdSetMsg_);
__RCD_ALIAS(%rcd_bname%_rcdSetSubScopePtr, _rcdSetSubScopePtr_);
__RCD_ALIAS(%rcd_bname%_rcdPushRcode     , _rcdPushRcode_);
__RCD_ALIAS(%rcd_bname%_rcdResetStack    , _rcdResetStack_);
__RCD_ALIAS(%rcd_bname%_rcdGetStatus     , _rcdGetStatus_);
__RCD_ALIAS(%rcd_bname%_rcdGetMinMsgBufSz, _rcdGetMinMsgBufSz_);

#ifndef %rcd_bname%_RCD_LINK_LIB

__RCD_ALIAS(%rcd_bname%_rcdGetCallStack    , _rcdGetCallStack_);
__RCD_ALIAS(%rcd_bname%_rcdGetMsg          , _rcdGetMsg_);

#define offsetof(type, member) __builtin_offsetof (type, member)

#if %rcd_bname%_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
static const void*
__rcd_bin_search(const char* arr, int32_t arsz, uint32_t itemsz, uint32_t voffs, uint32_t val) {
	const char *pitem;
	uint32_t mval;
	int32_t  idxL;
	int32_t  idxH;
	int32_t  idx;
	int32_t  idiff;

	idxL = 0;
	idxH = (arsz -1);
	arr += voffs; //jumping directly between target fields
loop:
	if (idxL > idxH) goto not_found;
	idiff = (idxH - idxL);
	idx   = (idiff >> 1);
	idx  += idxL;
	pitem = arr + (idx * itemsz);
	mval  = *((uint16_t*) pitem);
	if (mval < val) {
		idxL  = idx;
		idxL ++ ;
		goto loop;
	}
	if (mval > val) {
		idxH  = idx;
		idxH -- ;
		goto loop;
	}
	pitem -= voffs;
	return pitem;
not_found:
	return NULL;
}

#ifndef _CUSTOM_MSG_HDL
static char*
__rcd_cnv_s(char* buf, uint32_t val) {
// internal fn: args are checked by the caller

	char   cbuf[16];
	char  *pcbuf = cbuf + sizeof(cbuf);

	do {
		uint32_t utmp = val;
		val   /= 10;
		utmp  -= (val * 10);
		pcbuf -- ;
		*pcbuf = ((char) utmp + '0');
	} while (val != 0);

	/* copy buffer */
	while (pcbuf < (cbuf + sizeof(cbuf)) ) {
		*buf++ = *pcbuf++;
	}
	return buf;
}

static char*
__rcd_assemble_msg(char* buf, int32_t bsz, rcode retU,
										const rcd_scope_t *const rscp,
										const rcd_unit_t *p_un,
										const char* msg, uint32_t mlen) {
//min. buff size checked by the caller
//format: "%s[%u]: %s%s.%u [%d] %s": +7 to min_buf size-> autogen::F_APPEND_SORT_UNITS()
	char     *pend;
	uint32_t  len;

	pend  = buf;
	pend += bsz;
	len = rscp->hdr.bname_slen;
	memcpy(buf, rscp->hdr.base_name, (size_t) len); //base name
	buf += len;
	*buf = '['; buf ++ ;
	buf  = __rcd_cnv_s(buf, RCD_GET_UNIT(retU)); //unit number
	*buf = ']'; buf ++ ;
	*buf = ':'; buf ++ ;
	*buf = ' '; buf ++ ;
	len  = p_un->dir_slen;
	memcpy(buf, p_un->un_dir, (size_t) len); //unit dir
	buf += len;
	len  = p_un->file_slen;
	memcpy(buf, p_un->un_file, (size_t) len); //file name
	buf += len;
	*buf = '.'; buf ++ ;
	buf  = __rcd_cnv_s(buf, RCD_GET_LINE(retU));
	*buf = ' '; buf ++ ;
	*buf = '['; buf ++ ;
	{ //rcode
		int rcd;
		rcd = RCD_GET_RCODE(retU); // -2 .. +1
		if (rcd < 0) {
			*buf = '-'; buf ++ ;
			rcd  = -rcd;
		}
		rcd += '0';
		*buf = rcd; buf ++ ;
	}
	*buf = ']'; buf ++ ;
	*buf = ' '; buf ++ ;
	if (NULL == msg) goto end;
	len  = (pend - buf);
	if (mlen > len) mlen = len;
	memcpy(buf, msg, (size_t) mlen); //message
	buf += mlen;

end:
	*buf = 0;
	return buf;
}
#endif //_CUSTOM_MSG_HDL

int
%rcd_bname%_rcdGetMsg(struct rcd_scope* scp, rcode retU, char* buf, int bsz) {
	const rcd_unit_t  *p_un;
	const char        *msg;
	int32_t            itmp;
	const rcd_scope_t *rscp;

	if (NULL == scp) {
		scp = (struct rcd_scope*) &__tls_scope;
		__tls_scope.scope = &%rcd_bname%_scope;
	}
	rscp = ((tls_rcd_scope_t*) scp)->scope;

	if (0 == RCD_GET_UNIT(retU)) goto fallback; //rcode.detail.unit == 0 -> special case, reserved value.
	if (bsz < rscp->hdr.min_bufsz) goto fallback; //at least rscp->min_bufsz, +RCD_VMSG_MAX_SZ for formatted msg.

	itmp = rscp->hdr.unit_cnt;
	p_un = (rcd_unit_t*) &rscp->unit_ar[0];
//search units
	p_un = (rcd_unit_t*) __rcd_bin_search((char*) p_un, itmp,
												sizeof(rcd_unit_t),
												offsetof(rcd_unit_t, un_id),
												RCD_GET_UNIT(retU) );
	if ( NULL == p_un ) goto fallback; //unit not found
//get message
	msg = NULL;
	if ( RCD_GET_RCODE(retU) != RCD_FVMSG ) {
		rcd_msg_t *pmsg;
//search messages
		itmp = p_un->msg_cnt;
		pmsg = (rcd_msg_t*) &p_un->msg_ar[0];
		if ( NULL == pmsg ) goto no_msg; //no msg in the unit

		pmsg = (rcd_msg_t*) __rcd_bin_search((char*) pmsg, itmp,
													sizeof(rcd_msg_t),
													offsetof(rcd_msg_t, lnum),
													RCD_GET_LINE(retU) );
		if ( NULL == pmsg ) goto no_msg; //no msg for a given line, itmp=msg_len has no meaning in such case.
		msg  = pmsg->msg;
		itmp = pmsg->msg_len;

	} else { //RCD_FVMSG
		#if %rcd_bname%_RCD_AUTOGEN_MODE == RCD_MODE_FULL
		const rcd_vmsg_t *vmsg;
		vmsg = &((tls_rcd_scope_t*) scp)->vmsg;
//check if vmsg matches *this* rcode
		if (retU.value == vmsg->retU.value) {
			msg  = vmsg->msg_buf;
			itmp = vmsg->msg_len;
		}
		#else
		goto no_msg;
		#endif
	}

no_msg:
	{
		#ifndef _CUSTOM_MSG_HDL
		char *pend;
		pend = __rcd_assemble_msg( buf, bsz, retU, rscp, p_un, msg, itmp);
		itmp = (pend - buf);
		#else
		itmp = _CUSTOM_MSG_HDL (retU, &rscp->hdr, p_un, msg, itmp, buf, bsz);
		#endif

		return itmp;
	}
fallback:
	#ifndef _CUSTOM_MSG_HDL
	itmp = RCD_PRINT_BUF(retU, buf, bsz);
	#else
	itmp = _CUSTOM_MSG_HDL (retU, NULL, NULL, NULL, 0, buf, bsz);
	#endif
	return itmp;
}
#else //_RCD_AUTOGEN_MODE == RCD_MODE_DUMMY
#pragma GCC diagnostic ignored "-Wunused-parameter"

int
%rcd_bname%_rcdGetMsg(struct rcd_scope *scp, rcode rcd, char *buf, int bsz) {
	return RCD_PRINT_BUF(rcd, buf, bsz);
}
#pragma GCC diagnostic warning "-Wunused-parameter"
#endif //_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC

#endif //_RCD_LINK_LIB

#if %rcd_bname%_RCD_AUTOGEN_MODE == RCD_MODE_FULL
void
%rcd_bname%_rcdSetMsg(rcode rcd, const char* fmt, ... ) {
	rcd_vmsg_t *vmsg;
	int         len;
	va_list     argp;
//*this* unit
	vmsg = &__tls_scope.vmsg;

	va_start(argp, fmt);

	len = vsnprintf(vmsg->msg_buf, RCD_VMSG_MAX_SZ, fmt, argp );

	va_end(argp);

	vmsg->msg_len = len;
	vmsg->retU    = rcd;
}
#else
#pragma GCC diagnostic ignored "-Wunused-parameter"
void
%rcd_bname%_rcdSetMsg(rcode rcd, const char *fmt, ... ) {
	return;
}
#pragma GCC diagnostic warning "-Wunused-parameter"
#endif

__END_DECLS

