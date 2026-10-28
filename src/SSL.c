#include "SSL.h"
#include "Errors.h"

#if CC_SSL_BACKEND == CC_SSL_BACKEND_BEARSSL
#include "String_.h"
#include "Certs.h"
#include "../third_party/bearssl/bearssl.h"
#include "../misc/certs/certs.h"

// https://github.com/unkaktus/bearssl/blob/master/samples/client_basic.c#L283
#define SSL_ERROR_SHIFT 0xB5510000

typedef struct SSLContext {
	br_x509_minimal_context xc;
	br_ssl_client_context sc;
	struct X509CertContext x509;
	unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
	br_sslio_context ioc;
	cc_result readError, writeError;
	cc_socket socket;
} SSLContext;
static cc_bool _verifyCerts;

static void x509_start_cert(const br_x509_class** ctx, uint32_t length) {
	struct SSLContext* ssl = (struct SSLContext*)ctx;

	br_x509_minimal_vtable.start_cert(ctx, length);
	Certs_BeginCert(&ssl->x509, length);
}

static void x509_append(const br_x509_class** ctx, const unsigned char* buf, size_t len) {
	struct SSLContext* ssl = (struct SSLContext*)ctx;

	br_x509_minimal_vtable.append(ctx, buf, len);
	Certs_AppendCert(&ssl->x509, buf, len);
}

static void x509_end_cert(const br_x509_class** ctx) {
	struct SSLContext* ssl = (struct SSLContext*)ctx;

	br_x509_minimal_vtable.end_cert(ctx);
	Certs_FinishCert(&ssl->x509);
}

static void x509_start_chain(const br_x509_class** ctx, const char* server_name) {
	struct SSLContext* ssl = (struct SSLContext*)ctx;

	br_x509_minimal_vtable.start_chain(ctx, server_name);
	Certs_BeginChain(&ssl->x509);
}

static unsigned x509_maybe_skip_verify(unsigned r) {
	/* User selected to not care about certificate authenticity */
	if (r == BR_ERR_X509_NOT_TRUSTED && !_verifyCerts) return 0;

	return r;
}

static unsigned x509_maybe_system_verify(struct SSLContext* ssl, unsigned r) {
	/* Only fallback to system verification for potentially untrusted certificate case */
	/* This ensures consistent validation behaviour in other cases between all platforms */
	if (r != BR_ERR_X509_NOT_TRUSTED) return r;
	if (!ssl->x509.numCerts)          return r;

	if (Certs_VerifyChain(&ssl->x509) == 0) r = 0;
	return r;
}

static unsigned x509_end_chain(const br_x509_class** ctx) {
	struct SSLContext* ssl = (struct SSLContext*)ctx;
	
	unsigned r = br_x509_minimal_vtable.end_chain(ctx);
	r = x509_maybe_skip_verify(r);
	r = x509_maybe_system_verify(ssl, r);

	Certs_FreeChain(&ssl->x509);
	return r;
}

static const br_x509_pkey* x509_get_pkey(const br_x509_class*const* ctx, unsigned* usages) {
	return br_x509_minimal_vtable.get_pkey(ctx, usages);
}

static const br_x509_class cert_verifier_vtable = {
	sizeof(br_x509_minimal_context),
	x509_start_chain,
	x509_start_cert,
	x509_append,
	x509_end_cert,
	x509_end_chain,
	x509_get_pkey
};

void SSLBackend_Init(cc_bool verifyCerts) {
	_verifyCerts = verifyCerts;
	CertsBackend_Init();
}

cc_bool SSLBackend_DescribeError(cc_result res, cc_string* dst) {
	switch (res) {
	case SSL_ERROR_SHIFT | BR_ERR_X509_EXPIRED:
		String_AppendConst(dst, "The website's SSL certificate is expired or not yet valid");
		return true;
	case SSL_ERROR_SHIFT | BR_ERR_X509_NOT_TRUSTED:
		String_AppendConst(dst, "The website's SSL certificate was issued by an authority that is not trusted");
		return true;
	}
	return false; // TODO: error codes 
}

static void InjectEntropy(SSLContext* ctx) {
	char buf[32];
	cc_result res = Platform_GetEntropy(buf, 32);
	if (res) Platform_LogConst("SSL: Using insecure uninitialised stack data for entropy");

	br_ssl_engine_inject_entropy(&ctx->sc.eng, buf, 32);
}

static int ssl_time_check_callback(void* ctx,
	uint32_t not_before_days, uint32_t not_before_secs,
	uint32_t not_after_days,  uint32_t not_after_secs) 
{
#ifdef CC_BUILD_CONSOLE
	/* It's fairly common for RTC on older console systems to not be set correctly */
	return 0;
#else
	cc_uint64 cur = DateTime_CurrentUTC();
	uint32_t days = (uint32_t)(cur / 86400) + 366;
	uint32_t secs = (uint32_t)(cur % 86400);

	/* This matches bearssl's default time calculation:
		time_t x = time(NULL);
		vd = (uint32_t)(x / 86400) + 719528;
		vs = (uint32_t)(x % 86400);
	*/

	if (days < not_before_days || (days == not_before_days && secs < not_before_secs)) return -1;
	if (days > not_after_days  || (days == not_after_days  && secs > not_after_secs))  return  1;

	return 0;
#endif
}

static int sock_read(void* ctx_, unsigned char* buf, size_t len) {
	SSLContext* ctx = (SSLContext*)ctx_;
	cc_uint32 read;
	cc_result res = Socket_Read(ctx->socket, buf, len, &read);
	
	if (res) { ctx->readError = res; return -1; }
	/* In case socket has been partially closed (i.e. can't be read from anymore) */
	if (read == 0) { ctx->readError = ERR_END_OF_STREAM; return -1; }

	return read;
}

static int sock_write(void* ctx_, const unsigned char* buf, size_t len) {
	SSLContext* ctx = (SSLContext*)ctx_;
	cc_uint32 wrote;
	cc_result res = Socket_Write(ctx->socket, buf, len, &wrote);
	
	if (res) { ctx->writeError = res; return -1; }
	return wrote;
}

cc_result SSL_Init(cc_socket socket, const cc_string* host_, void** out_ctx) {
	SSLContext* ctx;
	char host[NATIVE_STR_LEN];
	String_EncodeUtf8(host, host_);
	
	ctx = (SSLContext*)Mem_TryAlloc(1, sizeof(SSLContext));
	if (!ctx) return ERR_OUT_OF_MEMORY;
	*out_ctx = (void*)ctx;
	
#if defined CC_BUILD_SYMBIAN
	{
		TAs[3].pkey.key.ec.curve = BR_EC_secp384r1;
		TAs[3].pkey.key.ec.q = (unsigned char *)TA3_EC_Q;
		TAs[3].pkey.key.ec.qlen = sizeof TA3_EC_Q;
		
		TAs[6].pkey.key.ec.curve = BR_EC_secp384r1;
		TAs[6].pkey.key.ec.q = (unsigned char *)TA6_EC_Q;
		TAs[6].pkey.key.ec.qlen = sizeof TA6_EC_Q;
	}
#endif
	
	br_ssl_client_init_full(&ctx->sc, &ctx->xc, TAs, TAs_NUM);
	InjectEntropy(ctx);
	br_x509_minimal_set_time_callback(&ctx->xc, NULL, ssl_time_check_callback);
	ctx->socket = socket;

	br_ssl_engine_set_buffer(&ctx->sc.eng, ctx->iobuf, sizeof(ctx->iobuf), 1);
	br_ssl_client_reset(&ctx->sc, host, 0);
	ctx->xc.vtable = &cert_verifier_vtable;
	
	/* Account login must be done over TLS 1.2 */
	if (String_CaselessEqualsConst(host_, "www.classicube.net")) {
		br_ssl_engine_set_versions(&ctx->sc.eng, BR_TLS12, BR_TLS12);
	}
	
	br_sslio_init(&ctx->ioc, &ctx->sc.eng, 
			sock_read,  ctx, 
			sock_write, ctx);
			
	ctx->readError  = 0;
	ctx->writeError = 0;
	
	return 0;
}

static CC_NOINLINE cc_result SSL_GetError(SSLContext* ctx) {
	int err;
	if (ctx->writeError) return ctx->writeError;
	if (ctx->readError)  return ctx->readError;
		
	// TODO session resumption, proper connection closing ??
	err = br_ssl_engine_last_error(&ctx->sc.eng);
	if (err == 0 && br_ssl_engine_current_state(&ctx->sc.eng) == BR_SSL_CLOSED)
		return SSL_ERR_CONTEXT_DEAD;
		
	return SSL_ERROR_SHIFT | (err & 0xFFFF);
}

cc_result SSL_Read(void* ctx_, cc_uint8* data, cc_uint32 count, cc_uint32* read) { 
	SSLContext* ctx = (SSLContext*)ctx_;
	int res = br_sslio_read(&ctx->ioc, data, count);
	if (res < 0) return SSL_GetError(ctx);	
	
	*read = res;
	return 0;
}

cc_result SSL_Write(void* ctx_, const cc_uint8* data, cc_uint32 count, cc_uint32* sent) {
	SSLContext* ctx = (SSLContext*)ctx_;
	int res = br_sslio_write(&ctx->ioc, data, count);
	if (res < 0) return SSL_GetError(ctx);
	
	br_sslio_flush(&ctx->ioc);
	*sent = res;
	return 0;
}

cc_result SSL_Free(void* ctx_) {
	SSLContext* ctx = (SSLContext*)ctx_;
	if (ctx) br_sslio_close(&ctx->ioc);
	
	Mem_Free(ctx_);
	return 0;
}
#elif CC_SSL_BACKEND == CC_SSL_BACKEND_ACORNSSL
/*
TLS on RISC OS through the AcornSSL module (RISC OS 5.28 or later; 1.09 and
later also do TLS 1.3). This is how riscos-ffmpeg (Reel) and TubeSearch use it:
 - AcornSSL_CreateSession takes over the connected socket (the Internet
   module's handle behind UnixLib's file descriptor);
 - SO_ACORNSSL_HOSTNAME gives the name for SNI and the certificate check;
 - the handshake happens inside Recv/Send. Until it is over they fail with
   ENOTCONN, for longer while AcornSSL's own desktop task asks the user
   about a certificate it can't verify ("Accept always" remembers it).
A blocking SWI would stop every thread in the task, the game included, so
the session is non-blocking (FIONBIO through AcornSSL_Ioctl) and the HTTP
worker thread sleeps between tries instead.
AcornSSL checks certificates itself, against its own store: the
"verify certificates" option has no effect here.
*/
#include "String_.h"
#include "Platform.h"
#include <kernel.h>

/* UnixLib: the RISC OS socket handle behind a file descriptor */
extern int __get_ro_socket(int sockfd);

#define XAcornSSL_Ioctl          (0x050F81 | 0x20000)
#define XAcornSSL_Close          (0x050F84 | 0x20000)
#define XAcornSSL_Recv           (0x050F87 | 0x20000)
#define XAcornSSL_CreateSession  (0x050F88 | 0x20000)
#define XAcornSSL_Setsockopt     (0x050F8B | 0x20000)
#define XAcornSSL_Version        (0x050F8D | 0x20000)
#define XAcornSSL_Send           (0x050F8F | 0x20000)
#define CreateSession_New        0
#define SO_ACORNSSL_HOSTNAME     0x11E0
#define ACORNSSL_SOL_SOCKET      0xFFFF
#define ACORNSSL_FIONBIO         0x8004667E /* _IOW('f', 126, int) */
#define ACORNSSL_MSG_PEEK        0x2

/* Internet module errors are 0x20E00 + a BSD errno */
#define DCI4_BASE        0x20E00
#define DCI4_EINTR       4
#define DCI4_EWOULDBLOCK 35
#define DCI4_EINPROGRESS 36
#define DCI4_ENOTCONN    57

/* How long to wait: the handshake allows time for the user to answer
   AcornSSL's certificate question; reads and writes are like a socket timeout */
#define ACORNSSL_HANDSHAKE_MS (5 * 60 * 1000)
#define ACORNSSL_IO_MS        (60 * 1000)
#define ACORNSSL_POLL_MS      5

/* Results are RISC OS error numbers with this added, so they can be told apart */
#define ACORNSSL_ERR_BASE  0xA5500000UL
#define ACORNSSL_ERR_TIMEOUT (ACORNSSL_ERR_BASE | 0xFFFFF)
#define ACORNSSL_ERR_NOMODULE (ACORNSSL_ERR_BASE | 0xFFFFE)

typedef struct SSLContext {
	int handle;      /* AcornSSL session */
	cc_bool ready;   /* handshake over */
	char host[256];
} SSLContext;

/* Only the HTTP worker thread makes connections, so one is enough */
static char lastError[256];
static cc_result lastErrorCode;

static int DCI4_Errno(const _kernel_oserror* e) {
	return (e->errnum & ~0xFF) == DCI4_BASE ? (int)(e->errnum & 0xFF) : -1;
}

static cc_bool IsRetry(const _kernel_oserror* e) {
	int err = DCI4_Errno(e);
	return err == DCI4_EWOULDBLOCK || err == DCI4_ENOTCONN
		|| err == DCI4_EINPROGRESS || err == DCI4_EINTR;
}

static cc_result SetError(const _kernel_oserror* e, const char* what) {
	int len;
	lastErrorCode = ACORNSSL_ERR_BASE | (e->errnum & 0xFFFFF);
	len = String_Length(e->errmess);
	if (len > 200) len = 200;

	Mem_Copy(lastError, "AcornSSL: ", 10);
	Mem_Copy(lastError + 10, e->errmess, len);
	lastError[10 + len] = '\0';
	if (what) Platform_Log2("AcornSSL %c failed: %c", what, e->errmess);
	return lastErrorCode;
}

static _kernel_oserror* AcornSSL_IO(int swi, int handle, void* buf, int len, int flags, int* done) {
	_kernel_swi_regs r;
	_kernel_oserror* e;
	r.r[0] = handle;
	r.r[1] = (int)buf;
	r.r[2] = len;
	r.r[3] = flags;
	e = _kernel_swi(swi, &r, &r);
	*done = e ? -1 : r.r[0];
	return e;
}

void SSLBackend_Init(cc_bool verifyCerts) { }

cc_bool SSLBackend_DescribeError(cc_result res, cc_string* dst) {
	if (res == ACORNSSL_ERR_TIMEOUT) {
		String_AppendConst(dst, "Secure connection timed out");
		return true;
	}
	if (res == ACORNSSL_ERR_NOMODULE) {
		String_AppendConst(dst, "Secure connections need the AcornSSL module (RISC OS 5.28 or later)");
		return true;
	}
	if (res && res == lastErrorCode) {
		String_AppendConst(dst, lastError);
		return true;
	}
	return false;
}

static cc_result Handshake(SSLContext* ctx) {
	cc_uint64 beg = Stopwatch_Measure();
	_kernel_oserror* e;
	cc_uint8 peek;
	int got;

	for (;;)
	{
		e = AcornSSL_IO(XAcornSSL_Recv, ctx->handle, &peek, 1, ACORNSSL_MSG_PEEK, &got);
		/* data waiting, closed (0), or nothing yet: the handshake is over */
		if (!e || DCI4_Errno(e) == DCI4_EWOULDBLOCK) break;
		if (!IsRetry(e)) return SetError(e, "handshake");

		if (Stopwatch_ElapsedMS(beg, Stopwatch_Measure()) > ACORNSSL_HANDSHAKE_MS)
			return ACORNSSL_ERR_TIMEOUT;
		Thread_Sleep(ACORNSSL_POLL_MS);
	}
	ctx->ready = true;
	return 0;
}

cc_result SSL_Init(cc_socket socket, const cc_string* host, void** out_ctx) {
	_kernel_swi_regs r;
	_kernel_oserror* e;
	SSLContext* ctx;
	int one = 1;
	*out_ctx = NULL;

	if (_kernel_swi(XAcornSSL_Version, &r, &r)) return ACORNSSL_ERR_NOMODULE;

	ctx = (SSLContext*)Mem_TryAllocCleared(1, sizeof(SSLContext));
	if (!ctx) return ERR_OUT_OF_MEMORY;
	String_EncodeUtf8(ctx->host, host);

	r.r[0] = __get_ro_socket(socket);
	r.r[1] = CreateSession_New;
	r.r[2] = 0;
	if ((e = _kernel_swi(XAcornSSL_CreateSession, &r, &r))) {
		Mem_Free(ctx);
		return SetError(e, "CreateSession");
	}
	ctx->handle = r.r[0];
	*out_ctx    = ctx; /* from here SSL_Free closes the session */

	/* SNI, and the certificate must be for this name */
	r.r[0] = ctx->handle;
	r.r[1] = ACORNSSL_SOL_SOCKET;
	r.r[2] = SO_ACORNSSL_HOSTNAME;
	r.r[3] = (int)ctx->host;
	r.r[4] = sizeof(const char*);
	if ((e = _kernel_swi(XAcornSSL_Setsockopt, &r, &r))) return SetError(e, "host name");

	r.r[0] = ctx->handle;
	r.r[1] = ACORNSSL_FIONBIO;
	r.r[2] = (int)&one;
	if ((e = _kernel_swi(XAcornSSL_Ioctl, &r, &r))) return SetError(e, "FIONBIO");

	return Handshake(ctx);
}

cc_result SSL_Read(void* ctx_, cc_uint8* data, cc_uint32 count, cc_uint32* read) {
	SSLContext* ctx = (SSLContext*)ctx_;
	cc_uint64 beg   = Stopwatch_Measure();
	_kernel_oserror* e;
	int got;
	*read = 0;

	for (;;)
	{
		e = AcornSSL_IO(XAcornSSL_Recv, ctx->handle, data, (int)count, 0, &got);
		if (!e) { *read = got; return 0; } /* 0: the server closed it */
		if (!IsRetry(e)) return SetError(e, NULL);

		if (Stopwatch_ElapsedMS(beg, Stopwatch_Measure()) > ACORNSSL_IO_MS)
			return ACORNSSL_ERR_TIMEOUT;
		Thread_Sleep(ACORNSSL_POLL_MS);
	}
}

cc_result SSL_Write(void* ctx_, const cc_uint8* data, cc_uint32 count, cc_uint32* sent) {
	SSLContext* ctx = (SSLContext*)ctx_;
	cc_uint64 beg   = Stopwatch_Measure();
	_kernel_oserror* e;
	int done;
	*sent = 0;

	for (;;)
	{
		e = AcornSSL_IO(XAcornSSL_Send, ctx->handle, (void*)data, (int)count, 0, &done);
		if (!e && done > 0) { *sent = done; return 0; }
		if (e && !IsRetry(e)) return SetError(e, NULL);

		if (Stopwatch_ElapsedMS(beg, Stopwatch_Measure()) > ACORNSSL_IO_MS)
			return ACORNSSL_ERR_TIMEOUT;
		Thread_Sleep(ACORNSSL_POLL_MS);
	}
}

cc_result SSL_Free(void* ctx_) {
	SSLContext* ctx = (SSLContext*)ctx_;
	_kernel_swi_regs r;
	if (!ctx) return 0;

	/* sends close_notify; the socket itself stays open for Socket_Close */
	r.r[0] = ctx->handle;
	_kernel_swi(XAcornSSL_Close, &r, &r);
	Mem_Free(ctx);
	return 0;
}
#else
void SSLBackend_Init(cc_bool verifyCerts) { }
cc_bool SSLBackend_DescribeError(cc_result res, cc_string* dst) { return false; }

cc_result SSL_Init(cc_socket socket, const cc_string* host, void** ctx) {
	return HTTP_ERR_NO_SSL;
}

cc_result SSL_Read(void* ctx, cc_uint8* data, cc_uint32 count, cc_uint32* read) { 
	return ERR_NOT_SUPPORTED; 
}

cc_result SSL_Write(void* ctx, const cc_uint8* data, cc_uint32 count, cc_uint32* sent) {
	return ERR_NOT_SUPPORTED; 
}

cc_result SSL_Free(void* ctx) { return 0; }
#endif
