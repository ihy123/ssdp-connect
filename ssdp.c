#include "ssdp.h"
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#ifdef SSDP_PLATFORM_UNIX
#include <unistd.h>
#endif

/* SSDP multicast address */
#define SSDP_ADDRESS "239.255.255.250:1900"

/* SSDP request headers */
#define h_msearch "M-SEARCH * HTTP/1.1\r\n"
#define h_msearch_size (sizeof(h_msearch) - 1)
#define h_notify "NOTIFY * HTTP/1.1\r\n"
#define h_notify_size (sizeof(h_notify) - 1)
#define h_response "HTTP/1.1 200 OK\r\n"
#define h_response_size (sizeof(h_response) - 1)

#if SSDP_PLATFORM_WINDOWS
#define strncasecmp _strnicmp
#else
#include <sys/ioctl.h>
#include <strings.h>
#define closesocket close
#endif

ssdp_socket_t ssdp_socket_init(int nonblocking) {
	/* create socket */
	ssdp_socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1)
		return -1;

	/* set reuse address */
	int sockopt = 1;
#ifdef SO_REUSEPORT
	setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (char*)&sockopt, sizeof(sockopt));
#else
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof(sockopt));
#endif

	/* set non-blocking */
	if (nonblocking) {
#ifdef SSDP_PLATFORM_WINDOWS
		u_long nonblock = 1;
		if (ioctlsocket(s, FIONBIO, &nonblock) == -1) {
#else
		int nonblock = 1;
		if (ioctl(s, FIONBIO, &nonblock) == -1) {
#endif
			closesocket(s);
			return -1;
		}
	}

	/* bind socket */
	struct sockaddr_in host;
	host.sin_family = AF_INET;
	host.sin_port = htons(SSDP_PORT);
	host.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s, (struct sockaddr*)&host, sizeof(host)) == -1) {
		closesocket(s);
		return -1;
	}

	/* join multicast group */
	struct ip_mreq mreq;
	inet_pton(AF_INET, SSDP_IP, &mreq.imr_multiaddr);
	mreq.imr_interface = host.sin_addr;
	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == -1) {
		closesocket(s);
		return -1;
	}
	return s;
}

int ssdp_socket_release(ssdp_socket_t ssdp_socket) {
	if (ssdp_socket == -1)
		return -1;
	return closesocket(ssdp_socket);
}

void ssdp_address(struct sockaddr_in* addr) {
	assert(addr != NULL);
	addr->sin_family = AF_INET;
	addr->sin_port = htons(SSDP_PORT);
	inet_pton(AF_INET, SSDP_IP, &addr->sin_addr);
}

/* Adjust start and end indices to skip leading and trailing spaces */
static void trim_spaces(const char* string, int* start, int* end) {
	int i = *start, j = *end;

	while (i <= *end && isspace(string[i])) ++i;
	while (j > i && isspace(string[j])) --j;

	*start = i;
	*end = j;
}

/* Parse line of a SSDP request to the packet struct */
static void parse_line(const char* data, int start, int colon, int end,
	SSDP_REQUEST_TYPE* type, char* service_type, int service_type_size,
	char* service_name, int service_name_size, char* user_agent, int user_agent_size) {
	/* trim field name */
	int field_start = start, field_end = colon - 1;
	trim_spaces(data, &field_start, &field_end);
	int field_len = field_end - field_start + 1;
	const char* field = data + field_start;

	/* trim value */
	int value_start = colon + 1, value_end = end;
	trim_spaces(data, &value_start, &value_end);
	int value_len = value_end - value_start + 1;
	const char* value = data + value_start;

	switch (tolower(field[0])) {
	case 'n':
		if (tolower(field[1]) == 't') {
			/* service_type */
			if (service_type && field_len == 2) {
				int len = value_len < service_type_size ? value_len : (service_type_size - 1);
				memcpy(service_type, value, len);
				service_type[len] = '\0';
			}
			/* type */
			else if (type && field_len == 3 && tolower(field[2]) == 's') {
				if (value_len == 10 && memcmp(value, "ssdp:alive", 10) == 0)
					*type = SSDP_RT_ALIVE;
				else if (value_len == 11 && memcmp(value, "ssdp:byebye", 11) == 0)
					*type = SSDP_RT_BYEBYE;
			}
		}
		break;
	case 's':
		/* service_type */
		if (service_type && field_len == 2 && tolower(field[1]) == 't') {
			int len = value_len < service_type_size ? value_len : (service_type_size - 1);
			memcpy(service_type, value, len);
			service_type[len] = '\0';
		}
		break;
	case 'u':
		/* service_name */
		if (service_name && field_len == 3 && strncasecmp(field + 1, "sn", 2) == 0) {
			int len = value_len < service_name_size ? value_len : (service_name_size - 1);
			memcpy(service_name, value, len);
			service_name[len] = '\0';
		}
		/* user_agent */
		else if (user_agent && field_len == 10 && strncasecmp(field + 1, "ser-agent", 9) == 0) {
			int len = value_len < user_agent_size ? value_len : (user_agent_size - 1);
			memcpy(user_agent, value, len);
			user_agent[len] = '\0';
		}
		break;
	case 'm':
		/* type */
		if (type && field_len == 3 && strncasecmp(field + 1, "an", 2) == 0 &&
			value_len == 15 && memcmp(value, "\"ssdp:discover\"", 15) == 0)
			*type = SSDP_RT_DISCOVER;
		break;
	}
}

int ssdp_parse_request(const char* data, int size, SSDP_REQUEST_TYPE* type, char* service_type, int service_type_size,
	char* service_name, int service_name_size, char* user_agent, int user_agent_size) {
	/* check packet type */
	int i = 0;
	switch (data[0]) {
	case 'M':
		if (size > h_msearch_size && memcmp(data, h_msearch, h_msearch_size) == 0)
			i = h_msearch_size;
		break;
	case 'N':
		if (size > h_notify_size && memcmp(data, h_notify, h_notify_size) == 0)
			i = h_notify_size;
		break;
	case 'H':
		if (size > h_response_size && memcmp(data, h_response, h_response_size) == 0) {
			i = h_response_size;
			if (type) *type = SSDP_RT_RESPONSE;
		}
		break;
	}
	if (i == 0)
		return 0;

	/* parse lines */
	int start = i;
	int colon = -1;
	for (; i < size; ++i) {
		if (data[i] == ':' && colon == -1)	/* colon separates field and its name */
			colon = i;						/* so we need first colon position */
		else if (data[i] == '\n' && data[i - 1] == '\r') {
			if (colon > start && /* line must contain a colon */
				colon < i)		 /* value (which is after colon) must not be empty */
				parse_line(data, start, colon, i, type, service_type,
					service_type_size, service_name, service_name_size, user_agent, user_agent_size);
			start = i + 1;
			colon = -1;
		}
	}

	return 1;
}

int ssdp_discover(const char* service_type, char* buffer, int size) {
	assert(service_type && buffer && size > 0);
	return snprintf(buffer, size,
		h_msearch
		"Host: " SSDP_ADDRESS "\r\n"
		"Man: \"ssdp:discover\"\r\n"
		"ST: %s\r\n"
		"MX: 1\r\n"
		"\r\n",
		service_type);
}

int ssdp_alive(const char* service_type, const char* service_name, char* buffer, int size) {
	assert(service_type && service_name && buffer && size > 0);
	return snprintf(buffer, size,
		h_notify
		"Host: " SSDP_ADDRESS "\r\n"
		"NT: %s\r\n"
		"NTS: ssdp:alive\r\n"
		"USN: %s\r\n"
		"Cache-Control: max-age=120\r\n"
		"\r\n",
		service_type, service_name);
}

int ssdp_byebye(const char* service_type, const char* service_name, char* buffer, int size) {
	assert(service_type && service_name && buffer && size > 0);
	return snprintf(buffer, size,
		h_notify
		"Host: " SSDP_ADDRESS "\r\n"
		"NT: %s\r\n"
		"NTS: ssdp:byebye\r\n"
		"USN: %s\r\n"
		"\r\n",
		service_type, service_name);
}

int ssdp_response(const char* service_type, const char* service_name, const char* user_agent, char* buffer, int size) {
	assert(service_type && service_name && buffer && size > 0);
	return snprintf(buffer, size,
		h_response
		"Ext:\r\n"
		"Cache-Control: no-cache=\"Ext\", max-age=120\r\n"
		"ST: %s\r\n"
		"USN: %s\r\n"
		"User-Agent: %s\r\n"
		"\r\n",
		service_type, service_name, user_agent);
}
