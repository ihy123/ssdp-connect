#pragma once

/* Services in SSDP are identified by a unique pairing of a Service Type (ST) URI and a Unique Service Name (USN) URI.
 *
 * Service Type (ST) is an URI that identifies the type or function of a particular service,
 * such as a refrigerator, clock/radio. (Example: upnp:clockradio).
 * Unique Service Name (USN) is a URI that uniquely identifies a particular instance of a
 * service. USNs are used to differentiate between two services with the same service type.
 * USN can be just an UUID, for example: upnp:uuid:k91d4fae-7dec-11d0-a765-00a0c91c6bf6
 * User Agent specifies information about the user agent originating the request.
 * There are no limitations on what you can write here, but you should specify
 * here info about your program or library (like that: "CERN-LineMode/2.15 libwww/2.17b3"), maybe about operating system. */

/* Platform specific */
#if defined(SSDP_PLATFORM_WINDOWS) || defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined (__WINDOWS__)

/* Windows */
#define SSDP_PLATFORM_WINDOWS 1

#include <Winsock2.h>
#include <WS2tcpip.h>

typedef SOCKET ssdp_socket_t;

#elif defined(SSDP_PLATFORM_UNIX) || defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))

/* Any Unix (Linux, macOS, Android) */
#define SSDP_PLATFORM_UNIX 1

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef int ssdp_socket_t;

#endif

/* SSDP multicast channel address */
#define SSDP_IP "239.255.255.250"
#define SSDP_PORT 1900

/* SSDP request types returned by ssdp_parse_request() */
typedef enum {
	SSDP_RT_NONE = 0,
	SSDP_RT_DISCOVER,  /* ssdp:discover */
	SSDP_RT_ALIVE,     /* ssdp:alive */
	SSDP_RT_BYEBYE,    /* ssdp:byebye */
	SSDP_RT_RESPONSE   /* Response to ssdp:discover */
} SSDP_REQUEST_TYPE;

#ifdef __cplusplus
extern "C" {
#endif

/* <nonblocking> - set 1 to make socket non-blocking (useful when using select() or poll())
 * Returns socket id on success, -1 on error */
ssdp_socket_t ssdp_socket_init(int nonblocking);

/* Closes ssdp socket. On Unix: close(). On Windows: closesocket() */
int ssdp_socket_release(ssdp_socket_t ssdp_socket);

/* Returns ssdp multicast address to <addr> param */
void ssdp_address(struct sockaddr_in* addr);

/* Parse data received from SSDP socket.
 * Returns 1 on success, 0 if request is unrecognized. */
int ssdp_parse_request(const char* data, int size, SSDP_REQUEST_TYPE* type, char* service_type, int service_type_size,
	char* service_name, int service_name_size, char* user_agent, int user_agent_size);

/* These 4 functions are formatting SSDP response. Writing result to <buffer> param
 * <size> param specifies size of the buffer.
 * Returns are the same as snprintf() [On success returns number of bytes written to buffer excluding zero-terminator] */
int ssdp_discover(const char* service_type, char* buffer, int size);
int ssdp_alive(const char* service_type, const char* service_name, char* buffer, int size);
int ssdp_byebye(const char* service_type, const char* service_name, char* buffer, int size);
int ssdp_response(const char* service_type, const char* service_name, const char* user_agent, char* buffer, int size);

#ifdef __cplusplus
}
#endif
