#include "../ssdp-connect.h"
#include <stdio.h>

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
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	/* create server socket */
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	/* set non-blocking */
#ifdef SSDP_PLATFORM_WINDOWS
	u_long nonblock = 1;
	ioctlsocket(s, FIONBIO, &nonblock);
#else
	int nonblock = 1;
	ioctl(s, FIONBIO, &nonblock);
#endif

	/* bind server socket */
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = 0;
	server_addr.sin_port = 0;
	bind(s, (struct sockaddr*)&server_addr, sizeof(server_addr));

	/* announce server's port */
	int namelen = sizeof(server_addr);
	getsockname(s, (struct sockaddr*)&server_addr, &namelen);
	printf("Server on port %hu\n", ntohs(server_addr.sin_port));

	/* find connection through SSDP */
	const char service_type[] = "someservice:type";
	const char service_name[] = "id:123456";
	const char user_agent[] = "Windows 10 PC";
	ssdp_listen(s, service_type, sizeof(service_type) - 1, service_name, user_agent, ssdp_server_example_callback, NULL);

	/* cleanup */
#ifdef SSDP_PLATFORM_WINDOWS
	closesocket(s);
	WSACleanup();
#else
	close(s);
#endif
}

int main() {
	ssdp_server_example();
	return 0;
}
