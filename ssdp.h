#pragma once
#include <stdint.h>

/* Platform specific */
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined (__WINDOWS__)
/* Windows */
#define SSDP_PLATFORM_WINDOWS 1
#include <Winsock2.h>
#include <WS2tcpip.h>
typedef SOCKET ssdp_socket_t;
#elif defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))
/* Any Unix (Linux, macOS, Android) */
#define SSDP_PLATFORM_UNIX 1
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int ssdp_socket_t;
#endif

typedef struct {
	ssdp_socket_t socket;

	char* recvbuf; /* Buffer for receiving multicasts */
	char* sendbuf; /* Intermediate buffer for sending requests or responses */

	int recvbuf_size;
	int sendbuf_size;

	/* Services in SSDP are identified by a unique pairing of a Service Type (ST) URI and a Unique Service Name (USN) URI.
	 *
	 * Unique Service Name (USN) is a URI that uniquely identifies a particular instance of a
	 * service. USNs are used to differentiate between two services with the same service type.
	 * USN can be just an UUID, for example: upnp:uuid:k91d4fae-7dec-11d0-a765-00a0c91c6bf6 */
	char service_name[256];
	/* Service Type (ST) is an URI that identifies the type or function of a particular service,
	 * such as a refrigerator, clock/radio. (Example: upnp:clockradio) */
	char service_type[128];

	uint8_t auto_respond; /* Automatically respond to ssdp:discover requests (if ST matches). Default: enabled. */

	/* Internal */
#if SSDP_PLATFORM_WINDOWS
	uint8_t winsock_initialized;
#endif
	uint8_t recvbuf_free;
	uint8_t sendbuf_free;
	struct ip_mreq mreq;
} ssdp_ctx;

typedef enum {
	SSDP_PT_NONE = 0,
	SSDP_PT_DISCOVER,  /* ssdp:discover */
	SSDP_PT_ALIVE,     /* ssdp:alive */
	SSDP_PT_BYEBYE,    /* ssdp:byebye */
	SSDP_PT_RESPONSE   /* Response to ssdp:discover */
} SSDP_PACKET_TYPE;

typedef struct {
	struct sockaddr_in addr;  /* Sender address */
	SSDP_PACKET_TYPE type;    /* Packet type (request type) */
	char service_name[256];   /* USN (see in comments to ssdp_ctx) */
	char service_type[128];   /* ST (see in comments to ssdp_ctx) */
} ssdp_packet;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize context: allocate buffers, create socket, on Windows also initialize WinSock
 *
 * ctx - pointer to pre allocated memory for ssdp_ctx
 * host_ipv4 - address of network interface on which to open the socket, or 0 for default
 * service_type - ST (see in comments to ssdp_ctx)
 * service_name - USN (see in comments to ssdp_ctx)
 * recvbuf_size - size in bytes of the buffer for receiving SSDP multicasts or 0 to not allocate
 * sendbuf_size - size in bytes of the buffer for sending requests and responses or 0 to not allocate
 * 
 * You may set both buffers manually in ssdp_ctx or not set if not needed
 * (for example, you may not allocate recvbuf if you are not going to receive SSDP multicasts)
 * 
 * Returns 0 on success, -1 on failure.
 * You must call ssdp_ctx_release() even if initialization failed */
int ssdp_ctx_init(ssdp_ctx* ctx, unsigned long host_ipv4, const char* service_type,
	const char* service_name, int recvbuf_size, int sendbuf_size);

/* Release allocated buffers and close socket. On Windows also release WinSock */
void ssdp_ctx_release(ssdp_ctx* ctx);

/* Join SSDP multicast group for receiving SSDP requests.
 * If you will not call this before ssdp_receive() you will not receive any of SSDP requests.
 *
 * Returns 0 on success, -1 on failure. */
int ssdp_begin_receiving(ssdp_ctx* ctx);

/* Leave SSDP multicast group and stop receiving SSDP requests
 *
 * Returns 0 on success, -1 on failure. */
int ssdp_end_receiving(ssdp_ctx* ctx);

/* Receive a SSDP request
 *
 * Returns >0 on success, 0 when received an empty datagram or failed to process it, -1 on failure. */
int ssdp_receive(ssdp_ctx* ctx, ssdp_packet* p);

/* Get packet type string: "ssdp:discovery", "ssdp:alive", "ssdp:byebye" or "response"
 *
 * Returns packet type string or NULL if given packet type value is SSDP_PT_NONE or an invalid value */
const char* ssdp_packet_type_string(SSDP_PACKET_TYPE pt);

/* If Auto Respond is enabled, then when you receive ssdp:discover
 * whose ST matches that of ssdp_ctx the response will be sent */
void ssdp_set_auto_respond(ssdp_ctx* ctx, uint8_t value);

/* Send Unicast response to a ssdp:discover request to inform client that service is available
 *
 * Returns >=0 on success, -1 on failure. */
int ssdp_respond(ssdp_ctx* ctx, ssdp_packet* request);

/* Send Multicast ssdp:discover request to find desired service
 *
 * Returns >=0 on success, -1 on failure. */
int ssdp_discover(ssdp_ctx* ctx);

/* Send Multicast ssdp:alive request to let interested client know of service presence
 *
 * Returns >=0 on success, -1 on failure. */
int ssdp_alive(ssdp_ctx* ctx);

/* Send Multicast ssdp:byebye request to inform client that service is going to deactivate
 *
 * Returns >=0 on success, -1 on failure. */
int ssdp_byebye(ssdp_ctx* ctx);

#ifdef __cplusplus
}
#endif
