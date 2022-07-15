#pragma once
#include <stddef.h>
#include "ssdp.h"

/* return <0 on error, return 0 to continue listening, return >0 to stop listening */
typedef int(*pf_ssdp_listen_callback)(const char* data, int size, const struct sockaddr_in* client, void* param);

/* server socket must be non-blocking */
int ssdp_listen(ssdp_socket_t server, const char* service_type, size_t service_type_len,
	const char* service_name, const char* user_agent, pf_ssdp_listen_callback callback, void* callback_param);

/* return <0 on error, return 0 to continue scanning, return >0 to manually stop scanning */
typedef int(*pf_ssdp_scan_callback)(const char* service_name, const char* user_agent, const struct sockaddr_in* server, void* param);

/* <discover_period_msec> - interval between subsequent ssdp:discover requests
 * <retries> - number of times ssdp:discover requests will be sent before returning
 * client socket must be non-blocking */
int ssdp_scan(ssdp_socket_t client, const char* service_type, size_t service_type_len,
	long discover_period_msec, int retries, pf_ssdp_scan_callback callback, void* callback_param);
