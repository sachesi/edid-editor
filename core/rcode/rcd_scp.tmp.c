/* rcode template file: rcd_scp.tmp.c */

#include "%rcd_bname%_rcd_scope.h"

// rcd_autogen: full/basic mode

#include <sys/cdefs.h>
__BEGIN_DECLS

#ifndef %rcd_bname%_RCD_LINK_LIB
int
_rcdGetMsg_(struct rcd_scope* scp, rcode retU, char* buf, int bsz)
__nonnull((3));
#endif

#define RCD_STACK_DEPTH %stk_sz%

#if %rcd_bname%_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
typedef struct __tls_rcd_scope_s tls_rcd_scope_t;

struct __tls_rcd_scope_s {
	const rcd_scope_t  *scope;
	#if %rcd_bname%_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	tls_rcd_scope_t    *cldscp;
	uint32_t            spos;
	// "volatile"/variable message:
	//  valid only right after the rcode is returned
	rcd_vmsg_t          vmsg;
	//  configurable stack size, rcd_autogen: -ssz|--stk_sz option
	rcode               stk[RCD_STACK_DEPTH];
	#endif
};

static __thread tls_rcd_scope_t __tls_scope;
#endif //_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC


struct rcd_scope*
%rcd_bname%_rcdGetScopePtr() {
	#if %rcd_bname%_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
	__tls_scope.scope = &%rcd_bname%_scope;
	return (struct rcd_scope*) &__tls_scope;
	#else
	return NULL;
	#endif
}

uint32_t
%rcd_bname%_rcdGetStackSize() {
	return RCD_STACK_DEPTH;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
void
%rcd_bname%_rcdSetSubScopePtr(struct rcd_scope* scp) {
	#if %rcd_bname%_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
	__tls_scope.scope  = &%rcd_bname%_scope;
	#if %rcd_bname%_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	__tls_scope.cldscp = (tls_rcd_scope_t*) scp;
	#endif
	#endif
}
#pragma GCC diagnostic warning "-Wunused-parameter"

void
%rcd_bname%_rcdResetStack() {
	#if %rcd_bname%_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	tls_rcd_scope_t *tscp = &__tls_scope;
	do {
		//0 means no error occured -> skip scanning scopes
		if (0 == tscp->spos) break;
		tscp->spos  = 0;
		tscp        = __tls_scope.cldscp;
	} while (tscp != NULL);
	#endif
}

rcode
%rcd_bname%_rcdGetStatus() {
	#if %rcd_bname%_RCD_AUTOGEN_MODE == RCD_MODE_FULL
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
//if top of stack is reached, replace last entry
void
%rcd_bname%_rcdPushRcode(rcode retU) {
	#if %rcd_bname%_RCD_AUTOGEN_MODE == RCD_MODE_FULL
	uint32_t  spos = __tls_scope.spos;

	if (spos >= RCD_STACK_DEPTH) spos = (RCD_STACK_DEPTH-1);
	__tls_scope.stk[spos] = retU;
	spos ++ ;
	__tls_scope.spos = spos;
	#endif
}
#pragma GCC diagnostic warning "-Wunused-parameter"


#ifndef %rcd_bname%_RCD_LINK_LIB

#if %rcd_bname%_RCD_AUTOGEN_MODE == RCD_MODE_FULL
//max number of linked stacks:
//prevent infinite loop in case if the chain is messed.
#define STACK_CHAIN_MAX 32
int
%rcd_bname%_rcdGetCallStack(struct rcd_scope* scp, char* buf, int bsz) {
	tls_rcd_scope_t *tscp;
	rcode    retU;
	int32_t  eidx;
	int32_t  m_sz;
	int32_t  m_len;
	uint32_t n_lnk;

	if (NULL == scp) {
		scp = (struct rcd_scope*) &__tls_scope;
		__tls_scope.scope = &%rcd_bname%_scope;
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
			buf[m_sz] = '\n'; //append LF
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
%rcd_bname%_rcdGetCallStack(struct rcd_scope* scp, char* buf, int bsz) {
	return 0;
}

#endif //_RCD_AUTOGEN_MODE
#endif //_RCD_LINK_LIB


#if %rcd_bname%_RCD_AUTOGEN_MODE >= RCD_MODE_BASIC
//full mode (2) -> VMSG defines min buffer size, otherwise
//it's the max length of static message
//the scope->hdr.rcdgen_md variable is injected by rcd_autogen
//RCD_MODE_BASIC comes from rcode.h
int
%rcd_bname%_rcdGetMinMsgBufSz(struct rcd_scope *scp) {
	int bsz;
	tls_rcd_scope_t *p_scp = (tls_rcd_scope_t*) scp;
	__tls_scope.scope = &%rcd_bname%_scope;
	if (NULL == p_scp) {p_scp = &__tls_scope; }
	bsz = ( p_scp->scope->hdr.rcdgen_md > RCD_MODE_BASIC) ? RCD_VMSG_MAX_SZ : p_scp->scope->hdr.min_bufsz;
	return bsz;
}
#else
int
%rcd_bname%_rcdGetMinMsgBufSz(struct rcd_scope* scp) {
	return 0;
}
#endif

#pragma GCC diagnostic warning "-Wunused-parameter"

__END_DECLS
