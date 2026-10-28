/* Fake RISC OS test of the AcornSSL backend in src/SSL.c.
   Built for 32-bit ARM Linux against riscos-crossdev's fake kernel.h and run
   under qemu-arm-aligntrap (see Makefile): the SWIs are fakes below. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fake.h"
#include "../../../src/SSL.h"

/* ---- the few ClassiCube functions SSL.c uses ---- */
void* Mem_TryAllocCleared(cc_uint32 n, cc_uint32 size) { return calloc(n, size); }
void  Mem_Free(void* mem) { free(mem); }
void* Mem_Copy(void* dst, const void* src, unsigned n) { return memcpy(dst, src, n); }
int String_Length(const char* raw) { return (int)strlen(raw); }
int String_EncodeUtf8(void* data, const cc_string* src) {
	memcpy(data, src->buffer, src->length); ((char*)data)[src->length] = '\0';
	return src->length;
}
void String_AppendConst(cc_string* str, const char* src) {
	while (*src && str->length < str->capacity) str->buffer[str->length++] = *src++;
}
void Platform_Log2(const char* format, const void* a1, const void* a2) { }
/* time from the fake clock: centiseconds */
static unsigned now_ms, sleeps;
cc_uint64 Stopwatch_Measure(void) { return now_ms; }
int Stopwatch_ElapsedMS(cc_uint64 beg, cc_uint64 end) { return (int)(end - beg); }
void Thread_Sleep(cc_uint32 ms) { now_ms += ms; sleeps++; }
int __get_ro_socket(int fd) { return fd + 100; }

/* ---- fake AcornSSL ---- */
#define AcornSSL_Ioctl          0x050F81
#define AcornSSL_Close          0x050F84
#define AcornSSL_Recv           0x050F87
#define AcornSSL_CreateSession  0x050F88
#define AcornSSL_Setsockopt     0x050F8B
#define AcornSSL_Version        0x050F8D
#define AcornSSL_Send           0x050F8F

static const _kernel_oserror e_nomod      = { 0x1E6, "SWI name not known" };
static const _kernel_oserror e_notconn    = { 0x20E00 + 57, "Socket is not connected" };
static const _kernel_oserror e_wouldblock = { 0x20E00 + 35, "Operation would block" };
static const _kernel_oserror e_reset      = { 0x20E00 + 54, "Connection reset by peer" };
static const _kernel_oserror e_cert       = { 0x816400, "Certificate rejected" };
static const _kernel_oserror e_nosock     = { 0x20E00 + 9, "Bad file descriptor" };

static struct {
	int createSock, createFail;
	char host[64]; int hostLen, optLevel, optName;
	int nonblock;
	int handshakeWaits;              /* ENOTCONN this many times */
	const _kernel_oserror* handshakeErr; /* then this (NULL = done) */
	int alwaysNotConn;
	int readWaits, readGive;         /* EWOULDBLOCK this many times, then give bytes */
	int writeWaits, writeTake;
	int closed, closedHandle;
} st;

static const _kernel_oserror* swi_version(int swi, int r[10]) { r[0] = 109; return NULL; }
static const _kernel_oserror* swi_noversion(int swi, int r[10]) { return &e_nomod; }
static const _kernel_oserror* swi_create(int swi, int r[10]) {
	st.createSock = r[0];
	if (st.createFail) return &e_nosock;
	CHECK(r[1] == 0 && r[2] == 0, "CreateSession: new session, no reuse");
	r[0] = 7; return NULL;
}
static const _kernel_oserror* swi_setopt(int swi, int r[10]) {
	CHECK(r[0] == 7, "Setsockopt on the session");
	st.optLevel = r[1]; st.optName = r[2];
	snprintf(st.host, sizeof st.host, "%s", (const char*)r[3]);
	st.hostLen = r[4];
	return NULL;
}
static const _kernel_oserror* swi_ioctl(int swi, int r[10]) {
	CHECK(r[0] == 7 && (unsigned)r[1] == 0x8004667E, "Ioctl FIONBIO on the session");
	st.nonblock = *(int*)r[2]; return NULL;
}
static const _kernel_oserror* swi_recv(int swi, int r[10]) {
	CHECK(r[0] == 7, "Recv on the session");
	if (r[3] & 2) { /* MSG_PEEK: the handshake */
		if (st.alwaysNotConn) return &e_notconn;
		if (st.handshakeWaits) { st.handshakeWaits--; return &e_notconn; }
		if (st.handshakeErr) return st.handshakeErr;
		return &e_wouldblock;
	}
	if (st.alwaysNotConn) return &e_wouldblock;
	if (st.readWaits) { st.readWaits--; return &e_wouldblock; }
	if (st.readGive > r[2]) st.readGive = r[2];
	memset((void*)r[1], 'x', st.readGive);
	r[0] = st.readGive; return NULL;
}
static const _kernel_oserror* swi_send(int swi, int r[10]) {
	CHECK(r[0] == 7, "Send on the session");
	if (st.writeWaits) { st.writeWaits--; return &e_wouldblock; }
	r[0] = st.writeTake < r[2] ? st.writeTake : r[2]; return NULL;
}
static const _kernel_oserror* swi_reset(int swi, int r[10]) { return &e_reset; }
static const _kernel_oserror* swi_close(int swi, int r[10]) {
	st.closed++; st.closedHandle = r[0]; return NULL;
}

static void setup(void) {
	fake_reset();
	memset(&st, 0, sizeof st);
	now_ms = 0; sleeps = 0;
	fake_swi(AcornSSL_Version,       "AcornSSL_Version",       swi_version);
	fake_swi(AcornSSL_CreateSession, "AcornSSL_CreateSession", swi_create);
	fake_swi(AcornSSL_Setsockopt,    "AcornSSL_Setsockopt",    swi_setopt);
	fake_swi(AcornSSL_Ioctl,         "AcornSSL_Ioctl",         swi_ioctl);
	fake_swi(AcornSSL_Recv,          "AcornSSL_Recv",          swi_recv);
	fake_swi(AcornSSL_Send,          "AcornSSL_Send",          swi_send);
	fake_swi(AcornSSL_Close,         "AcornSSL_Close",         swi_close);
}

static cc_string HOST = { (char*)"static.classicube.net", 21, 21 };

static int describe(cc_result res, char* buf, int len) {
	cc_string s = { buf, 0, (cc_uint16)(len - 1) };
	int ok = SSLBackend_DescribeError(res, &s);
	buf[s.length] = '\0';
	return ok;
}

int main(void) {
	void* ctx; cc_result res; cc_uint32 n; char buf[300]; cc_uint8 data[64];

	/* 1. a normal connection */
	setup();
	st.handshakeWaits = 3; st.readWaits = 2; st.readGive = 10; st.writeWaits = 1; st.writeTake = 4;
	res = SSL_Init(5, &HOST, &ctx);
	CHECK(res == 0 && ctx, "connects");
	CHECK(st.createSock == 105, "hands over UnixLib's RISC OS socket handle");
	CHECK(!strcmp(st.host, "static.classicube.net"), "host name for SNI");
	CHECK(st.optLevel == 0xFFFF && st.optName == 0x11E0 && st.hostLen == 4, "SO_ACORNSSL_HOSTNAME as RISC OS wants it");
	CHECK(st.nonblock == 1, "session made non-blocking");
	CHECK(fake_calls(AcornSSL_Recv) == 4 && sleeps == 3, "handshake: sleeps between ENOTCONN tries");
	res = SSL_Read(ctx, data, sizeof data, &n);
	CHECK(res == 0 && n == 10 && sleeps == 5, "read waits through EWOULDBLOCK");
	res = SSL_Write(ctx, data, 9, &n);
	CHECK(res == 0 && n == 4, "write returns a partial count");
	st.readGive = 0;
	res = SSL_Read(ctx, data, sizeof data, &n);
	CHECK(res == 0 && n == 0, "read of 0 = closed by the server");
	SSL_Free(ctx);
	CHECK(st.closed == 1 && st.closedHandle == 7, "free closes the session");

	/* 2. no AcornSSL module */
	setup();
	fake_swi(AcornSSL_Version, "AcornSSL_Version", swi_noversion);
	res = SSL_Init(5, &HOST, &ctx);
	CHECK(res != 0 && ctx == NULL, "fails without the module");
	CHECK(describe(res, buf, sizeof buf) && strstr(buf, "AcornSSL module"), "says the module is needed");
	CHECK(fake_calls(AcornSSL_CreateSession) == 0, "no session tried");

	/* 3. the handshake fails (certificate rejected in AcornSSL's dialogue) */
	setup();
	st.handshakeWaits = 2; st.handshakeErr = &e_cert;
	res = SSL_Init(5, &HOST, &ctx);
	CHECK(res != 0 && ctx != NULL, "handshake error returned, context kept for SSL_Free");
	CHECK(describe(res, buf, sizeof buf) && !strcmp(buf, "AcornSSL: Certificate rejected"), "error text from AcornSSL");
	CHECK(!describe(res + 1, buf, sizeof buf), "other codes not described");
	SSL_Free(ctx);
	CHECK(st.closed == 1, "session closed after a failed handshake");

	/* 4. connection reset while reading */
	setup();
	res = SSL_Init(5, &HOST, &ctx);
	fake_swi(AcornSSL_Recv, "AcornSSL_Recv", swi_reset);
	res = SSL_Read(ctx, data, sizeof data, &n);
	CHECK(res != 0 && n == 0 && sleeps == 0, "reset: error at once, no retry");
	CHECK(describe(res, buf, sizeof buf) && !strcmp(buf, "AcornSSL: Connection reset by peer"), "reset error text");
	SSL_Free(ctx);

	/* 5. the handshake never finishes: times out after 5 minutes */
	setup();
	st.alwaysNotConn = 1;
	res = SSL_Init(5, &HOST, &ctx);
	CHECK(res != 0 && now_ms > 5 * 60 * 1000 && now_ms < 5 * 60 * 1000 + 100, "handshake timeout");
	CHECK(describe(res, buf, sizeof buf) && strstr(buf, "timed out"), "says it timed out");
	SSL_Free(ctx);

	/* 6. a read that never gets data: times out after 60 s */
	setup();
	res = SSL_Init(5, &HOST, &ctx);
	st.alwaysNotConn = 1; now_ms = 0;
	res = SSL_Read(ctx, data, sizeof data, &n);
	CHECK(res != 0 && n == 0 && now_ms > 60000 && now_ms < 60100, "read timeout");
	SSL_Free(ctx);

	/* 7. CreateSession refused: nothing to close */
	setup();
	st.createFail = 1;
	res = SSL_Init(5, &HOST, &ctx);
	CHECK(res != 0 && ctx == NULL, "CreateSession error, no context");
	CHECK(describe(res, buf, sizeof buf) && strstr(buf, "Bad file descriptor"), "CreateSession error text");
	SSL_Free(ctx);
	CHECK(st.closed == 0, "nothing closed");

	return fake_done("test_acornssl");
}
