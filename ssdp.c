#include "ssdp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef SSDP_PLATFORM_UNIX
#include <unistd.h>
#endif

/* SSDP Addresses */
#define SSDP_MULTICAST_IP "239.255.255.250"
#define SSDP_PORT 1900
#define SSDP_MULTICAST_ADDRESS "239.255.255.250:1900"

static struct sockaddr_in ssdp_multicast_addr = { 0 };

/* Invalid socket value */
#define SSDP_INVALID_SOCKET ((ssdp_socket_t)-1)

/* SSDP request headers */
#define h_msearch "M-SEARCH * HTTP/1.1\r\n"
#define h_msearch_size (sizeof(h_msearch) - 1)
#define h_notify "NOTIFY * HTTP/1.1\r\n"
#define h_notify_size (sizeof(h_notify) - 1)
#define h_response "HTTP/1.1 200 OK\r\n"
#define h_response_size (sizeof(h_response) - 1)

/*     +---------------+
		    HELPERS
	   +---------------+     */

/* Adjust start and end indices to skip leading and trailing spaces */
static void trim_spaces(const char* string, int* start, int* end) {
	int i = *start, j = *end;

	while (i <= *end && isspace(string[i])) ++i;
	while (j >= *start && isspace(string[j])) --j;

	if (i <= j) {
		*start = i;
		*end = j;
	}
}

/* Parse line of a SSDP request to the packet struct */
static void parse_line(const char* data, int start, int colon, int end, ssdp_packet* p) {
#ifndef SSDP_PLATFORM_WINDOWS
#define _strnicmp strncmp
#endif

	/* trim field name */
	int fieldStart = start, fieldEnd = colon - 1;
	trim_spaces(data, &fieldStart, &fieldEnd);
	int fieldLen = fieldEnd - fieldStart + 1;
	const char* field = data + fieldStart;

	/* trim value */
	int valueStart = colon + 1, valueEnd = end;
	trim_spaces(data, &valueStart, &valueEnd);
	int valueLen = valueEnd - valueStart + 1;
	const char* value = data + valueStart;

	switch (tolower(field[0])) {
	case 'n':
		if (tolower(field[1]) == 't') {
			if (fieldLen == 2)
				memcpy(p->service_type, value, valueLen < sizeof(p->service_type) ? valueLen : (sizeof(p->service_type) - 1));
			else if (fieldLen == 3 && tolower(field[2]) == 's') {
				if (valueLen == 10 && memcmp(value, "ssdp:alive", 10) == 0)
					p->type = SSDP_PT_ALIVE;
				else if (valueLen == 11 && memcmp(value, "ssdp:byebye", 11) == 0)
					p->type = SSDP_PT_BYEBYE;
			}
		}
		break;
	case 's':
		if (fieldLen == 2 && tolower(field[1]) == 't')
			memcpy(p->service_type, value, valueLen < sizeof(p->service_type) ? valueLen : (sizeof(p->service_type) - 1));
		break;
	case 'u':
		if (fieldLen == 3 && _strnicmp(field + 1, "sn", 2) == 0)
			memcpy(p->service_name, value, valueLen < sizeof(p->service_name) ? valueLen : (sizeof(p->service_name) - 1));
		break;
	case 'm':
		if (fieldLen == 3 && _strnicmp(field + 1, "an", 2) == 0 &&
			valueLen == 13 && memcmp(value, "ssdp:discover", 13) == 0)
			p->type = SSDP_PT_DISCOVER;
		break;
	}
}

/*     +---------------+
		IMPLEMENTATIONS
	   +---------------+     */

int ssdp_ctx_init(ssdp_ctx* ctx, unsigned long host_ipv4, const char* service_type,
	const char* service_name, int recvbuf_size, int sendbuf_size) {
	assert(ctx != NULL);

	memset(ctx, 0, sizeof(*ctx));
	ctx->socket = SSDP_INVALID_SOCKET;
	ctx->auto_respond = 1;

	/* Assign multicast addr */
	if (ssdp_multicast_addr.sin_family == 0) {
		ssdp_multicast_addr.sin_family = AF_INET;
		ssdp_multicast_addr.sin_port = htons(SSDP_PORT);
		ssdp_multicast_addr.sin_addr.s_addr = inet_addr(SSDP_MULTICAST_IP);
	}

	/* Fill the structure needed to join multicast group */
	ctx->mreq.imr_multiaddr = ssdp_multicast_addr.sin_addr;
	ctx->mreq.imr_interface.s_addr = host_ipv4;

	if (service_name != NULL) strncpy(ctx->service_name, service_name, sizeof(ctx->service_name) - 1);
	if (service_type != NULL) strncpy(ctx->service_type, service_type, sizeof(ctx->service_type) - 1);

	/* Init winsock */
#if SSDP_PLATFORM_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		return -1;
	}
	ctx->winsock_initialized = 1;
#endif

	/* create socket */
	ctx->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ctx->socket == SSDP_INVALID_SOCKET)
		return -1;

	/* set reuse address */
	int sockopt = 1;
#if defined(SO_REUSEPORT) && !(defined(__APPLE__) && defined(__MACH__))
	if (setsockopt(ctx->socket, SOL_SOCKET, SO_REUSEPORT, (char*)&sockopt, sizeof(sockopt)) == -1) {

	}
#else
	if (setsockopt(ctx->socket, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof(sockopt)) == -1) {

	}
#endif

	/* bind socket */
	struct sockaddr_in host = {
		.sin_family = AF_INET,
		.sin_port = ssdp_multicast_addr.sin_port,
		.sin_addr = ctx->mreq.imr_interface
	};
	if (bind(ctx->socket, (struct sockaddr*)&host, sizeof(host)) == -1)
		return -1;

	/* allocate buffers */
	if (recvbuf_size > 0) {
		ctx->recvbuf = (char*)calloc(1, recvbuf_size);
		if (ctx->recvbuf == NULL)
			return -1;
		ctx->recvbuf_size = recvbuf_size;
		ctx->recvbuf_free = 1;
	}
	if (sendbuf_size > 0) {
		ctx->sendbuf = (char*)calloc(1, sendbuf_size);
		if (ctx->sendbuf == NULL)
			return -1;
		ctx->sendbuf_size = sendbuf_size;
		ctx->sendbuf_free = 1;
	}

	return 0;
}

void ssdp_ctx_release(ssdp_ctx* ctx) {
	if (ctx->sendbuf_free && ctx->sendbuf != NULL) free(ctx->sendbuf);
	if (ctx->recvbuf_free && ctx->recvbuf != NULL) free(ctx->recvbuf);
	if (ctx->socket != SSDP_INVALID_SOCKET)
#if SSDP_PLATFORM_WINDOWS
	closesocket(ctx->socket);
	if (ctx->winsock_initialized) WSACleanup();
#elif SSDP_PLATFORM_UNIX
	close(ctx->socket);
#endif
}

int ssdp_begin_receiving(ssdp_ctx* ctx) {
	return setsockopt(ctx->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&ctx->mreq, sizeof(ctx->mreq));
}

int ssdp_end_receiving(ssdp_ctx* ctx) {
	return setsockopt(ctx->socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&ctx->mreq, sizeof(ctx->mreq));
}

int ssdp_receive(ssdp_ctx* ctx, ssdp_packet* p) {
	assert(ctx->recvbuf && "Receive buffer must be allocated before receiving SSDP requests");

	/* receive packet */
	char* buf = ctx->recvbuf;
	int addrLen = sizeof(p->addr);
	int len = recvfrom(ctx->socket, buf, ctx->recvbuf_size, 0, (struct sockaddr*)&p->addr, &addrLen);
	if (len <= 0)
		return len;

	/* check packet type */
	int i = 0;
	switch (buf[0]) {
	case 'M':
		if (len > h_msearch_size && memcmp(buf, h_msearch, h_msearch_size) == 0)
			i = h_msearch_size;
		break;
	case 'N':
		if (len > h_notify_size && memcmp(buf, h_notify, h_notify_size) == 0)
			i = h_notify_size;
		break;
	case 'H':
		if (len > h_response_size && memcmp(buf, h_response, h_response_size) == 0) {
			i = h_response_size;
			p->type = SSDP_PT_RESPONSE;
		}
		break;
	}
	if (i == 0) {
		return len;
	}

	/* parse lines */
	int start = i;
	int colon = -1;
	for (; i < len; ++i) {
		if (buf[i] == ':' && colon == -1)	/* colon separates field and its name */
			colon = i;						/* so we need first colon position */
		else if (buf[i] == '\n' && buf[i - 1] == '\r') {
			if (colon > start && /* line must contain a colon */
				colon < i)		 /* value (which is after colon) must not be empty */
				parse_line(buf, start, colon, i, p);
			start = i + 1;
			colon = -1;
		}
	}

	/* send response */
	if (ctx->auto_respond && p->type == SSDP_PT_DISCOVER && strcmp(p->service_type, ctx->service_type) == 0)
		if (ssdp_respond(ctx, p) <= 0)
			return len;

	return len;
}

const char* ssdp_packet_type_string(SSDP_PACKET_TYPE pt) {
	switch (pt) {
		case SSDP_PT_DISCOVER:  return "ssdp:discover";
		case SSDP_PT_ALIVE:     return "ssdp:alive";
		case SSDP_PT_BYEBYE:    return "ssdp:byebye";
		case SSDP_PT_RESPONSE:  return "response";
	}
	return NULL;
}

void ssdp_set_auto_respond(ssdp_ctx* ctx, uint8_t value) {
	ctx->auto_respond = value;
}

int ssdp_respond(ssdp_ctx* ctx, ssdp_packet* request) {
	assert(request->type == SSDP_PT_DISCOVER && "Response can be sent only to a ssdp:discovery request."
		"There is no response to ssdp:alive and ssdp:byebye requests.");
	assert(ctx->sendbuf && "Send buffer must be allocated before sending a request or response");
	int len = snprintf(ctx->sendbuf, ctx->sendbuf_size,
		h_response
		"Ext:\r\n"
		"Cache-Control: no-cache=\"Ext\", max-age=120\r\n"
		"ST: %s\r\n"
		"USN: %s\r\n"
		"\r\n",
		ctx->service_type, ctx->service_name);
	if (len < 0)
		return -1;
	return sendto(ctx->socket, ctx->sendbuf, len, 0, (struct sockaddr*)&request->addr, sizeof(request->addr));
}

int ssdp_discover(ssdp_ctx* ctx) {
	assert(ctx->sendbuf && "Send buffer must be allocated before sending a request or response");
	int len = snprintf(ctx->sendbuf, ctx->sendbuf_size,
		h_msearch
		"Host: " SSDP_MULTICAST_ADDRESS "\r\n"
		"Man: \"ssdp:discover\"\r\n"
		"ST: %s\r\n"
		"MX: 1\r\n"
		"\r\n",
		ctx->service_type);
	if (len < 0)
		return -1;
	return sendto(ctx->socket, ctx->sendbuf, len, 0, (struct sockaddr*)&ssdp_multicast_addr, sizeof(ssdp_multicast_addr));
}

int ssdp_alive(ssdp_ctx* ctx) {
	assert(ctx->sendbuf && "Send buffer must be allocated before sending a request or response");
	int len = snprintf(ctx->sendbuf, ctx->sendbuf_size,
		h_notify
		"Host: " SSDP_MULTICAST_ADDRESS "\r\n"
		"NT: %s\r\n"
		"NTS: ssdp:alive\r\n"
		"USN: %s\r\n"
		"Cache-Control: max-age=120\r\n"
		"\r\n",
		ctx->service_type, ctx->service_name);
	if (len < 0)
		return -1;
	return sendto(ctx->socket, ctx->sendbuf, len, 0, (struct sockaddr*)&ssdp_multicast_addr, sizeof(ssdp_multicast_addr));
}

int ssdp_byebye(ssdp_ctx* ctx) {
	assert(ctx->sendbuf && "Send buffer must be allocated before sending a request or response");
	int len = snprintf(ctx->sendbuf, ctx->sendbuf_size,
		h_notify
		"Host: " SSDP_MULTICAST_ADDRESS "\r\n"
		"NT: %s\r\n"
		"NTS: ssdp:byebye\r\n"
		"USN: %s\r\n"
		"\r\n",
		ctx->service_type, ctx->service_name);
	if (len < 0)
		return -1;
	return sendto(ctx->socket, ctx->sendbuf, len, 0, (struct sockaddr*)&ssdp_multicast_addr, sizeof(ssdp_multicast_addr));
}
