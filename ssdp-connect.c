#include "ssdp-connect.h"
#include <string.h>

#ifdef SSDP_PLATFORM_WINDOWS
#define poll WSAPoll
#else
#include <sys/poll.h>
#endif

#ifdef SSDP_PLATFORM_WINDOWS

#include <profileapi.h>

struct looper {
	LARGE_INTEGER prevtime;
	LARGE_INTEGER curtime;
	LARGE_INTEGER time;
	LARGE_INTEGER period;
};

inline static void looper_start(struct looper* l, long period) {
	LARGE_INTEGER perf_freq;
	QueryPerformanceFrequency(&perf_freq);
	QueryPerformanceCounter(&l->prevtime);
	l->curtime = l->prevtime;
	l->time.QuadPart = period * perf_freq.QuadPart / 1000;
	l->period = l->time;
}

inline static int looper_check(struct looper* l) {
	QueryPerformanceCounter(&l->curtime);
	l->time.QuadPart += l->curtime.QuadPart - l->prevtime.QuadPart;
	l->prevtime = l->curtime;
	return l->time.QuadPart >= l->period.QuadPart;
}

inline static void looper_reset(struct looper* l) {
	l->time.QuadPart = 0;
}

#else

#include <time.h>

struct looper {
	struct timespec prevtime;
	struct timespec curtime;
	long time;
	long period;
};

inline static void looper_start(struct looper* l, long period) {
	clock_gettime(CLOCK_MONOTONIC, &l->prevtime);
	l->curtime = l->prevtime;
	l->period = l->time = period;
}

inline static int looper_check(struct looper* l) {
	clock_gettime(CLOCK_MONOTONIC, &l->curtime);
	l->time += ((l->curtime.tv_sec * 1000) + (l->curtime.tv_nsec / 1000000)) - ((l->prevtime.tv_sec * 1000) + (l->prevtime.tv_nsec / 1000000));
	l->prevtime = l->curtime;
	return l->time >= l->period;
}

inline static void looper_reset(struct looper* l) {
	l->time = 0;
}

#endif

int ssdp_listen(ssdp_socket_t server, const char* service_type, size_t service_type_len,
	const char* service_name, const char* user_agent, pf_ssdp_listen_callback callback, void* callback_param) {
	/* create SSDP socket */
	ssdp_socket_t s = ssdp_socket_init(1);
	if (s == -1)
		return -1;

	/* for socket I/O */
	int result = 0;
	char buffer[512] = { 0 };
	struct sockaddr_in from;
	int fromsize = sizeof(from);

	/* request information */
	SSDP_REQUEST_TYPE req_type;
	char req_svc_type[128];

	/* pollfd for poll() */
	struct pollfd pfd[2];
	pfd[0].fd = s;
	pfd[1].fd = server;
	pfd[1].events = pfd[0].events = POLLIN;

	while (result >= 0) {
		/* wait on SSDP and server socket */
		result = poll(pfd, 2, -1);
		if (result <= 0)
			continue;
		/* receive requests and respond */
		if (pfd[0].revents & POLLIN) {
			pfd[0].revents = 0;
			result = recvfrom(s, buffer, sizeof(buffer), 0, (struct sockaddr*)&from, &fromsize);
			if (result < 0)
				break;
			if (ssdp_parse_request(buffer, result, &req_type, req_svc_type, sizeof(req_svc_type), NULL, 0, NULL, 0) > 0 &&
				req_type == SSDP_RT_DISCOVER && strncmp(req_svc_type, service_type, service_type_len) == 0) {
				result = ssdp_response(service_type, service_name, user_agent, buffer, sizeof(buffer));
				sendto(server, buffer, result, 0, (struct sockaddr*)&from, sizeof(from));
			}
			memset(buffer, 0, sizeof(buffer));
		}
		/* receive data on server socket and forward it to the callback */
		if (pfd[1].revents & POLLIN) {
			pfd[1].revents = 0;
			result = recvfrom(server, buffer, sizeof(buffer), 0, (struct sockaddr*)&from, &fromsize);
			if (result <= 0)
				continue;
			result = callback(buffer, result, &from, callback_param);
			if (result)
				break;
			memset(buffer, 0, sizeof(buffer));
		}
	}

	/* close SSDP socket */
	ssdp_socket_release(s);
	return result;
}

int ssdp_scan(ssdp_socket_t client, const char* service_type, size_t service_type_len,
	long discover_period_msec, int retries, pf_ssdp_scan_callback callback, void* callback_param) {
	/* SSDP multicast address */
	struct sockaddr_in ssdp_addr;
	ssdp_address(&ssdp_addr);

	/* for socket I/O */
	int result = 0;
	char buffer[512];
	struct sockaddr_in from;
	int fromsize = sizeof(from);

	/* request information */
	SSDP_REQUEST_TYPE req_type;
	char req_svc_type[128], req_svc_name[128], req_user_agent[128];

	/* for periodic sending */
	struct looper l;
	looper_start(&l, discover_period_msec);

	/* pollfd for poll() */
	struct pollfd pfd = {
		.fd = client,
		.events = POLLIN
	};

	while (result >= 0) {
		/* send ssdp:discover every N msec */
		if (looper_check(&l)) {
			if (retries-- == 0)
				break;
			looper_reset(&l);
			result = ssdp_discover(service_type, buffer, sizeof(buffer));
			sendto(client, buffer, result, 0, (struct sockaddr*)&ssdp_addr, sizeof(ssdp_addr));
			memset(buffer, 0, sizeof(buffer));
		}
		/* receive response */
		poll(&pfd, 1, discover_period_msec);
		if (pfd.revents & POLLIN) {
			pfd.revents = 0;
			result = recvfrom(client, buffer, sizeof(buffer), 0, (struct sockaddr*)&from, &fromsize);
			if (result <= 0)
				continue;
			ssdp_parse_request(buffer, result, &req_type, req_svc_type, sizeof(req_svc_type),
				req_svc_name, sizeof(req_svc_name), req_user_agent, sizeof(req_user_agent));
			if (req_type == SSDP_RT_RESPONSE && strncmp(req_svc_type, service_type, service_type_len) == 0) {
				result = callback(req_svc_name, req_user_agent, &from, callback_param);
				if (result)
					break;
			}
			memset(buffer, 0, sizeof(buffer));
		}
	}

	return result;
}
