#include "../ssdp-connect.h"
#include <stdio.h>
#include <string.h>

#ifdef SSDP_PLATFORM_UNIX
#include <unistd.h>
#include <sys/ioctl.h>
#endif

static int ssdp_server_example_callback(const char* data, int size, const struct sockaddr_in* client, void* param) {
	if (size >= 12 && memcmp(data, "Hello world!", 12) == 0) {
		printf("Connection established with %s:%hu!\n", inet_ntoa(client->sin_addr), ntohs(client->sin_port));
		return 1;
	}
	return 0;
}

void ssdp_server_example() {
#ifdef SSDP_PLATFORM_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("Failed to init WinSock2\n");
		return;
	}
#endif

	/* create server socket */
	ssdp_socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1) {
		printf("Failed to create server socket\n");
		goto End;
	}

	/* create SSDP socket */
	ssdp_socket_t ssdp_sock = ssdp_socket_init(1);
	if (ssdp_sock == -1) {
		printf("Failed to create SSDP socket\n");
		goto End;
	}

	/* set non-blocking */
#ifdef SSDP_PLATFORM_WINDOWS
	u_long nonblock = 1;
	if (ioctlsocket(s, FIONBIO, &nonblock) == -1) {
#else
	int nonblock = 1;
	if (ioctl(s, FIONBIO, &nonblock) == -1) {
#endif
		printf("Failed to make server socket non-blocking\n");
		goto End;
	}

	/* set reuse addr */
	int sockopt = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&sockopt, sizeof(sockopt));

	/* bind server socket */
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = 0;
	if (bind(s, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		printf("Failed to bind server socket\n");
		goto End;
	}

	/* announce server's port */
	int namelen = sizeof(server_addr);
	getsockname(s, (struct sockaddr*)&server_addr, &namelen);
	printf("Server on port %hu\n", ntohs(server_addr.sin_port));

	/* find connection through SSDP */
	const char service_type[] = "someservice:type";
	const char service_name[] = "id:123456";
	const char user_agent[] = "Windows 10 PC";
	ssdp_listen(ssdp_sock, s, service_type, sizeof(service_type) - 1, service_name, user_agent, ssdp_server_example_callback, NULL);

	/* cleanup */
End:
	if (ssdp_sock != -1) ssdp_socket_release(ssdp_sock);
#ifdef SSDP_PLATFORM_WINDOWS
	if (s != -1) closesocket(s);
	WSACleanup();
#else
	if (s != -1) close(s);
#endif
}

int main() {
	ssdp_server_example();
	return 0;
}
