#include "../ssdp-connect.h"
#include <stdio.h>
#include "socket_helper.h"

#ifdef SSDP_PLATFORM_UNIX
#include <unistd.h>
#include <sys/ioctl.h>
#endif

static int server_count = 0;
static struct sockaddr_in server_list[10];

static int ssdp_scan_callback(const char* service_name, const char* user_agent, const struct sockaddr_in* server, void* param) {
	/* dont duplicate */
	for (int i = 0; i < server_count; ++i)
		if (server_list[i].sin_port == server->sin_port && server_list[i].sin_addr.s_addr == server->sin_addr.s_addr)
			return 0;
	server_list[server_count++] = *server;
	printf("%d) %s\n", server_count, user_agent);
	return 0;
}

void ssdp_client_example() {
#ifdef SSDP_PLATFORM_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("Failed to init WinSock2\n");
		return;
	}
#endif

	/* create client socket */
	ssdp_socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1) {
		printf("Failed to create client socket\n");
		goto End;
	}

	/* set non-blocking */
	if (set_socket_blocking_mode(s, 1) == -1) {
		printf("Failed to make client socket non-blocking\n");
		goto End;
	}

	/* set reuse addr */
	int sockopt = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof(sockopt));

	/* bind client socket */
	struct sockaddr_in host;
	host.sin_family = AF_INET;
	host.sin_addr.s_addr = htonl(INADDR_ANY);
	host.sin_port = 0;
	if (bind(s, (struct sockaddr*)&host, sizeof(host)) == -1) {
		printf("Failed to bind client socket\n");
		goto End;
	}

	/* announce client's port */
	int namelen = sizeof(host);
	getsockname(s, (struct sockaddr*)&host, &namelen);
	printf("Client on port %hu\n", ntohs(host.sin_port));

	printf("Available servers:\n");

	/* find servers through SSDP */
	const char service_type[] = "someservice:type";
	ssdp_scan(s, service_type, sizeof(service_type) - 1, 3000, 3, ssdp_scan_callback, NULL);

	if (server_count) {
		printf("Choose server to connect: ");
		int choice;
		scanf("%d", &choice);
		if (choice > 0 && choice <= server_count)
			sendto(s, "Hello world!", 12, 0, (struct sockaddr*)&server_list[choice - 1], sizeof(struct sockaddr_in));
		else
			printf("Invalid server index: %d\n", choice);
	}
	else
		printf("No servers found.\n");

	/* cleanup */
End:
#ifdef SSDP_PLATFORM_WINDOWS
	if (s != -1) closesocket(s);
	WSACleanup();
#else
	if (s != -1) close(s);
#endif
}

int main() {
	ssdp_client_example();
	return 0;
}
