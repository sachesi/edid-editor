
/* File generated with rcd_autogen v4.0 mode=2 (full) */

#include <sys/cdefs.h>
#include "rcode/rcode_scp.h"

__BEGIN_DECLS

static const char __base_name[]="wxedid";

static const rcd_msg_t msg___EDID_base_cpp[] = {
	{"field_count == 0", 16, 1433}
};

static const rcd_msg_t msg___EDID_main_cpp[] = {
	{"[E!] Invalid number of extension blocks", 39, 864},
	{"[E!] Assebling FAILED, internal error", 37, 928}
};

static const rcd_msg_t msg___CEA_cpp[] = {
	{"[E!] SAD: invalid Audio Format Code (AFC)", 41, 692},
	{"[E!] SAD: invalid Audio Coding Extension Type Code (ACE)", 56, 726},
	{"[E!] SAD: invalid Audio Coding Extension Type Code (ACE)", 56, 758},
	{"[E!] SAD: invalid Audio Coding Extension Type Code (ACE)", 56, 823}
};

typedef struct rcd_scope_s { //internal
	rcd_scphdr_t hdr;
	rcd_unit_t unit_ar[7];
} rcd_scope_t;

static rcd_scope_t wxedid_scope = {
	.hdr = {
		.base_name = __base_name,
		.bname_slen = 6,
		.unit_cnt = 7,
		.min_bufsz = 95,
		.rcdgen_md = 2,
		.reserve = 0
	},
	.unit_ar = { //units
		{msg___EDID_main_cpp, "./", "EDID_main.cpp", 2, 13, 2, 2},
		{msg___EDID_base_cpp, "./", "EDID_base.cpp", 2, 13, 3, 1},
		{NULL, "./", "EDID_dsc.cpp", 2, 12, 4, 0},
		{msg___CEA_cpp, "./", "CEA.cpp", 2, 7, 5, 4},
		{NULL, "./", "CEA_ET.cpp", 2, 10, 6, 0},
		{NULL, "./", "grpar.cpp", 2, 9, 7, 0},
		{NULL, "./", "vmap.cpp", 2, 8, 8, 0}
	}
};

__END_DECLS

/* rcode template file: rcd_scp.tmp.c */
#include "wxedid_rcd_scope.h"
#include <sys/cdefs.h>
__BEGIN_DECLS
#ifndef wxedid_RCD_LINK_LIB
int
_rcdGetMsg_(struct rcd_scope* scp, rcode retU, char* buf, int bsz)
__nonnull((3));
#endif
#define RCD_STACK_DEPTH 32
#if wxedid_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
typedef struct __tls_rcd_scope_s tls_rcd_scope_t;
struct __tls_rcd_scope_s {
	const rcd_scope_t  *scope;
	#if wxedid_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	tls_rcd_scope_t    *cldscp;
	uint32_t            spos;
	
	
	rcd_vmsg_t          vmsg;
	
	rcode               stk[RCD_STACK_DEPTH];
	#endif
};
static __thread tls_rcd_scope_t __tls_scope;
#endif
struct rcd_scope*
wxedid_rcdGetScopePtr() {
	#if wxedid_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
	__tls_scope.scope = &wxedid_scope;
	return (struct rcd_scope*) &__tls_scope;
	#else
	return NULL;
	#endif
}
uint32_t
wxedid_rcdGetStackSize() {
	return RCD_STACK_DEPTH;
}
#pragma GCC diagnostic ignored "-Wunused-parameter"
void
wxedid_rcdSetSubScopePtr(struct rcd_scope* scp) {
	#if wxedid_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
	__tls_scope.scope  = &wxedid_scope;
	#if wxedid_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	__tls_scope.cldscp = (tls_rcd_scope_t*) scp;
	#endif
	#endif
}
#pragma GCC diagnostic warning "-Wunused-parameter"
void
wxedid_rcdResetStack() {
	#if wxedid_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	tls_rcd_scope_t *tscp = &__tls_scope;
	do {
		
		if (0 == tscp->spos) break;
		tscp->spos  = 0;
		tscp        = __tls_scope.cldscp;
	} while (tscp != NULL);
	#endif
}
rcode
wxedid_rcdGetStatus() {
	#if wxedid_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	rcode   retU;
	int32_t eidx;
	tls_rcd_scope_t *tscp;
	tscp = &__tls_scope;
	if (0 == tscp->spos) {
		retU = RCD_SET_VAL(RCD_UNIT_MAX, RCD_LINE_MAX, RCD_OK);
		return retU;
	}
	eidx = (tscp->spos -1);
	retU = tscp->stk[eidx];
	return retU;
	#else
	return (rcode) 0;
	#endif
}
#pragma GCC diagnostic ignored "-Wunused-parameter"
void
wxedid_rcdPushRcode(rcode retU) {
	#if wxedid_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	uint32_t  spos = __tls_scope.spos;
	if (spos >= RCD_STACK_DEPTH) spos = (RCD_STACK_DEPTH-1);
	__tls_scope.stk[spos] = retU;
	spos ++ ;
	__tls_scope.spos = spos;
	#endif
}
#pragma GCC diagnostic warning "-Wunused-parameter"
#ifndef wxedid_RCD_LINK_LIB
#if wxedid_RCD_AUTOGEN_MODE == RCD_MODE_FULL
#define STACK_CHAIN_MAX 32
int
wxedid_rcdGetCallStack(struct rcd_scope* scp, char* buf, int bsz) {
	tls_rcd_scope_t *tscp;
	rcode    retU;
	int32_t  eidx;
	int32_t  m_sz;
	int32_t  m_len;
	uint32_t n_lnk;
	if (NULL == scp) {
		scp = (struct rcd_scope*) &__tls_scope;
		__tls_scope.scope = &wxedid_scope;
	}
	tscp  = (tls_rcd_scope_t*) scp;
	m_len = 0;
	n_lnk = STACK_CHAIN_MAX;
_cld_scope:
	eidx  = (tscp->spos -1);
	for (; eidx>=0; --eidx) {
		retU   = tscp->stk[eidx];
		m_sz   = _rcdGetMsg_((struct rcd_scope*) tscp, retU, buf, bsz);
		if (m_sz < 0) return m_sz;
		m_len += m_sz;
		if (bsz > m_sz) {
			buf[m_sz] = '\n';
			m_sz += 1;
			bsz  -= m_sz;
			buf  += m_sz;
		} else {
			return m_len;
		}
	}
	n_lnk -- ;
	if (n_lnk == 0) goto _exit;
	tscp = tscp->cldscp;
	if (tscp != NULL) goto _cld_scope;
_exit:
	if (m_len > 0) {
		buf -- ;
	}
	buf[0] = 0;
	return m_len;
}
#else
#pragma GCC diagnostic ignored "-Wunused-parameter"
int
wxedid_rcdGetCallStack(struct rcd_scope* scp, char* buf, int bsz) {
	return 0;
}
#endif
#endif
#if wxedid_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
int
wxedid_rcdGetMinMsgBufSz(struct rcd_scope *scp) {
	int bsz;
	tls_rcd_scope_t *p_scp = (tls_rcd_scope_t*) scp;
	__tls_scope.scope = &wxedid_scope;
	if (NULL == p_scp) {p_scp = &__tls_scope; }
	bsz = ( p_scp->scope->hdr.rcdgen_md > RCD_MODE_BASIC) ? RCD_VMSG_MAX_SZ : p_scp->scope->hdr.min_bufsz;
	return bsz;
}
#else
int
wxedid_rcdGetMinMsgBufSz(struct rcd_scope* scp) {
	return 0;
}
#endif
#pragma GCC diagnostic warning "-Wunused-parameter"
__END_DECLS
/* rcode template file: rcd_fn.tmp.c */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "wxedid_rcd_scope.h"
#include <sys/cdefs.h>
__BEGIN_DECLS
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
__RCD_ALIAS_PURE (wxedid_rcdGetScopePtr , _rcdGetScopePtr_ );
__RCD_ALIAS_CONST(wxedid_rcdGetStackSize, _rcdGetStackSize_);
__RCD_ALIAS(wxedid_rcdSetMsg        , _rcdSetMsg_);
__RCD_ALIAS(wxedid_rcdSetSubScopePtr, _rcdSetSubScopePtr_);
__RCD_ALIAS(wxedid_rcdPushRcode     , _rcdPushRcode_);
__RCD_ALIAS(wxedid_rcdResetStack    , _rcdResetStack_);
__RCD_ALIAS(wxedid_rcdGetStatus     , _rcdGetStatus_);
__RCD_ALIAS(wxedid_rcdGetMinMsgBufSz, _rcdGetMinMsgBufSz_);
#ifndef wxedid_RCD_LINK_LIB
__RCD_ALIAS(wxedid_rcdGetCallStack    , _rcdGetCallStack_);
__RCD_ALIAS(wxedid_rcdGetMsg          , _rcdGetMsg_);
#define offsetof(type, member) __builtin_offsetof (type, member)
#if wxedid_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
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
	arr += voffs;
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
	char     *pend;
	uint32_t  len;
	pend  = buf;
	pend += bsz;
	len = rscp->hdr.bname_slen;
	memcpy(buf, rscp->hdr.base_name, (size_t) len);
	buf += len;
	*buf = '['; buf ++ ;
	buf  = __rcd_cnv_s(buf, RCD_GET_UNIT(retU));
	*buf = ']'; buf ++ ;
	*buf = ':'; buf ++ ;
	*buf = ' '; buf ++ ;
	len  = p_un->dir_slen;
	memcpy(buf, p_un->un_dir, (size_t) len);
	buf += len;
	len  = p_un->file_slen;
	memcpy(buf, p_un->un_file, (size_t) len);
	buf += len;
	*buf = '.'; buf ++ ;
	buf  = __rcd_cnv_s(buf, RCD_GET_LINE(retU));
	*buf = ' '; buf ++ ;
	*buf = '['; buf ++ ;
	{
		int rcd;
		rcd = RCD_GET_RCODE(retU);
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
	memcpy(buf, msg, (size_t) mlen);
	buf += mlen;
end:
	*buf = 0;
	return buf;
}
#endif
int
wxedid_rcdGetMsg(struct rcd_scope* scp, rcode retU, char* buf, int bsz) {
	const rcd_unit_t  *p_un;
	const char        *msg;
	int32_t            itmp;
	const rcd_scope_t *rscp;
	if (NULL == scp) {
		scp = (struct rcd_scope*) &__tls_scope;
		__tls_scope.scope = &wxedid_scope;
	}
	rscp = ((tls_rcd_scope_t*) scp)->scope;
	if (0 == RCD_GET_UNIT(retU)) goto fallback;
	if (bsz < rscp->hdr.min_bufsz) goto fallback;
	itmp = rscp->hdr.unit_cnt;
	p_un = (rcd_unit_t*) &rscp->unit_ar[0];
	p_un = (rcd_unit_t*) __rcd_bin_search((char*) p_un, itmp,
												sizeof(rcd_unit_t),
												offsetof(rcd_unit_t, un_id),
												RCD_GET_UNIT(retU) );
	if ( NULL == p_un ) goto fallback;
	msg = NULL;
	if ( RCD_GET_RCODE(retU) != RCD_FVMSG ) {
		rcd_msg_t *pmsg;
		itmp = p_un->msg_cnt;
		pmsg = (rcd_msg_t*) &p_un->msg_ar[0];
		if ( NULL == pmsg ) goto no_msg;
		pmsg = (rcd_msg_t*) __rcd_bin_search((char*) pmsg, itmp,
													sizeof(rcd_msg_t),
													offsetof(rcd_msg_t, lnum),
													RCD_GET_LINE(retU) );
		if ( NULL == pmsg ) goto no_msg;
		msg  = pmsg->msg;
		itmp = pmsg->msg_len;
	} else {
		#if wxedid_RCD_AUTOGEN_MODE == RCD_MODE_FULL
		const rcd_vmsg_t *vmsg;
		vmsg = &((tls_rcd_scope_t*) scp)->vmsg;
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
#else
#pragma GCC diagnostic ignored "-Wunused-parameter"
int
wxedid_rcdGetMsg(struct rcd_scope *scp, rcode rcd, char *buf, int bsz) {
	return RCD_PRINT_BUF(rcd, buf, bsz);
}
#pragma GCC diagnostic warning "-Wunused-parameter"
#endif
#endif
#if wxedid_RCD_AUTOGEN_MODE == RCD_MODE_FULL
void
wxedid_rcdSetMsg(rcode rcd, const char* fmt, ... ) {
	rcd_vmsg_t *vmsg;
	int         len;
	va_list     argp;
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
wxedid_rcdSetMsg(rcode rcd, const char *fmt, ... ) {
	return;
}
#pragma GCC diagnostic warning "-Wunused-parameter"
#endif
__END_DECLS
