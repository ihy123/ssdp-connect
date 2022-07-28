#include "../ssdp.h"
#include "socket_helper.h"

int set_socket_blocking_mode(ssdp_socket_t s, int value) {
#ifdef SSDP_PLATFORM_WINDOWS
	u_long nonblock = !value;
	return ioctlsocket(s, FIONBIO, &nonblock);
#else
	int nonblock = !value;
	return ioctl(s, FIONBIO, &nonblock);
#endif
}
