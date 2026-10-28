/* Test of Socket_Poll's RISC OS connect check in src/Platform_Posix.c.
   Platform_Posix.c is compiled for ARM Linux with the RISC OS branch on and
   linked with -Wl,--wrap so poll/getpeername/getsockopt are the fakes
   below (the Internet module's behaviour: writable while still connecting). */
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include "../../../src/Core.h"
#include "../../../src/Platform.h"

static int checks, fails;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; fprintf(stderr, "FAIL %d: %s\n", __LINE__, what); } } while (0)

static int writable, connected, soError, peerCalls;
/* UnixLib's poll() is select() underneath: a connecting socket is writable */
int __wrap_poll(struct pollfd* fds, nfds_t n, int timeout) {
	fds[0].revents = writable ? (fds[0].events & (POLLIN | POLLOUT)) : 0;
	return writable ? 1 : 0;
}
int __wrap_getpeername(int s, struct sockaddr* a, socklen_t* l) {
	peerCalls++;
	if (connected) return 0;
	errno = ENOTCONN; return -1;
}
int __wrap_getsockopt(int s, int level, int name, void* v, socklen_t* l) {
	*(int*)v = soError; return 0;
}
/* glibc's headers rename getsockopt to this on 32-bit ARM */
int __wrap___getsockopt64(int s, int level, int name, void* v, socklen_t* l) {
	return __wrap_getsockopt(s, level, name, v, l);
}

int main(void) {
	cc_bool ok; cc_result res;

	writable = 0; connected = 0; soError = 0;
	res = Socket_Poll(3, 0, SOCKET_POLL_WRITE, &ok);
	CHECK(res == 0 && !ok, "not writable: not ready");

	writable = 1; connected = 0; soError = 0;
	res = Socket_Poll(3, 0, SOCKET_POLL_WRITE, &ok);
	CHECK(res == 0 && !ok, "writable but still connecting: not ready (the error 57 case)");

	writable = 1; connected = 1;
	res = Socket_Poll(3, 0, SOCKET_POLL_WRITE, &ok);
	CHECK(res == 0 && ok, "writable and connected: ready");

	writable = 1; connected = 0; soError = ECONNREFUSED;
	res = Socket_Poll(3, 0, SOCKET_POLL_WRITE, &ok);
	CHECK(res == ECONNREFUSED && !ok, "failed connect: its error");

	peerCalls = 0; writable = 1; connected = 0;
	res = Socket_Poll(3, 0, SOCKET_POLL_READ, &ok);
	CHECK(res == 0 && ok && peerCalls == 0, "read polls unchanged");

	printf("test_socketpoll: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}
