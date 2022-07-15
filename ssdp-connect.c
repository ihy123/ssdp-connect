#include "ssdp-connect.h"
#include <time.h>

#ifdef SSDP_PLATFORM_WINDOWS
#define poll WSAPoll
#endif

int ssdp_listen(ssdp_socket_t server, const char* service_type, size_t service_type_len,
	const char* service_name, const char* user_agent, pf_ssdp_listen_callback callback, void* callback_param) {
	/* create SSDP socket */
	ssdp_socket_t s = ssdp_socket_init(1);

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
				result = sendto(server, buffer, result, 0, (struct sockaddr*)&from, sizeof(from));
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
	clock_t prevtime = clock();
	clock_t curtime;
	clock_t time = (clock_t)(discover_period_msec / 1000.0 * CLOCKS_PER_SEC);
	const clock_t period = time;

	/* pollfd for poll() */
	struct pollfd pfd;
	pfd.fd = client;
	pfd.events = POLLIN;

	while (result >= 0) {
		/* send ssdp:discover every N sec */
		curtime = clock();
		time += curtime - prevtime;
		prevtime = curtime;
		if (time >= period) {
			if (retries-- == 0)
				break;
			time = 0;
			result = ssdp_discover(service_type, buffer, sizeof(buffer));
			result = sendto(client, buffer, result, 0, (struct sockaddr*)&ssdp_addr, sizeof(ssdp_addr));
			memset(buffer, 0, sizeof(buffer));
		}
		/* receive response */
		result = poll(&pfd, 1, discover_period_msec);
		if (result <= 0)
			continue;
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
